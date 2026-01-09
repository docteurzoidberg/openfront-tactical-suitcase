#!/usr/bin/env python3
"""
Flash firmware to ESP32 device using PlatformIO.

Usage:
    ./flash_firmware.py --project ots-fw-audiomodule --env esp32-a1s-espidf --port /dev/ttyUSB0
    ./flash_firmware.py --project ots-fw-main --env esp32-s3-dev --port /dev/ttyACM0
    ./flash_firmware.py --project ots-fw-cantest --env esp32-s3-devkit --port /dev/ttyACM0

Exit codes:
    0 - Success
    1 - Flash failed
    2 - Project not found
    3 - PlatformIO command error
"""

import argparse
import subprocess
import sys
from pathlib import Path
import time


def find_project_path(project_name: str) -> Path:
    """Find project directory relative to tools folder."""
    # Assume we're in ots-fw-cantest/tools, go up to ots/ root
    tools_dir = Path(__file__).parent
    ots_root = tools_dir.parent.parent
    
    project_path = ots_root / project_name
    
    if not project_path.exists():
        print(f"Error: Project not found: {project_path}", file=sys.stderr)
        return None
    
    return project_path


def flash_firmware(project_path: Path, env: str, port: str, build_first: bool = True, verbose: bool = False) -> bool:
    """
    Flash firmware using PlatformIO.
    
    Returns True on success, False on failure.
    """
    # Build command
    cmd = ["pio", "run", "-e", env, "-t", "upload", "--upload-port", port]
    
    if verbose:
        print(f"Working directory: {project_path}")
        print(f"Command: {' '.join(cmd)}")
    
    # Execute PlatformIO upload
    try:
        result = subprocess.run(
            cmd,
            cwd=project_path,
            capture_output=True,
            text=True,
            timeout=120  # 2 minute timeout for flash
        )
        
        # Print output
        if verbose or result.returncode != 0:
            print(result.stdout)
            if result.stderr:
                print(result.stderr, file=sys.stderr)
        
        # Check for success indicators in output
        output = result.stdout + result.stderr
        
        if result.returncode == 0 or "SUCCESS" in output or "Leaving..." in output:
            if verbose:
                print("✓ Flash successful")
            return True
        else:
            print("✗ Flash failed", file=sys.stderr)
            return False
            
    except subprocess.TimeoutExpired:
        print("✗ Flash timeout (>120s)", file=sys.stderr)
        return False
    except FileNotFoundError:
        print("✗ PlatformIO not found. Install with: pip install platformio", file=sys.stderr)
        return False
    except Exception as e:
        print(f"✗ Flash error: {e}", file=sys.stderr)
        return False


def main():
    parser = argparse.ArgumentParser(
        description="Flash firmware to ESP32 device using PlatformIO",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Flash audio module to ESP32-A1S
  %(prog)s --project ots-fw-audiomodule --env esp32-a1s-espidf --port /dev/ttyUSB0

  # Flash main firmware to ESP32-S3
  %(prog)s --project ots-fw-main --env esp32-s3-dev --port /dev/ttyACM0

  # Flash cantest firmware
  %(prog)s --project ots-fw-cantest --env esp32-s3-devkit --port /dev/ttyACM0
  
  # Verbose output
  %(prog)s --project ots-fw-main --env esp32-s3-dev --port /dev/ttyACM0 --verbose
"""
    )
    
    parser.add_argument("--project", required=True,
                       choices=["ots-fw-main", "ots-fw-audiomodule", "ots-fw-cantest"],
                       help="Project name (firmware to flash)")
    
    parser.add_argument("--env", required=True,
                       help="PlatformIO environment (e.g., esp32-s3-dev, esp32-a1s-espidf)")
    
    parser.add_argument("--port", required=True,
                       help="Serial port (e.g., /dev/ttyUSB0, /dev/ttyACM0)")
    
    parser.add_argument("--no-build", action="store_true",
                       help="Skip build step (upload existing binary)")
    
    parser.add_argument("--verbose", "-v", action="store_true",
                       help="Verbose output")
    
    parser.add_argument("--wait-reconnect", type=float, default=0,
                       help="Wait X seconds for device to reconnect after flash (default: 0)")
    
    args = parser.parse_args()
    
    # Find project path
    project_path = find_project_path(args.project)
    if not project_path:
        return 2
    
    # Flash firmware
    if args.verbose:
        print(f"=== Flashing {args.project} ===")
        print(f"Environment: {args.env}")
        print(f"Port: {args.port}")
    
    success = flash_firmware(
        project_path=project_path,
        env=args.env,
        port=args.port,
        build_first=not args.no_build,
        verbose=args.verbose
    )
    
    if not success:
        return 1
    
    # Wait for device reconnection if requested
    if args.wait_reconnect > 0:
        if args.verbose:
            print(f"Waiting {args.wait_reconnect}s for device to reconnect...")
        time.sleep(args.wait_reconnect)
    
    return 0


if __name__ == "__main__":
    sys.exit(main())
