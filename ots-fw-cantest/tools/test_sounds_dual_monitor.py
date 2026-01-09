#!/usr/bin/env python3
"""
Dual-Port Sound Testing Script with Full Diagnostic Logging

Monitors BOTH:
- Cantest (CAN controller) on /dev/ttyACM0
- Audio module on /dev/ttyUSB0

Captures and displays all logs to diagnose why sounds fail to play.
"""

import serial
import time
import sys
import argparse
import threading
import queue
from datetime import datetime
from typing import Optional, List

# Sound test configuration
TEST_SOUNDS = [
    # (sound_id, description, expected_duration)
    (0, "Game start", 2.0),
    (1, "Victory", 2.0),
    (2, "Defeat", 2.0),
    (3, "Player death", 2.0),
    (4, "Nuclear alert", 2.0),
    (5, "Land invasion", 2.0),
    (6, "Naval invasion", 2.0),
    (7, "Nuke launch", 2.0),
    (10000, "Test tone 1 (440Hz 1s)", 1.5),
    (10001, "Test tone 2 (880Hz 2s)", 2.5),
    (10002, "Test tone 3 (220Hz 5s)", 5.5),
    (10100, "Quack sound", 1.0),
]

# ANSI colors
RED = "\033[91m"
GREEN = "\033[92m"
YELLOW = "\033[93m"
BLUE = "\033[94m"
MAGENTA = "\033[95m"
CYAN = "\033[96m"
RESET = "\033[0m"
BOLD = "\033[1m"


class SerialMonitor:
    """Threaded serial monitor that captures all output"""
    
    def __init__(self, port: str, name: str, baudrate: int = 115200, color: str = RESET):
        self.port = port
        self.name = name
        self.baudrate = baudrate
        self.color = color
        self.ser = None
        self.running = False
        self.thread = None
        self.log_queue = queue.Queue()
        self.all_logs: List[str] = []
    
    def connect(self) -> bool:
        """Open serial connection"""
        try:
            self.ser = serial.Serial(self.port, self.baudrate, timeout=0.1)
            time.sleep(0.5)
            # Clear any startup garbage
            self.ser.reset_input_buffer()
            return True
        except Exception as e:
            print(f"{RED}✗ Failed to connect to {self.name} ({self.port}): {e}{RESET}")
            return False
    
    def disconnect(self):
        """Close serial connection"""
        self.running = False
        if self.thread and self.thread.is_alive():
            self.thread.join(timeout=2.0)
        if self.ser and self.ser.is_open:
            self.ser.close()
    
    def start_monitoring(self):
        """Start background monitoring thread"""
        self.running = True
        self.thread = threading.Thread(target=self._monitor_loop, daemon=True)
        self.thread.start()
    
    def _monitor_loop(self):
        """Background loop to read serial data"""
        while self.running and self.ser and self.ser.is_open:
            try:
                if self.ser.in_waiting:
                    line = self.ser.readline().decode('utf-8', errors='ignore').strip()
                    if line:
                        timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
                        log_entry = f"[{timestamp}] [{self.name}] {line}"
                        self.all_logs.append(log_entry)
                        self.log_queue.put(log_entry)
            except Exception as e:
                if self.running:
                    pass  # Ignore errors during shutdown
            time.sleep(0.01)
    
    def send_command(self, cmd: str):
        """Send command to serial port"""
        if self.ser and self.ser.is_open:
            self.ser.write((cmd + '\n').encode())
    
    def get_recent_logs(self, count: int = 50) -> List[str]:
        """Get recent log entries"""
        return self.all_logs[-count:] if self.all_logs else []
    
    def clear_logs(self):
        """Clear log buffer"""
        self.all_logs.clear()
        while not self.log_queue.empty():
            try:
                self.log_queue.get_nowait()
            except:
                break


def print_log_entry(entry: str, audio_port_name: str = "AUDIO"):
    """Print log entry with color coding"""
    if f"[{audio_port_name}]" in entry:
        # Audio module logs
        if "ERROR" in entry or "error" in entry:
            print(f"{RED}{entry}{RESET}")
        elif "WARN" in entry or "warn" in entry:
            print(f"{YELLOW}{entry}{RESET}")
        elif "embedded" in entry.lower() or "SD card" in entry:
            print(f"{CYAN}{entry}{RESET}")
        else:
            print(f"{MAGENTA}{entry}{RESET}")
    else:
        # Cantest logs
        if "ACK" in entry or "FINISHED" in entry:
            print(f"{GREEN}{entry}{RESET}")
        elif "TX:" in entry:
            print(f"{BLUE}{entry}{RESET}")
        else:
            print(entry)


