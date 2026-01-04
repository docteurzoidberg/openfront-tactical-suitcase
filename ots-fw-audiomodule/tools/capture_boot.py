#!/usr/bin/env python3
"""Capture full boot logs from ESP32-A1S audio module.

This script starts monitoring the serial port, then resets the device
to capture the complete boot sequence from the very beginning.

Usage:
    ./tools/capture_boot.py [--port /dev/ttyUSB0] [--baud 115200] [--duration 10]

Examples:
    # Auto-detect port, capture 10 seconds
    ./tools/capture_boot.py

    # Specific port, capture 15 seconds
    ./tools/capture_boot.py --port /dev/ttyACM0 --duration 15

    # Save to file
    ./tools/capture_boot.py --output boot_logs.txt
"""

import argparse
import glob
import os
import subprocess
import sys
import termios
import threading
import time
from typing import Optional


def find_serial_port() -> str:
    """Auto-detect ESP32 serial port."""
    candidates = []
    
    # Check /dev/serial/by-id first (most reliable)
    by_id = "/dev/serial/by-id"
    if os.path.isdir(by_id):
        for name in sorted(os.listdir(by_id)):
            path = os.path.join(by_id, name)
            if os.path.exists(path):
                candidates.append(path)
    
    # Check common device names
    for prefix in ["/dev/ttyUSB", "/dev/ttyACM"]:
        for i in range(10):
            path = f"{prefix}{i}"
            if os.path.exists(path):
                candidates.append(path)
    
    if not candidates:
        print("ERROR: No serial ports found", file=sys.stderr)
        print("Available devices:", file=sys.stderr)
        subprocess.run(["ls", "-l", "/dev/tty*"], stderr=subprocess.DEVNULL)
        sys.exit(1)
    
    # Deduplicate
    seen = set()
    unique = []
    for p in candidates:
        if p not in seen:
            unique.append(p)
            seen.add(p)
    
    return unique[0]


def reset_device(port: str) -> bool:
    """Reset ESP32 device using esptool.py."""
    # Try multiple esptool locations
    esptool_paths = [
        "esptool.py",  # System-wide install
        "python3 -m esptool",  # Python module
        os.path.expanduser("~/.platformio/packages/tool-esptoolpy/esptool.py"),  # PlatformIO
        os.path.expanduser("~/.platformio/packages/tool-esptoolpy@*/esptool.py"),  # PlatformIO versioned
    ]
    
    for tool in esptool_paths:
        try:
            if tool.startswith("python3"):
                # Module-based invocation
                result = subprocess.run(
                    ["python3", "-m", "esptool", "--chip", "esp32", "--port", port, "run"],
                    capture_output=True,
                    timeout=5
                )
            elif "*" in tool:
                # Glob pattern - find first match
                import glob
                matches = glob.glob(tool)
                if not matches:
                    continue
                result = subprocess.run(
                    ["python3", matches[0], "--chip", "esp32", "--port", port, "run"],
                    capture_output=True,
                    timeout=5
                )
            elif os.path.exists(tool):
                # Direct path
                result = subprocess.run(
                    ["python3", tool, "--chip", "esp32", "--port", port, "run"],
                    capture_output=True,
                    timeout=5
                )
            else:
                # Command in PATH
                result = subprocess.run(
                    [tool, "--chip", "esp32", "--port", port, "run"],
                    capture_output=True,
                    timeout=5
                )
            
            if result.returncode == 0:
                return True
                
        except (FileNotFoundError, subprocess.TimeoutExpired):
            continue
        except Exception:
            continue
    
    print("WARNING: esptool.py not found in any known location", file=sys.stderr)
    print("Checked:", file=sys.stderr)
    print("  - System PATH (esptool.py)", file=sys.stderr)
    print("  - Python module (python3 -m esptool)", file=sys.stderr)
    print("  - PlatformIO (~/.platformio/packages/tool-esptoolpy/)", file=sys.stderr)
    print("Install with: pip install esptool", file=sys.stderr)
    return False


