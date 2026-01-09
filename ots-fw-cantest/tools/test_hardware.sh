#!/bin/bash
# Quick CAN hardware test - checks TWAI state after power cycle

echo "=== CAN Hardware Quick Test ==="
echo ""
echo "This script will:"
echo "1. Monitor TWAI state for 10 seconds"
echo "2. Check if state changes from RUNNING to BUS_OFF"
echo "3. Count TX/RX messages"
echo ""
echo "Expected with working hardware: State stays RUNNING"
echo "Expected with broken hardware: State changes to BUS_OFF within 5 seconds"
echo ""

if [ -z "$1" ]; then
    echo "Usage: $0 <serial_port>"
    echo "Example: $0 /dev/ttyACM1"
    exit 1
fi

PORT=$1

echo "Connecting to $PORT..."
echo "Press Ctrl+C to stop"
echo ""

# Reset device and monitor for 10 seconds
stty -F $PORT 115200 raw -echo
sleep 0.5

# Send 't' command to show stats
echo "t" > $PORT
sleep 1

# Read output for 10 seconds
timeout 10 cat $PORT | grep -E "State:|Messages|BUS_OFF|RUNNING" | head -20

echo ""
echo "=== Test Complete ==="
echo ""
echo "If you saw 'State: BUS_OFF':"
echo "  → Check TJA1050 pin 8 (S) is connected to GND"
echo "  → Add 120Ω resistors between CANH-CANL on both boards"
echo ""
echo "If you saw 'State: RUNNING':"
echo "  → Hardware OK! Problem is elsewhere"
