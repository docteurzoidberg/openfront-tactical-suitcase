#!/usr/bin/env python3
"""Phase 3.4: Production Main + Production Audio — Live Game Mock (no cantest)

This script validates the "prod main ↔ prod audio" integration path while
mocking the userscript/game via WebSocket events.

It:
- Reads serial logs from BOTH boards (main controller + audio module)
- Derives main controller IP from serial logs (or accepts --ws-host)
- Connects to firmware WSS server and sends a representative event stream
- Asserts expected behaviors end-to-end:
  - CAN discovery triggers audio MODULE_ANNOUNCE
  - main discovers audio module
  - audio emits periodic SOUND_STATUS (0x426)
  - SOUND_PLAY event triggers CAN PLAY_SOUND, audio ACK, and SOUND_FINISHED

Designed to be automation-friendly and stdlib-only (no pyserial).

Usage example:
  ./ots-fw-cantest/tools/test_production_live_game.py \
    --main-port /dev/ttyACM0 --audio-port /dev/ttyUSB0

"""

from __future__ import annotations

import argparse
import collections
import datetime as _dt
import json
import os
import re
import sys
import queue
import socket
import threading
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Optional


# Import stdlib-only WSS + serial helpers from ots-fw-main/tools
_REPO_ROOT = Path(__file__).resolve().parents[2]
_FW_MAIN_TOOLS = _REPO_ROOT / "ots-fw-main" / "tools"
sys.path.insert(0, str(_FW_MAIN_TOOLS))

from ots_device_tool import (  # type: ignore
    OtsTestError,
    SerialLogWatcher,
    SerialPort,
    WsClient,
    derive_ip_from_serial,
)


def _sanitize_terminal_text(text: str) -> str:
    """Remove/control-escape unsafe characters from device logs.

    Some firmwares emit terminal escape sequences (e.g. `\x1b[0n` device status
    queries). If printed raw, those sequences can be interpreted by the local
    terminal and corrupt the operator session (including injecting stray input).

    We keep the content readable while ensuring it is safe to print.
    """

    if not text:
        return text

    # Drop carriage returns to avoid confusing the live log layout.
    text = text.replace("\r", "")

    # Make ESC and a few common control bytes visible & inert.
    text = text.replace("\x1b", "\\x1b")
    text = text.replace("\x07", "\\x07")  # BEL
    text = text.replace("\x08", "\\x08")  # BS

    # Collapse other non-printable ASCII control chars (except tab/newline).
    text = re.sub(r"[\x00-\x08\x0b\x0c\x0e-\x1f\x7f]", "?", text)
    return text


class LogWriter:
    """Persist logs for post-run analysis.

    Writes per-tag logs plus a merged log (all tags interleaved).
    """

    def __init__(self, output_dir: Path):
        self.output_dir = output_dir
        self.output_dir.mkdir(parents=True, exist_ok=True)

        self._lock = threading.Lock()
        self._merged = (self.output_dir / "merged.log").open("w", encoding="utf-8")
        self._by_tag = {
            "TEST": (self.output_dir / "test.log").open("w", encoding="utf-8"),
            "MAIN": (self.output_dir / "main.log").open("w", encoding="utf-8"),
            "AUDIO": (self.output_dir / "audio.log").open("w", encoding="utf-8"),
        }

    def close(self) -> None:
        with self._lock:
            try:
                self._merged.close()
            except Exception:
                pass
            for fh in self._by_tag.values():
                try:
                    fh.close()
                except Exception:
                    pass

    def log(self, tag: str, text: str) -> None:
        ts = _dt.datetime.now().isoformat(timespec="milliseconds")
        safe = _sanitize_terminal_text(text)
        line = f"{ts} [{tag}] {safe}\n"
        with self._lock:
            self._merged.write(line)
            self._merged.flush()
            fh = self._by_tag.get(tag)
            if fh is not None:
                fh.write(line)
                fh.flush()


