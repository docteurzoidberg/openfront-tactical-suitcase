#!/usr/bin/env python3
"""Generic utilities for testing OTS firmware devices.

This module is intentionally stdlib-only.

Provided building blocks:
- `WsClient`: minimal WebSocket-over-TLS (WSS) client with proper client masking.
- `SerialLogWatcher`: read/parse serial logs with regex wait/timeout.
- `maybe_pio_upload()`: optional firmware upload helper.

These primitives are designed so individual test scripts can be small and focused.
"""

from __future__ import annotations

import base64
import collections
import http.client
import os
import re
import socket
import ssl
import struct
import subprocess
import termios
import threading
import time
from dataclasses import dataclass
from typing import Deque, Optional


class OtsTestError(RuntimeError):
    pass


def _now_ts() -> str:
    return time.strftime("%Y-%m-%d %H:%M:%S", time.localtime())


def _list_serial_candidates() -> list[str]:
    candidates: list[str] = []

    by_id = "/dev/serial/by-id"
    if os.path.isdir(by_id):
        try:
            for name in sorted(os.listdir(by_id)):
                p = os.path.join(by_id, name)
                if os.path.islink(p) or os.path.exists(p):
                    candidates.append(p)
        except Exception:
            pass

    # Common Linux device names.
    for prefix in ("/dev/ttyACM", "/dev/ttyUSB"):
        for i in range(0, 10):
            p = f"{prefix}{i}"
            if os.path.exists(p):
                candidates.append(p)

    # De-dupe preserving order.
    seen: set[str] = set()
    out: list[str] = []
    for p in candidates:
        if p not in seen:
            out.append(p)
            seen.add(p)
    return out


def auto_detect_serial_port() -> str:
    cands = _list_serial_candidates()
    if not cands:
        raise OtsTestError("No serial devices found. Pass --port explicitly (e.g. /dev/ttyACM0).")
    return cands[0]


def _recv_exact(sock: socket.socket, n: int) -> bytes:
    buf = bytearray()
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            raise OtsTestError(f"socket closed while reading {n} bytes")
        buf.extend(chunk)
    return bytes(buf)


def _http_read_headers(sock: socket.socket, timeout_s: float = 5.0) -> bytes:
    sock.settimeout(timeout_s)
    data = bytearray()
    while b"\r\n\r\n" not in data:
        chunk = sock.recv(4096)
        if not chunk:
            raise OtsTestError("socket closed while reading HTTP headers")
        data.extend(chunk)
        if len(data) > 64 * 1024:
            raise OtsTestError("HTTP header too large")
    return bytes(data)


def _mask_payload(payload: bytes, mask_key: bytes) -> bytes:
    return bytes(b ^ mask_key[i % 4] for i, b in enumerate(payload))


@dataclass
class WsFrame:
    opcode: int
    payload: bytes
    fin: bool = True