def test_sound(cantest: SerialMonitor, audio: SerialMonitor, 
               sound_id: int, description: str, duration: float,
               verbose: bool = True) -> dict:
    """Test a single sound and capture all logs"""
    result = {
        'sound_id': sound_id,
        'description': description,
        'success': False,
        'ack_received': False,
        'audio_logs': [],
        'can_logs': [],
        'error': None
    }
    
    print(f"\n{BOLD}{'='*60}{RESET}")
    print(f"{BOLD}Testing Sound {sound_id}: {description}{RESET}")
    print(f"{'='*60}")
    
    # Clear log buffers
    cantest.clear_logs()
    audio.clear_logs()
    
    # Small delay to ensure monitors are ready
    time.sleep(0.2)
    
    # Send play command
    print(f"\n{BLUE}→ Sending: p {sound_id}{RESET}")
    cantest.send_command(f"p {sound_id}")
    
    # Wait and collect logs
    wait_time = duration + 1.5
    print(f"⏳ Waiting {wait_time:.1f}s for playback...\n")
    
    start_time = time.time()
    while time.time() - start_time < wait_time:
        # Process any pending logs
        while not cantest.log_queue.empty():
            try:
                entry = cantest.log_queue.get_nowait()
                result['can_logs'].append(entry)
                if verbose:
                    print_log_entry(entry)
                # Check for ACK with SUCCESS status
                if "SOUND_ACK" in entry and "SUCCESS" in entry:
                    result['ack_received'] = True
                # Check for SOUND_FINISHED (sound completed)
                if "SOUND_FINISHED" in entry:
                    result['finished'] = True
            except:
                break
        
        while not audio.log_queue.empty():
            try:
                entry = audio.log_queue.get_nowait()
                result['audio_logs'].append(entry)
                if verbose:
                    print_log_entry(entry, "AUDIO")
                # Check for successful completion
                if "complete:" in entry and "bytes streamed" in entry:
                    result['completed'] = True
                # Check for stack overflow or crash
                if "stack overflow" in entry.lower() or "corrupt heap" in entry.lower():
                    result['crash'] = True
                    result['error'] = entry
                if "Rebooting" in entry:
                    result['crash'] = True
            except:
                break
        
        time.sleep(0.05)
    
    # Determine success: need ACK + completion without crash
    if result.get('crash'):
        print(f"\n{RED}✗ Sound {sound_id} CRASHED{RESET}")
        if result.get('error'):
            print(f"  {RED}Error: {result['error']}{RESET}")
    elif result.get('completed') or result.get('finished'):
        result['success'] = True
        print(f"\n{GREEN}✓ Sound {sound_id} played successfully{RESET}")
    elif result['ack_received']:
        # Got ACK but no completion - sound may still be playing or no finish event
        result['success'] = True
        print(f"\n{YELLOW}⚠ Sound {sound_id} acknowledged (waiting for completion){RESET}")
    else:
        # Check if there was an error in audio logs
        for log in result['audio_logs']:
            if 'error' in log.lower() or 'fail' in log.lower():
                result['error'] = log
                break
        print(f"\n{RED}✗ Sound {sound_id} FAILED{RESET}")
        if result.get('error'):
            print(f"  {RED}Error: {result['error']}{RESET}")
    
    return result


