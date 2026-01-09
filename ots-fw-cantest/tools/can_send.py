#!/usr/bin/env python3
"""Send CAN bus commands to cantest firmware.

This is a simple command-line utility for sending CAN messages to the
ESP32 running cantest firmware. It demonstrates the use of the lib/
helper modules and serves as the foundation for more complex tools.

Usage examples:
    # Auto-detect device and send help command
    ./can_send.py h
    
    # Specify port explicitly
    ./can_send.py --port /dev/ttyUSB0 m
    
    # Reset device and capture boot log
    ./can_send.py --reset h
    
    # JSON output
    ./can_send.py --json d
    
Exit codes:
    0 - Success (command sent and response received)
    1 - Error (connection failed, timeout, etc.)
"""

import sys
import argparse
import json
import time
from pathlib import Path

# Add lib/ to path for imports
sys.path.insert(0, str(Path(__file__).parent))

from lib.device import ESP32Device
from lib.serial_helper import quick_connect


def parse_args():
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser(
        description='Send commands to cantest firmware',
        epilog='''
Command examples:
  h              Show help
  m              Enter monitor mode
  a              Enter audio simulator mode
  c              Enter controller mode  
  d              Send discovery query (MODULE_QUERY)
  q              Quit/exit current mode
        ''',
        formatter_class=argparse.RawDescriptionHelpFormatter
    )
    
    parser.add_argument('command', 
                       help='Command to send (single character or string)')
    
    parser.add_argument('--port', '-p',
                       help='Serial port (default: auto-detect)')
    
    parser.add_argument('--baudrate', '-b',
                       type=int,
                       default=115200,
                       help='Baud rate (default: 115200)')
    
    parser.add_argument('--reset', '-r',
                       action='store_true',
                       help='Reset device before sending command')
    
    parser.add_argument('--capture-boot',
                       action='store_true',
                       help='Capture and show boot log (implies --reset)')
    
    parser.add_argument('--timeout', '-t',
                       type=float,
                       default=2.0,
                       help='Response timeout in seconds (default: 2.0)')
    
    parser.add_argument('--wait', '-w',
                       type=float,
                       default=0.2,
                       help='Wait time before reading response (default: 0.2)')
    
    parser.add_argument('--json', '-j',
                       action='store_true',
                       help='Output in JSON format')
    
    parser.add_argument('--verbose', '-v',
                       action='store_true',
                       help='Verbose output (stderr)')
    
    return parser.parse_args()


def send_command(args):
    """Send command to device and return response.
    
    Returns:
        tuple: (success: bool, response: str, boot_log: list)
    """
    port = args.port
    
    # Auto-detect if not specified
    if port is None:
        if args.verbose:
            print("Auto-detecting device...", file=sys.stderr)
        port = ESP32Device.auto_detect_port()
        if port is None:
            print("Error: No ESP32 device found", file=sys.stderr)
            print("Hint: Try specifying port with --port /dev/ttyUSB0", file=sys.stderr)
            return (False, "", [])
        if args.verbose:
            print(f"Found device: {port}", file=sys.stderr)
    
    # Connect to device
    if args.verbose:
        print(f"Connecting to {port}...", file=sys.stderr)
    
    device = ESP32Device(port, baudrate=args.baudrate)
    capture_boot = args.capture_boot or args.reset
    
    if not device.open(capture_boot_log=capture_boot):
        print(f"Error: Failed to connect to {port}", file=sys.stderr)
        return (False, "", [])
    
    boot_log = device.boot_log if capture_boot else []
    
    if args.verbose and capture_boot:
        print(f"Boot log captured: {len(boot_log)} lines", file=sys.stderr)
    
    try:
        # Small delay for device stability
        time.sleep(0.1)
        
        # Clear any pending data
        device.flush_input()
        
        # Send command
        if args.verbose:
            print(f"Sending command: {repr(args.command)}", file=sys.stderr)
        
        device.write(args.command)
        
        # Wait before reading
        time.sleep(args.wait)
        
        # Read response
        response = device.read(timeout=args.timeout)
        
        if args.verbose:
            print(f"Received {len(response)} chars", file=sys.stderr)
        
        return (True, response, boot_log)
        
    finally:
        device.close()


def output_text(success, response, boot_log, args):
    """Output response in human-readable text format."""
    if boot_log and args.verbose:
        print("\n=== Boot Log ===", file=sys.stderr)
        for line in boot_log:
            print(f"  {line}", file=sys.stderr)
        print("================\n", file=sys.stderr)
    
    if response:
        print(response)
    else:
        print("(no response)", file=sys.stderr)
    
    return 0 if success else 1


def output_json(success, response, boot_log, args):
    """Output response in JSON format."""
    result = {
        'success': success,
        'command': args.command,
        'port': args.port if args.port else ESP32Device.auto_detect_port(),
        'response': response,
        'response_length': len(response),
        'boot_log': boot_log if boot_log else None,
        'boot_log_lines': len(boot_log) if boot_log else 0
    }
    
    print(json.dumps(result, indent=2))
    return 0 if success else 1


def main():
    """Main entry point."""
    args = parse_args()
    
    # Send command
    success, response, boot_log = send_command(args)
    
    # Output result
    if args.json:
        return output_json(success, response, boot_log, args)
    else:
        return output_text(success, response, boot_log, args)


if __name__ == '__main__':
    sys.exit(main())
