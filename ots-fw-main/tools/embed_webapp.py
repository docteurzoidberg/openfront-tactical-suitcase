#!/usr/bin/env python3
"""Embed the WiFi config webapp into a C header.

- Reads files from ots-fw-main/webapp/
- Gzips each asset
- Emits ots-fw-main/include/webapp/ots_webapp.h

This is intentionally dependency-free (stdlib only).
"""

from __future__ import annotations

import argparse
import gzip
import hashlib
import os
from pathlib import Path
from typing import Iterable


ROOT = Path(__file__).resolve().parents[1]
WEBAPP_DIR = ROOT / "webapp"
OUT_HEADER = ROOT / "include" / "webapp" / "ots_webapp.h"


CONTENT_TYPES = {
    ".html": "text/html; charset=utf-8",
    ".css": "text/css; charset=utf-8",
    ".js": "application/javascript; charset=utf-8",
}


def _read_bytes(path: Path) -> bytes:
    data = path.read_bytes()
    if not data:
        raise RuntimeError(f"Empty asset: {path}")
    return data


def _gzip_bytes(data: bytes) -> bytes:
    # mtime=0 gives deterministic output.
    return gzip.compress(data, compresslevel=9, mtime=0)


def _c_array(name: str, data: bytes) -> str:
    # Format: 12 bytes per line for readability.
    hex_bytes = [f"0x{b:02x}" for b in data]
    lines = []
    for i in range(0, len(hex_bytes), 12):
        lines.append(", ".join(hex_bytes[i : i + 12]))
    joined = ",\n  ".join(lines)
    return (
        f"static const unsigned char {name}[] = {{\n  {joined}\n}};\n"
        f"static const unsigned int {name}_len = {len(data)};\n"
    )


def _var_name_for_path(path: str) -> str:
    # path like /index.html -> ots_webapp_index_html
    safe = path.strip("/").replace("-", "_").replace(".", "_").replace("/", "_")
    if not safe:
        safe = "index_html"
    return f"ots_webapp_{safe}"


def _iter_assets(webapp_dir: Path) -> list[tuple[str, Path]]:
    assets: list[tuple[str, Path]] = []

    for rel in ["index.html", "style.css", "app.js"]:
        p = webapp_dir / rel
        if not p.exists():
            raise RuntimeError(f"Missing required asset: {p}")
        assets.append(("/" + rel, p))

    return assets


def build_header(webapp_dir: Path, out_header: Path) -> str:
    assets = _iter_assets(webapp_dir)

    # Create a stable fingerprint for debugging/versioning.
    sha = hashlib.sha256()
    for url_path, file_path in assets:
        sha.update(url_path.encode("utf-8"))
        sha.update(b"\0")
        # Read raw bytes and inject fingerprint into HTML before hashing
        raw_data = _read_bytes(file_path)
        sha.update(raw_data)
        sha.update(b"\0")
    fingerprint = sha.hexdigest()[:16]
    
    # Now re-read assets and inject fingerprint into HTML
    processed_assets: list[tuple[str, bytes]] = []
    for url_path, file_path in assets:
        raw_data = _read_bytes(file_path)
        # Inject fingerprint into HTML
        if file_path.suffix.lower() == ".html":
            raw_data = raw_data.replace(b"__OTS_BUILD_HASH__", fingerprint.encode("utf-8"))
        processed_assets.append((url_path, raw_data))

    parts: list[str] = []
    parts.append("#pragma once\n")
    parts.append("#include <stdbool.h>\n")
    parts.append("#include <stddef.h>\n")
    parts.append("#include <stdint.h>\n\n")

    parts.append("typedef struct {\n")
    parts.append("  const char* path;\n")
    parts.append("  const char* content_type;\n")
    parts.append("  const uint8_t* data;\n")
    parts.append("  size_t len;\n")
    parts.append("  bool gzip;\n")
    parts.append("} ots_webapp_asset_t;\n\n")

    parts.append(f"#define OTS_WEBAPP_FINGERPRINT \"{fingerprint}\"\n\n")

    # Data blobs
    for url_path, raw_data in processed_assets:
        # Determine content type from URL path
        ext = "." + url_path.split(".")[-1] if "." in url_path else ""
        ct = CONTENT_TYPES.get(ext.lower())
        if not ct:
            raise RuntimeError(f"Unknown content-type for: {url_path}")

        var_name = _var_name_for_path(url_path)
        gz = _gzip_bytes(raw_data)
        parts.append(_c_array(var_name, gz))
        parts.append("\n")

    # Asset table
    parts.append("static const ots_webapp_asset_t ots_webapp_assets[] = {\n")
    for url_path, raw_data in processed_assets:
        ext = "." + url_path.split(".")[-1] if "." in url_path else ""
        ct = CONTENT_TYPES[ext.lower()]
        var_name = _var_name_for_path(url_path)
        parts.append(
            f"  {{\"{url_path}\", \"{ct}\", (const uint8_t*){var_name}, (size_t){var_name}_len, true}},\n"
        )
    parts.append("};\n")
    parts.append(
        "static const size_t ots_webapp_assets_count = sizeof(ots_webapp_assets) / sizeof(ots_webapp_assets[0]);\n\n"
    )

    parts.append("static inline const ots_webapp_asset_t* ots_webapp_find(const char* path) {\n")
    parts.append("  if (!path) return NULL;\n")
    parts.append("  for (size_t i = 0; i < ots_webapp_assets_count; i++) {\n")
    parts.append("    const ots_webapp_asset_t* a = &ots_webapp_assets[i];\n")
    parts.append("    const char* p = a->path;\n")
    parts.append("    size_t j = 0;\n")
    parts.append("    while (p[j] && path[j] && p[j] == path[j]) j++;\n")
    parts.append("    if (p[j] == '\\0' && path[j] == '\\0') return a;\n")
    parts.append("  }\n")
    parts.append("  return NULL;\n")
    parts.append("}\n")

    out_header.parent.mkdir(parents=True, exist_ok=True)
    out_header.write_text("".join(parts), encoding="utf-8")
    
    return fingerprint


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--webapp", type=Path, default=WEBAPP_DIR)
    ap.add_argument("--out", type=Path, default=OUT_HEADER)
    args = ap.parse_args()

    fingerprint = build_header(args.webapp, args.out)
    
    print(f"Wrote {args.out}")
    print(f"Build hash: {fingerprint}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