class WsClient:
    """Tiny WSS WebSocket client.

    Notes:
    - Client->server frames MUST be masked (we do that).
    - This is not a full RFC implementation, just enough for OTS tests.
    """

    def __init__(
        self,
        host: str,
        port: int = 443,
        path: str = "/ws",
        insecure: bool = True,
        sni: Optional[str] = None,
    ):
        self.host = host
        self.port = port
        self.path = path
        self.insecure = insecure
        self.sni = sni
        self._raw: Optional[socket.socket] = None
        self._tls: Optional[ssl.SSLSocket] = None

    @property
    def tls(self) -> ssl.SSLSocket:
        if self._tls is None:
            raise OtsTestError("WsClient not connected")
        return self._tls

    def connect(self, timeout_s: float = 5.0) -> None:
        raw = socket.create_connection((self.host, self.port), timeout=timeout_s)
        ctx = ssl.create_default_context()
        if self.insecure:
            ctx.check_hostname = False
            ctx.verify_mode = ssl.CERT_NONE
        server_hostname = self.sni or self.host
        tls = ctx.wrap_socket(raw, server_hostname=server_hostname)

        key = ssl.RAND_bytes(16)
        key_b64 = base64.b64encode(key).decode("ascii")

        req = (
            f"GET {self.path} HTTP/1.1\r\n"
            f"Host: {self.host}:{self.port}\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            f"Sec-WebSocket-Key: {key_b64}\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n"
        )
        tls.sendall(req.encode("utf-8"))
        resp = _http_read_headers(tls, timeout_s=timeout_s)
        if b" 101 " not in resp.split(b"\r\n", 1)[0]:
            raise OtsTestError(f"expected HTTP 101, got: {resp[:200]!r}")

        self._raw = raw
        self._tls = tls

    def close(self) -> None:
        """Gracefully close the WebSocket.

        Sends a CLOSE frame (masked), waits briefly for server response/EOF, then
        shuts down the underlying socket.
        """
        tls = self._tls
        raw = self._raw
        try:
            if tls is None:
                return

            try:
                self.send_close()
            except Exception:
                pass

            end_by = time.time() + 1.0
            while time.time() < end_by:
                try:
                    frame = self.recv_frame(timeout_s=0.25)
                except (socket.timeout, TimeoutError):
                    continue
                except Exception:
                    break
                if frame.opcode == 0x8:
                    break

            try:
                if raw is not None:
                    raw.shutdown(socket.SHUT_RDWR)
            except Exception:
                pass
            try:
                tls.close()
            except Exception:
                pass
        finally:
            self._tls = None
            self._raw = None

    def drop(self) -> None:
        """Abruptly drop the connection (no CLOSE frame)."""
        tls = self._tls
        raw = self._raw
        try:
            if raw is not None:
                try:
                    raw.shutdown(socket.SHUT_RDWR)
                except Exception:
                    pass
            if tls is not None:
                try:
                    tls.close()
                except Exception:
                    pass
        finally:
            self._tls = None
            self._raw = None

    def send_frame(self, opcode: int, payload: bytes = b"") -> None:
        fin_opcode = 0x80 | (opcode & 0x0F)
        mask_bit = 0x80
        length = len(payload)
        header = bytearray([fin_opcode])

        if length <= 125:
            header.append(mask_bit | length)
        elif length <= 0xFFFF:
            header.append(mask_bit | 126)
            header.extend(struct.pack("!H", length))
        else:
            header.append(mask_bit | 127)
            header.extend(struct.pack("!Q", length))

        mask_key = os.urandom(4)
        masked = _mask_payload(payload, mask_key)
        frame = bytes(header) + mask_key + masked
        self.tls.sendall(frame)

    def send_text(self, text: str) -> None:
        self.send_frame(0x1, text.encode("utf-8"))

    def send_close(self) -> None:
        # Some embedded WS stacks are picky about CLOSE payload parsing.
        # Firmware doesn't read CLOSE payload, so keep it empty.
        self.send_frame(0x8, b"")

    def recv_frame(self, timeout_s: float = 1.0) -> WsFrame:
        s = self.tls
        s.settimeout(timeout_s)
        b1, b2 = _recv_exact(s, 2)
        fin = (b1 & 0x80) != 0
        opcode = b1 & 0x0F
        masked = (b2 & 0x80) != 0
        length = b2 & 0x7F

        if length == 126:
            length = struct.unpack("!H", _recv_exact(s, 2))[0]
        elif length == 127:
            length = struct.unpack("!Q", _recv_exact(s, 8))[0]

        mask_key = b""
        if masked:
            mask_key = _recv_exact(s, 4)

        payload = _recv_exact(s, length) if length else b""
        if masked:
            payload = _mask_payload(payload, mask_key)

        return WsFrame(opcode=opcode, payload=payload, fin=fin)