class LiveLogPrinter:
    """Real-time printer for [TEST]/[MAIN]/[AUDIO] lines.

    Uses a dedicated thread so serial reader threads never block on stdout.
    """

    _RESET = "\033[0m"
    _DIM = "\033[2m"
    _COLORS = {
        "TEST": "\033[33;1m",  # bright yellow
        "MAIN": "\033[36;1m",  # bright cyan
        "AUDIO": "\033[35;1m",  # bright magenta
    }

    def __init__(self, *, enable: bool, color: bool, writer: Optional[LogWriter] = None):
        self._enable = enable
        self._color = color
        self._writer = writer
        self._q: "queue.Queue[tuple[str, str]]" = queue.Queue()
        self._stop = threading.Event()
        self._thread = threading.Thread(target=self._run, daemon=True)

    def start(self) -> None:
        if not self._enable:
            return
        self._thread.start()

    def stop(self) -> None:
        if not self._enable:
            if self._writer is not None:
                self._writer.close()
            return
        self._stop.set()
        try:
            self._q.put_nowait(("TEST", ""))
        except Exception:
            pass
        self._thread.join(timeout=1.0)
        if self._writer is not None:
            self._writer.close()

    def log(self, tag: str, text: str) -> None:
        if self._stop.is_set():
            return

        if self._writer is not None:
            self._writer.log(tag, text)

        if not self._enable:
            return

        self._q.put((tag, _sanitize_terminal_text(text)))

    def _format_prefix(self, tag: str) -> str:
        if not self._color:
            return f"[{tag}]"
        c = self._COLORS.get(tag, "")
        return f"{c}[{tag}]{self._RESET}"

    def _run(self) -> None:
        while not self._stop.is_set():
            try:
                tag, text = self._q.get(timeout=0.25)
            except queue.Empty:
                continue

            if self._stop.is_set():
                return

            prefix = self._format_prefix(tag)
            if tag == "TEST" and self._color:
                line = f"{self._DIM}{prefix}{self._RESET} {text}"
            else:
                line = f"{prefix} {text}"
            print(line, flush=True)


def _best_effort_pulse_reset(port: str, baud: int) -> None:
    """Best-effort reset pulse using serial modem control lines.

    Many ESP32 USB-UART adapters wire DTR/RTS to the auto-reset circuit.
    This is intentionally best-effort: if it fails, we keep going.
    """

    try:
        import array
        import fcntl
        import termios

        sp = SerialPort(port=port, baud=baud)
        sp.open()
        try:
            if not (hasattr(termios, "TIOCMGET") and hasattr(termios, "TIOCMSET")):
                return
            if not (hasattr(termios, "TIOCM_DTR") and hasattr(termios, "TIOCM_RTS")):
                return

            buf = array.array("i", [0])
            fcntl.ioctl(sp.fd, termios.TIOCMGET, buf, True)
            status = int(buf[0])

            def set_lines(*, dtr: bool, rts: bool) -> None:
                nonlocal status
                if dtr:
                    status |= termios.TIOCM_DTR
                else:
                    status &= ~termios.TIOCM_DTR
                if rts:
                    status |= termios.TIOCM_RTS
                else:
                    status &= ~termios.TIOCM_RTS
                buf[0] = status
                fcntl.ioctl(sp.fd, termios.TIOCMSET, buf, True)

            # Common reset pulse: assert reset briefly then release.
            # (In many auto-reset circuits, RTS is inverted to EN.)
            set_lines(dtr=False, rts=True)
            time.sleep(0.12)
            # IMPORTANT: some USB-UART adapters (notably CP2102 on ttyUSB*)
            # gate UART output unless RTS is asserted. Conversely, CDC ACM
            # devices can have RTS wired to EN/reset and may reset-loop if
            # asserted. Restore lines accordingly.
            if "ttyUSB" in port:
                set_lines(dtr=True, rts=True)
            else:
                set_lines(dtr=True, rts=False)
            time.sleep(0.12)
        finally:
            sp.close()
    except Exception:
        return


def _ts_ms() -> int:
    return int(time.time() * 1000)


def _sleep_s(seconds: float) -> None:
    if seconds <= 0:
        return
    time.sleep(seconds)


def build_handshake(client_type: str) -> str:
    return json.dumps({"type": "handshake", "clientType": client_type}, separators=(",", ":"))


