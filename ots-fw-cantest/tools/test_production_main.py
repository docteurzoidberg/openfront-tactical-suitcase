#!/usr/bin/env python3
"""
Phase 3.3: Production Main Controller Validation
Tests production ots-fw-main → cantest audio simulator

This script:
1. Opens both serial ports (production main + cantest audio simulator)
2. Sets up cantest in audio simulator mode
3. Monitors boot-time discovery from main controller
4. Validates MODULE_QUERY sent automatically
5. Tests audio module response handling
6. Documents any gaps in response processing
"""

import sys
import time
import json
import threading
import argparse
import re
from datetime import datetime
from queue import Queue
from pathlib import Path

# Add lib to path
sys.path.insert(0, str(Path(__file__).parent))

from lib.device import ESP32Device
from lib.protocol import decode_frame, CANFrame, CANId


class ProductionMainTest:
    """Test framework for production main controller + cantest audio simulator."""
    
    def __init__(self, main_port: str, audio_port: str, verbose: bool = False):
        self.main_port = main_port
        self.audio_port = audio_port
        self.verbose = verbose
        
        self.main = None
        self.audio_sim = None
        
        # Message capture queues
        self.main_queue = Queue()
        self.audio_queue = Queue()
        
        # Discovery events
        self.discovery_events = []
        
        # Reader threads
        self.main_reader = None
        self.audio_reader = None
        self.stop_readers = False
        
        # Test results
        self.results = {
            'boot_discovery': {'passed': False, 'details': {}},
            'response_handling': {'passed': False, 'details': {}},
            'statistics': {}
        }
    
    def setup(self) -> bool:
        """Initialize both devices and enter audio simulator mode."""
        print("=== Phase 3.3: Production Main Controller Test ===")
        print(f"Main Controller: {self.main_port} (production ots-fw-main)")
        print(f"Audio Simulator: {self.audio_port} (cantest)")
        print("")
        
        # Connect to both devices
        print("Connecting to devices...")
        try:
            self.main = ESP32Device(self.main_port)
            self.audio_sim = ESP32Device(self.audio_port)
            
            if not self.main.open():
                print(f"✗ Failed to connect to main controller on {self.main_port}")
                return False
            
            if not self.audio_sim.open():
                print(f"✗ Failed to connect to audio simulator on {self.audio_port}")
                return False
            
            print("✓ Both devices connected")
            print("")
            
        except Exception as e:
            print(f"✗ Connection error: {e}")
            return False
        
        # Enter audio simulator mode on cantest
        print("Entering audio simulator mode on cantest...")
        self.audio_sim.write("a\n")
        time.sleep(0.5)
        
        # Read response
        response = ""
        for _ in range(10):
            line = self.audio_sim.read_line(timeout=0.1)
            if line:
                response += line + "\n"
                if self.verbose:
                    print(f"[DEBUG] Audio sim response: {line[:80]}")
                if "AUDIO MODULE SIMULATOR" in line:
                    break
        
        if "AUDIO MODULE SIMULATOR" not in response:
            print("✗ Failed to enter audio simulator mode")
            print(f"Response: {response[:200]}")
            return False
        
        print("✓ Audio simulator mode active")
        print("")
        
        # Start message capture threads
        print("Starting message capture threads...")
        self.stop_readers = False
        self.main_reader = threading.Thread(target=self._read_main_messages, 
                                           args=(self.main, self.main_queue, "MAIN"))
        self.audio_reader = threading.Thread(target=self._read_audio_messages, 
                                            args=(self.audio_sim, self.audio_queue, "AUDIO"))
        
        self.main_reader.daemon = True
        self.audio_reader.daemon = True
        self.main_reader.start()
        self.audio_reader.start()
        
        time.sleep(0.5)
        print("✓ Capture threads started")
        print("")
        
        return True
    
    def _read_main_messages(self, device: ESP32Device, queue: Queue, prefix: str):
        """Background thread to read CAN messages from main controller."""
        while not self.stop_readers:
            try:
                line = device.read_line(timeout=0.1)
                if line:
                    # Debug: Show all lines for troubleshooting
                    if self.verbose:
                        print(f"[{prefix} RAW] {line[:150]}")
                    
                    # Check for discovery events
                    if "MODULE_QUERY" in line or "Audio module" in line:
                        self.discovery_events.append({
                            'timestamp': time.time(),
                            'line': line,
                            'source': 'main'
                        })
                    
                    # Parse CAN message if it looks like one
                    if '0x' in line and ('TX:' in line or 'RX:' in line):
                        msg = self._parse_can_line(line, prefix)
                        if msg:
                            queue.put(msg)
            except Exception as e:
                if self.verbose:
                    print(f"[{prefix} EXCEPTION] {str(e)}")
            time.sleep(0.01)
    
    def _read_audio_messages(self, device: ESP32Device, queue: Queue, prefix: str):
        """Background thread to read messages from cantest audio simulator."""
        while not self.stop_readers:
            try:
                line = device.read_line(timeout=0.1)
                if line:
                    # Debug: Show all lines
                    if self.verbose:
                        print(f"[{prefix} RAW] {line[:150]}")
                    
                    # Parse CAN message if it looks like one
                    if '0x' in line and ('TX:' in line or 'RX:' in line or '→' in line or '←' in line):
                        msg = self._parse_can_line(line, prefix)
                        if msg:
                            queue.put(msg)
            except Exception as e:
                if self.verbose:
                    print(f"[{prefix} EXCEPTION] {str(e)}")
            time.sleep(0.01)
    
    def _parse_can_line(self, line: str, source: str) -> dict:
        """Parse CAN message from firmware output line."""
        try:
            # Example: "← RX: 0x420 [8] PLAY_SOUND | 01 00 64 00 00 00 00 00"
            # Or: "I (123) CAN_DRV: ✓ TX: ID=0x411 DLC=8"
            
            parts = line.split('|')
            if len(parts) != 2:
                return None
            
            header = parts[0].strip()
            data_hex = parts[1].strip()
            
            # Extract CAN ID
            can_id_str = None
            for token in header.split():
                if token.startswith('0x') or token.startswith('ID=0x'):
                    can_id_str = token.replace('ID=', '')
                    break
            
            if not can_id_str:
                return None
            
            can_id = int(can_id_str, 16)
            
            # Extract data bytes
            data_bytes = [int(b, 16) for b in data_hex.split()]
            
            # Decode message using CANFrame
            try:
                frame = CANFrame(
                    can_id=can_id,
                    dlc=len(data_bytes),
                    data=data_bytes
                )
                decoded = decode_frame(frame)
            except:
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
        
        except Exception as e:
            if self.verbose:
                print(f"[PARSE ERROR] {str(e)}: {line[:100]}")
            return None
    
    def test_boot_discovery(self) -> bool:
        """Test boot-time discovery from main controller.
        
        Expected behavior:
        1. Main controller sends MODULE_QUERY automatically on boot
        2. Audio simulator responds with MODULE_ANNOUNCE
        3. Main controller logs "Audio module detected"
        """
        print("=== Test 1: Boot-Time Discovery ===")
        print("Monitoring for automatic MODULE_QUERY from main controller...")
        print("(This should happen within first few seconds of boot)")
        print("")
        
        # Clear queues
        while not self.main_queue.empty():
            self.main_queue.get()
        while not self.audio_queue.empty():
            self.audio_queue.get()
        
        self.discovery_events.clear()
        
        # Wait for MODULE_QUERY from main controller
        query_sent = False
        announce_received = False
        module_detected_logged = False
        
        print("Waiting up to 10 seconds for discovery...")
        timeout = time.time() + 10.0
        
        while time.time() < timeout:
            # Check main controller queue for MODULE_QUERY
            while not self.main_queue.empty():
                msg = self.main_queue.get()
                if msg['can_id_int'] == 0x411:  # MODULE_QUERY
                    query_sent = True
                    print(f"  ✓ Main controller sent MODULE_QUERY")
                    if self.verbose:
                        print(f"    Data: {msg['data_hex']}")
            
            # Check audio simulator queue for MODULE_ANNOUNCE
            while not self.audio_queue.empty():
                msg = self.audio_queue.get()
                if msg['can_id_int'] == 0x410:  # MODULE_ANNOUNCE
                    announce_received = True
                    decoded_dict = msg.get('decoded', {})
                    if decoded_dict:
                        announce_data = decoded_dict.get('decoded', {})
                        module_type = announce_data.get('module_type_name', 'UNKNOWN')
                        version = f"{announce_data.get('version_major', 0)}.{announce_data.get('version_minor', 0)}"
                        can_block = announce_data.get('can_block_base', 0)
                        print(f"  ✓ Audio simulator responded with MODULE_ANNOUNCE:")
                        print(f"    - Module Type: {module_type}")
                        print(f"    - Version: {version}")
                        print(f"    - CAN Block: 0x{can_block:02X}")
            
            # Check discovery events for "Audio module detected" log
            for event in self.discovery_events:
                if 'Audio module' in event['line'] and 'detected' in event['line']:
                    module_detected_logged = True
                    print(f"  ✓ Main controller logged: {event['line'].strip()}")
            
            if query_sent and announce_received:
                break
            
            time.sleep(0.1)
        
        print("")
        
        # Evaluate results
        passed = query_sent and announce_received
        
        if passed:
            print("✓ Boot discovery test PASSED")
            if module_detected_logged:
                print("  (Bonus: Main controller recognized audio module)")
            else:
                print("  (Note: No 'Audio module detected' log seen - may be parsing issue)")
        else:
            print("✗ Boot discovery test FAILED")
            if not query_sent:
                print("  - MODULE_QUERY not sent by main controller")
                print("  - Check if sound_module initialized properly")
            if not announce_received:
                print("  - MODULE_ANNOUNCE not received from audio simulator")
                print("  - Check audio simulator is responding")
        
        self.results['boot_discovery']['passed'] = passed
        self.results['boot_discovery']['details'] = {
            'query_sent': query_sent,
            'announce_received': announce_received,
            'module_detected_logged': module_detected_logged
        }
        
        print("")
        return passed
    
    def test_response_handling(self) -> bool:
        """Test how main controller handles audio module responses.
        
        This is a documentation test - we expect some gaps in the current
        implementation. The goal is to document what works and what doesn't.
        """
        print("=== Test 2: Response Handling (Documentation) ===")
        print("Testing main controller's ability to process audio module responses...")
        print("")
        
        # For now, this is a placeholder test
        # In reality, we'd need to trigger PLAY_SOUND commands from main controller
        # This may require:
        # - WebSocket command injection
        # - Button press simulation
        # - Serial command interface
        
        print("⚠️  Response handling test not yet implemented")
        print("    Reason: Need mechanism to trigger PLAY_SOUND from main controller")
        print("    Options:")
        print("      A. Mock WebSocket commands via test interface")
        print("      B. Hardware button press simulation")
        print("      C. Serial command injection (if supported)")
        print("")
        print("    Current implementation status:")
        print("      - sound_module.c has can_rx_task for receiving messages")
        print("      - May have gaps in ACK parsing (to be documented)")
        print("")
        
        # Mark as skipped for now
        self.results['response_handling']['passed'] = None  # None = skipped
        self.results['response_handling']['details'] = {
            'reason': 'No trigger mechanism for PLAY_SOUND commands',
            'suggestions': [
                'Add serial command interface to main controller',
                'Add WebSocket mock interface for testing',
                'Use hardware buttons (requires physical setup)'
            ]
        }
        
        print("⏭️  Response handling test SKIPPED (to be implemented)")
        print("")
        
        return True  # Don't fail on skipped tests
    
    def get_statistics(self) -> dict:
        """Get message statistics from both devices."""
        print("=== Statistics Check ===")
        
        # Count messages in queues (approximate, as we've been consuming them)
        stats = {
            'main_rx': 0,
            'main_tx': 0,
            'audio_rx': 0,
            'audio_tx': 0
        }
        
        # This is a simplified version - in reality we'd track all messages
        print("(Statistics tracking not yet fully implemented)")
        print("")
        
        return stats
    
    def run_all_tests(self) -> bool:
        """Run all test scenarios."""
        print("=" * 60)
        print("Starting Phase 3.3 test suite...")
        print("=" * 60)
        print("")
        
        # Test 1: Boot discovery
        test1 = self.test_boot_discovery()
        
        # Test 2: Response handling
        test2 = self.test_response_handling()
        
        # Get statistics
        self.results['statistics'] = self.get_statistics()
        
        # Summary
        print("=== Test Summary ===")
        
        test_results = []
        if test1:
            print("✓ boot_discovery: PASSED")
            test_results.append(True)
        else:
            print("✗ boot_discovery: FAILED")
            test_results.append(False)
        
        if test2 is None or test2:
            print("⏭️  response_handling: SKIPPED")
        else:
            print("✗ response_handling: FAILED")
            test_results.append(False)
        
        print("")
        
        passed_count = sum(test_results)
        total_count = len(test_results)
        
        print(f"Result: {passed_count}/{total_count} tests passed")
        print("")
        
        # Phase 2.5 comparison
        print("=== Comparison with Phase 2.5 Baseline ===")
        if test1:
            print("✓ Main controller discovery working (matches Phase 2.5 cantest behavior)")
        else:
            print("✗ Main controller discovery not working")
        
        print("⚠️  Response handling testing blocked (need trigger mechanism)")
        print("")
        
        return all(test_results)
    
    def cleanup(self):
        """Stop threads and close connections."""
        print("Cleaning up...")
        self.stop_readers = True
        
        if self.main_reader:
            self.main_reader.join(timeout=1.0)
        if self.audio_reader:
            self.audio_reader.join(timeout=1.0)
        
        if self.main:
            self.main.close()
        if self.audio_sim:
            self.audio_sim.close()
        
        print("✓ Cleanup complete")