def start_ws_keepalive(client: WsClient, stop_evt: threading.Event) -> threading.Thread:
    """Read loop to respond to server pings (best-effort)."""

    def _run() -> None:
        while not stop_evt.is_set():
            try:
                frame = client.recv_frame(timeout_s=0.5)
            except (socket.timeout, TimeoutError):
                continue
            except Exception:
                return

            if frame.opcode == 0x9:  # ping
                try:
                    client.send_frame(0xA, frame.payload)  # pong
                except Exception:
                    return

    t = threading.Thread(target=_run, daemon=True)
    t.start()
    return t


class SerialLogWatcher:
    """Tail a serial port and allow regex waits with timeouts."""

    def __init__(self, port: str, baud: int = 115200, keep_lines: int = 250):
        self.port = port
        self.baud = baud
        self.keep_lines = keep_lines
        self._fd: Optional[int] = None
        self._stop = threading.Event()
        self._thread: Optional[threading.Thread] = None
        self._lines: Deque[str] = collections.deque(maxlen=keep_lines)
        self._cv = threading.Condition()

    def start(self) -> None:
        fd = os.open(self.port, os.O_RDONLY | os.O_NOCTTY | os.O_NONBLOCK)
        attrs = termios.tcgetattr(fd)

        # Raw-ish mode.
        attrs[0] &= ~(termios.IGNBRK | termios.BRKINT | termios.PARMRK | termios.ISTRIP | termios.INLCR | termios.IGNCR | termios.ICRNL | termios.IXON)
        attrs[1] &= ~termios.OPOST
        attrs[2] &= ~(termios.CSIZE | termios.PARENB)
        attrs[2] |= termios.CS8
        attrs[3] &= ~(termios.ECHO | termios.ECHONL | termios.ICANON | termios.ISIG | termios.IEXTEN)

        baud_map = {
            9600: termios.B9600,
            19200: termios.B19200,
            38400: termios.B38400,
            57600: termios.B57600,
            115200: termios.B115200,
        }
        if self.baud not in baud_map:
            raise OtsTestError(f"Unsupported baud for stdlib termios: {self.baud}")

        # POSIX speed fields are ispeed/ospeed at indexes 4/5.
        attrs[4] = baud_map[self.baud]
        attrs[5] = baud_map[self.baud]
        termios.tcsetattr(fd, termios.TCSANOW, attrs)

        self._fd = fd
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    def stop(self) -> None:
        self._stop.set()
        if self._thread is not None:
            self._thread.join(timeout=1.0)
        if self._fd is not None:
            try:
                os.close(self._fd)
            except Exception:
                pass
        self._fd = None

    def _run(self) -> None:
        assert self._fd is not None
        buf = bytearray()
        while not self._stop.is_set():
            try:
                chunk = os.read(self._fd, 4096)
            except BlockingIOError:
                time.sleep(0.02)
                continue
            except Exception:
                return

            if not chunk:
                time.sleep(0.02)
                continue

            buf.extend(chunk)
            while b"\n" in buf:
                line, _, rest = buf.partition(b"\n")
                buf = bytearray(rest)
                text = line.decode("utf-8", errors="ignore").rstrip("\r")
                with self._cv:
                    self._lines.append(text)
                    self._cv.notify_all()

    def wait_for(self, pattern: str, timeout_s: float) -> str:
        rx = re.compile(pattern)
        deadline = time.time() + timeout_s

        # Fast path: check existing buffer first.
        with self._cv:
            while True:
                for line in list(self._lines):
                    if rx.search(line):
                        return line

                remaining = deadline - time.time()
                if remaining <= 0:
                    tail = "\n".join(list(self._lines)[-60:])
                    raise OtsTestError(f"Timeout waiting for /{pattern}/. Last lines:\n{tail}")
                self._cv.wait(timeout=min(0.25, remaining))

    def last_lines(self, n: int = 60) -> str:
        with self._cv:
            return "\n".join(list(self._lines)[-n:])


