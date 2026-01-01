#!/usr/bin/env python3
"""End-to-end WiFi provisioning flow test.

This test drives the firmware using:
- the durable CLI: tools/ots_device_tool.py
- serial log assertions for reboot + mode transitions
- HTTP checks against the WiFi config server in STA mode

See docs/TEST_WIFI_PROVISIONING_FLOW.md for the test spec.

Typical run:
  python3 tools/tests/wifi_provisioning_flow.py \
    --serial-port /dev/ttyACM0 --ssid MySSID --password MyPassword
"""

from __future__ import annotations

import argparse
import http.client
import os
import re
import subprocess
import sys
import time
from typing import Optional


def _ensure_repo_root_on_syspath() -> str:
    repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
    if repo_root not in sys.path:
        sys.path.insert(0, repo_root)
    return repo_root


REPO_ROOT = _ensure_repo_root_on_syspath()

from tools.ots_device_tool import (  # noqa: E402
    OtsTestError,
    SerialLogWatcher,
    derive_ip_from_serial,
)


def _tool_path() -> str:
    return os.path.join(REPO_ROOT, "tools", "ots_device_tool.py")


def _run_tool(args: list[str]) -> None:
    cmd = [sys.executable, _tool_path(), *args]
    subprocess.check_call(cmd)


def _wait_for_serial_boot(port: str, baud: int, timeout_s: float) -> SerialLogWatcher:
    """Open a SerialLogWatcher and wait for a boot marker.

    On some hosts the USB CDC device may reset and require reopening.
    """
    deadline = time.time() + timeout_s
    last_err: Optional[Exception] = None

    while time.time() < deadline:
        watcher = SerialLogWatcher(port, baud=baud)
        try:
            watcher.start()
            # Boot banner marker
            watcher.wait_for(r"OTS_MAIN: ={10,}", timeout_s=min(8.0, max(1.0, deadline - time.time())))
            return watcher
        except Exception as e:
            last_err = e
            try:
                watcher.stop()
            except Exception:
                pass
            time.sleep(0.5)

    raise RuntimeError(f"Failed to observe reboot/boot marker on {port} within timeout: {last_err}")


def _assert_portal_mode(watcher: SerialLogWatcher, timeout_s: float) -> None:
    watcher.wait_for(r"No stored WiFi credentials .* starting captive portal mode", timeout_s=timeout_s)
    watcher.wait_for(r"Starting captive portal AP: OTS-SETUP", timeout_s=timeout_s)

    # Ensure we do NOT get a STA GOT_IP quickly (we're in AP portal).
    try:
        _ = derive_ip_from_serial(watcher, timeout_s=6.0)
        raise AssertionError("Unexpectedly derived STA GOT_IP while expecting portal mode")
    except OtsTestError:
        pass


def _assert_sta_mode_and_get_ip(watcher: SerialLogWatcher, ssid: str, timeout_s: float) -> str:
    # Confirm the device intends to connect to the target SSID (best-effort).
    watcher.wait_for(re.escape(f"WiFi started, connecting to {ssid}"), timeout_s=timeout_s)
    ip = derive_ip_from_serial(watcher, timeout_s=timeout_s)
    return ip


def _http_get(host: str, path: str, timeout_s: float) -> tuple[int, bytes]:
    conn = http.client.HTTPConnection(host, 80, timeout=timeout_s)
    try:
        conn.request("GET", path)
        resp = conn.getresponse()
        body = resp.read(64 * 1024)
        return resp.status, body
    finally:
        try:
            conn.close()
        except Exception:
            pass


def _http_post_form(host: str, path: str, body: str, timeout_s: float) -> tuple[int, bytes]:
    conn = http.client.HTTPConnection(host, 80, timeout=timeout_s)
    try:
        conn.request(
            "POST",
            path,
            body=body.encode("utf-8"),
            headers={
                "Content-Type": "application/x-www-form-urlencoded",
                "Content-Length": str(len(body.encode("utf-8"))),
            },
        )
        resp = conn.getresponse()
        data = resp.read(64 * 1024)
        return resp.status, data
    finally:
        try:
            conn.close()
        except Exception:
            pass


