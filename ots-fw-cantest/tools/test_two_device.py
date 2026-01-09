#!/usr/bin/env python3
"""
Phase 2.5: Two-Device CAN Protocol Validation
Tests cantest-to-cantest communication (controller → audio simulator)

This script:
1. Opens both serial ports simultaneously
2. Sets up controller and audio simulator modes
3. Runs test scenarios (discovery, audio protocol, error handling)
4. Captures traffic from both devices in parallel
5. Validates protocol compliance
"""

import sys
import time
import json
import threading
import argparse
from datetime import datetime
from queue import Queue
from pathlib import Path

# Add lib to path
sys.path.insert(0, str(Path(__file__).parent))

from lib.device import ESP32Device
from lib.protocol import decode_frame, CANFrame, CANId


class DualDeviceTest:
    """Test framework for two cantest devices."""
    
    def __init__(self, controller_port: str, audio_port: str, verbose: bool = False):
        self.controller_port = controller_port
        self.audio_port = audio_port
        self.verbose = verbose
        
        self.controller = None
        self.audio = None
        
        # Message capture queues
        self.controller_queue = Queue()
        self.audio_queue = Queue()
        
        # Reader threads
        self.controller_reader = None
        self.audio_reader = None
        self.stop_readers = False
        
        # Test results
        self.results = {
            'discovery': {'passed': False, 'details': []},
            'audio_protocol': {'passed': False, 'details': []},
            'statistics': {}
        }
    
    def setup(self) -> bool:
        """Initialize both devices and enter simulator modes."""
        print("=== Phase 2.5: Two-Device CAN Test ===")
        print(f"Controller: {self.controller_port}")
        print(f"Audio Sim:  {self.audio_port}")
        print("")
        
        # Connect to both devices
        print("Connecting to devices...")
        try:
            self.controller = ESP32Device(self.controller_port)
            self.audio = ESP32Device(self.audio_port)
            
            if not self.controller.open():
                print(f"✗ Failed to connect to controller on {self.controller_port}")
                return False
            
            if not self.audio.open():
                print(f"✗ Failed to connect to audio module on {self.audio_port}")
                return False
            
            print("✓ Both devices connected")
            
        except Exception as e:
            print(f"✗ Connection error: {e}")
            return False
        
        # Enter simulator modes
        print("\nEntering simulator modes...")
        try:
            # Audio simulator mode
            self.audio.flush_input()
            self.audio.write('i')  # Reset to idle first
            time.sleep(0.3)
            self.audio.flush_input()
            self.audio.write('a')
            time.sleep(0.5)
            audio_response = self.audio.read(timeout=0.5)
            if self.verbose:
                print(f"[DEBUG] Audio response: {audio_response[:200]}")
            if 'AUDIO' not in audio_response.upper():
                print(f"✗ Failed to enter audio simulator mode")
                print(f"  Response: {audio_response[:200]}")
                return False
            print("✓ Audio simulator mode active")
            
            # Controller simulator mode
            self.controller.flush_input()
            self.controller.write('i')  # Reset to idle first
            time.sleep(0.5)
            self.controller.flush_input()
            self.controller.write('c')
            time.sleep(1.0)  # Longer wait
            ctrl_response = self.controller.read(timeout=2.0)  # Longer timeout
            if self.verbose:
                print(f"[DEBUG] Controller response: {ctrl_response[:200]}")
            if 'CONTROLLER' not in ctrl_response.upper():
                print(f"✗ Failed to enter controller simulator mode")
                print(f"  Response: {ctrl_response[:200]}")
                return False
            print("✓ Controller simulator mode active")
            
        except Exception as e:
            print(f"✗ Mode setup error: {e}")
            return False
        
        # Start reader threads
        print("\nStarting message capture threads...")
        self.stop_readers = False
        
        self.controller_reader = threading.Thread(
            target=self._read_messages,
            args=(self.controller, self.controller_queue, "CTRL"),
            daemon=True
        )
        self.audio_reader = threading.Thread(
            target=self._read_messages,
            args=(self.audio, self.audio_queue, "AUDIO"),
            daemon=True
        )
        
        self.controller_reader.start()
        self.audio_reader.start()
        
        time.sleep(0.5)
        print("✓ Capture threads started")
        
        return True
    
    def _read_messages(self, device: ESP32Device, queue: Queue, prefix: str):
        """Background thread to read messages from device."""
        while not self.stop_readers:
            try:
                line = device.read_line(timeout=0.1)
                if line:
                    # Debug: Show all lines for troubleshooting
                    if self.verbose and ('→' in line or '←' in line or 'TX:' in line or 'RX:' in line):
                        print(f"[{prefix} RAW] {line[:150]}")
                    
                    # Parse CAN message if it looks like one
                    # Format: "→ TX: 0xID [DLC] TYPE | data bytes" or "← RX: 0xID [DLC] TYPE | data bytes"
                    if '0x' in line and '|' in line:
                        msg = self._parse_can_line(line, prefix)
                        if msg:
                            queue.put(msg)
                            if self.verbose:
                                msg_type = msg.get('decoded', {}).get('message_type', 'UNKNOWN') if msg.get('decoded') else 'UNKNOWN'
                                print(f"[{prefix} PARSED] {msg['can_id']} {msg_type}")
                        elif self.verbose:
                            print(f"[{prefix} PARSE FAILED] {line[:100]}")
            except Exception as e:
                if self.verbose:
                    print(f"[{prefix} EXCEPTION] {str(e)}")
            time.sleep(0.01)
    
    def _parse_can_line(self, line: str, source: str) -> dict:
        """Parse CAN message from firmware output line."""
        try:
            # Example: "← RX: 0x420 [8] PLAY_SOUND | 01 00 64 00 00 00 00 00"
            parts = line.split('|')
            if len(parts) != 2:
                if self.verbose and '0x' in line:
                    print(f"[PARSE] No pipe found in: {line[:80]}")
                return None
            
            header = parts[0].strip()
            data_hex = parts[1].strip()
            
            if self.verbose:
                print(f"[PARSE] Header: {header[:60]}, Data: {data_hex[:40]}")
            
            # Extract CAN ID
            can_id_str = None
            for token in header.split():
                if token.startswith('0x'):
                    can_id_str = token
                    break
            
            if not can_id_str:
                if self.verbose:
                    print(f"[PARSE] No CAN ID found in header: {header}")
                return None
            
            can_id = int(can_id_str, 16)
            
            # Extract data bytes
            data_bytes = [int(b, 16) for b in data_hex.split()]
            
            if self.verbose:
                print(f"[PARSE] CAN ID: {can_id_str}, DLC: {len(data_bytes)}, Data: {[f'{b:02X}' for b in data_bytes]}")
            
            # Decode message using CANFrame
            try:
                frame = CANFrame(
                    can_id=can_id,
                    dlc=len(data_bytes),
                    data=data_bytes
                )
                decoded = decode_frame(frame)
                
                if self.verbose:
                    msg_type = decoded.get('message_type', 'UNKNOWN') if decoded else 'NONE'
                    print(f"[PARSE] Decoded: {msg_type}")
            except Exception as e:
                if self.verbose:
                    print(f"[PARSE] decode_frame() error: {e}")
                decoded = None
            
            return {
                'timestamp': time.time(),
                'source': source,
                'can_id': f"0x{can_id:03X}",
                'can_id_int': can_id,
                'dlc': len(data_bytes),
                'data': data_bytes,
                'data_hex': ' '.join(f'{b:02X}' for b in data_bytes),
                'decoded': decoded
            }
        except:
            return None
    
    def test_discovery(self) -> bool:
        """Test MODULE_QUERY/MODULE_ANNOUNCE protocol."""
        print("\n=== Test 1: Discovery Protocol ===")
        
        # Clear queues
        while not self.controller_queue.empty():
            self.controller_queue.get()
        while not self.audio_queue.empty():
            self.audio_queue.get()
        
        # Send discovery command
        print("Sending MODULE_QUERY from controller...")
        self.controller.write('d')
        
        # Wait for messages
        time.sleep(2.0)
        
        # Check controller sent MODULE_QUERY
        query_sent = False
        announce_received = False
        
        while not self.controller_queue.empty():
            msg = self.controller_queue.get()
            if msg['can_id_int'] == 0x411:  # MODULE_QUERY (controller TX)
                query_sent = True
                print(f"  ✓ Controller sent MODULE_QUERY")
            elif msg['can_id_int'] == 0x410:  # MODULE_ANNOUNCE (controller RX from audio module)
                announce_received = True
                decoded = msg.get('decoded', {}).get('decoded', {})
                if decoded:
                    print(f"  ✓ Controller received MODULE_ANNOUNCE:")
                    print(f"    - Module Type: {decoded.get('module_type_name', 'Unknown')}")
                    print(f"    - Version: {decoded.get('version', 'Unknown')}")
                    print(f"    - CAN Block: {decoded.get('can_block_base', 'Unknown')}")
        
        # Clear audio queue (not used in this test)
        while not self.audio_queue.empty():
            self.audio_queue.get()
        
        passed = query_sent and announce_received
        
        if passed:
            print("✓ Discovery test PASSED")
        else:
            print("✗ Discovery test FAILED")
            if not query_sent:
                print("  - MODULE_QUERY not sent")
            if not announce_received:
                print("  - MODULE_ANNOUNCE not received")
        
        self.results['discovery']['passed'] = passed
        self.results['discovery']['details'] = {
            'query_sent': query_sent,
            'announce_received': announce_received
        }
        
        return passed
    
    def test_audio_protocol(self) -> bool:
        """Test PLAY_SOUND command and ACK response."""
        print("\n=== Test 2: Audio Protocol ===")
        
        # Clear queues
        while not self.controller_queue.empty():
            self.controller_queue.get()
        while not self.audio_queue.empty():
            self.audio_queue.get()
        
        # Send play sound command
        sound_index = 3
        print(f"Sending PLAY_SOUND (sound {sound_index}) from controller...")
        self.controller.write(f'p {sound_index}')
        
        # Wait for ACK
        time.sleep(1.0)
        
        # Check for PLAY_SOUND_ACK (controller receives it from audio module)
        ack_received = False
        queue_id = None
        
        while not self.controller_queue.empty():
            msg = self.controller_queue.get()
            if msg['can_id_int'] == 0x423:  # PLAY_SOUND_ACK (controller RX)
                ack_received = True
                decoded = msg.get('decoded', {}).get('decoded', {})
                if decoded:
                    queue_id = decoded.get('queue_id', None)
                    status = decoded.get('status', 'Unknown')
                    print(f"  ✓ Controller received PLAY_SOUND_ACK:")
                    print(f"    - Status: {status}")
                    print(f"    - Queue ID: {queue_id}")
        
        # Clear audio queue (not used in this test)
        while not self.audio_queue.empty():
            self.audio_queue.get()
        
        passed = ack_received and queue_id is not None
        
        if passed:
            print("✓ Audio protocol test PASSED")
        else:
            print("✗ Audio protocol test FAILED")
            if not ack_received:
                print("  - PLAY_SOUND_ACK not received")
        
        self.results['audio_protocol']['passed'] = passed
        self.results['audio_protocol']['details'] = {
            'ack_received': ack_received,
            'queue_id': queue_id
        }
        
        return passed
    
    def check_statistics(self):
        """Check statistics on both devices."""
        print("\n=== Statistics Check ===")
        
        # Get controller stats
        print("Controller statistics:")
        self.controller.flush_input()
        self.controller.write('t')
        time.sleep(0.5)
        ctrl_stats = self.controller.read(timeout=0.5)
        print(ctrl_stats)
        
        # Get audio stats
        print("\nAudio module statistics:")
        self.audio.flush_input()
        self.audio.write('t')
        time.sleep(0.5)
        audio_stats = self.audio.read(timeout=0.5)
        print(audio_stats)
        
        self.results['statistics'] = {
            'controller': ctrl_stats,
            'audio': audio_stats
        }
    
    def cleanup(self):
        """Stop reader threads and close connections."""
        print("\nCleaning up...")
        
        # Stop reader threads
        self.stop_readers = True
        if self.controller_reader:
            self.controller_reader.join(timeout=1.0)
        if self.audio_reader:
            self.audio_reader.join(timeout=1.0)
        
        # Close devices
        if self.controller:
            self.controller.close()
        if self.audio:
            self.audio.close()
        
        print("✓ Cleanup complete")
    
    def run_all_tests(self) -> bool:
        """Run complete test suite."""
        if not self.setup():
            return False
        
        try:
            # Run tests
            test1 = self.test_discovery()
            test2 = self.test_audio_protocol()
            
            # Check stats
            self.check_statistics()
            
            # Summary
            print("\n=== Test Summary ===")
            total = 0
            passed = 0
            
            for test_name, test_result in self.results.items():
                if test_name == 'statistics':
                    continue
                total += 1
                if test_result['passed']:
                    passed += 1
                    print(f"✓ {test_name}: PASSED")
                else:
                    print(f"✗ {test_name}: FAILED")
            
            print(f"\nResult: {passed}/{total} tests passed")
            
            return passed == total
            
        finally:
            self.cleanup()


def main():
    parser = argparse.ArgumentParser(
        description='Phase 2.5: Two-Device CAN Protocol Validation'
    )
    parser.add_argument(
        '--controller',
        default='/dev/ttyACM0',
        help='Controller serial port (default: /dev/ttyACM0)'
    )
    parser.add_argument(
        '--audio',
        default='/dev/ttyUSB0',
        help='Audio module serial port (default: /dev/ttyUSB0)'
    )
    parser.add_argument(
        '-v', '--verbose',
        action='store_true',
        help='Verbose message logging'
    )
    parser.add_argument(
        '--json',
        action='store_true',
        help='Output results as JSON'
    )
    
    args = parser.parse_args()
    
    # Run tests
    test = DualDeviceTest(args.controller, args.audio, args.verbose)
    success = test.run_all_tests()
    
    # Output results
    if args.json:
        print(json.dumps(test.results, indent=2))
    
    sys.exit(0 if success else 1)


if __name__ == '__main__':
    main()
