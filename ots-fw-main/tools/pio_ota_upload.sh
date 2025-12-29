#!/usr/bin/env bash
# PlatformIO OTA Upload Helper Script
# Uploads firmware using PlatformIO's built-in OTA support

set -e

BLUE='\033[0;34m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

print_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
print_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
print_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
print_error() { echo -e "${RED}[ERROR]${NC} $1"; }

echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}PlatformIO OTA Upload${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Check we're in the right directory
if [ ! -f "platformio.ini" ]; then
    print_error "Must be run from ots-fw-main directory"
    exit 1
fi

# Step 1: Check for device via mDNS
print_info "Looking for device at ots-fw-main.local..."
if ping -c 1 -W 2 ots-fw-main.local &> /dev/null; then
    DEVICE_HOST="ots-fw-main.local"
    DEVICE_IP=$(ping -c 1 ots-fw-main.local | grep -oP '\(\K[0-9.]+(?=\))' | head -1)
    print_success "Found device at $DEVICE_HOST ($DEVICE_IP)"
else
    print_warning "Device not found via mDNS"
    echo ""
    echo "Please enter device IP address or hostname:"
    read -p "> " DEVICE_HOST
    
    if [ -z "$DEVICE_HOST" ]; then
        print_error "No hostname/IP provided"
        exit 1
    fi
    
    print_info "Testing connection to $DEVICE_HOST..."
    if ! ping -c 1 -W 2 "$DEVICE_HOST" &> /dev/null; then
        print_error "Cannot reach $DEVICE_HOST"
        exit 1
    fi
    print_success "Device is reachable"
fi

# Step 2: Build if needed
print_info "Checking if firmware needs building..."
if [ ! -f ".pio/build/esp32-s3-dev/firmware.bin" ]; then
    print_info "Building firmware..."
    pio run -e esp32-s3-dev
fi

FIRMWARE_SIZE=$(stat -f%z ".pio/build/esp32-s3-dev/firmware.bin" 2>/dev/null || stat -c%s ".pio/build/esp32-s3-dev/firmware.bin")
print_success "Firmware ready: $(numfmt --to=iec-i --suffix=B $FIRMWARE_SIZE 2>/dev/null || echo $FIRMWARE_SIZE bytes)"

# Step 3: Perform OTA upload
echo ""
print_info "Starting OTA upload to $DEVICE_HOST..."
print_warning "This will take 30-60 seconds. Device will reboot automatically."
echo ""

# PlatformIO OTA upload command
# The --upload-port triggers OTA mode automatically
pio run -e esp32-s3-dev -t upload --upload-port "$DEVICE_HOST"

if [ $? -eq 0 ]; then
    echo ""
    print_success "OTA upload completed successfully!"
    print_info "Device is rebooting..."
    echo ""
    print_info "Waiting 10 seconds for reboot..."
    sleep 10
    
    print_info "To monitor device: pio device monitor"
else
    echo ""
    print_error "OTA upload failed!"
    echo ""
    print_info "Troubleshooting:"
    echo "  1. Ensure device is connected to WiFi (check serial monitor)"
    echo "  2. Verify OTA server started (look for 'OTA server started' in logs)"
    echo "  3. Check firewall isn't blocking port 3232"
    echo "  4. Try with IP address instead of hostname"
    exit 1
fi
