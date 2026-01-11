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
    
    def __init__(
        self,
        controller_port: str,
        audio_port: str,
        verbose: bool = False,
        live_logs: bool = True,
    ):
        self.controller_port = controller_port
        self.audio_port = audio_port
        self.verbose = verbose
        self.live_logs = live_logs
        
        self.controller = None
        self.audio = None
        
        # Message capture queues
        self.controller_queue = Queue()
        self.audio_queue = Queue()
        
        # Reader threads
        self.controller_reader = None
        self.audio_reader = None
        self.log_printer = None
        self.stop_readers = False

        # Live log printing
        self.log_queue = Queue()
        self.raw_line_queues = {
            'CTRL': Queue(),
            'AUDIO': Queue(),
        }
        self.print_lock = threading.Lock()

        # Test results
        self.results = {
            'discovery': {'passed': False, 'details': []},
            'audio_protocol': {'passed': False, 'details': []},
            'stop_sound': {'passed': False, 'details': []},
            'stop_all': {'passed': False, 'details': []},
            'sound_finished': {'passed': False, 'details': []},
            'statistics': {}
        }

    def _collect_can_messages(self, q: Queue, duration_s: float) -> list[dict]:
        """Collect parsed CAN messages from a queue for a short window."""
        msgs: list[dict] = []
        end = time.time() + duration_s
        while time.time() < end:
            try:
                msgs.append(q.get(timeout=0.1))
            except Exception:
                pass
        return msgs

    def _clear_queue(self, q: Queue):
        while not q.empty():
            try:
                q.get_nowait()
            except Exception:
                break

    def _drain_lines(self, prefix: str, duration_s: float) -> list[str]:
        """Collect raw serial lines for a device for a short window."""
        lines: list[str] = []
        end = time.time() + duration_s
        q = self.raw_line_queues[prefix]
        while time.time() < end:
            try:
                _ts, _prefix, line = q.get(timeout=0.1)
                if line:
                    lines.append(line)
            except Exception:
                pass
        return lines
    
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

        if self.live_logs:
            self.log_printer = threading.Thread(
                target=self._print_logs,
                daemon=True,
            )
            self.log_printer.start()
        
        time.sleep(0.5)
        print("✓ Capture threads started")
        
        return True
    
    def _print_logs(self):
        """Background thread that prints raw serial logs from both devices."""
        while not self.stop_readers:
            try:
                item = self.log_queue.get(timeout=0.1)
            except Exception:
                continue

            if not item:
                continue

            ts, prefix, line = item
            with self.print_lock:
                print(f"[{ts}] [{prefix}] {line}")

    def _read_messages(self, device: ESP32Device, queue: Queue, prefix: str):
        """Background thread to read messages from device."""
        while not self.stop_readers:
            try:
                line = device.read_line(timeout=0.1)
                if line:
                    ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]
                    # Always capture raw lines so other parts of the script can
                    # consume them without touching the serial port.
                    if prefix in self.raw_line_queues:
                        self.raw_line_queues[prefix].put((ts, prefix, line))
                    if self.live_logs:
                        self.log_queue.put((ts, prefix, line))

                    # Debug: Show all lines for troubleshooting
                    if self.verbose and ('→' in line or '←' in line or 'TX:' in line or 'RX:' in line):
                        with self.print_lock:
                            print(f"[{prefix} RAW] {line[:150]}")
                    
                    # Parse CAN message if it looks like one
                    # Format: "→ TX: 0xID [DLC] TYPE | data bytes" or "← RX: 0xID [DLC] TYPE | data bytes"
                    if '0x' in line and '|' in line:
                        msg = self._parse_can_line(line, prefix)
                        if msg:
                            queue.put(msg)
                            if self.verbose:
                                msg_type = msg.get('decoded', {}).get('message_type', 'UNKNOWN') if msg.get('decoded') else 'UNKNOWN'
                                with self.print_lock:
                                    print(f"[{prefix} PARSED] {msg['can_id']} {msg_type}")
                        elif self.verbose:
                            with self.print_lock:
                                print(f"[{prefix} PARSE FAILED] {line[:100]}")
            except Exception as e:
                if self.verbose:
                    with self.print_lock:
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
        with self.print_lock:
            print("\n=== Test 1: Discovery Protocol ===")
        
        # Clear queues
        while not self.controller_queue.empty():
            self.controller_queue.get()
        while not self.audio_queue.empty():
            self.audio_queue.get()
        
        # Send discovery command
        with self.print_lock:
            print("Sending MODULE_QUERY from controller...")
        self.controller.write('d')
        
        controller_msgs = self._collect_can_messages(self.controller_queue, 2.0)
        
        # Check controller sent MODULE_QUERY
        query_sent = False
        announce_received = False
        
        query_payload_ok = False
        announce_payload_ok = False

        for msg in controller_msgs:
            if msg['can_id_int'] == 0x411:  # MODULE_QUERY (controller TX)
                query_sent = True
                # Spec: [FF 00 00 00 00 00 00 00]
                query_payload_ok = (
                    msg.get('dlc') == 8
                    and len(msg.get('data', [])) == 8
                    and msg['data'][0] == 0xFF
                    and all(b == 0x00 for b in msg['data'][1:])
                )
                with self.print_lock:
                    print("  ✓ Controller sent MODULE_QUERY")
                    if not query_payload_ok:
                        print(f"    - WARN: payload not spec-matching: {msg.get('data_hex', '')}")

            elif msg['can_id_int'] == 0x410:  # MODULE_ANNOUNCE (controller RX from audio module)
                announce_received = True
                decoded = msg.get('decoded', {}).get('decoded', {})
                if decoded:
                    can_block = decoded.get('can_block_base')
                    if isinstance(can_block, str) and can_block.lower().startswith('0x'):
                        try:
                            can_block = int(can_block, 16)
                        except Exception:
                            can_block = None

                    # We expect AUDIO (0x01) v1.0, block 0x42, node 0.
                    announce_payload_ok = (
                        decoded.get('module_type') == 0x01
                        and decoded.get('version_major') == 1
                        and decoded.get('version_minor') == 0
                        and can_block == 0x42
                        and decoded.get('node_id') == 0
                    )
                    with self.print_lock:
                        print("  ✓ Controller received MODULE_ANNOUNCE:")
                        print(f"    - Module Type: {decoded.get('module_type_name', 'Unknown')}")
                        print(f"    - Version: {decoded.get('version', 'Unknown')}")
                        print(f"    - CAN Block: {decoded.get('can_block_base', 'Unknown')}")
                        if not announce_payload_ok:
                            print("    - WARN: announce fields not as expected")
        
        # Clear audio queue (not used in this test)
        while not self.audio_queue.empty():
            self.audio_queue.get()
        
        passed = query_sent and announce_received and query_payload_ok
        
        if passed:
            with self.print_lock:
                print("✓ Discovery test PASSED")
        else:
            with self.print_lock:
                print("✗ Discovery test FAILED")
            if not query_sent:
                with self.print_lock:
                    print("  - MODULE_QUERY not sent")
            if not announce_received:
                with self.print_lock:
                    print("  - MODULE_ANNOUNCE not received")
        
        self.results['discovery']['passed'] = passed
        self.results['discovery']['details'] = {
            'query_sent': query_sent,
            'query_payload_ok': query_payload_ok,
            'announce_received': announce_received,
            'announce_payload_ok': announce_payload_ok,
        }
        
        return passed
    
    def test_audio_protocol(self) -> bool:
        """Test PLAY_SOUND command and ACK response."""
        with self.print_lock:
            print("\n=== Test 2: Audio Protocol ===")
        
        # Clear queues
        while not self.controller_queue.empty():
            self.controller_queue.get()
        while not self.audio_queue.empty():
            self.audio_queue.get()
        
        # Send play sound command
        sound_index = 3
        with self.print_lock:
            print(f"Sending PLAY_SOUND (sound {sound_index}) from controller...")
        self.controller.write(f'p {sound_index}')
        
        controller_msgs = self._collect_can_messages(self.controller_queue, 1.2)
        audio_msgs = self._collect_can_messages(self.audio_queue, 1.2)
        
        # Check for PLAY_SOUND_ACK (controller receives it from audio module)
        ack_received = False
        queue_id = None
        
        play_sent = False
        play_payload_ok = False

        for msg in controller_msgs:
            if msg['can_id_int'] == 0x420:  # PLAY_SOUND (controller TX)
                play_sent = True
                play_payload_ok = (
                    msg.get('dlc') == 8
                    and len(msg.get('data', [])) == 8
                    and msg['data'][0] == sound_index
                )
            if msg['can_id_int'] == 0x423:  # PLAY_SOUND_ACK (controller RX)
                ack_received = True
                decoded = msg.get('decoded', {}).get('decoded', {})
                if decoded:
                    queue_id = decoded.get('queue_id', None)
                    status = decoded.get('status', 'Unknown')
                    with self.print_lock:
                        print(f"  ✓ Controller received PLAY_SOUND_ACK:")
                        print(f"    - Status: {status}")
                        print(f"    - Queue ID: {queue_id}")
        
        audio_received_play = any(m.get('can_id_int') == 0x420 for m in audio_msgs)
        
        passed = (
            play_sent
            and play_payload_ok
            and audio_received_play
            and ack_received
            and queue_id is not None
            and int(queue_id) > 0
        )
        
        if passed:
            with self.print_lock:
                print("✓ Audio protocol test PASSED")
        else:
            with self.print_lock:
                print("✗ Audio protocol test FAILED")
            if not ack_received:
                with self.print_lock:
                    print("  - PLAY_SOUND_ACK not received")
        
        self.results['audio_protocol']['passed'] = passed
        self.results['audio_protocol']['details'] = {
            'play_sent': play_sent,
            'play_payload_ok': play_payload_ok,
            'audio_received_play': audio_received_play,
            'ack_received': ack_received,
            'queue_id': queue_id
        }
        
        return passed

    def test_stop_sound(self) -> bool:
        """Test STOP_SOUND (0x421) and STOP_SOUND_ACK (0x424)."""
        with self.print_lock:
            print("\n=== Test 3: STOP_SOUND ===")

        # Clear queues
        self._clear_queue(self.controller_queue)
        self._clear_queue(self.audio_queue)

        # 1) Start a sound to obtain a queue_id
        sound_index = 3
        with self.print_lock:
            print(f"Sending PLAY_SOUND (sound {sound_index}) to get queue_id...")
        self.controller.write(f'p {sound_index}')

        controller_msgs = self._collect_can_messages(self.controller_queue, 1.2)
        queue_id = None
        for msg in controller_msgs:
            if msg.get('can_id_int') == 0x423:
                decoded = msg.get('decoded', {}).get('decoded', {})
                if decoded:
                    queue_id = decoded.get('queue_id', None)
        if not queue_id:
            with self.print_lock:
                print("✗ Failed to obtain queue_id from PLAY_SOUND_ACK")
            self.results['stop_sound']['passed'] = False
            self.results['stop_sound']['details'] = {'queue_id': queue_id}
            return False

        # 2) Send STOP_SOUND for that queue_id
        self._clear_queue(self.controller_queue)
        self._clear_queue(self.audio_queue)
        with self.print_lock:
            print(f"Sending STOP_SOUND (queue_id={queue_id}) from controller...")
        self.controller.write(f's {int(queue_id)}')

        controller_msgs = self._collect_can_messages(self.controller_queue, 1.2)
        audio_msgs = self._collect_can_messages(self.audio_queue, 1.2)

        stop_sent = False
        stop_payload_ok = False
        audio_received_stop = any(m.get('can_id_int') == 0x421 for m in audio_msgs)
        ack_received = False
        ack_ok = False

        for msg in controller_msgs:
            if msg.get('can_id_int') == 0x421:
                stop_sent = True
                stop_payload_ok = (
                    msg.get('dlc') == 8
                    and len(msg.get('data', [])) == 8
                    and msg['data'][0] == int(queue_id)
                    and all(b == 0x00 for b in msg['data'][1:])
                )
            elif msg.get('can_id_int') == 0x424:
                ack_received = True
                decoded = msg.get('decoded', {}).get('decoded', {})
                if decoded:
                    ack_ok = (
                        decoded.get('queue_id') == int(queue_id)
                        and decoded.get('status') == 0
                    )
                    with self.print_lock:
                        print("  ✓ Controller received STOP_SOUND_ACK:")
                        print(f"    - Status: {decoded.get('status')}")
                        print(f"    - Queue ID: {decoded.get('queue_id')}")

        passed = stop_sent and stop_payload_ok and audio_received_stop and ack_received and ack_ok

        if passed:
            with self.print_lock:
                print("✓ STOP_SOUND test PASSED")
        else:
            with self.print_lock:
                print("✗ STOP_SOUND test FAILED")

        self.results['stop_sound']['passed'] = passed
        self.results['stop_sound']['details'] = {
            'queue_id': int(queue_id),
            'stop_sent': stop_sent,
            'stop_payload_ok': stop_payload_ok,
            'audio_received_stop': audio_received_stop,
            'ack_received': ack_received,
            'ack_ok': ack_ok,
        }
        return passed

    def test_stop_all(self) -> bool:
        """Test STOP_ALL (0x422) request (no ACK expected by spec)."""
        with self.print_lock:
            print("\n=== Test 4: STOP_ALL ===")

        self._clear_queue(self.controller_queue)
        self._clear_queue(self.audio_queue)
        self._clear_queue(self.raw_line_queues['AUDIO'])

        with self.print_lock:
            print("Sending STOP_ALL from controller...")
        self.controller.write('x')

        controller_msgs = self._collect_can_messages(self.controller_queue, 1.0)
        audio_lines = self._drain_lines('AUDIO', 1.0)

        stop_all_sent = False
        stop_all_payload_ok = False

        for msg in controller_msgs:
            if msg.get('can_id_int') == 0x422:
                stop_all_sent = True
                stop_all_payload_ok = (
                    msg.get('dlc') == 8
                    and len(msg.get('data', [])) == 8
                    and all(b == 0x00 for b in msg['data'])
                )

        # Best-effort confirmation from audio simulator logs.
        audio_confirmed = any('All sounds stopped' in line for line in audio_lines)

        passed = stop_all_sent and stop_all_payload_ok
        if passed:
            with self.print_lock:
                print("✓ STOP_ALL test PASSED")
                if not audio_confirmed:
                    print("  - WARN: did not see '(All sounds stopped)' in audio logs")
        else:
            with self.print_lock:
                print("✗ STOP_ALL test FAILED")

        self.results['stop_all']['passed'] = passed
        self.results['stop_all']['details'] = {
            'stop_all_sent': stop_all_sent,
            'stop_all_payload_ok': stop_all_payload_ok,
            'audio_confirmed': audio_confirmed,
        }
        return passed

    def test_sound_finished(self) -> bool:
        """Test SOUND_FINISHED (0x425) sent from audio module to controller."""
        with self.print_lock:
            print("\n=== Test 5: SOUND_FINISHED ===")

        self._clear_queue(self.controller_queue)
        self._clear_queue(self.audio_queue)

        # Obtain a queue_id
        sound_index = 3
        self.controller.write(f'p {sound_index}')
        controller_msgs = self._collect_can_messages(self.controller_queue, 1.2)
        queue_id = None
        for msg in controller_msgs:
            if msg.get('can_id_int') == 0x423:
                decoded = msg.get('decoded', {}).get('decoded', {})
                if decoded:
                    queue_id = decoded.get('queue_id', None)

        if not queue_id:
            with self.print_lock:
                print("✗ Failed to obtain queue_id")
            self.results['sound_finished']['passed'] = False
            self.results['sound_finished']['details'] = {'queue_id': queue_id}
            return False

        # Ask audio simulator to send SOUND_FINISHED for that queue_id
        self._clear_queue(self.controller_queue)
        with self.print_lock:
            print(f"Sending SOUND_FINISHED from audio (queue_id={queue_id})...")
        self.audio.write(f'f {int(queue_id)}')

        controller_msgs = self._collect_can_messages(self.controller_queue, 1.2)

        finished_received = False
        finished_ok = False
        decoded_payload = None
        for msg in controller_msgs:
            if msg.get('can_id_int') == 0x425:
                finished_received = True
                decoded = msg.get('decoded', {}).get('decoded', {})
                decoded_payload = decoded
                if decoded:
                    finished_ok = (
                        decoded.get('queue_id') == int(queue_id)
                        and decoded.get('sound_index') == 1
                        and decoded.get('reason') == 0x00
                    )
                    with self.print_lock:
                        print("  ✓ Controller received SOUND_FINISHED:")
                        print(f"    - Queue ID: {decoded.get('queue_id')}")
                        print(f"    - Sound Index: {decoded.get('sound_index')}")
                        print(f"    - Reason: {decoded.get('reason_name', decoded.get('reason'))}")

        passed = finished_received and finished_ok
        if passed:
            with self.print_lock:
                print("✓ SOUND_FINISHED test PASSED")
        else:
            with self.print_lock:
                print("✗ SOUND_FINISHED test FAILED")

        self.results['sound_finished']['passed'] = passed
        self.results['sound_finished']['details'] = {
            'queue_id': int(queue_id),
            'finished_received': finished_received,
            'finished_ok': finished_ok,
            'decoded': decoded_payload,
        }
        return passed
    
    def check_statistics(self):
        """Check statistics on both devices."""
        with self.print_lock:
            print("\n=== Statistics Check ===")
        
        # Get controller stats
        with self.print_lock:
            print("Controller statistics:")

        # Clear captured lines so we only show new stats output.
        self._clear_queue(self.raw_line_queues['CTRL'])
        self._clear_queue(self.raw_line_queues['AUDIO'])

        self.controller.flush_input()
        self.controller.write('t')

        ctrl_stats_lines = self._drain_lines('CTRL', 1.5)
        ctrl_stats = "\n".join(ctrl_stats_lines).strip()
        with self.print_lock:
            print(ctrl_stats or "(no statistics output captured)")
        
        # Get audio stats
        with self.print_lock:
            print("\nAudio module statistics:")
        self.audio.flush_input()
        self.audio.write('t')

        audio_stats_lines = self._drain_lines('AUDIO', 1.5)
        audio_stats = "\n".join(audio_stats_lines).strip()
        with self.print_lock:
            print(audio_stats or "(no statistics output captured)")
        
        self.results['statistics'] = {
            'controller': ctrl_stats,
            'audio': audio_stats
        }
    
    def cleanup(self):
        """Stop reader threads and close connections."""
        with self.print_lock:
            print("\nCleaning up...")
        
        # Stop reader threads
        self.stop_readers = True
        if self.controller_reader:
            self.controller_reader.join(timeout=1.0)
        if self.audio_reader:
            self.audio_reader.join(timeout=1.0)
        if self.log_printer:
            self.log_printer.join(timeout=1.0)
        
        # Close devices
        if self.controller:
            self.controller.close()
        if self.audio:
            self.audio.close()
        
        with self.print_lock:
            print("✓ Cleanup complete")
    
    def run_all_tests(self) -> bool:
        """Run complete test suite."""
        if not self.setup():
            return False
        
        try:
            # Run tests
            test1 = self.test_discovery()
            test2 = self.test_audio_protocol()
            test3 = self.test_stop_sound()
            test4 = self.test_stop_all()
            test5 = self.test_sound_finished()
            
            # Check stats
            self.check_statistics()
            
            # Summary
            with self.print_lock:
                print("\n=== Test Summary ===")
            total = 0
            passed = 0
            
            for test_name, test_result in self.results.items():
                if test_name == 'statistics':
                    continue
                total += 1
                if test_result['passed']:
                    passed += 1
                    with self.print_lock:
                        print(f"✓ {test_name}: PASSED")
                else:
                    with self.print_lock:
                        print(f"✗ {test_name}: FAILED")
            
            with self.print_lock:
                print(f"\nResult: {passed}/{total} tests passed")
            
            return passed == total
            
        finally:
            self.cleanup()


