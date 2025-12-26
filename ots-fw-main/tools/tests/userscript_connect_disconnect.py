#!/usr/bin/env python3
"""Integration test: userscript presence drives WS_CONNECTED/DISCONNECTED.

This script is intentionally small and relies on generic primitives in
`tools/ots_device_tool.py`.

Typical run (auto-discover IP from serial):
  python3 tools/tests/userscript_connect_disconnect.py \
    --serial-port /dev/ttyACM0 --auto-host --insecure

Optional upload first:
  python3 tools/tests/userscript_connect_disconnect.py \
    --upload --env esp32-s3-dev-factory --serial-port /dev/ttyACM0 --auto-host --insecure
"""

from __future__ import annotations

import argparse
import json
import os
import sys
import threading
import time


def _ensure_repo_root_on_syspath() -> None:
    repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
    if repo_root not in sys.path:
        sys.path.insert(0, repo_root)


_ensure_repo_root_on_syspath()


from tools.ots_device_tool import (  # noqa: E402
    SerialLogWatcher,
    WsClient,
    derive_ip_from_serial,
    maybe_pio_upload,
    start_ws_keepalive,
)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--serial-port", required=True)
    ap.add_argument("--serial-baud", type=int, default=115200)

    ap.add_argument("--host", default=None)
    ap.add_argument("--port", type=int, default=443)
    ap.add_argument("--path", default="/ws")
    ap.add_argument("--auto-host", action="store_true")
    ap.add_argument("--insecure", action="store_true", help="Disable TLS verification (self-signed certs)")

    ap.add_argument("--upload", action="store_true")
    ap.add_argument("--env", default="esp32-s3-dev-factory")

    ap.add_argument("--abrupt-close", action="store_true", help="Drop socket without CLOSE frame")

    ap.add_argument("--timeout-connect", type=float, default=15.0)
    ap.add_argument("--timeout-disconnect", type=float, default=20.0)

    args = ap.parse_args()

    if args.upload:
        maybe_pio_upload(env=args.env, serial_port=args.serial_port)

    watcher = SerialLogWatcher(args.serial_port, baud=args.serial_baud)
    watcher.start()
    try:
        host = args.host
        if args.auto_host:
            host = derive_ip_from_serial(watcher)
        if not host:
            raise SystemExit("Provide --host or use --auto-host")

        client = WsClient(host=host, port=args.port, path=args.path, insecure=args.insecure)
        # After reboot, WiFi/IP may be up before the HTTPS/WSS server is listening.
        # Retry connect for a short window to avoid flaky failures.
        connect_deadline = time.time() + args.timeout_connect
        last_err: Exception | None = None
        while True:
            try:
                client.connect()
                break
            except (ConnectionRefusedError, OSError) as e:
                last_err = e
                if time.time() >= connect_deadline:
                    tail = watcher.last_lines(80)
                    raise RuntimeError(
                        f"Failed to connect to wss://{host}:{args.port}{args.path} before timeout. "
                        f"Last error: {e}.\nSerial tail:\n{tail}"
                    )
                time.sleep(0.5)

        stop_evt = threading.Event()
        keepalive_thread = start_ws_keepalive(client, stop_evt)

        # Identify as userscript.
        client.send_text(json.dumps({"type": "handshake", "clientType": "userscript"}))
        client.send_text(json.dumps({
            "type": "event",
            "payload": {"type": "INFO", "timestamp": int(time.time() * 1000), "message": "userscript-connected"},
        }))

        # Assertions via log contract.
        watcher.wait_for(r"Identified userscript client", timeout_s=args.timeout_connect)
        watcher.wait_for(r"SysStatus: WebSocket connected - showing lobby screen", timeout_s=args.timeout_connect)

        if args.abrupt_close:
            client.drop()
        else:
            client.close()

        stop_evt.set()
        keepalive_thread.join(timeout=1.0)

        watcher.wait_for(r"Last userscript disconnected; userscript_clients=0", timeout_s=args.timeout_disconnect)
        watcher.wait_for(r"SysStatus: WebSocket disconnected - showing waiting screen", timeout_s=args.timeout_disconnect)

        print("PASS: userscript connect/disconnect semantics")
        return 0

    finally:
        watcher.stop()


if __name__ == "__main__":
    raise SystemExit(main())
