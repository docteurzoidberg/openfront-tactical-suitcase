#!/usr/bin/env python3
"""
Focused loopback test for CAN bus debugging.
Tests if devices can receive their own transmitted frames in loopback mode.
"""

import serial
import time
import sys
import threading
import queue

class SimpleCANTest:
    def __init__(self, port, name):
        self.port = port
        self.name = name
        self.ser = None
        self.rx_queue = queue.Queue()
        self.reader_thread = None
        self.running = False
        
    def connect(self):
        """Open serial connection"""
        try:
            self.ser = serial.Serial(self.port, 115200, timeout=0.1)
            time.sleep(0.5)  # Wait for device reset
            # Clear any startup messages
            self.ser.reset_input_buffer()
            print(f"✓ {self.name} connected on {self.port}")
            return True
        except Exception as e:
            print(f"✗ {self.name} connection failed: {e}")
            return False
    
    def start_reader(self):
        """Start background thread to read serial data"""
        self.running = True
        self.reader_thread = threading.Thread(target=self._reader_loop, daemon=True)
        self.reader_thread.start()
        print(f"✓ {self.name} reader thread started")
    
    def _reader_loop(self):
        """Background thread that reads serial data"""
        buffer = ""
        while self.running:
            try:
                if self.ser.in_waiting:
                    chunk = self.ser.read(self.ser.in_waiting).decode('utf-8', errors='ignore')
                    buffer += chunk
                    
                    # Process complete lines
                    while '\n' in buffer:
                        line, buffer = buffer.split('\n', 1)
                        line = line.strip()
                        if line:
                            self.rx_queue.put(line)
                            # Print everything we receive
                            print(f"[{self.name}] {line}")
                else:
                    time.sleep(0.01)
            except Exception as e:
                if self.running:
                    print(f"[{self.name}] Reader error: {e}")
                break
    
    def send_command(self, cmd):
        """Send a command to the device"""
        if self.ser:
            self.ser.write(f"{cmd}\n".encode())
            self.ser.flush()
            print(f"→ {self.name}: '{cmd}'")
            time.sleep(0.1)
    
    def wait_for_pattern(self, pattern, timeout=2.0):
        """Wait for a specific pattern in the output"""
        start = time.time()
        while time.time() - start < timeout:
            try:
                line = self.rx_queue.get(timeout=0.1)
                if pattern.lower() in line.lower():
                    return True, line
            except queue.Empty:
                pass
        return False, None
    
    def get_statistics(self):
        """Request and parse statistics"""
        self.send_command('t')
        time.sleep(0.5)
        
        # Look for statistics in queue
        stats = {'rx': 0, 'tx': 0, 'errors': 0}
        lines = []
        while not self.rx_queue.empty():
            try:
                line = self.rx_queue.get_nowait()
                lines.append(line)
                if 'Messages RX:' in line:
                    stats['rx'] = int(line.split('Messages RX:')[1].strip().split()[0])
                elif 'Messages TX:' in line:
                    stats['tx'] = int(line.split('Messages TX:')[1].strip().split()[0])
                elif 'Errors:' in line:
                    stats['errors'] = int(line.split('Errors:')[1].strip().split()[0])
            except:
                pass
        
        return stats
    
    def close(self):
        """Close connection"""
        self.running = False
        if self.reader_thread:
            self.reader_thread.join(timeout=1)
        if self.ser:
            self.ser.close()