def derive_ip_from_serial(watcher: SerialLogWatcher, timeout_s: float = 25.0) -> str:
    # Match a few common log formats:
    # - Firmware contract: "[IP] GOT_IP: 192.168.x.y"
    # - ESP-IDF defaults: "got ip:192.168.x.y" (varies by component/tag)
    patterns = [
        r"\[IP\]\s+GOT_IP:\s*(\d+\.\d+\.\d+\.\d+)",
        r"\bgot\s+ip:?(\d+\.\d+\.\d+\.\d+)",
        r"\bip:?(\d+\.\d+\.\d+\.\d+)",
    ]

    deadline = time.time() + timeout_s
    while True:
        remaining = deadline - time.time()
        if remaining <= 0:
            tail = watcher.last_lines(80)
            raise OtsTestError(
                "Timeout waiting to derive device IP from serial logs. "
                "Expected a line containing an IPv4 address.\n"
                f"Serial tail:\n{tail}"
            )

        # Wait for any new line; then scan buffer for any pattern.
        try:
            watcher.wait_for(r".+", timeout_s=min(0.75, remaining))
        except Exception:
            pass

        buf = watcher.last_lines(250)
        for pat in patterns:
            m = re.search(pat, buf, flags=re.IGNORECASE)
            if m:
                return m.group(1)


def maybe_pio_upload(env: str, serial_port: str) -> None:
    cmd = ["pio", "run", "-e", env, "-t", "upload", "--upload-port", serial_port]
    subprocess.check_call(cmd)