class SerialMonitor:
    """Simple serial port monitor."""
    
    def __init__(self, port: str, baud: int = 115200):
        self.port = port
        self.baud = baud
        self.fd: Optional[int] = None
        self.stop_event = threading.Event()
        self.output_file: Optional[str] = None
        
    def open(self) -> None:
        """Open serial port in raw mode."""
        self.fd = os.open(self.port, os.O_RDONLY | os.O_NOCTTY | os.O_NONBLOCK)
        
        # Configure serial port
        attrs = termios.tcgetattr(self.fd)
        
        # Raw mode: no echo, no canonical mode, no signals
        attrs[0] &= ~(termios.IGNBRK | termios.BRKINT | termios.PARMRK | 
                     termios.ISTRIP | termios.INLCR | termios.IGNCR | 
                     termios.ICRNL | termios.IXON)
        attrs[1] &= ~termios.OPOST
        attrs[2] &= ~(termios.CSIZE | termios.PARENB)
        attrs[2] |= termios.CS8
        attrs[3] &= ~(termios.ECHO | termios.ECHONL | termios.ICANON | 
                     termios.ISIG | termios.IEXTEN)
        
        # Set baud rate
        baud_map = {
            9600: termios.B9600,
            19200: termios.B19200,
            38400: termios.B38400,
            57600: termios.B57600,
            115200: termios.B115200,
            230400: termios.B230400,
        }
        
        if self.baud not in baud_map:
            print(f"ERROR: Unsupported baud rate: {self.baud}", file=sys.stderr)
            sys.exit(1)
        
        attrs[4] = baud_map[self.baud]  # ispeed
        attrs[5] = baud_map[self.baud]  # ospeed
        
        termios.tcsetattr(self.fd, termios.TCSANOW, attrs)
    
    def close(self) -> None:
        """Close serial port."""
        if self.fd is not None:
            try:
                os.close(self.fd)
            except:
                pass
            self.fd = None
    
    def monitor(self, duration: float) -> None:
        """Monitor serial output for specified duration."""
        if self.fd is None:
            raise RuntimeError("Serial port not opened")
        
        start_time = time.time()
        file_handle = None
        
        try:
            if self.output_file:
                file_handle = open(self.output_file, 'w', buffering=1)  # Line buffered
            
            buf = bytearray()
            
            while time.time() - start_time < duration and not self.stop_event.is_set():
                try:
                    chunk = os.read(self.fd, 4096)
                    if chunk:
                        buf.extend(chunk)
                        
                        # Process complete lines
                        while b'\n' in buf:
                            line_bytes, sep, rest = buf.partition(b'\n')
                            buf = bytearray(rest)
                            
                            # Decode and print
                            try:
                                line = line_bytes.decode('utf-8', errors='replace').rstrip('\r')
                                print(line)
                                if file_handle:
                                    file_handle.write(line + '\n')
                            except:
                                pass
                    else:
                        time.sleep(0.01)
                
                except BlockingIOError:
                    time.sleep(0.01)
                except KeyboardInterrupt:
                    break
        
        finally:
            # Flush remaining buffer
            if buf:
                try:
                    line = buf.decode('utf-8', errors='replace').rstrip('\r\n')
                    if line:
                        print(line)
                        if file_handle:
                            file_handle.write(line + '\n')
                except:
                    pass
            
            if file_handle:
                file_handle.close()


def main():
    parser = argparse.ArgumentParser(
        description="Capture ESP32-A1S boot logs from serial port",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s                                    # Auto-detect port, 10s capture
  %(prog)s --port /dev/ttyUSB0                # Specific port
  %(prog)s --duration 15                      # Longer capture
  %(prog)s --output boot.txt                  # Save to file
  %(prog)s --no-reset                         # Monitor without reset
        """
    )
    
    parser.add_argument(
        "--port", "-p",
        type=str,
        default=None,
        help="Serial port (auto-detect if not specified)"
    )
    
    parser.add_argument(
        "--baud", "-b",
        type=int,
        default=115200,
        help="Baud rate (default: 115200)"
    )
    
    parser.add_argument(
        "--duration", "-d",
        type=float,
        default=10.0,
        help="Capture duration in seconds (default: 10)"
    )
    
    parser.add_argument(
        "--output", "-o",
        type=str,
        default=None,
        help="Save output to file"
    )
    
    parser.add_argument(
        "--no-reset",
        action="store_true",
        help="Skip device reset (monitor only)"
    )
    
    args = parser.parse_args()
    
    # Auto-detect or validate port
    port = args.port or find_serial_port()
    
    if not os.path.exists(port):
        print(f"ERROR: Serial port not found: {port}", file=sys.stderr)
        sys.exit(1)
    
    print(f"Using serial port: {port}")
    print(f"Baud rate: {args.baud}")
    print(f"Capture duration: {args.duration}s")
    
    if args.output:
        print(f"Output file: {args.output}")
    
    # Create monitor
    monitor = SerialMonitor(port, args.baud)
    monitor.output_file = args.output
    
    try:
        # Open serial port
        print("Opening serial port...")
        monitor.open()
        
        # Small delay to let serial port stabilize
        time.sleep(0.5)
        
        # Reset device if requested
        if not args.no_reset:
            print("Resetting device...")
            if reset_device(port):
                print("Device reset successful")
            else:
                print("WARNING: Reset may have failed, continuing anyway...")
            
            # Small delay after reset
            time.sleep(0.2)
        
        # Start monitoring
        print(f"\n{'='*60}")
        print(f"Capturing serial output for {args.duration}s...")
        print(f"{'='*60}\n")
        
        monitor.monitor(args.duration)
        
        print(f"\n{'='*60}")
        print("Capture complete")
        print(f"{'='*60}")
        
        if args.output:
            print(f"\nLogs saved to: {args.output}")
    
    except KeyboardInterrupt:
        print("\n\nInterrupted by user")
    
    except Exception as e:
        print(f"\nERROR: {e}", file=sys.stderr)
        sys.exit(1)
    
    finally:
        monitor.close()


if __name__ == "__main__":
    main()