def build_event(event_type: str, message: str, data: Optional[dict[str, Any]] = None, ts_ms: Optional[int] = None) -> str:
    payload: dict[str, Any] = {
        "type": event_type,
        "timestamp": ts_ms if ts_ms is not None else _ts_ms(),
        "message": message,
    }
    if data is not None:
        payload["data"] = data
    return json.dumps({"type": "event", "payload": payload}, separators=(",", ":"))


def _try_parse_ws_json(payload: bytes) -> Optional[dict[str, Any]]:
    try:
        return json.loads(payload.decode("utf-8"))
    except Exception:
        return None


@dataclass
class Ports:
    main_port: str
    audio_port: str


@dataclass
class CheckItem:
    name: str
    ok: bool
    detail: str
    duration_s: float


@dataclass
class RunReport:
    started_at: str
    ended_at: str
    ok: bool
    ws_host: str
    ws_port: int
    ws_path: str
    ports: Ports
    output_dir: str
    checks: list[CheckItem]
    error: str = ""

    def to_dict(self) -> dict[str, Any]:
        return {
            "startedAt": self.started_at,
            "endedAt": self.ended_at,
            "ok": self.ok,
            "ws": {"host": self.ws_host, "port": self.ws_port, "path": self.ws_path},
            "ports": {"main": self.ports.main_port, "audio": self.ports.audio_port},
            "outputDir": self.output_dir,
            "checks": [
                {
                    "name": c.name,
                    "ok": c.ok,
                    "detail": c.detail,
                    "durationS": c.duration_s,
                }
                for c in self.checks
            ],
            "error": self.error,
        }


def _write_report(output_dir: Path, report: RunReport) -> None:
    (output_dir / "report.json").write_text(json.dumps(report.to_dict(), indent=2) + "\n", encoding="utf-8")

    lines: list[str] = []
    lines.append("=== Phase 3.4 Report ===")
    lines.append(f"Result      : {'PASS' if report.ok else 'FAIL'}")
    lines.append(f"Started     : {report.started_at}")
    lines.append(f"Ended       : {report.ended_at}")
    lines.append(f"Main port   : {report.ports.main_port}")
    lines.append(f"Audio port  : {report.ports.audio_port}")
    lines.append(f"WSS         : wss://{report.ws_host}:{report.ws_port}{report.ws_path}")
    lines.append(f"Artifacts   : {output_dir}")
    lines.append("\nChecks:")
    for c in report.checks:
        status = "OK" if c.ok else "FAIL"
        lines.append(f"- {status:<4} {c.name} ({c.duration_s:.2f}s) :: {c.detail}")
    if report.error:
        lines.append("\nError:")
        lines.append(report.error)
    (output_dir / "report.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")


def _print_match(printer: Optional[LiveLogPrinter], prefix: str, line: str) -> None:
    if printer is not None:
        printer.log(prefix, f"✓ {line}")
    else:
        print(f"[{prefix}] ✓ {line}", flush=True)


def _poll_for_new_serial_match(
    watcher: SerialLogWatcher,
    pattern: str,
    *,
    timeout_s: float,
    initial_seen: set[str],
    poll_interval_s: float = 0.05,
    tail_lines_on_fail: int = 120,
) -> str:
    """Wait for a regex match that appears *after* the initial snapshot.

    This avoids matching stale buffered lines when the device has been running
    for a while and logs are noisy.
    """

    rx = re.compile(pattern)
    end_by = time.monotonic() + timeout_s
    while True:
        if time.monotonic() >= end_by:
            tail = watcher.last_lines(tail_lines_on_fail)
            raise OtsTestError(
                f"Timed out waiting for serial match: {pattern}\n"
                f"--- tail ({tail_lines_on_fail}) ---\n{tail}"
            )

        buf = watcher.last_lines(2000)
        for line in buf.splitlines():
            if line in initial_seen:
                continue
            initial_seen.add(line)
            if rx.search(line):
                return line

        _sleep_s(poll_interval_s)