def main():
    parser = argparse.ArgumentParser(
        description='Phase 3.3: Test production main controller with cantest audio simulator'
    )
    parser.add_argument('--main-port', required=True,
                       help='Serial port for production main controller (e.g., /dev/ttyACM0)')
    parser.add_argument('--audio-port', required=True,
                       help='Serial port for cantest audio simulator (e.g., /dev/ttyUSB0)')
    parser.add_argument('-v', '--verbose', action='store_true',
                       help='Enable verbose logging (show all serial lines)')
    parser.add_argument('--flash', action='store_true',
                       help='Flash firmwares before testing')
    parser.add_argument('--json', action='store_true',
                       help='Output results in JSON format')
    
    args = parser.parse_args()
    
    # Flash firmwares if requested
    if args.flash:
        print("Flashing firmwares...")
        print("  1. Main controller: ots-fw-main")
        print("  2. Audio simulator: ots-fw-cantest")
        print("")
        # TODO: Implement flashing
        print("⚠️  Auto-flash not yet implemented - flash manually first")
        print("")
    
    # Run tests
    test = ProductionMainTest(args.main_port, args.audio_port, args.verbose)
    
    try:
        if not test.setup():
            print("✗ Setup failed")
            sys.exit(1)
        
        success = test.run_all_tests()
        
        if args.json:
            print("\n=== JSON Results ===")
            print(json.dumps(test.results, indent=2))
        
        sys.exit(0 if success else 1)
    
    except KeyboardInterrupt:
        print("\n\nTest interrupted by user")
        sys.exit(130)
    
    finally:
        test.cleanup()


if __name__ == '__main__':
    main()