def _validate_no_unsupported_form_chars(label: str, value: str) -> None:
    # The firmware form parser does not decode %xx, and uses & and = as delimiters.
    bad = ["&", "=", "%", " "]
    for ch in bad:
        if ch in value:
            raise SystemExit(
                f"{label} contains unsupported character {ch!r} for this test. "
                "Use values without spaces, '&', '=', or '%'."
            )


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--serial-port", required=True)
    ap.add_argument("--serial-baud", type=int, default=115200)

    ap.add_argument("--ssid", required=True)
    ap.add_argument("--password", required=True)

    ap.add_argument("--timeout-reboot", type=float, default=35.0)
    ap.add_argument("--timeout-portal", type=float, default=20.0)
    ap.add_argument("--timeout-sta", type=float, default=45.0)
    ap.add_argument("--http-timeout", type=float, default=5.0)

    args = ap.parse_args()

    _validate_no_unsupported_form_chars("SSID", args.ssid)
    _validate_no_unsupported_form_chars("password", args.password)

    # 1) Clear WiFi credentials via CLI; assert reboot
    print("STEP 1: wifi-clear (expect reboot)")
    _run_tool(["serial", "send", "--port", args.serial_port, "--baud", str(args.serial_baud), "--cmd", "wifi-clear"])

    watcher = _wait_for_serial_boot(args.serial_port, args.serial_baud, timeout_s=args.timeout_reboot)
    try:
        print("PASS: observed reboot after wifi-clear")

        # 2) After reboot, assert portal mode
        print("STEP 2: assert captive portal mode")
        _assert_portal_mode(watcher, timeout_s=args.timeout_portal)
        print("PASS: captive portal mode (by serial logs)")

    finally:
        watcher.stop()

    # 3) Provide credentials via CLI; assert reboot
    print("STEP 3: wifi-provision (expect reboot)")
    _run_tool([
        "serial",
        "send",
        "--port",
        args.serial_port,
        "--baud",
        str(args.serial_baud),
        "--cmd",
        f"wifi-provision {args.ssid} {args.password}",
    ])

    watcher = _wait_for_serial_boot(args.serial_port, args.serial_baud, timeout_s=args.timeout_reboot)
    try:
        print("PASS: observed reboot after wifi-provision")

        # 4) After reboot, assert STA mode and get IP
        print("STEP 4: assert STA connect + derive IP")
        ip = _assert_sta_mode_and_get_ip(watcher, ssid=args.ssid, timeout_s=args.timeout_sta)
        print(f"PASS: STA mode; device IP = {ip}")

    finally:
        watcher.stop()

    # 5) Check web UI in STA mode and credential-change route.
    #    (We don't attempt to reach the portal web UI from host in AP mode.)
    print("STEP 5: HTTP checks on WiFi config server")

    status, body = _http_get(ip, "/wifi", timeout_s=args.http_timeout)
    if status != 200 or b"OTS WiFi Setup" not in body:
        raise AssertionError(f"GET /wifi failed: HTTP {status}, body={body[:200]!r}")
    print("PASS: GET /wifi returns setup page")

    # POST /wifi should respond and reboot the device.
    form = f"ssid={args.ssid}&password={args.password}"
    status, body = _http_post_form(ip, "/wifi", body=form, timeout_s=args.http_timeout)
    if status != 200 or b"Rebooting" not in body:
        raise AssertionError(f"POST /wifi failed: HTTP {status}, body={body[:200]!r}")
    print("PASS: POST /wifi accepted (device should reboot)")

    # Wait for reboot + reconnect.
    watcher = _wait_for_serial_boot(args.serial_port, args.serial_baud, timeout_s=args.timeout_reboot)
    try:
        ip2 = _assert_sta_mode_and_get_ip(watcher, ssid=args.ssid, timeout_s=args.timeout_sta)
        print(f"PASS: device rebooted and returned to STA; IP={ip2}")
    finally:
        watcher.stop()

    status, body = _http_get(ip2, "/wifi", timeout_s=args.http_timeout)
    if status != 200 or b"OTS WiFi Setup" not in body:
        raise AssertionError(f"GET /wifi after reboot failed: HTTP {status}, body={body[:200]!r}")
    print("PASS: GET /wifi works after reboot in STA mode")

    print("PASS: WiFi provisioning flow")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