class SerialPort:
    """Very small stdlib-only serial helper (read/write lines)."""

    def __init__(self, port: str, baud: int = 115200):
        self.port = port
        self.baud = baud
        self._fd: Optional[int] = None

    def open(self) -> None:
        fd = os.open(self.port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        attrs = termios.tcgetattr(fd)

        # Raw-ish mode.
        attrs[0] &= ~(termios.IGNBRK | termios.BRKINT | termios.PARMRK | termios.ISTRIP | termios.INLCR | termios.IGNCR | termios.ICRNL | termios.IXON)
        attrs[1] &= ~termios.OPOST
        attrs[2] &= ~(termios.CSIZE | termios.PARENB)
        attrs[2] |= termios.CS8
        attrs[3] &= ~(termios.ECHO | termios.ECHONL | termios.ICANON | termios.ISIG | termios.IEXTEN)

        baud_map = {
            9600: termios.B9600,
            19200: termios.B19200,
            38400: termios.B38400,
            57600: termios.B57600,
            115200: termios.B115200,
        }
        if self.baud not in baud_map:
            raise OtsTestError(f"Unsupported baud for stdlib termios: {self.baud}")

        attrs[4] = baud_map[self.baud]
        attrs[5] = baud_map[self.baud]
        termios.tcsetattr(fd, termios.TCSANOW, attrs)

        # For CDC ACM devices (e.g. ESP32-S3 USB-Serial/JTAG), the device may not
        # transmit until the host asserts DTR. Also ensure RTS is deasserted to
        # avoid holding the target in reset on some auto-reset circuits.
        try:
            import array
            import fcntl

            if hasattr(termios, "TIOCMGET") and hasattr(termios, "TIOCMSET"):
                buf = array.array("i", [0])
                fcntl.ioctl(fd, termios.TIOCMGET, buf, True)
                status = int(buf[0])

                if hasattr(termios, "TIOCM_DTR"):
                    status |= termios.TIOCM_DTR
                if hasattr(termios, "TIOCM_RTS"):
                    status &= ~termios.TIOCM_RTS

                buf[0] = status
                fcntl.ioctl(fd, termios.TIOCMSET, buf, True)
        except Exception:
            # Best-effort; serial logging still works on classic UARTs.
            pass

        self._fd = fd

    def close(self) -> None:
        if self._fd is None:
            return
        try:
            os.close(self._fd)
        except Exception:
            pass
        self._fd = None

    @property
    def fd(self) -> int:
        if self._fd is None:
            raise OtsTestError("SerialPort not opened")
        return self._fd

    def write_line(self, line: str) -> None:
        data = (line.rstrip("\r\n") + "\n").encode("utf-8")
        try:
            os.write(self.fd, data)
        except BlockingIOError:
            # Retry briefly.
            end = time.time() + 1.0
            while time.time() < end:
                try:
                    os.write(self.fd, data)
                    return
                except BlockingIOError:
                    time.sleep(0.01)
            raise

    def iter_lines(self, stop_evt: threading.Event) -> "collections.abc.Iterator[str]":
        buf = bytearray()
        while not stop_evt.is_set():
            try:
                chunk = os.read(self.fd, 4096)
            except BlockingIOError:
                time.sleep(0.02)
                continue
            except Exception:
                return

            if not chunk:
                time.sleep(0.02)
                continue

            buf.extend(chunk)
            while b"\n" in buf:
                line, _, rest = buf.partition(b"\n")
                buf = bytearray(rest)
                yield line.decode("utf-8", errors="ignore").rstrip("\r")


def ota_upload(host: str, port: int, bin_path: str, timeout_s: float = 60.0) -> None:
    if not os.path.exists(bin_path):
        raise OtsTestError(f"Binary not found: {bin_path}")

    size = os.path.getsize(bin_path)
    conn = http.client.HTTPConnection(host, port, timeout=timeout_s)
    try:
        conn.putrequest("POST", "/update")
        conn.putheader("Content-Type", "application/octet-stream")
        conn.putheader("Content-Length", str(size))
        conn.endheaders()

        sent = 0
        with open(bin_path, "rb") as f:
            while True:
                chunk = f.read(16 * 1024)
                if not chunk:
                    break
                conn.send(chunk)
                sent += len(chunk)
                # best-effort progress
                if size > 0:
                    pct = int((sent * 100) / size)
                    if pct % 10 == 0:
                        pass

        resp = conn.getresponse()
        body = resp.read(1024)
        if resp.status < 200 or resp.status >= 300:
            raise OtsTestError(f"OTA upload failed: HTTP {resp.status} {resp.reason}: {body!r}")
    finally:
        try:
            conn.close()
        except Exception:
            pass


def _cli() -> int:
    import argparse

    ap = argparse.ArgumentParser(description="OTS device tool (serial + OTA)")
    sub = ap.add_subparsers(dest="cmd", required=True)

    sp_serial = sub.add_parser("serial", help="Serial helpers (monitor/send/reboot)")
    serial_sub = sp_serial.add_subparsers(dest="serial_cmd", required=True)

    p_mon = serial_sub.add_parser("monitor", help="Stream serial logs")
    p_mon.add_argument("--port", default="auto", help="Serial port path or 'auto'")
    p_mon.add_argument("--baud", type=int, default=115200)
    p_mon.add_argument("--log-file", default=None, help="Append logs to file")
    p_mon.add_argument("--timestamps", action="store_true")

    p_send = serial_sub.add_parser("send", help="Send one command line")
    p_send.add_argument("--port", default="auto")
    p_send.add_argument("--baud", type=int, default=115200)
    p_send.add_argument("--cmd", required=True, dest="line")
    p_send.add_argument("--monitor", action="store_true", help="Also print serial output for a short window")
    p_send.add_argument("--monitor-seconds", type=float, default=2.0)

    p_reboot = serial_sub.add_parser("reboot", help="Reboot device via serial command")
    p_reboot.add_argument("--port", default="auto")
    p_reboot.add_argument("--baud", type=int, default=115200)

    sp_wifi = sub.add_parser("wifi", help="WiFi helpers (provision/clear/status)")
    wifi_sub = sp_wifi.add_subparsers(dest="wifi_cmd", required=True)

    p_wifi_provision = wifi_sub.add_parser("provision", help="Provision WiFi credentials")
    p_wifi_provision.add_argument("--port", default="auto")
    p_wifi_provision.add_argument("--baud", type=int, default=115200)
    p_wifi_provision.add_argument("--ssid", required=True, help="WiFi SSID")
    p_wifi_provision.add_argument("--password", required=True, help="WiFi password")

    p_wifi_clear = wifi_sub.add_parser("clear", help="Clear WiFi credentials")
    p_wifi_clear.add_argument("--port", default="auto")
    p_wifi_clear.add_argument("--baud", type=int, default=115200)

    p_wifi_status = wifi_sub.add_parser("status", help="Get WiFi status")
    p_wifi_status.add_argument("--port", default="auto")
    p_wifi_status.add_argument("--baud", type=int, default=115200)

    sp_version = sub.add_parser("version", help="Get firmware version")
    sp_version.add_argument("--port", default="auto")
    sp_version.add_argument("--baud", type=int, default=115200)

    sp_nvs = sub.add_parser("nvs", help="NVS helpers (set/clear owner name and serial number)")
    nvs_sub = sp_nvs.add_subparsers(dest="nvs_cmd", required=True)

    p_nvs_set = nvs_sub.add_parser("set-owner", help="Set owner name in NVS")
    p_nvs_set.add_argument("--port", default="auto")
    p_nvs_set.add_argument("--baud", type=int, default=115200)
    p_nvs_set.add_argument("--name", required=True, help="Owner name to set")

    p_nvs_clear = nvs_sub.add_parser("clear-owner", help="Clear owner name from NVS")
    p_nvs_clear.add_argument("--port", default="auto")
    p_nvs_clear.add_argument("--baud", type=int, default=115200)

    p_nvs_get_owner = nvs_sub.add_parser("get-owner", help="Get owner name from device")
    p_nvs_get_owner.add_argument("--port", default="auto")
    p_nvs_get_owner.add_argument("--baud", type=int, default=115200)

    p_nvs_set_serial = nvs_sub.add_parser("set-serial", help="Set serial number in NVS")
    p_nvs_set_serial.add_argument("--port", default="auto")
    p_nvs_set_serial.add_argument("--baud", type=int, default=115200)
    p_nvs_set_serial.add_argument("--serial", required=True, help="Serial number to set")

    p_nvs_clear_serial = nvs_sub.add_parser("clear-serial", help="Clear serial number from NVS")
    p_nvs_clear_serial.add_argument("--port", default="auto")
    p_nvs_clear_serial.add_argument("--baud", type=int, default=115200)

    p_nvs_get_serial = nvs_sub.add_parser("get-serial", help="Get serial number from device")
    p_nvs_get_serial.add_argument("--port", default="auto")
    p_nvs_get_serial.add_argument("--baud", type=int, default=115200)

    sp_ota = sub.add_parser("ota", help="OTA helpers")
    ota_sub = sp_ota.add_subparsers(dest="ota_cmd", required=True)

    p_up = ota_sub.add_parser("upload", help="Upload firmware via HTTP OTA")
    p_up.add_argument("--host", default=None)
    p_up.add_argument("--port", type=int, default=3232)
    p_up.add_argument("--bin", required=True)
    p_up.add_argument("--timeout", type=float, default=120.0)
    p_up.add_argument("--serial-port", default=None, help="Serial port for --auto-host")
    p_up.add_argument("--serial-baud", type=int, default=115200)
    p_up.add_argument("--auto-host", action="store_true", help="Derive host IP from serial logs")

    args = ap.parse_args()

    if os.getenv("OTS_DEVICE_TOOL_DEBUG"):
        print(f"[DEBUG] args={args!r}")

    if args.cmd == "serial":
        port = args.port
        if port == "auto":
            port = auto_detect_serial_port()

        if args.serial_cmd == "monitor":
            sp = SerialPort(port=port, baud=args.baud)
            sp.open()
            stop_evt = threading.Event()
            try:
                out_f = None
                if args.log_file:
                    os.makedirs(os.path.dirname(args.log_file) or ".", exist_ok=True)
                    out_f = open(args.log_file, "a", encoding="utf-8")

                for line in sp.iter_lines(stop_evt):
                    prefix = f"[{_now_ts()}] " if args.timestamps else ""
                    text = prefix + line
                    print(text, flush=True)
                    if out_f:
                        out_f.write(text + "\n")
                        out_f.flush()
            except KeyboardInterrupt:
                return 0
            finally:
                stop_evt.set()
                sp.close()
                try:
                    if out_f:
                        out_f.close()
                except Exception:
                    pass
            return 0

        if args.serial_cmd == "send":
            sp = SerialPort(port=port, baud=args.baud)
            sp.open()
            try:
                sp.write_line(args.line)
                if args.monitor:
                    stop_evt = threading.Event()
                    end = time.time() + args.monitor_seconds
                    for line in sp.iter_lines(stop_evt):
                        print(line, flush=True)
                        if time.time() >= end:
                            break
            finally:
                sp.close()
            return 0

        if args.serial_cmd == "reboot":
            sp = SerialPort(port=port, baud=args.baud)
            sp.open()
            try:
                sp.write_line("reboot")
            finally:
                sp.close()
            return 0

    if args.cmd == "wifi":
        port = args.port
        if not port or port == "auto":
            port = auto_detect_serial_port()
            if not port:
                raise SystemExit("No serial port found. Use --port to specify.")

        if args.wifi_cmd == "provision":
            print(f"Provisioning WiFi: {args.ssid}")
            sp = SerialPort(port=port, baud=args.baud)
            sp.open()
            try:
                sp.write_line(f"wifi-provision {args.ssid} {args.password}")
                stop_evt = threading.Event()
                end = time.time() + 3
                for line in sp.iter_lines(stop_evt):
                    print(line, flush=True)
                    if "wifi-provision:" in line or "saved" in line or "stored" in line:
                        break
                    if time.time() >= end:
                        print("No confirmation received (may have succeeded anyway)")
                        break
            finally:
                sp.close()
            return 0

        if args.wifi_cmd == "clear":
            print("Clearing WiFi credentials")
            sp = SerialPort(port=port, baud=args.baud)
            sp.open()
            try:
                sp.write_line("wifi-clear")
                stop_evt = threading.Event()
                end = time.time() + 2
                for line in sp.iter_lines(stop_evt):
                    print(line, flush=True)
                    if "wifi-clear:" in line or "cleared" in line or "erased" in line:
                        break
                    if time.time() >= end:
                        print("No confirmation received (may have succeeded anyway)")
                        break
            finally:
                sp.close()
            return 0

        if args.wifi_cmd == "status":
            print("Getting WiFi status")
            sp = SerialPort(port=port, baud=args.baud)
            sp.open()
            try:
                sp.write_line("wifi-status")
                stop_evt = threading.Event()
                end = time.time() + 2
                for line in sp.iter_lines(stop_evt):
                    print(line, flush=True)
                    if "wifi-status:" in line:
                        break
                    if time.time() >= end:
                        print("No response received")
                        break
            finally:
                sp.close()
            return 0

    if args.cmd == "version":
        port = args.port
        if not port or port == "auto":
            port = auto_detect_serial_port()
            if not port:
                raise SystemExit("No serial port found. Use --port to specify.")

        print("Getting firmware version")
        sp = SerialPort(port=port, baud=args.baud)
        sp.open()
        try:
            sp.write_line("version")
            stop_evt = threading.Event()
            end = time.time() + 2
            for line in sp.iter_lines(stop_evt):
                print(line, flush=True)
                if "Firmware:" in line:
                    break
                if time.time() >= end:
                    print("No response received")
                    break
        finally:
            sp.close()
        return 0

    if args.cmd == "nvs":
        port = args.port
        if not port or port == "auto":
            port = auto_detect_serial_port()
            if not port:
                raise SystemExit("No serial port found. Use --port to specify.")

        if args.nvs_cmd == "set-owner":
            print(f"Setting owner name to: {args.name}")
            sp = SerialPort(port=port, baud=args.baud)
            sp.open()
            try:
                sp.write_line(f"nvs set owner_name {args.name}")
                stop_evt = threading.Event()
                end = time.time() + 2
                for line in sp.iter_lines(stop_evt):
                    print(line, flush=True)
                    if "Owner name set" in line or "saved" in line:
                        break
                    if time.time() >= end:
                        print("No confirmation received (may have succeeded anyway)")
                        break
            finally:
                sp.close()
            return 0

        if args.nvs_cmd == "clear-owner":
            print("Clearing owner name from NVS")
            sp = SerialPort(port=port, baud=args.baud)
            sp.open()
            try:
                sp.write_line("nvs erase owner_name")
                stop_evt = threading.Event()
                end = time.time() + 2
                for line in sp.iter_lines(stop_evt):
                    print(line, flush=True)
                    if "erased" in line or "cleared" in line or "deleted" in line:
                        break
                    if time.time() >= end:
                        print("No confirmation received (may have succeeded anyway)")
                        break
            finally:
                sp.close()
            return 0

        if args.nvs_cmd == "get-owner":
            print("Getting owner name from device")
            sp = SerialPort(port=port, baud=args.baud)
            sp.open()
            try:
                sp.write_line("nvs get owner_name")
                stop_evt = threading.Event()
                end = time.time() + 2
                for line in sp.iter_lines(stop_evt):
                    print(line, flush=True)
                    if "Owner name:" in line:
                        break
                    if time.time() >= end:
                        print("No response received")
                        break
            finally:
                sp.close()
            return 0

        if args.nvs_cmd == "set-serial":
            print(f"Setting serial number to: {args.serial}")
            sp = SerialPort(port=port, baud=args.baud)
            sp.open()
            try:
                sp.write_line(f"nvs set serial_number {args.serial}")
                stop_evt = threading.Event()
                end = time.time() + 2
                for line in sp.iter_lines(stop_evt):
                    print(line, flush=True)
                    if "Serial number set" in line or "saved" in line:
                        break
                    if time.time() >= end:
                        print("No confirmation received (may have succeeded anyway)")
                        break
            finally:
                sp.close()
            return 0

        if args.nvs_cmd == "clear-serial":
            print("Clearing serial number from NVS")
            sp = SerialPort(port=port, baud=args.baud)
            sp.open()
            try:
                sp.write_line("nvs erase serial_number")
                stop_evt = threading.Event()
                end = time.time() + 2
                for line in sp.iter_lines(stop_evt):
                    print(line, flush=True)
                    if "erased" in line or "cleared" in line or "deleted" in line:
                        break
                    if time.time() >= end:
                        print("No confirmation received (may have succeeded anyway)")
                        break
            finally:
                sp.close()
            return 0

        if args.nvs_cmd == "get-serial":
            print("Getting serial number from device")
            sp = SerialPort(port=port, baud=args.baud)
            sp.open()
            try:
                sp.write_line("nvs get serial_number")
                stop_evt = threading.Event()
                end = time.time() + 2
                for line in sp.iter_lines(stop_evt):
                    print(line, flush=True)
                    if "Serial number:" in line:
                        break
                    if time.time() >= end:
                        print("No response received")
                        break
            finally:
                sp.close()
            return 0

    if args.cmd == "ota":
        host = args.host
        if args.auto_host:
            if not args.serial_port:
                raise SystemExit("--auto-host requires --serial-port")
            watcher = SerialLogWatcher(args.serial_port, baud=args.serial_baud)
            watcher.start()
            try:
                host = derive_ip_from_serial(watcher)
            finally:
                watcher.stop()
        if not host:
            raise SystemExit("Provide --host or use --auto-host")

        ota_upload(host=host, port=args.port, bin_path=args.bin, timeout_s=args.timeout)
        print(f"OTA upload OK: http://{host}:{args.port}/update")
        return 0

    raise SystemExit("Unhandled command")


def main() -> int:
    return _cli()


if __name__ == "__main__":
    raise SystemExit(main())