def _troop_update_data(current: int, max_troops: int, attack_ratio: float) -> dict[str, Any]:
    # Firmware expects currentTroops/maxTroops/attackRatio.
    # Spec uses current/max; include both to be resilient.
    return {
        "currentTroops": current,
        "maxTroops": max_troops,
        "attackRatio": attack_ratio,
        "current": current,
        "max": max_troops,
    }


def _sound_payload(sound_id: str, *, interrupt: bool, priority: str, request_id: str, context: dict[str, Any]) -> dict[str, Any]:
    # Spec shape is soundId/priority/interrupt/requestId.
    # Firmware also supports optional soundIndex (preferred) for determinism.
    payload: dict[str, Any] = {
        "soundId": sound_id,
        "interrupt": interrupt,
        "priority": priority,
        "requestId": request_id,
        "context": context,
    }

    # For production-live integration tests, always use deterministic embedded tone.
    # This avoids SD-card dependence and keeps the test stable.
    payload["soundIndex"] = 10000  # Embedded 440Hz 1s test tone in audio firmware
    payload["test"] = True

    return payload


def _send_sound_and_wait(
    *,
    client: WsClient,
    main_w: Optional[SerialLogWatcher],
    audio_w: SerialLogWatcher,
    printer: Optional[LiveLogPrinter] = None,
    sound_id: str,
    message: str,
    interrupt: bool,
    priority: str,
    request_id: str,
    context: dict[str, Any],
    ack_timeout_s: float = 2.5,
    finish_timeout_s: float = 4.0,
) -> None:
    """Send a SOUND_PLAY and wait for audio module to ACK and finish."""

    audio_seen = set(audio_w.last_lines(2000).splitlines())
    main_seen: Optional[set[str]] = None
    if main_w is not None:
        main_seen = set(main_w.last_lines(2000).splitlines())

    if printer is not None:
        printer.log(
            "TEST",
            f"WS send SOUND_PLAY soundId={sound_id} interrupt={interrupt} priority={priority}",
        )

    client.send_text(
        build_event(
            "SOUND_PLAY",
            message,
            data=_sound_payload(
                sound_id,
                interrupt=interrupt,
                priority=priority,
                request_id=request_id,
                context=context,
            ),
        )
    )

    # Main firmware should receive SOUND_PLAY and queue the CAN request.
    # (Optional: some deployments may have different log verbosity.)
    if main_w is not None:
        _poll_for_new_serial_match(main_w, r"Received SOUND_PLAY event", timeout_s=ack_timeout_s, initial_seen=main_seen)
        _poll_for_new_serial_match(main_w, r"Queued PLAY_SOUND: index=\d+", timeout_s=ack_timeout_s, initial_seen=main_seen)

    # Wait for the audio module to receive the CAN PLAY_SOUND and respond.
    _poll_for_new_serial_match(audio_w, r"CAN_AUDIO: PLAY_SOUND: index=\d+", timeout_s=ack_timeout_s, initial_seen=audio_seen)
    ack_line = _poll_for_new_serial_match(audio_w, r"CAN_AUDIO: Sent ACK: ok=([01])", timeout_s=ack_timeout_s, initial_seen=audio_seen)
    if "ok=0" in ack_line:
        raise OtsTestError(f"Audio module failed SOUND_PLAY (ACK ok=0): {ack_line}")

    # For one-shot sounds we expect a finish event.
    _poll_for_new_serial_match(audio_w, r"CAN_AUDIO: Sound finished: queue_id=\d+", timeout_s=finish_timeout_s, initial_seen=audio_seen)


