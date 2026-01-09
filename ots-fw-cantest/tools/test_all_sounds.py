#!/usr/bin/env python3
"""
Comprehensive CAN Sound Testing Script

Tests all sound indices defined in the CANBUS_MESSAGE_SPEC.md:
- SD card sounds (0-9999): Tests a representative sample
- Embedded tones (10000-10002): Tests all three test tones
- Embedded sounds (10100): Tests quack sound

Uses cantest firmware in controller mode to send PLAY_SOUND commands
and monitors for SOUND_ACK and SOUND_FINISHED responses.
"""

import serial
import time
import sys
import argparse
import re
from datetime import datetime
from typing import Dict, List, Tuple, Optional

# Sound categories from CANBUS_MESSAGE_SPEC.md
SOUND_CATEGORIES = {
    'sd_defined': [
        (0, "Game start sound"),
        (1, "Victory sound"),
        (2, "Defeat sound"),
        (3, "Player death"),
        (4, "Nuclear alert"),
        (5, "Land invasion"),
        (6, "Naval invasion"),
        (7, "Nuke launch"),
    ],
    'sd_custom_sample': [
        (100, "Custom sound sample"),
        (500, "Custom sound sample"),
        (1000, "Custom sound sample"),
    ],
    'embedded_tones': [
        (10000, "Test tone 1 (1s 440Hz)"),
        (10001, "Test tone 2 (2s 880Hz)"),
        (10002, "Test tone 3 (5s 220Hz)"),
    ],
    'embedded_sounds': [
        (10100, "Quack sound"),
    ],
}

# Expected durations (seconds) for timeout calculations
SOUND_DURATIONS = {
    10000: 1.5,   # 1s tone + margin
    10001: 2.5,   # 2s tone + margin
    10002: 5.5,   # 5s tone + margin
    10100: 1.0,   # Quack (short)
}

# Default timeout for unknown sounds
DEFAULT_TIMEOUT = 3.0

class SoundTestResult:
    """Result of a single sound test"""
    def __init__(self, sound_id: int, description: str):
        self.sound_id = sound_id
        self.description = description
        self.command_sent = False
        self.ack_received = False
        self.queue_id = None
        self.error_code = None
        self.finished_received = False
        self.finish_reason = None
        self.start_time = None
        self.ack_time = None
        self.finish_time = None
        self.error_message = None
    
    @property
    def success(self) -> bool:
        """Test succeeded if ACK received with valid queue ID"""
        return self.ack_received and self.queue_id is not None and self.error_code == 0
    
    @property
    def ack_latency(self) -> Optional[float]:
        """Time from command to ACK (ms)"""
        if self.start_time and self.ack_time:
            return (self.ack_time - self.start_time) * 1000
        return None
    
    @property
    def total_duration(self) -> Optional[float]:
        """Time from command to SOUND_FINISHED (seconds)"""
        if self.start_time and self.finish_time:
            return self.finish_time - self.start_time
        return None
    
    def __str__(self) -> str:
        status = "✓ PASS" if self.success else "✗ FAIL"
        details = []
        if self.ack_received:
            if self.error_code == 0:
                details.append(f"queue_id={self.queue_id}")
                if self.ack_latency:
                    details.append(f"ack={self.ack_latency:.1f}ms")
                if self.finished_received:
                    details.append(f"finished (reason={self.finish_reason})")
                    if self.total_duration:
                        details.append(f"duration={self.total_duration:.2f}s")
            else:
                details.append(f"error_code={self.error_code}")
        elif self.error_message:
            details.append(self.error_message)
        
        detail_str = f" ({', '.join(details)})" if details else ""
        return f"{status} [{self.sound_id:5d}] {self.description}{detail_str}"


