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
