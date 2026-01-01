#!/usr/bin/env python3
"""Validate WiFi config HTTP endpoints.

Checks:
- GET / (web UI) serves gzip asset with expected headers
- GET /wifi serves the same UI (or redirects appropriately)
- GET /device returns JSON with expected keys (status + device info)
- GET /api/scan returns JSON with an "aps" list (optional)

This is intentionally stdlib-only.

Examples:

  # STA mode (host is device IP on your LAN)
  python3 tools/tests/http_endpoints_check.py --host 192.168.1.50

  # Captive portal mode (host is the AP IP; your computer must be on OTS-SETUP)
  python3 tools/tests/http_endpoints_check.py --host 192.168.4.1 --expect-mode portal

  # Skip scan if you don't want to wait (or if scan is flaky)
  python3 tools/tests/http_endpoints_check.py --host 192.168.4.1 --skip-scan

Optional: auto-detect STA IP from serial logs:
  python3 tools/tests/http_endpoints_check.py \
    --auto-host --serial-port /dev/ttyACM0

"""

from __future__ import annotations

import argparse
import http.client
import json
import os
import sys
import time
from typing import Any, Optional


def _ensure_repo_root_on_syspath() -> str:
    repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
    if repo_root not in sys.path:
        sys.path.insert(0, repo_root)
    return repo_root


REPO_ROOT = _ensure_repo_root_on_syspath()


def _http_get(host: str, port: int, path: str, timeout_s: float) -> tuple[int, dict[str, str], bytes]:
    conn = http.client.HTTPConnection(host, port, timeout=timeout_s)
    try:
        conn.request(
            "GET",
            path,
            headers={
                "Accept": "*/*",
                # The firmware always serves pre-gzipped assets; this header is
                # mostly for clarity.
                "Accept-Encoding": "gzip",
                "Cache-Control": "no-cache",
                "Pragma": "no-cache",
                "Connection": "close",
                "User-Agent": "ots-http-endpoints-check/1.0",
            },
        )
        resp = conn.getresponse()
        body = resp.read(256 * 1024)
        headers = {k.lower(): v for (k, v) in resp.getheaders()}
        return resp.status, headers, body
    finally:
        try:
            conn.close()
        except Exception:
            pass


def _expect(cond: bool, msg: str) -> None:
    if not cond:
        raise AssertionError(msg)


def _expect_header(headers: dict[str, str], key: str, contains: Optional[str] = None) -> None:
    k = key.lower()
    _expect(k in headers, f"missing header: {key}")
    if contains is not None:
        _expect(
            contains.lower() in headers[k].lower(),
            f"header {key} does not contain {contains!r}; got {headers[k]!r}",
        )


def _parse_json(body: bytes) -> Any:
    try:
        return json.loads(body.decode("utf-8", errors="strict"))
    except Exception as e:
        snippet = body[:240]
        raise AssertionError(f"invalid JSON: {e}; body[:240]={snippet!r}")


def _print_ok(label: str) -> None:
    print(f"PASS: {label}")


def _print_warn(label: str) -> None:
    print(f"WARN: {label}")


def _resolve_host(args: argparse.Namespace) -> str:
    if not args.auto_host:
        if not args.host:
            raise SystemExit("Provide --host, or use --auto-host --serial-port")
        return args.host

    if not args.serial_port:
        raise SystemExit("--auto-host requires --serial-port")

    from tools.ots_device_tool import SerialLogWatcher, derive_ip_from_serial  # type: ignore

    watcher = SerialLogWatcher(args.serial_port, baud=args.serial_baud)
    watcher.start()
    try:
        # Only works in STA mode (GOT_IP).
        return derive_ip_from_serial(watcher, timeout_s=args.auto_host_timeout)
    finally:
        watcher.stop()


def _check_ui_asset(host: str, port: int, path: str, timeout_s: float) -> None:
    status, headers, body = _http_get(host, port, path, timeout_s=timeout_s)
    _expect(status == 200, f"GET {path} expected 200, got {status}")

    _expect_header(headers, "Content-Type")
    _expect_header(headers, "Cache-Control")
    _expect_header(headers, "X-OTS-WebApp")

    # For embedded assets we expect gzip encoding and a gzip magic header.
    _expect_header(headers, "Content-Encoding", contains="gzip")
    _expect(len(body) >= 2, f"GET {path} empty body")
    _expect(body[0:2] == b"\x1f\x8b", f"GET {path} not gzip data (missing 0x1f8b)")