class CanTestSoundTester:
    """CAN sound testing via cantest firmware"""
    
    def __init__(self, port: str, baudrate: int = 115200, verbose: bool = False):
        self.port = port
        self.baudrate = baudrate
        self.verbose = verbose
        self.ser = None
        self.results: Dict[int, SoundTestResult] = {}
    
    def connect(self):
        """Open serial connection to cantest"""
        try:
            self.ser = serial.Serial(self.port, self.baudrate, timeout=1)
            time.sleep(2)  # Wait for device to be ready
            self._clear_buffer()
            print(f"✓ Connected to {self.port} at {self.baudrate} baud")
            return True
        except Exception as e:
            print(f"✗ Failed to connect: {e}")
            return False
    
    def disconnect(self):
        """Close serial connection"""
        if self.ser and self.ser.is_open:
            self.ser.close()
            print("✓ Disconnected")
    
    def _clear_buffer(self):
        """Clear serial input buffer"""
        if self.ser:
            self.ser.reset_input_buffer()
            # Read and discard any pending data
            time.sleep(0.1)
            while self.ser.in_waiting:
                self.ser.read(self.ser.in_waiting)
    
    def _send_command(self, cmd: str):
        """Send command to cantest"""
        if self.ser:
            self.ser.write((cmd + '\n').encode())
            if self.verbose:
                print(f"  → {cmd}")
    
    def _read_line(self, timeout: float = 1.0) -> Optional[str]:
        """Read one line from serial with timeout"""
        if not self.ser:
            return None
        
        self.ser.timeout = timeout
        try:
            line = self.ser.readline().decode('utf-8', errors='ignore').strip()
            if line and self.verbose:
                print(f"  ← {line}")
            return line
        except:
            return None
    
    def _wait_for_pattern(self, pattern: str, timeout: float = 2.0) -> Optional[str]:
        """Wait for a line matching the pattern"""
        start_time = time.time()
        while time.time() - start_time < timeout:
            line = self._read_line(timeout=0.5)
            if line and re.search(pattern, line, re.IGNORECASE):
                return line
        return None
    
    def enter_controller_mode(self) -> bool:
        """Enter controller mode ('c' command)"""
        print("\n→ Entering controller mode...")
        self._clear_buffer()
        self._send_command('c')
        
        # Wait for mode confirmation
        line = self._wait_for_pattern(r'controller|mode', timeout=2.0)
        if line:
            print("✓ Controller mode active")
            return True
        else:
            print("✗ Failed to enter controller mode")
            return False
    
    def parse_sound_ack(self, line: str) -> Optional[Tuple[int, int, int]]:
        """Parse SOUND_ACK message from decoder output
        Returns: (sound_index, queue_id, error_code) or None
        """
        # Look for SOUND_ACK in decoded output
        # Example: "Sound: 16, Status: SUCCESS, Queue ID: 9"
        match = re.search(r'Sound:\s*(\d+).*Queue\s+ID:\s*(\d+)', line)
        if match:
            sound_idx = int(match.group(1))
            queue_id = int(match.group(2))
            # Check for SUCCESS status
            error_code = 0 if 'SUCCESS' in line else 1
            return (sound_idx, queue_id, error_code)
        return None
    
    def parse_sound_finished(self, line: str) -> Optional[Tuple[int, int, int]]:
        """Parse SOUND_FINISHED message from decoder output
        Returns: (queue_id, sound_index, reason) or None
        """
        # Look for SOUND_FINISHED in decoded output
        # Example: "Queue ID: 9, Sound: 16, Reason: UNKNOWN"
        match = re.search(r'Queue\s+ID:\s*(\d+).*Sound:\s*(\d+)', line)
        if match:
            queue_id = int(match.group(1))
            sound_idx = int(match.group(2))
            # Reason code - 0 for normal completion
            reason = 0
            return (queue_id, sound_idx, reason)
        return None
    
    def test_sound(self, sound_id: int, description: str, wait_for_finish: bool = True) -> SoundTestResult:
        """Test playing a single sound"""
        result = SoundTestResult(sound_id, description)
        
        print(f"\nTesting sound {sound_id}: {description}")
        self._clear_buffer()
        
        # Send play command
        cmd = f"p {sound_id}"
        result.start_time = time.time()
        self._send_command(cmd)
        result.command_sent = True
        
        # Wait for SOUND_ACK (should come within 200ms)
        ack_timeout = 1.0
        start_wait = time.time()
        while time.time() - start_wait < ack_timeout:
            line = self._read_line(timeout=0.5)
            if line:
                ack_data = self.parse_sound_ack(line)
                if ack_data:
                    result.ack_received = True
                    result.ack_time = time.time()
                    result.queue_id = ack_data[1]
                    result.error_code = ack_data[2]
                    
                    if result.error_code == 0:
                        print(f"  ✓ ACK received: queue_id={result.queue_id}, latency={result.ack_latency:.1f}ms")
                    else:
                        print(f"  ✗ ACK with error: error_code={result.error_code}")
                        result.error_message = f"Error code {result.error_code}"
                    break
        
        if not result.ack_received:
            print(f"  ✗ No ACK received within {ack_timeout}s")
            result.error_message = "Timeout waiting for ACK"
            return result
        
        if result.error_code != 0:
            # Error occurred, no need to wait for SOUND_FINISHED
            return result
        
        # Wait for SOUND_FINISHED if requested
        if wait_for_finish and result.queue_id:
            finish_timeout = SOUND_DURATIONS.get(sound_id, DEFAULT_TIMEOUT) + 1.0
            print(f"  ⏳ Waiting for SOUND_FINISHED (timeout={finish_timeout:.1f}s)...")
            
            start_wait = time.time()
            while time.time() - start_wait < finish_timeout:
                line = self._read_line(timeout=0.5)
                if line:
                    finish_data = self.parse_sound_finished(line)
                    if finish_data and finish_data[0] == result.queue_id:
                        result.finished_received = True
                        result.finish_time = time.time()
                        result.finish_reason = finish_data[2]
                        print(f"  ✓ SOUND_FINISHED: reason={result.finish_reason}, duration={result.total_duration:.2f}s")
                        break
            
            if not result.finished_received:
                print(f"  ⚠ No SOUND_FINISHED within {finish_timeout}s (may be looping or too long)")
        
        return result
    
    def run_tests(self, categories: List[str] = None, wait_for_finish: bool = True) -> Dict[str, List[SoundTestResult]]:
        """Run tests for specified categories"""
        if categories is None:
            categories = list(SOUND_CATEGORIES.keys())
        
        all_results = {}
        
        for category in categories:
            if category not in SOUND_CATEGORIES:
                print(f"⚠ Unknown category: {category}")
                continue
            
            print(f"\n{'='*70}")
            print(f"Testing category: {category.upper().replace('_', ' ')}")
            print(f"{'='*70}")
            
            category_results = []
            sounds = SOUND_CATEGORIES[category]
            
            for sound_id, description in sounds:
                result = self.test_sound(sound_id, description, wait_for_finish)
                category_results.append(result)
                self.results[sound_id] = result
                
                # Small delay between tests
                time.sleep(0.5)
            
            all_results[category] = category_results
        
        return all_results
    
    def print_summary(self):
        """Print test summary"""
        print(f"\n{'='*70}")
        print("TEST SUMMARY")
        print(f"{'='*70}")
        
        total = len(self.results)
        passed = sum(1 for r in self.results.values() if r.success)
        failed = total - passed
        
        print(f"\nTotal tests: {total}")
        print(f"  ✓ Passed: {passed}")
        print(f"  ✗ Failed: {failed}")
        print(f"  Success rate: {(passed/total*100):.1f}%")
        
        print(f"\n{'─'*70}")
        print("DETAILED RESULTS")
        print(f"{'─'*70}")
        
        for category, sounds in SOUND_CATEGORIES.items():
            print(f"\n{category.upper().replace('_', ' ')}:")
            for sound_id, _ in sounds:
                if sound_id in self.results:
                    print(f"  {self.results[sound_id]}")
        
        # List failures
        failures = [r for r in self.results.values() if not r.success]
        if failures:
            print(f"\n{'─'*70}")
            print("FAILURES")
            print(f"{'─'*70}")
            for result in failures:
                print(f"  {result}")
                if result.error_message:
                    print(f"    Error: {result.error_message}")
    
    def save_report(self, filename: str):
        """Save test results to file"""
        with open(filename, 'w') as f:
            f.write(f"CAN Sound Test Report\n")
            f.write(f"{'='*70}\n")
            f.write(f"Date: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
            f.write(f"Port: {self.port}\n")
            f.write(f"\n")
            
            total = len(self.results)
            passed = sum(1 for r in self.results.values() if r.success)
            
            f.write(f"Summary: {passed}/{total} passed ({passed/total*100:.1f}%)\n")
            f.write(f"\n{'─'*70}\n\n")
            
            for category, sounds in SOUND_CATEGORIES.items():
                f.write(f"\n{category.upper().replace('_', ' ')}:\n")
                for sound_id, _ in sounds:
                    if sound_id in self.results:
                        f.write(f"  {self.results[sound_id]}\n")
        
        print(f"\n✓ Report saved to: {filename}")


def main():
    parser = argparse.ArgumentParser(
        description='Test all CAN sound indices via cantest firmware',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Test all sounds
  python test_all_sounds.py /dev/ttyACM0
  
  # Test only embedded tones
  python test_all_sounds.py /dev/ttyACM0 --categories embedded_tones
  
  # Test with verbose output
  python test_all_sounds.py /dev/ttyACM0 -v
  
  # Quick test (don't wait for SOUND_FINISHED)
  python test_all_sounds.py /dev/ttyACM0 --no-wait
  
Categories:
  sd_defined        - Standard SD card sounds (0-7)
  sd_custom_sample  - Sample custom SD card sounds
  embedded_tones    - Embedded test tones (10000-10002)
  embedded_sounds   - Embedded sounds (10100)
        """
    )
    
    parser.add_argument('port', help='Serial port (e.g., /dev/ttyACM0)')
    parser.add_argument('-b', '--baudrate', type=int, default=115200,
                        help='Baud rate (default: 115200)')
    parser.add_argument('-c', '--categories', nargs='+',
                        choices=list(SOUND_CATEGORIES.keys()),
                        help='Test specific categories only')
    parser.add_argument('-v', '--verbose', action='store_true',
                        help='Verbose output (show all serial traffic)')
    parser.add_argument('--no-wait', action='store_true',
                        help='Don\'t wait for SOUND_FINISHED messages')
    parser.add_argument('-o', '--output', help='Save report to file')
    
    args = parser.parse_args()
    
    # Create tester
    tester = CanTestSoundTester(args.port, args.baudrate, args.verbose)
    
    try:
        # Connect
        if not tester.connect():
            return 1
        
        # Enter controller mode
        if not tester.enter_controller_mode():
            return 1
        
        # Run tests
        wait_for_finish = not args.no_wait
        tester.run_tests(args.categories, wait_for_finish)
        
        # Print summary
        tester.print_summary()
        
        # Save report if requested
        if args.output:
            tester.save_report(args.output)
        
        return 0
        
    except KeyboardInterrupt:
        print("\n\n⚠ Test interrupted by user")
        return 1
    
    finally:
        tester.disconnect()


if __name__ == '__main__':
    sys.exit(main())
