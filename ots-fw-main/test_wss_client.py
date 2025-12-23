#!/usr/bin/env python3
"""
WSS (WebSocket Secure) Test Client for OTS Firmware

Tests the firmware's WSS server by connecting and sending test messages.
"""

import asyncio
import websockets
import json
import ssl
import sys
from datetime import datetime

class Colors:
    """ANSI color codes for terminal output"""
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    RED = '\033[91m'
    BLUE = '\033[94m'
    CYAN = '\033[96m'
    RESET = '\033[0m'
    BOLD = '\033[1m'

def print_colored(emoji, color, message):
    """Print colored message with emoji"""
    print(f"{emoji} {color}{message}{Colors.RESET}")

async def test_wss_connection(device_ip, port=3000):
    """Test WSS connection to OTS firmware"""
    
    # Disable SSL certificate verification (for self-signed cert)
    ssl_context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
    ssl_context.check_hostname = False
    ssl_context.verify_mode = ssl.CERT_NONE
    
    uri = f"wss://{device_ip}:{port}/ws"
    
    print_colored("🔧", Colors.CYAN, f"Testing WSS connection to {uri}")
    print_colored("⚠️", Colors.YELLOW, "Note: Using self-signed certificate (verification disabled)")
    print()
    
    try:
        async with websockets.connect(uri, ssl=ssl_context, ping_interval=None) as websocket:
            print_colored("✅", Colors.GREEN, f"Connected to WSS server at {uri}")
            
            # Test 1: Send handshake
            print()
            print_colored("📤", Colors.BLUE, "Test 1: Sending handshake...")
            handshake = {
                "type": "handshake",
                "clientType": "python-test-client"
            }
            await websocket.send(json.dumps(handshake))
            print(f"   Sent: {json.dumps(handshake, indent=2)}")
            
            # Wait a bit
            await asyncio.sleep(0.5)
            
            # Test 2: Send INFO event
            print()
            print_colored("📤", Colors.BLUE, "Test 2: Sending INFO event...")
            info_event = {
                "type": "event",
                "payload": {
                    "type": "INFO",
                    "timestamp": int(datetime.now().timestamp() * 1000),
                    "message": "Python test client connected"
                }
            }
            await websocket.send(json.dumps(info_event))
            print(f"   Sent: {json.dumps(info_event, indent=2)}")
            
            # Test 3: Send NUKE_LAUNCHED event
            print()
            print_colored("📤", Colors.BLUE, "Test 3: Sending NUKE_LAUNCHED event...")
            nuke_event = {
                "type": "event",
                "payload": {
                    "type": "NUKE_LAUNCHED",
                    "timestamp": int(datetime.now().timestamp() * 1000),
                    "message": "Test nuke launch",
                    "data": json.dumps({"nukeType": "atom"})
                }
            }
            await websocket.send(json.dumps(nuke_event))
            print(f"   Sent: {json.dumps(nuke_event, indent=2)}")
            
            # Test 4: Send command
            print()
            print_colored("📤", Colors.BLUE, "Test 4: Sending send-nuke command...")
            command = {
                "type": "cmd",
                "payload": {
                    "action": "send-nuke",
                    "params": {
                        "nukeType": "hydro"
                    }
                }
            }
            await websocket.send(json.dumps(command))
            print(f"   Sent: {json.dumps(command, indent=2)}")
            
            # Listen for responses
            print()
            print_colored("👂", Colors.CYAN, "Listening for responses (5 seconds)...")
            try:
                while True:
                    message = await asyncio.wait_for(websocket.recv(), timeout=5.0)
                    print_colored("📨", Colors.GREEN, "Received message:")
                    try:
                        parsed = json.loads(message)
                        print(f"   {json.dumps(parsed, indent=2)}")
                    except:
                        print(f"   {message}")
            except asyncio.TimeoutError:
                print_colored("⏱️", Colors.YELLOW, "No more messages (timeout)")
            
            print()
            print_colored("✅", Colors.GREEN, "All tests completed successfully!")
            print_colored("🔌", Colors.CYAN, "Closing connection...")
            
    except websockets.exceptions.InvalidStatusCode as e:
        print_colored("❌", Colors.RED, f"Connection failed: Invalid status code {e.status_code}")
        print_colored("💡", Colors.YELLOW, "Tip: Check if HTTPS server is running on device")
        sys.exit(1)
        
    except ConnectionRefusedError:
        print_colored("❌", Colors.RED, f"Connection refused to {uri}")
        print_colored("💡", Colors.YELLOW, "Tip: Check device IP and ensure firmware is running")
        sys.exit(1)
        
    except Exception as e:
        print_colored("❌", Colors.RED, f"Error: {type(e).__name__}: {e}")
        sys.exit(1)

def main():
    """Main entry point"""
    if len(sys.argv) < 2:
        print_colored("❌", Colors.RED, "Usage: python3 test_wss_client.py <device-ip> [port]")
        print()
        print("Examples:")
        print("  python3 test_wss_client.py 192.168.1.123")
        print("  python3 test_wss_client.py 192.168.1.123 3000")
        print("  python3 test_wss_client.py ots-device.local")
        sys.exit(1)
    
    device_ip = sys.argv[1]
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 3000
    
    print()
    print_colored("🚀", Colors.BOLD, "OTS WSS Test Client")
    print_colored("━" * 50, Colors.CYAN, "")
    print()
    
    try:
        asyncio.run(test_wss_connection(device_ip, port))
    except KeyboardInterrupt:
        print()
        print_colored("⚠️", Colors.YELLOW, "Test interrupted by user")
        sys.exit(0)

if __name__ == "__main__":
    main()