def main():
    parser = argparse.ArgumentParser(
        description='Phase 2.5: Two-Device CAN Protocol Validation'
    )
    parser.add_argument(
        '--controller', '--controller-port',
        default='/dev/ttyACM0',
        help='Controller serial port (default: /dev/ttyACM0)'
    )
    parser.add_argument(
        '--audio', '--audio-port',
        default='/dev/ttyUSB0',
        help='Audio module serial port (default: /dev/ttyUSB0)'
    )
    parser.add_argument(
        '-v', '--verbose',
        dest='verbose',
        action='store_true',
        default=True,
        help='Verbose message logging (default: enabled)'
    )
    parser.add_argument(
        '--no-verbose', '--quiet',
        dest='verbose',
        action='store_false',
        help='Reduce logging output (default: verbose)'
    )
    parser.add_argument(
        '--no-live-logs',
        dest='live_logs',
        action='store_false',
        help='Disable live raw serial log streaming during the run'
    )
    parser.add_argument(
        '--json',
        action='store_true',
        help='Output results as JSON'
    )
    
    args = parser.parse_args()
    
    # Run tests
    test = DualDeviceTest(args.controller, args.audio, args.verbose, live_logs=args.live_logs)
    success = test.run_all_tests()
    
    # Output results
    if args.json:
        print(json.dumps(test.results, indent=2))
    
    sys.exit(0 if success else 1)


if __name__ == '__main__':
    main()
