#!/usr/bin/env python3
"""
Simple serial helper to reset the ESP32-S3 and run the LCD diag command.

Usage:
  python tests/lcd_diag.py --port /dev/ttyACM0 --command diag

Requires: pyserial (`pip install pyserial`)
"""

from __future__ import annotations

import argparse
import signal
import sys
import time
from typing import Optional

import serial


# Global flag for graceful shutdown
interrupted = False


def signal_handler(sig, frame):
    global interrupted
    interrupted = True
    sys.stderr.write("\n[Interrupted by user]\n")
    sys.exit(130)


def pulse_reset(ser: serial.Serial, reset_ms: int) -> None:
    """Toggle RTS/DTR to reset the board via USB serial."""
    ser.dtr = False
    ser.rts = True
    time.sleep(reset_ms / 1000.0)
    ser.rts = False
    ser.dtr = True


def read_for(ser: serial.Serial, duration_s: float) -> None:
    """Stream serial output for the requested duration."""
    global interrupted
    end = time.time() + duration_s
    sys.stdout.write(f"[Reading serial for {duration_s}s...]\n")
    sys.stdout.flush()
    
    while time.time() < end and not interrupted:
        if ser.in_waiting > 0:
            line = ser.readline()
            if line:
                try:
                    sys.stdout.write(line.decode(errors="replace"))
                    sys.stdout.flush()
                except UnicodeDecodeError:
                    continue
        else:
            time.sleep(0.05)  # Brief sleep if no data available


def run(port: str, baud: int, command: str, reset_ms: int, boot_delay: float, read_seconds: float, skip_reset: bool) -> int:
    print(f"Opening {port} at {baud} baud...")
    try:
        ser = serial.Serial(port=port, baudrate=baud, timeout=0.2, write_timeout=0.5)
    except serial.SerialException as exc:
        sys.stderr.write(f"Failed to open {port}: {exc}\n")
        return 1

    with ser:
        # Ensure lines are deasserted so the board is not held in reset
        ser.dtr = False
        ser.rts = False

        if not skip_reset:
            print(f"Resetting board (RTS pulse {reset_ms}ms)...")
            pulse_reset(ser, reset_ms)
            print(f"Waiting {boot_delay}s for boot...")
            time.sleep(boot_delay)
        else:
            print("Skipping reset (reuse existing session)...")
            time.sleep(0.1)

        ser.reset_input_buffer()

        print(f"Sending command: '{command}'")
        try:
            ser.write((command + "\n").encode())
        except serial.SerialTimeoutException:
            sys.stderr.write("Write timed out; continuing to read anyway\n")

        read_for(ser, read_seconds)
        print("\n[Done]")

    return 0


def parse_args(argv: Optional[list[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Send LCD diag command over serial")
    parser.add_argument("--port", default="/dev/ttyACM0", help="Serial port (default: /dev/ttyACM0)")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate (default: 115200)")
    parser.add_argument("--command", default="diag", help="Command to send (default: diag)")
    parser.add_argument("--reset-ms", type=int, default=80, help="Pulse length for RTS reset (ms)")
    parser.add_argument("--boot-delay", type=float, default=2.0, help="Delay after reset before sending command (s)")
    parser.add_argument("--read-seconds", type=float, default=5.0, help="How long to read after sending command (s)")
    parser.add_argument("--skip-reset", action="store_true", help="Do not toggle RTS/DTR; reuse current session")
    return parser.parse_args(argv)


if __name__ == "__main__":
    signal.signal(signal.SIGINT, signal_handler)
    args = parse_args()
    sys.exit(run(
        port=args.port,
        baud=args.baud,
        command=args.command,
        reset_ms=args.reset_ms,
        boot_delay=args.boot_delay,
        read_seconds=args.read_seconds,
        skip_reset=args.skip_reset,
    ))