def run_scenario_minimal(
    client: WsClient,
    main_w: SerialLogWatcher,
    audio_w: SerialLogWatcher,
    nuke_unit_out: int,
    nuke_unit_in: int,
    *,
    printer: Optional[LiveLogPrinter] = None,
) -> None:
    """Send a compact, representative stream of userscript events."""

    # Identify as userscript.
    client.send_text(build_handshake("userscript"))

    # Firmware also recognizes the userscript via this INFO message.
    client.send_text(build_event("INFO", "userscript-connected"))

    # Minimal lifecycle.
    if printer is not None:
        printer.log("TEST", "WS send GAME_SPAWNING")
    client.send_text(build_event("GAME_SPAWNING", "spawning", data={"playerID": "player-xyz", "playerName": "MockUser"}))
    _sleep_s(0.15)

    if printer is not None:
        printer.log("TEST", "WS send GAME_START")
    client.send_text(build_event("GAME_START", "game-start"))

    # A couple troop updates (keep low frequency; TROOP_UPDATE can be very spammy).
    for i in range(3):
        client.send_text(
            build_event(
                "TROOP_UPDATE",
                "troops",
                data={
                    "currentTroops": 2500 + i * 750,
                    "maxTroops": 12141,
                    "attackRatio": 0.20 + 0.05 * i,
                },
            )
        )
        _sleep_s(0.25)

    # Threat + nuke tracking (exercise unitID-based state machines).
    main_seen = set(main_w.last_lines(2000).splitlines())
    if printer is not None:
        printer.log("TEST", f"WS send ALERT_ATOM nukeUnitID={nuke_unit_in}")
    client.send_text(build_event("ALERT_ATOM", "incoming-atom", data={"nukeType": "atom", "nukeUnitID": nuke_unit_in}))
    _poll_for_new_serial_match(main_w, rf"Atom alert! \(unit={nuke_unit_in}\)", timeout_s=4.0, initial_seen=main_seen)
    _sleep_s(0.25)

    main_seen = set(main_w.last_lines(2000).splitlines())
    if printer is not None:
        printer.log("TEST", f"WS send NUKE_LAUNCHED atom nukeUnitID={nuke_unit_out}")
    client.send_text(
        build_event(
            "NUKE_LAUNCHED",
            "atom-launched",
            data={"nukeType": "atom", "nukeUnitID": nuke_unit_out},
        )
    )
    _poll_for_new_serial_match(main_w, rf"Nuke launched: .*unit={nuke_unit_out}", timeout_s=4.0, initial_seen=main_seen)

    _send_sound_and_wait(
        client=client,
        main_w=main_w,
        audio_w=audio_w,
        printer=printer,
        sound_id="game_start",
        message="game-start-sound",
        interrupt=True,
        priority="high",
        request_id="min-game-start",
        context={"phase": "GAME_START"},
    )

    # Resolve nukes to exercise state clearing.
    client.send_text(build_event("NUKE_EXPLODED", "impact", data={"unitID": nuke_unit_out, "nukeUnitID": nuke_unit_out, "nukeType": "atom"}))
    _sleep_s(0.25)
    client.send_text(build_event("NUKE_INTERCEPTED", "intercept", data={"unitID": nuke_unit_in, "nukeUnitID": nuke_unit_in, "nukeType": "atom"}))

    _sleep_s(0.25)
    client.send_text(build_event("GAME_END", "game-end", data={"victory": False}))


