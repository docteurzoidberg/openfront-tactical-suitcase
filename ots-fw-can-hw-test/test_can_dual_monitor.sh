#!/bin/bash

# Dual CAN Board Monitor Script
BOARD1="/dev/ttyUSB0"
BOARD2="/dev/ttyACM0"
LOG_DIR="logs"

mkdir -p $LOG_DIR
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

echo "╔════════════════════════════════════════════════╗"
echo "║  CAN Two-Device Test - Dual Monitor            ║"
echo "║  Board 1: $BOARD1 (CP2102)                    ║"
echo "║  Board 2: $BOARD2 (USB JTAG)                  ║"
echo "╚════════════════════════════════════════════════╝"
echo ""
echo "Logs will be saved to:"
echo "  - $LOG_DIR/board1_$TIMESTAMP.log"
echo "  - $LOG_DIR/board2_$TIMESTAMP.log"
echo ""
echo "Press CTRL+C to stop monitoring"
echo "Resetting both boards in 3 seconds..."
sleep 3

# Reset both boards (toggle DTR/RTS)
python3 << 'PYTHON'
import serial
import time

ports = ["/dev/ttyUSB0", "/dev/ttyACM0"]
for port in ports:
    try:
        ser = serial.Serial(port, 115200)
        ser.setDTR(False)
        ser.setRTS(True)
        time.sleep(0.1)
        ser.setDTR(True)
        ser.setRTS(False)
        ser.close()
        print(f"✓ Reset {port}")
    except Exception as e:
        print(f"✗ Failed to reset {port}: {e}")
PYTHON

echo ""
echo "Starting monitors..."
sleep 1

# Start monitoring both boards
pio device monitor --port $BOARD1 --baud 115200 | tee $LOG_DIR/board1_$TIMESTAMP.log | sed 's/^/[BOARD1] /' &
PID1=$!

pio device monitor --port $BOARD2 --baud 115200 | tee $LOG_DIR/board2_$TIMESTAMP.log | sed 's/^/[BOARD2] /' &
PID2=$!

# Wait for user interrupt
trap "kill $PID1 $PID2 2>/dev/null; echo ''; echo 'Monitoring stopped.'; exit 0" INT

wait