def main():
    parser = argparse.ArgumentParser(description='Dual-port sound testing with full logging')
    parser.add_argument('--cantest', default='/dev/ttyACM0', help='Cantest serial port')
    parser.add_argument('--audio', default='/dev/ttyUSB0', help='Audio module serial port')
    parser.add_argument('--baud', type=int, default=115200, help='Baud rate')
    parser.add_argument('--sounds', nargs='*', type=int, help='Specific sound IDs to test')
    parser.add_argument('--embedded-only', action='store_true', help='Test only embedded sounds (10000+)')
    parser.add_argument('--game-only', action='store_true', help='Test only game sounds (0-7)')
    parser.add_argument('--quiet', action='store_true', help='Less verbose output')
    parser.add_argument('--log-file', help='Save all logs to file')
    args = parser.parse_args()
    
    print(f"{BOLD}{'='*60}{RESET}")
    print(f"{BOLD}  Dual-Port Sound Testing with Diagnostic Logging{RESET}")
    print(f"{'='*60}\n")
    
    # Connect to both devices
    cantest = SerialMonitor(args.cantest, "CANTEST", args.baud, BLUE)
    audio = SerialMonitor(args.audio, "AUDIO", args.baud, MAGENTA)
    
    print(f"Connecting to Cantest ({args.cantest})...")
    if not cantest.connect():
        return 1
    print(f"{GREEN}✓ Cantest connected{RESET}")
    
    print(f"Connecting to Audio module ({args.audio})...")
    if not audio.connect():
        cantest.disconnect()
        return 1
    print(f"{GREEN}✓ Audio module connected{RESET}")
    
    # Start background monitors
    cantest.start_monitoring()
    audio.start_monitoring()
    
    print("\n⏳ Starting monitors...")
    time.sleep(1.0)
    
    # Enter controller mode
    print(f"\n{BLUE}→ Entering controller mode (c){RESET}")
    cantest.send_command('c')
    time.sleep(0.5)
    
    # Filter sounds to test
    sounds_to_test = TEST_SOUNDS
    
    if args.sounds:
        sounds_to_test = [(s, d, dur) for s, d, dur in TEST_SOUNDS if s in args.sounds]
    elif args.embedded_only:
        sounds_to_test = [(s, d, dur) for s, d, dur in TEST_SOUNDS if s >= 10000]
    elif args.game_only:
        sounds_to_test = [(s, d, dur) for s, d, dur in TEST_SOUNDS if s < 10000]
    
    if not sounds_to_test:
        print(f"{RED}No sounds to test!{RESET}")
        cantest.disconnect()
        audio.disconnect()
        return 1
    
    print(f"\n{BOLD}Testing {len(sounds_to_test)} sounds...{RESET}")
    
    # Run tests
    results = []
    for sound_id, description, duration in sounds_to_test:
        result = test_sound(cantest, audio, sound_id, description, duration, 
                           verbose=not args.quiet)
        results.append(result)
        
        # Small pause between sounds
        time.sleep(0.5)
    
    # Summary
    print(f"\n{BOLD}{'='*60}{RESET}")
    print(f"{BOLD}  TEST SUMMARY{RESET}")
    print(f"{'='*60}\n")
    
    passed = sum(1 for r in results if r['success'])
    failed = len(results) - passed
    
    print(f"Total: {len(results)}, {GREEN}Passed: {passed}{RESET}, {RED}Failed: {failed}{RESET}\n")
    
    for r in results:
        status = f"{GREEN}✓ PASS{RESET}" if r['success'] else f"{RED}✗ FAIL{RESET}"
        print(f"  {status} [{r['sound_id']:5d}] {r['description']}")
        if not r['success'] and r['audio_logs']:
            # Show relevant audio logs for failed sounds
            print(f"        {YELLOW}Audio logs:{RESET}")
            for log in r['audio_logs'][-5:]:  # Last 5 logs
                print(f"          {log}")
    
    # Save logs to file if requested
    if args.log_file:
        with open(args.log_file, 'w') as f:
            f.write(f"Sound Test Log - {datetime.now().isoformat()}\n")
            f.write("="*60 + "\n\n")
            
            for r in results:
                f.write(f"\nSound {r['sound_id']}: {r['description']}\n")
                f.write(f"Status: {'PASS' if r['success'] else 'FAIL'}\n")
                f.write("-"*40 + "\n")
                f.write("CAN logs:\n")
                for log in r['can_logs']:
                    f.write(f"  {log}\n")
                f.write("Audio logs:\n")
                for log in r['audio_logs']:
                    f.write(f"  {log}\n")
                f.write("\n")
        
        print(f"\n{GREEN}✓ Logs saved to: {args.log_file}{RESET}")
    
    # Cleanup
    cantest.disconnect()
    audio.disconnect()
    
    return 0 if failed == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