def main() -> int:
    parser = argparse.ArgumentParser(description="Phase 3.4: Prod main + prod audio live-game mock")
    parser.add_argument("--main-port", required=True, help="Main controller serial port (e.g. /dev/ttyACM0)")
    parser.add_argument("--audio-port", required=True, help="Audio module serial port (e.g. /dev/ttyUSB0)")
    parser.add_argument("--baud", type=int, default=115200, help="Serial baud rate")

    parser.add_argument(
        "--no-live-logs",
        action="store_true",
        help="Disable real-time [TEST]/[MAIN]/[AUDIO] streaming output (default: enabled)",
    )
    parser.add_argument(
        "--no-color",
        action="store_true",
        help="Disable ANSI colors for live logs (default: enabled)",
    )

    parser.add_argument(
        "--output-dir",
        default="",
        help="Directory to write log artifacts (default: ots-fw-cantest/logs/prod-live-game/<timestamp>)",
    )
    parser.add_argument(
        "--no-save-logs",
        action="store_true",
        help="Do not write any log artifacts (default: write logs + report)",
    )

    parser.add_argument(
        "--verbose",
        dest="verbose",
        action="store_true",
        default=True,
        help="Verbose TEST progress logging (default: enabled)",
    )
    parser.add_argument(
        "--no-verbose",
        "--quiet",
        dest="verbose",
        action="store_false",
        help="Reduce TEST progress logging (serial live logs still shown unless --no-live-logs)",
    )

    parser.add_argument("--ws-host", default="", help="Firmware host/IP (if empty, derived from main serial logs)")
    parser.add_argument("--ws-port", type=int, default=443, help="Firmware WSS port (default: 443)")
    parser.add_argument("--ws-path", default="/ws", help="Firmware WS path (default: /ws)")
    parser.add_argument("--insecure", action="store_true", default=True, help="Allow self-signed TLS (default: true)")

    parser.add_argument(
        "--no-reboot-for-ip",
        action="store_true",
        help="When deriving IP from serial logs, do NOT reboot the main controller first (default is to reboot so we capture boot logs).",
    )

    parser.add_argument("--timeout", type=float, default=35.0, help="Overall timeout budget for waits")

    parser.add_argument(
        "--no-reset-audio",
        action="store_true",
        help="Do not attempt to reset audio module at start (default: reset best-effort)",
    )

    args = parser.parse_args()

    live_enabled = not args.no_live_logs
    term_ok = os.environ.get("TERM", "") not in ("", "dumb")
    color_enabled = (not args.no_color) and term_ok

    # Default artifact directory lives under ots-fw-cantest/logs/.
    if args.output_dir.strip():
        output_dir = Path(args.output_dir).expanduser().resolve()
    else:
        run_id = _dt.datetime.now().strftime("%Y%m%d-%H%M%S")
        output_dir = Path(__file__).resolve().parents[1] / "logs" / "prod-live-game" / run_id

    writer: Optional[LogWriter] = None
    if not args.no_save_logs:
        writer = LogWriter(output_dir)

    printer = LiveLogPrinter(enable=live_enabled, color=color_enabled, writer=writer)

    def _alog(msg: str) -> None:
        if live_enabled:
            printer.log("TEST", msg)
        else:
            print(msg, flush=True)

    def tlog(msg: str) -> None:
        if not args.verbose:
            return
        _alog(msg)

    ports = Ports(main_port=args.main_port, audio_port=args.audio_port)

    started_at = _dt.datetime.now().isoformat(timespec="seconds")
    checks: list[CheckItem] = []

    def _check(name: str, fn) -> Any:
        t0 = time.monotonic()
        try:
            detail = fn()
            d = str(detail) if detail is not None else ""
            checks.append(CheckItem(name=name, ok=True, detail=d, duration_s=time.monotonic() - t0))
            return detail
        except Exception as e:
            checks.append(CheckItem(name=name, ok=False, detail=str(e), duration_s=time.monotonic() - t0))
            raise

    # Keep a larger buffer because firmware logs can be chatty.
    main_w = SerialLogWatcher(ports.main_port, baud=args.baud, keep_lines=8000)
    audio_w = SerialLogWatcher(ports.audio_port, baud=args.baud, keep_lines=8000)

    client: Optional[WsClient] = None
    ws_host = ""
    ws_port = int(args.ws_port)
    ws_path = str(args.ws_path)

    try:
        printer.start()

        _alog("=== Phase 3.4: Production Live Game Mock ===")
        _alog(f"Main serial : {ports.main_port}")
        _alog(f"Audio serial: {ports.audio_port}")

        if not args.no_reset_audio:
            tlog("Resetting audio module (best-effort DTR/RTS pulse) ...")
            _best_effort_pulse_reset(ports.audio_port, args.baud)
            _sleep_s(2.0)

        main_w.start()
        audio_w.start()

        main_listener = None
        audio_listener = None
        if live_enabled:
            def _emit_main(line: str) -> None:
                if line.strip():
                    printer.log("MAIN", line)

            def _emit_audio(line: str) -> None:
                if line.strip():
                    printer.log("AUDIO", line)

            main_listener = _emit_main
            audio_listener = _emit_audio
            main_w.add_listener(main_listener)
            audio_w.add_listener(audio_listener)

        ws_host = args.ws_host.strip()
        if not ws_host:
            if not args.no_reboot_for_ip:
                tlog("Rebooting main controller to capture IP from boot logs...")
                sp = SerialPort(port=ports.main_port, baud=args.baud)
                sp.open()
                try:
                    sp.write_line("reboot")
                finally:
                    sp.close()
                _sleep_s(0.5)

            tlog("Deriving firmware IP from main serial logs...")
            ws_host = derive_ip_from_serial(main_w, timeout_s=min(25.0, args.timeout))
            tlog(f"Derived firmware IP: {ws_host}")
        else:
            tlog(f"Using firmware host: {ws_host}")

        # Discovery + liveness checks.
        _check(
            "Audio status frames observed",
            lambda: audio_w.wait_for(r"(CAN_AUDIO:\s+)?STATUS: bits=0x[0-9A-Fa-f]{2}", timeout_s=min(12.0, args.timeout)),
        )
        _check(
            "Audio responds to MODULE_QUERY",
            lambda: audio_w.wait_for(r"Received MODULE_QUERY, announcing", timeout_s=min(20.0, args.timeout)),
        )
        _check(
            "Main discovers audio module",
            lambda: main_w.wait_for(r"Audio module v\d+\.\d+ discovered", timeout_s=min(25.0, args.timeout)),
        )

        tlog(f"Connecting WSS to {ws_host}:{ws_port}{ws_path} ...")
        client = WsClient(host=ws_host, port=ws_port, path=ws_path, insecure=args.insecure, sni=ws_host)
        client.connect(timeout_s=8.0)

        tlog("Sending mock game session...")
        _check(
            "Mock session stream + assertions",
            lambda: run_scenario_minimal(
                client,
                main_w,
                audio_w,
                nuke_unit_out=123456,
                nuke_unit_in=654321,
                printer=printer if live_enabled else None,
            ),
        )

        # Assertions are performed inline while emitting the stream (sound waits),
        # plus the discovery/liveness checks above.

        ended_at = _dt.datetime.now().isoformat(timespec="seconds")
        report = RunReport(
            started_at=started_at,
            ended_at=ended_at,
            ok=True,
            ws_host=ws_host,
            ws_port=ws_port,
            ws_path=ws_path,
            ports=ports,
            output_dir=str(output_dir),
            checks=checks,
        )

        if not args.no_save_logs:
            _write_report(output_dir, report)

        _alog("\n✓ Phase 3.4 completed")
        _alog(f"Artifacts saved to: {output_dir}")
        _alog((output_dir / "report.txt").read_text(encoding="utf-8").rstrip())
        return 0

    except OtsTestError as e:
        ended_at = _dt.datetime.now().isoformat(timespec="seconds")
        report = RunReport(
            started_at=started_at,
            ended_at=ended_at,
            ok=False,
            ws_host=ws_host,
            ws_port=ws_port,
            ws_path=ws_path,
            ports=ports,
            output_dir=str(output_dir),
            checks=checks,
            error=str(e),
        )
        if not args.no_save_logs:
            _write_report(output_dir, report)

        try:
            printer.stop()
        except Exception:
            pass

        print("\n✗ Phase 3.4 FAILED")
        print(str(e))
        print("\n--- Main serial tail ---")
        print(main_w.last_lines(80))
        print("\n--- Audio serial tail ---")
        print(audio_w.last_lines(80))

        if not args.no_save_logs:
            print(f"\nArtifacts saved to: {output_dir}")
            print((output_dir / "report.txt").read_text(encoding="utf-8").rstrip())
        return 2

    except KeyboardInterrupt:
        try:
            printer.stop()
        except Exception:
            pass

        print("\nInterrupted")
        return 130

    finally:
        try:
            printer.stop()
        except Exception:
            pass

        try:
            if inbox is not None:
                inbox.stop()
        except Exception:
            pass

        try:
            if client is not None:
                client.close()
        except Exception:
            pass

        try:
            main_w.stop()
        except Exception:
            pass

        try:
            audio_w.stop()
        except Exception:
            pass


if __name__ == "__main__":
    raise SystemExit(main())