def _check_device(host: str, port: int, timeout_s: float, expect_mode: Optional[str]) -> None:
    status, headers, body = _http_get(host, port, "/device", timeout_s=timeout_s)
    _expect(status == 200, f"GET /device expected 200, got {status}")
    _expect_header(headers, "Content-Type", contains="application/json")

    data = _parse_json(body)
    _expect(isinstance(data, dict), f"/device expected JSON object, got {type(data)}")

    for k in ("mode", "ip", "hasCredentials", "savedSsid", "serialNumber", "ownerName", "ownerConfigured", "firmwareVersion"):
        _expect(k in data, f"/device missing key {k!r}")

    if expect_mode:
        mode = str(data.get("mode") or "")
        _expect(mode == expect_mode, f"/device mode expected {expect_mode!r}, got {mode!r}")


def _check_api_scan(host: str, port: int, timeout_s: float, scan_timeout_s: float) -> None:
    # Some scans can take a while. Retry once if we get a non-200.
    start = time.time()
    last_err: Optional[str] = None

    while time.time() - start < scan_timeout_s:
        status, headers, body = _http_get(host, port, "/api/scan", timeout_s=timeout_s)
        if status != 200:
            last_err = f"HTTP {status}"
            time.sleep(0.5)
            continue

        _expect_header(headers, "Content-Type", contains="application/json")
        data = _parse_json(body)
        _expect(isinstance(data, dict), f"/api/scan expected JSON object, got {type(data)}")
        _expect("aps" in data, "/api/scan missing key 'aps'")
        aps = data.get("aps")
        _expect(isinstance(aps, list), f"/api/scan aps expected list, got {type(aps)}")

        # We don't require any APs (could be zero), but if present validate shape.
        for ap in aps[:5]:
            if not isinstance(ap, dict):
                raise AssertionError(f"/api/scan aps entries must be objects; got {type(ap)}")
            _expect("ssid" in ap, "ap missing ssid")
            _expect("rssi" in ap, "ap missing rssi")
            _expect("auth" in ap, "ap missing auth")
        return

    raise AssertionError(f"GET /api/scan failed within {scan_timeout_s}s; last_err={last_err}")


def main() -> int:
    ap = argparse.ArgumentParser(description="Validate OTS WiFi config HTTP endpoints")
    ap.add_argument("--host", default=None, help="Device IP/host (e.g. 192.168.4.1 or LAN IP)")
    ap.add_argument("--port", type=int, default=80, help="HTTP port (default: 80)")

    ap.add_argument("--expect-mode", choices=["portal", "normal"], default=None)

    ap.add_argument("--timeout", type=float, default=4.0, help="Per-request timeout")
    ap.add_argument("--scan-timeout", type=float, default=8.0, help="Total time allowed for /api/scan")
    ap.add_argument("--skip-scan", action="store_true", help="Skip /api/scan")

    ap.add_argument("--auto-host", action="store_true", help="Derive STA IP from serial GOT_IP logs")
    ap.add_argument("--serial-port", default=None)
    ap.add_argument("--serial-baud", type=int, default=115200)
    ap.add_argument("--auto-host-timeout", type=float, default=25.0)

    args = ap.parse_args()

    host = _resolve_host(args)
    port = args.port

    print(f"Checking HTTP endpoints on http://{host}:{port}")

    # UI assets
    _check_ui_asset(host, port, "/", timeout_s=args.timeout)
    _print_ok("GET / serves gzip web UI")

    # /wifi should also work (it maps to index.html in firmware).
    _check_ui_asset(host, port, "/wifi", timeout_s=args.timeout)
    _print_ok("GET /wifi serves gzip web UI")

    # Supporting assets
    _check_ui_asset(host, port, "/style.css", timeout_s=args.timeout)
    _print_ok("GET /style.css serves gzip asset")

    _check_ui_asset(host, port, "/app.js", timeout_s=args.timeout)
    _print_ok("GET /app.js serves gzip asset")

    # APIs
    _check_device(host, port, timeout_s=args.timeout, expect_mode=args.expect_mode)
    _print_ok("GET /device returns expected JSON")

    if args.skip_scan:
        _print_warn("skipped /api/scan")
    else:
        _check_api_scan(host, port, timeout_s=args.timeout, scan_timeout_s=args.scan_timeout)
        _print_ok("GET /api/scan returns expected JSON")

    print("PASS: http endpoint checks")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
