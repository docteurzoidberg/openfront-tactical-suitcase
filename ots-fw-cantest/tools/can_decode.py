#!/usr/bin/env python3
"""
CAN Decoder - Decode CAN Messages Using Protocol Definitions

Decodes CAN messages from can_monitor.py output using protocol.py definitions.
Adds human-readable 'decoded' field to each event.

Usage:
    ./can_monitor.py | ./can_decode.py                    # Live decoding
    ./can_decode.py --input capture.json                  # Decode from file
    ./can_decode.py --input capture.json --output decoded.json
    cat capture.json | ./can_decode.py | jq '.decoded'   # Pretty print

Input Format (JSON lines from can_monitor.py):
    {"timestamp": 1234567890.123, "event": "rx", "data": {"can_id": 1040, ...}}

Output Format (JSON lines with decoded field):
    {"timestamp": 1234567890.123, "event": "rx", "data": {...},
     "decoded": {"name": "MODULE_ANNOUNCE", "description": "...", "fields": {...}}}
"""

import argparse
import json
import sys
from pathlib import Path

# Add lib to path for imports
sys.path.insert(0, str(Path(__file__).parent / 'lib'))

try:
    from lib.protocol import CANFrame, CANId, decode_frame
except ImportError:
    from protocol import CANFrame, CANId, decode_frame


def decode_event(event):
    """
    Decode a CAN event using protocol definitions.
    
    Args:
        event: Event dict from can_monitor.py
    
    Returns:
        Event dict with added 'decoded' field
    """
    # Extract CAN message data
    data = event.get('data', {})
    can_id = data.get('can_id')
    payload = data.get('data', [])
    
    if can_id is None:
        # Can't decode without CAN ID
        event['decoded'] = {'error': 'Missing CAN ID'}
        return event
    
    # Create CANFrame (only uses can_id, dlc, data, timestamp)
    frame = CANFrame(
        can_id=can_id,
        data=payload,
        dlc=data.get('dlc', len(payload)),
        timestamp=event.get('timestamp')
    )
    
    # Try to decode using protocol.py decode_frame()
    try:
        decoded = decode_frame(frame)
        
        # decode_frame returns: {'raw': {...}, 'decoded': {...}, 'message_type': 'NAME'}
        if decoded and decoded.get('message_type') != 'UNKNOWN':
            # Successfully decoded
            event['decoded'] = {
                'name': decoded['message_type'],
                'description': f"{decoded['message_type']} message",
                'fields': decoded.get('decoded', {}),
                'raw': decoded.get('raw', {})
            }
        else:
            # Unknown message
            event['decoded'] = {
                'name': 'UNKNOWN',
                'description': f'Unknown CAN ID: 0x{can_id:03X}',
                'fields': {},
                'raw': {'hex': ' '.join(f'{b:02X}' for b in payload)}
            }
    
    except Exception as e:
        # Decoding error
        event['decoded'] = {
            'error': f'Decoding failed: {str(e)}',
            'raw': {'hex': ' '.join(f'{b:02X}' for b in payload)}
        }
    
    return event


def decode_stream(input_stream, output_stream, verbose=False):
    """
    Decode CAN events from input stream to output stream.
    
    Args:
        input_stream: Input file object (JSON lines)
        output_stream: Output file object (JSON lines)
        verbose: Enable verbose logging to stderr
    
    Returns:
        Number of events decoded
    """
    count = 0
    
    try:
        for line in input_stream:
            line = line.strip()
            if not line:
                continue
            
            try:
                # Parse JSON event
                event = json.loads(line)
                
                # Decode event
                decoded_event = decode_event(event)
                
                # Output decoded event
                json.dump(decoded_event, output_stream)
                output_stream.write('\n')
                output_stream.flush()  # Ensure immediate output for piping
                
                count += 1
                
                if verbose and count % 10 == 0:
                    print(f"Decoded {count} events...", file=sys.stderr)
            
            except json.JSONDecodeError as e:
                if verbose:
                    print(f"Warning: Invalid JSON line: {e}", file=sys.stderr)
                continue
            
            except KeyboardInterrupt:
                raise  # Re-raise to handle in outer try
    
    except KeyboardInterrupt:
        if verbose:
            print(f"\nStopped by user", file=sys.stderr)
    
    return count


def main():
    parser = argparse.ArgumentParser(
        description='Decode CAN messages using protocol definitions',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Live decoding pipeline
  ./can_monitor.py | ./can_decode.py
  
  # Decode from file
  ./can_decode.py --input capture.json
  
  # Decode to file
  ./can_decode.py --input capture.json --output decoded.json
  
  # Pretty print decoded messages
  cat capture.json | ./can_decode.py | jq '.decoded'
  
  # Filter MODULE_ANNOUNCE messages
  cat capture.json | ./can_decode.py | jq 'select(.decoded.name == "MODULE_ANNOUNCE")'
"""
    )
    
    parser.add_argument('--input', '-i',
                        help='Input file (JSON lines from can_monitor.py, default: stdin)')
    parser.add_argument('--output', '-o',
                        help='Output file (JSON lines with decoded field, default: stdout)')
    parser.add_argument('--verbose', '-v', action='store_true',
                        help='Verbose output to stderr')
    
    args = parser.parse_args()
    
    try:
        # Open input/output streams
        if args.input:
            input_stream = open(args.input, 'r')
            if args.verbose:
                print(f"Reading from {args.input}", file=sys.stderr)
        else:
            input_stream = sys.stdin
            if args.verbose:
                print("Reading from stdin", file=sys.stderr)
        
        if args.output:
            output_stream = open(args.output, 'w')
            if args.verbose:
                print(f"Writing to {args.output}", file=sys.stderr)
        else:
            output_stream = sys.stdout
        
        # Decode stream
        count = decode_stream(input_stream, output_stream, args.verbose)
        
        # Close streams
        if args.input:
            input_stream.close()
        if args.output:
            output_stream.close()
        
        if args.verbose:
            print(f"\nTotal events decoded: {count}", file=sys.stderr)
        
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