def main():
    if len(sys.argv) < 3:
        print("Usage: test_loopback.py <controller_port> <audio_port>")
        print("Example: test_loopback.py /dev/ttyACM1 /dev/ttyUSB1")
        sys.exit(1)
    
    controller_port = sys.argv[1]
    audio_port = sys.argv[2]
    
    print("\n" + "="*70)
    print("CAN LOOPBACK TEST - Focused Debugging")
    print("="*70)
    print(f"Controller: {controller_port}")
    print(f"Audio:      {audio_port}")
    print()
    
    # Create test instances
    controller = SimpleCANTest(controller_port, "CTRL")
    audio = SimpleCANTest(audio_port, "AUDIO")
    
    try:
        # Step 1: Connect both devices
        print("\n=== STEP 1: Connecting Devices ===")
        if not controller.connect():
            print("Failed to connect to controller")
            return
        if not audio.connect():
            print("Failed to connect to audio module")
            return
        
        # Step 2: Start readers
        print("\n=== STEP 2: Starting Readers ===")
        controller.start_reader()
        audio.start_reader()
        time.sleep(0.5)
        
        # Step 3: Enter monitor mode on both (passive observation)
        print("\n=== STEP 3: Entering Monitor Mode ===")
        print("(Passive mode - devices will show any CAN traffic)")
        controller.send_command('m')
        audio.send_command('m')
        time.sleep(1)
        
        # Step 4: Get initial statistics
        print("\n=== STEP 4: Initial Statistics ===")
        ctrl_stats = controller.get_statistics()
        audio_stats = audio.get_statistics()
        print(f"Controller: RX={ctrl_stats['rx']}, TX={ctrl_stats['tx']}, Errors={ctrl_stats['errors']}")
        print(f"Audio:      RX={audio_stats['rx']}, TX={audio_stats['tx']}, Errors={audio_stats['errors']}")
        
        # Step 5: Send a single test frame from controller
        print("\n=== STEP 5: Sending Test Frame from Controller ===")
        print("Command: 'ct' (send test frame)")
        controller.send_command('ct')
        time.sleep(1)
        
        # Step 6: Check if frame was received
        print("\n=== STEP 6: Checking Frame Reception ===")
        time.sleep(1)
        ctrl_stats_after = controller.get_statistics()
        audio_stats_after = audio.get_statistics()
        
        print(f"\nController AFTER:")
        print(f"  TX: {ctrl_stats['tx']} → {ctrl_stats_after['tx']} (change: +{ctrl_stats_after['tx'] - ctrl_stats['tx']})")
        print(f"  RX: {ctrl_stats['rx']} → {ctrl_stats_after['rx']} (change: +{ctrl_stats_after['rx'] - ctrl_stats['rx']})")
        print(f"  Expected in LOOPBACK mode: TX +1, RX +1 (device receives own frame)")
        
        print(f"\nAudio AFTER:")
        print(f"  TX: {audio_stats['tx']} → {audio_stats_after['tx']} (change: +{audio_stats_after['tx'] - audio_stats['tx']})")
        print(f"  RX: {audio_stats['rx']} → {audio_stats_after['rx']} (change: +{audio_stats_after['rx'] - audio_stats['rx']})")
        print(f"  Expected with working bus: RX +1 (receives frame from controller)")
        
        # Step 7: Analysis
        print("\n=== STEP 7: Analysis ===")
        tx_works = ctrl_stats_after['tx'] > ctrl_stats['tx']
        self_rx_works = ctrl_stats_after['rx'] > ctrl_stats['rx']
        bus_rx_works = audio_stats_after['rx'] > audio_stats['rx']
        
        print(f"✓ TX works: {tx_works} (controller can transmit)")
        print(f"✓ Self-RX (loopback): {self_rx_works} (controller receives own frame)")
        print(f"✓ Bus RX: {bus_rx_works} (audio receives frame from bus)")
        
        if not tx_works:
            print("\n⚠ TX not working - controller cannot transmit at all")
            print("  Check: GPIO pin configuration, TWAI driver initialization")
        
        if tx_works and not self_rx_works:
            print("\n⚠ LOOPBACK not working - device transmits but doesn't receive own frame")
            print("  Check: TWAI mode (should be NO_ACK for loopback)")
            print("  Check: RX GPIO pin configuration")
            print("  Possible: RX path is broken in hardware or driver")
        
        if tx_works and self_rx_works and not bus_rx_works:
            print("\n⚠ BUS communication not working - loopback OK but no cross-device RX")
            print("  Check: Physical CAN bus wiring (CANH/CANL connected?)")
            print("  Check: Termination resistors (120Ω at both ends)")
            print("  Check: Both transceivers powered and enabled (RS pin to GND)")
        
        if tx_works and self_rx_works and bus_rx_works:
            print("\n✅ ALL TESTS PASSED!")
            print("  - TX works")
            print("  - Loopback works (device receives own frames)")
            print("  - Bus communication works (cross-device reception)")
            print("\n  Next step: Proceed to Phase 2.5 full protocol validation")
        
        # Step 8: Send test frame from audio too
        print("\n=== STEP 8: Testing Audio Transmission ===")
        audio.send_command('ct')
        time.sleep(1)
        
        ctrl_stats_final = controller.get_statistics()
        audio_stats_final = audio.get_statistics()
        
        print(f"\nAudio TX: {audio_stats_after['tx']} → {audio_stats_final['tx']} (change: +{audio_stats_final['tx'] - audio_stats_after['tx']})")
        print(f"Audio RX: {audio_stats_after['rx']} → {audio_stats_final['rx']} (change: +{audio_stats_final['rx'] - audio_stats_after['rx']})")
        print(f"Controller RX: {ctrl_stats_after['rx']} → {ctrl_stats_final['rx']} (change: +{ctrl_stats_final['rx'] - ctrl_stats_after['rx']})")
        
        audio_tx_works = audio_stats_final['tx'] > audio_stats_after['tx']
        audio_self_rx = audio_stats_final['rx'] > audio_stats_after['rx']
        ctrl_rx_from_audio = ctrl_stats_final['rx'] > ctrl_stats_after['rx']
        
        print(f"\n✓ Audio TX: {audio_tx_works}")
        print(f"✓ Audio self-RX: {audio_self_rx}")
        print(f"✓ Controller RX from audio: {ctrl_rx_from_audio}")
        
        print("\n" + "="*70)
        print("FINAL SUMMARY")
        print("="*70)
        print(f"Controller: TX={ctrl_stats_final['tx']}, RX={ctrl_stats_final['rx']}, Errors={ctrl_stats_final['errors']}")
        print(f"Audio:      TX={audio_stats_final['tx']}, RX={audio_stats_final['rx']}, Errors={audio_stats_final['errors']}")
        
        if ctrl_stats_final['rx'] == 0 and audio_stats_final['rx'] == 0:
            print("\n❌ CRITICAL: Both devices have RX=0")
            print("   Even in loopback mode, devices should receive own frames")
            print("   Problem is in RX path - likely:")
            print("   1. RX GPIO pin not connected correctly")
            print("   2. TWAI driver RX not working")
            print("   3. Transceiver RXD output not connected to ESP32 RX pin")
        
    except KeyboardInterrupt:
        print("\n\nTest interrupted by user")
    finally:
        print("\nCleaning up...")
        controller.close()
        audio.close()
        print("Done.")


if __name__ == '__main__':
    main()
