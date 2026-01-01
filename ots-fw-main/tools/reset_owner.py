#!/usr/bin/env python3
"""Reset owner name in device NVS via HTTP API.

This allows testing the onboarding wizard by clearing the owner name.
"""

import argparse
import sys
import urllib.request
import urllib.error


def reset_owner(host: str, port: int = 80) -> bool:
    """Reset owner name by sending empty POST to /device endpoint."""
    url = f"http://{host}:{port}/device"
    
    # Send empty owner name to clear it
    data = "ownerName=".encode('utf-8')
    
    try:
        req = urllib.request.Request(url, data=data, method='POST')
        req.add_header('Content-Type', 'application/x-www-form-urlencoded')
        
        with urllib.request.urlopen(req, timeout=5) as response:
            result = response.read().decode('utf-8')
            print(f"✓ Owner name reset successfully")
            print(f"  Response: {result.strip()}")
            return True
            
    except urllib.error.HTTPError as e:
        print(f"✗ HTTP Error {e.code}: {e.reason}")
        try:
            error_body = e.read().decode('utf-8')
            print(f"  Error: {error_body}")
        except:
            pass
        return False
        
    except urllib.error.URLError as e:
        print(f"✗ Connection failed: {e.reason}")
        print(f"  Make sure device is reachable at {host}:{port}")
        return False
        
    except Exception as e:
        print(f"✗ Unexpected error: {e}")
        return False


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Reset owner name in OTS device NVS",
        epilog="Example: %(prog)s 192.168.77.153"
    )
    parser.add_argument(
        'host',
        help='Device IP address or hostname (e.g., 192.168.77.153)'
    )
    parser.add_argument(
        '--port',
        type=int,
        default=80,
        help='HTTP port (default: 80)'
    )
    
    args = parser.parse_args()
    
    print(f"Resetting owner name on {args.host}:{args.port}...")
    success = reset_owner(args.host, args.port)
    
    if success:
        print("\n✓ Done! Reload the webapp to see the onboarding wizard.")
        return 0
    else:
        print("\n✗ Failed to reset owner name.")
        return 1


if __name__ == '__main__':
    sys.exit(main())
