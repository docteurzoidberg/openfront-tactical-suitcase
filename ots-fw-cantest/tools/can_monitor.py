#!/usr/bin/env python3
"""
CAN Monitor - Live CAN Bus Traffic Capture

Captures CAN messages from cantest firmware in monitor mode and outputs
timestamped JSON events. Designed for piping to can_decode.py or logging.

Usage:
    ./can_monitor.py                    # Auto-detect, capture until Ctrl+C
    ./can_monitor.py --duration 30      # Capture for 30 seconds
    ./can_monitor.py --port /dev/ttyUSB0 --output capture.json
    ./can_monitor.py | ./can_decode.py  # Live decoding pipeline
    ./can_monitor.py | jq '.data.can_id'  # Filter with jq

Output Format (JSON lines):
    {"timestamp": 1234567890.123, "event": "rx", "data": {"can_id": 1040, ...}}
"""

import argparse
import json
import sys
import time
import re
from pathlib import Path

# Add lib to path for imports
sys.path.insert(0, str(Path(__file__).parent / 'lib'))

try:
    from lib.device import ESP32Device
    from lib.serial_helper import quick_connect
except ImportError:
    from device import ESP32Device
    from serial_helper import quick_connect


def parse_can_message(line):
    """
    Parse CAN message from firmware output.
    
    Expected format from cantest firmware:
    "RX: ID=0x410 DLC=8 RTR=0 EXT=0 DATA=[01 02 03 04 05 06 07 08]"
    
    Returns dict with parsed fields or None if not a CAN message.
    """
    # Match RX/TX CAN messages with hex ID
    match = re.match(
        r'(RX|TX):\s+ID=0x([0-9A-Fa-f]+)\s+DLC=(\d+)\s+RTR=(\d+)\s+EXT=(\d+)\s+DATA=\[([0-9A-Fa-f\s]+)\]',
        line
    )
    
    if not match:
        return None
    
    direction, can_id_hex, dlc, rtr, ext, data_hex = match.groups()
    
    # Parse data bytes
    data_bytes = [int(b, 16) for b in data_hex.split()]
    
    return {
        'direction': direction.lower(),
        'can_id': int(can_id_hex, 16),
        'dlc': int(dlc),
        'rtr': bool(int(rtr)),
        'extended': bool(int(ext)),
        'data': data_bytes
    }


def monitor_can_bus(device, duration=None, output_file=None, verbose=False):
    """
    Monitor CAN bus and output JSON events.
    
    Args:
        device: ESP32Device instance (already connected)
        duration: Optional duration in seconds (None = until Ctrl+C)
        output_file: Optional file to write output (None = stdout)
        verbose: Enable verbose logging to stderr
    
    Returns:
        Number of messages captured
    """
    start_time = time.time()
    message_count = 0
    
    # Open output file or use stdout
    if output_file:
        output = open(output_file, 'w')
        if verbose:
            print(f"Writing to {output_file}", file=sys.stderr)
    else:
        output = sys.stdout
    
    try:
        # Enter monitor mode
        if verbose:
            print("Entering monitor mode...", file=sys.stderr)
        
        device.write('m\n')
        time.sleep(0.5)
        
        # Clear initial response
        device.read(timeout=0.5)
        
        if verbose:
            print("Monitoring CAN bus (Ctrl+C to stop)...", file=sys.stderr)
        
        # Monitor loop
        while True:
            # Check duration
            if duration and (time.time() - start_time) >= duration:
                if verbose:
                    print(f"\nDuration {duration}s reached", file=sys.stderr)
                break
            
            # Read line with timeout
            line = device.read_line(timeout=0.1)
            if not line:
                continue
            
            # Try to parse as CAN message
            can_msg = parse_can_message(line)
            if can_msg:
                # Create event
                event = {
                    'timestamp': time.time(),
                    'event': can_msg['direction'],  # 'rx' or 'tx'
                    'data': {
                        'can_id': can_msg['can_id'],
                        'dlc': can_msg['dlc'],
                        'rtr': can_msg['rtr'],
                        'extended': can_msg['extended'],
                        'data': can_msg['data']
                    }
                }
                
                # Output as JSON line
                json.dump(event, output)
                output.write('\n')
                output.flush()  # Ensure immediate output for piping
                
                message_count += 1
                
                if verbose and message_count % 10 == 0:
                    print(f"Captured {message_count} messages...", file=sys.stderr)
    
    except KeyboardInterrupt:
        if verbose:
            print(f"\nStopped by user", file=sys.stderr)
    
    finally:
        # Exit monitor mode
        if verbose:
            print("Exiting monitor mode...", file=sys.stderr)
        
        device.write('q\n')
        time.sleep(0.2)
        
        # Close output file if opened
        if output_file:
            output.close()
            if verbose:
                print(f"Wrote {message_count} messages to {output_file}", file=sys.stderr)
    
    return message_count


def main():
    parser = argparse.ArgumentParser(
        description='Capture live CAN bus traffic from cantest firmware',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Capture for 30 seconds
  ./can_monitor.py --duration 30
  
  # Capture to file
  ./can_monitor.py --output capture.json
  
  # Live decoding pipeline
  ./can_monitor.py | ./can_decode.py
  
  # Filter with jq
  ./can_monitor.py | jq '.data.can_id'
  
  # Capture until Ctrl+C (default)
  ./can_monitor.py
"""
    )
    
    parser.add_argument('--port', '-p',
                        help='Serial port (default: auto-detect)')
    parser.add_argument('--duration', '-d', type=float,
                        help='Capture duration in seconds (default: until Ctrl+C)')
    parser.add_argument('--output', '-o',
                        help='Output file (default: stdout)')
    parser.add_argument('--verbose', '-v', action='store_true',
                        help='Verbose output to stderr')
    
    args = parser.parse_args()
    
    try:
        # Connect to device
        if args.verbose:
            print("Connecting to device...", file=sys.stderr)
        
        device = quick_connect(args.port)
        
        if args.verbose:
            print(f"Connected to {device.port}", file=sys.stderr)
        
        # Monitor CAN bus
        count = monitor_can_bus(device, args.duration, args.output, args.verbose)
        
        # Close device
        device.close()
        
        if args.verbose:
            print(f"\nTotal messages captured: {count}", file=sys.stderr)
        
        sys.exit(0)
    
    except KeyboardInterrupt:
        if args.verbose:
            print("\nInterrupted by user", file=sys.stderr)
        sys.exit(0)
    
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
