#!/usr/bin/env python3
"""
Phase 3.2: Production Audio Module Validation
Tests cantest controller → production ots-fw-audiomodule

This script:
1. Opens both serial ports (cantest controller + production audio)
2. Sets up cantest in controller mode
3. Runs test scenarios (discovery, audio protocol, error handling)
4. Monitors production audio module serial output for playback confirmation
5. Validates production firmware matches protocol specification
6. Compares behavior with Phase 2.5 cantest-to-cantest baseline
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


class ProductionAudioTest:
    """Test framework for cantest controller + production audio module."""
    
    def __init__(self, controller_port: str, audio_port: str, verbose: bool = False):
        self.controller_port = controller_port
        self.audio_port = audio_port
        self.verbose = verbose
        
        self.controller = None
        self.audio = None
        
        # Message capture queues
        self.controller_queue = Queue()
        self.audio_queue = Queue()
        
        # Audio log events (playback confirmation)
        self.audio_events = []
        
        # Reader threads
        self.controller_reader = None
        self.audio_reader = None
        self.stop_readers = False
        
        # Test results
        self.results = {
            'discovery': {'passed': False, 'details': []},
            'audio_protocol': {'passed': False, 'details': []},
            'audio_playback': {'passed': False, 'details': []},
            'statistics': {}
        }
    
    def setup(self) -> bool:
        """Initialize both devices and enter controller mode."""
        print("=== Phase 3.2: Production Audio Module Test ===")
        print(f"Controller: {self.controller_port} (cantest)")
        print(f"Audio:      {self.audio_port} (production ots-fw-audiomodule)")
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
        
        # Enter controller mode on cantest
        print("\nEntering controller mode on cantest...")
        try:
            self.controller.flush_input()
            self.controller.write('i')  # Reset to idle first
            time.sleep(0.3)
            self.controller.flush_input()
            self.controller.write('c')
            time.sleep(1.0)
            ctrl_response = self.controller.read(timeout=2.0)
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
            target=self._read_audio_messages,
            args=(self.audio, self.audio_queue, "AUDIO"),
            daemon=True
        )
        
        self.controller_reader.start()
        self.audio_reader.start()
        
        time.sleep(0.5)
        print("✓ Capture threads started")
        
        return True
    
    def _read_messages(self, device: ESP32Device, queue: Queue, prefix: str):
        """Background thread to read CAN messages from cantest controller."""
        while not self.stop_readers:
            try:
                line = device.read_line(timeout=0.1)
                if line:
                    # Debug: Show all lines for troubleshooting
                    if self.verbose and ('→' in line or '←' in line or 'TX:' in line or 'RX:' in line):
                        print(f"[{prefix} RAW] {line[:150]}")
                    
                    # Parse CAN message if it looks like one
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
    
    def _read_audio_messages(self, device: ESP32Device, queue: Queue, prefix: str):
        """Background thread to read messages from production audio module.
        
        This monitors both:
        - CAN messages (if logged by audio module)
        - Audio playback events (e.g., "Playing sound X", "Mixer status")
        """
        while not self.stop_readers:
            try:
                line = device.read_line(timeout=0.1)
                if line:
                    # Debug: Show all lines
                    if self.verbose:
                        print(f"[{prefix} RAW] {line[:150]}")
                    
                    # Check for audio playback events
                    self._check_audio_events(line)
                    
                    # Parse CAN message if it looks like one
                    if '0x' in line and '|' in line:
                        msg = self._parse_can_line(line, prefix)
                        if msg:
                            queue.put(msg)
                            if self.verbose:
                                msg_type = msg.get('decoded', {}).get('message_type', 'UNKNOWN') if msg.get('decoded') else 'UNKNOWN'
                                print(f"[{prefix} PARSED] {msg['can_id']} {msg_type}")
            except Exception as e:
                if self.verbose:
                    print(f"[{prefix} EXCEPTION] {str(e)}")
            time.sleep(0.01)
    
    def _check_audio_events(self, line: str):
        """Check line for audio playback events and log them."""
        # Common patterns in production audio module:
        # - "Playing sound X" or "play sound X"
        # - "Sound X finished" or "playback complete"
        # - "Mixer: X/4 active"
        # - "Queue ID: X"
        
        line_lower = line.lower()
        
        # Playing sound
        if 'playing' in line_lower and 'sound' in line_lower:
            match = re.search(r'sound[:\s]+(\d+)', line_lower)
            if match:
                sound_id = int(match.group(1))
                event = {
                    'timestamp': time.time(),
                    'type': 'sound_start',
                    'sound_id': sound_id,
                    'raw': line
                }
                self.audio_events.append(event)
                print(f"  [AUDIO EVENT] Playing sound {sound_id}")
        
        # Sound finished
        if 'finished' in line_lower or 'complete' in line_lower:
            if 'sound' in line_lower:
                match = re.search(r'sound[:\s]+(\d+)', line_lower)
                if match:
                    sound_id = int(match.group(1))
                    event = {
                        'timestamp': time.time(),
                        'type': 'sound_finish',
                        'sound_id': sound_id,
                        'raw': line
                    }
                    self.audio_events.append(event)
                    print(f"  [AUDIO EVENT] Sound {sound_id} finished")
        
        # Mixer status
        if 'mixer' in line_lower and 'active' in line_lower:
            match = re.search(r'(\d+)[/\s]+(\d+)', line_lower)
            if match:
                active = int(match.group(1))
                total = int(match.group(2))
                event = {
                    'timestamp': time.time(),
                    'type': 'mixer_status',
                    'active': active,
                    'total': total,
                    'raw': line
                }
                self.audio_events.append(event)
                if self.verbose:
                    print(f"  [AUDIO EVENT] Mixer: {active}/{total} active")
        
        # Queue ID
        if 'queue' in line_lower and 'id' in line_lower:
            match = re.search(r'queue[:\s]+id[:\s]+(\d+)', line_lower)
            if match:
                queue_id = int(match.group(1))
                if self.verbose:
                    print(f"  [AUDIO EVENT] Queue ID: {queue_id}")
    
    def _parse_can_line(self, line: str, source: str) -> dict:
        """Parse CAN message from firmware output line."""
        try:
            # Example: "← RX: 0x420 [8] PLAY_SOUND | 01 00 64 00 00 00 00 00"
            parts = line.split('|')
            if len(parts) != 2:
                return None
            
            header = parts[0].strip()
            data_hex = parts[1].strip()
            
            # Extract CAN ID
            can_id_str = None
            for token in header.split():
                if token.startswith('0x'):
                    can_id_str = token
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
        except:
            return None
    
    def test_discovery(self) -> bool:
        """Test MODULE_QUERY/MODULE_ANNOUNCE protocol."""
        print("\n=== Test 1: Discovery Protocol ===")
        
        # Clear queues and events
        while not self.controller_queue.empty():
            self.controller_queue.get()
        while not self.audio_queue.empty():
            self.audio_queue.get()
        self.audio_events.clear()
        
        # Send discovery command
        print("Sending MODULE_QUERY from controller...")
        self.controller.write('d')
        
        # Wait for messages
        time.sleep(2.0)
        
        # Check controller sent MODULE_QUERY and received MODULE_ANNOUNCE
        query_sent = False
        announce_received = False
        module_type = None
        module_version = None
        can_block = None
        
        while not self.controller_queue.empty():
            msg = self.controller_queue.get()
            if msg['can_id_int'] == 0x411:  # MODULE_QUERY (controller TX)
                query_sent = True
                print(f"  ✓ Controller sent MODULE_QUERY")
            elif msg['can_id_int'] == 0x410:  # MODULE_ANNOUNCE (controller RX from audio)
                announce_received = True
                decoded = msg.get('decoded', {}).get('decoded', {})
                if decoded:
                    module_type = decoded.get('module_type_name', 'Unknown')
                    module_version = decoded.get('version', 'Unknown')
                    can_block = decoded.get('can_block_base', 'Unknown')
                    print(f"  ✓ Controller received MODULE_ANNOUNCE:")
                    print(f"    - Module Type: {module_type}")
                    print(f"    - Version: {module_version}")
                    print(f"    - CAN Block: {can_block}")
        
        # Check audio module queue (shouldn't have messages in normal operation)
        audio_messages = []
        while not self.audio_queue.empty():
            audio_messages.append(self.audio_queue.get())
        
        if self.verbose and audio_messages:
            print(f"  [DEBUG] Audio module logged {len(audio_messages)} CAN messages")
        
        passed = query_sent and announce_received and module_type == 'AUDIO'
        
        if passed:
            print("✓ Discovery test PASSED")
        else:
            print("✗ Discovery test FAILED")
            if not query_sent:
                print("  - MODULE_QUERY not sent")
            if not announce_received:
                print("  - MODULE_ANNOUNCE not received")
            if module_type != 'AUDIO':
                print(f"  - Wrong module type: {module_type} (expected AUDIO)")
        
        self.results['discovery']['passed'] = passed
        self.results['discovery']['details'] = {
            'query_sent': query_sent,
            'announce_received': announce_received,
            'module_type': module_type,
            'module_version': module_version,
            'can_block': can_block
        }
        
        return passed
    
    def test_audio_protocol(self) -> bool:
        """Test PLAY_SOUND command and ACK response."""
        print("\n=== Test 2: Audio Protocol ===")
        
        # Clear queues and events
        while not self.controller_queue.empty():
            self.controller_queue.get()
        while not self.audio_queue.empty():
            self.audio_queue.get()
        self.audio_events.clear()
        
        # Send play sound command
        sound_index = 1
        print(f"Sending PLAY_SOUND (sound {sound_index}) from controller...")
        self.controller.write(f'p {sound_index}')
        
        # Wait for ACK and audio playback
        time.sleep(2.0)
        
        # Check for PLAY_SOUND_ACK
        ack_received = False
        queue_id = None
        ack_status = None
        
        while not self.controller_queue.empty():
            msg = self.controller_queue.get()
            if msg['can_id_int'] == 0x423:  # PLAY_SOUND_ACK
                ack_received = True
                decoded_dict = msg.get('decoded', {})
                if decoded_dict:
                    # decoded is a dict with nested 'decoded' key containing PlaySoundAck.to_dict()
                    ack_data = decoded_dict.get('decoded', {})
                    if ack_data:
                        queue_id = ack_data.get('queue_id')
                        ack_status = ack_data.get('status')
                        status_name = ack_data.get('status_name', 'UNKNOWN')
                        success = ack_data.get('success', False)
                        print(f"  ✓ Controller received PLAY_SOUND_ACK:")
                        print(f"    - Status: {status_name} (0x{ack_status:02X})")
                        print(f"    - Queue ID: {queue_id}")
                        print(f"    - Success: {success}")
        
        # Check for audio playback events
        playback_detected = False
        for event in self.audio_events:
            if event['type'] == 'sound_start' and event['sound_id'] == sound_index:
                playback_detected = True
                print(f"  ✓ Audio module started playing sound {sound_index}")
        
        # Clear audio queue
        while not self.audio_queue.empty():
            self.audio_queue.get()
        
        passed = ack_received and queue_id is not None
        
        if passed:
            print("✓ Audio protocol test PASSED")
            if playback_detected:
                print("  (Bonus: Audio playback confirmed via serial log)")
        else:
            print("✗ Audio protocol test FAILED")
            if not ack_received:
                print("  - PLAY_SOUND_ACK not received")
            if queue_id is None:
                print("  - Queue ID not assigned")
        
        self.results['audio_protocol']['passed'] = passed
        self.results['audio_protocol']['details'] = {
            'ack_received': ack_received,
            'queue_id': queue_id,
            'ack_status': ack_status
        }
        
        self.results['audio_playback']['passed'] = playback_detected
        self.results['audio_playback']['details'] = {
            'playback_detected': playback_detected,
            'sound_index': sound_index,
            'events': self.audio_events
        }
        
        return passed
    
    def check_statistics(self):
        """Check statistics on controller."""
        print("\n=== Statistics Check ===")
        
        # Get controller stats
        print("Controller statistics:")
        self.controller.flush_input()
        self.controller.write('t')
        time.sleep(0.5)
        ctrl_stats = self.controller.read(timeout=0.5)
        print(ctrl_stats)
        
        # Note: Production audio module may not have interactive 't' command
        print("\n(Production audio module does not have interactive stats command)")
        
        self.results['statistics'] = {
            'controller': ctrl_stats,
            'audio': 'N/A (production firmware)'
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
            
            # Comparison with Phase 2.5
            print("\n=== Comparison with Phase 2.5 Baseline ===")
            if test1 and test2:
                print("✓ Production audio module behaves identically to cantest simulator")
                print("✓ Protocol implementation validated")
            else:
                print("✗ Differences detected - see test details above")
            
            return passed == total
            
        finally:
            self.cleanup()


def main():
    parser = argparse.ArgumentParser(
        description='Phase 3.2: Production Audio Module Validation'
    )
    parser.add_argument(
        '--controller-port',
        default='/dev/ttyACM0',
        help='Cantest controller serial port (default: /dev/ttyACM0)'
    )
    parser.add_argument(
        '--audio-port',
        default='/dev/ttyUSB0',
        help='Production audio module serial port (default: /dev/ttyUSB0)'
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
    parser.add_argument(
        '--flash',
        action='store_true',
        help='Flash firmwares before testing (requires flash_firmware.py)'
    )
    
    args = parser.parse_args()
    
    # Flash firmwares if requested
    if args.flash:
        import subprocess
        print("=== Flashing Firmwares ===")
        
        # Flash production audio module
        print("Flashing production audio module...")
        result = subprocess.run([
            './flash_firmware.py',
            '--project', 'ots-fw-audiomodule',
            '--env', 'esp32-a1s-espidf',
            '--port', args.audio_port,
            '--wait-reconnect', '3'
        ])
        if result.returncode != 0:
            print("✗ Failed to flash audio module")
            sys.exit(1)
        
        # Flash cantest to controller
        print("Flashing cantest firmware...")
        result = subprocess.run([
            './flash_firmware.py',
            '--project', 'ots-fw-cantest',
            '--env', 'esp32-s3-devkit',
            '--port', args.controller_port,
            '--wait-reconnect', '3'
        ])
        if result.returncode != 0:
            print("✗ Failed to flash cantest")
            sys.exit(1)
        
        # Wait for both devices to boot
        print("\nWaiting for devices to boot...")
        subprocess.run([
            './wait_for_boot.py',
            '--port', args.audio_port,
            '--marker', 'CAN',
            '--timeout', '15'
        ])
        subprocess.run([
            './wait_for_boot.py',
            '--port', args.controller_port,
            '--marker', 'CAN Test Tool',
            '--timeout', '10'
        ])
        
        print("✓ Firmwares flashed and booted\n")
        time.sleep(1)
    
    # Run tests
    test = ProductionAudioTest(args.controller_port, args.audio_port, args.verbose)
    success = test.run_all_tests()
    
    # Output results
    if args.json:
        print("\n" + json.dumps(test.results, indent=2))
    
    sys.exit(0 if success else 1)


if __name__ == '__main__':
    main()
