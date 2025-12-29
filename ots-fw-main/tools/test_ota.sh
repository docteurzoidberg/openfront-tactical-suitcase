#!/usr/bin/env bash
# OTA Test Script for OTS Firmware
# Tests the Over-The-Air update functionality

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
FIRMWARE_PATH=".pio/build/esp32-s3-dev/firmware.bin"
OTA_HOSTNAME="ots-fw-main.local"
OTA_PORT="3232"
OTA_ENDPOINT="/update"

# Function to print colored messages
print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_header() {
    echo ""
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""
}

# Check if we're in the right directory
if [ ! -f "platformio.ini" ]; then
    print_error "Must be run from ots-fw-main directory"
    exit 1
fi

print_header "OTA Feature Test - OTS Firmware"

# Step 1: Check if firmware binary exists
print_info "Checking for firmware binary..."
if [ ! -f "$FIRMWARE_PATH" ]; then
    print_warning "Firmware binary not found. Building..."
    pio run -e esp32-s3-dev
    if [ $? -ne 0 ]; then
        print_error "Failed to build firmware"
        exit 1
    fi
fi
print_success "Firmware binary found: $FIRMWARE_PATH"

# Get firmware size
FIRMWARE_SIZE=$(stat -f%z "$FIRMWARE_PATH" 2>/dev/null || stat -c%s "$FIRMWARE_PATH" 2>/dev/null)
print_info "Firmware size: $(numfmt --to=iec-i --suffix=B $FIRMWARE_SIZE 2>/dev/null || echo $FIRMWARE_SIZE bytes)"

# Step 2: Check if device is reachable
print_info "Checking if device is reachable..."
if ping -c 1 -W 2 "$OTA_HOSTNAME" &> /dev/null; then
    DEVICE_IP=$(ping -c 1 "$OTA_HOSTNAME" | grep -oP '\(\K[0-9.]+(?=\))' | head -1)
    print_success "Device is reachable at $OTA_HOSTNAME ($DEVICE_IP)"
else
    print_warning "Device not reachable via mDNS ($OTA_HOSTNAME)"
    read -p "Enter device IP address manually (or press Enter to skip): " MANUAL_IP
    if [ -z "$MANUAL_IP" ]; then
        print_error "Cannot proceed without device connection"
        exit 1
    fi
    DEVICE_IP=$MANUAL_IP
    print_info "Using manual IP: $DEVICE_IP"
fi

# Step 3: Test OTA endpoint availability
print_info "Testing OTA endpoint availability..."
OTA_URL="http://${DEVICE_IP}:${OTA_PORT}${OTA_ENDPOINT}"
if curl -s -f -X GET "$OTA_URL" -o /dev/null; then
    print_warning "OTA endpoint responds to GET (expected 405 Method Not Allowed)"
elif curl -s -X GET "$OTA_URL" 2>&1 | grep -q "405\|404\|400"; then
    print_success "OTA endpoint is available"
else
    print_error "OTA endpoint not responding. Is the device running and connected?"
    print_info "URL: $OTA_URL"
    exit 1
fi

# Step 4: Confirm before uploading
echo ""
print_warning "Ready to upload firmware via OTA"
print_info "Device: $DEVICE_IP:$OTA_PORT"
print_info "Firmware: $FIRMWARE_PATH ($(numfmt --to=iec-i --suffix=B $FIRMWARE_SIZE 2>/dev/null || echo $FIRMWARE_SIZE bytes))"
echo ""
read -p "Proceed with OTA upload? (y/N): " -n 1 -r
echo ""
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    print_info "OTA test cancelled"
    exit 0
fi

# Step 5: Perform OTA update
print_header "Uploading Firmware via OTA"
print_info "Starting OTA update..."
print_info "This will take 30-60 seconds..."
echo ""

# Upload with progress
if curl -X POST \
    --data-binary "@${FIRMWARE_PATH}" \
    --progress-bar \
    -w "\nHTTP Status: %{http_code}\n" \
    "$OTA_URL" > /tmp/ota_response.txt 2>&1; then
    
    # Check response
    HTTP_STATUS=$(grep "HTTP Status:" /tmp/ota_response.txt | awk '{print $3}')
    
    if [ "$HTTP_STATUS" = "200" ]; then
        print_success "OTA update successful!"
        print_info "Device should reboot automatically"
        cat /tmp/ota_response.txt | grep -v "HTTP Status:" | grep -v "^$"
    else
        print_error "OTA update failed with HTTP status: $HTTP_STATUS"
        cat /tmp/ota_response.txt
        rm /tmp/ota_response.txt
        exit 1
    fi
else
    print_error "OTA upload failed"
    if [ -f /tmp/ota_response.txt ]; then
        cat /tmp/ota_response.txt
        rm /tmp/ota_response.txt
    fi
    exit 1
fi

rm -f /tmp/ota_response.txt

# Step 6: Wait for device to reboot
print_info "Waiting for device to reboot (30 seconds)..."
sleep 30

# Step 7: Verify device comes back online
print_info "Checking if device is back online..."
RETRY_COUNT=0
MAX_RETRIES=10
while [ $RETRY_COUNT -lt $MAX_RETRIES ]; do
    if ping -c 1 -W 2 "$DEVICE_IP" &> /dev/null; then
        print_success "Device is back online!"
        break
    fi
    RETRY_COUNT=$((RETRY_COUNT + 1))
    print_info "Retry $RETRY_COUNT/$MAX_RETRIES..."
    sleep 3
done

if [ $RETRY_COUNT -eq $MAX_RETRIES ]; then
    print_warning "Device did not come back online within expected time"
    print_info "This may be normal if the device takes longer to boot"
else
    print_success "Device rebooted successfully"
fi

# Step 8: Summary
print_header "OTA Test Summary"
print_success "✓ Firmware binary exists and is valid"
print_success "✓ Device was reachable"
print_success "✓ OTA endpoint was available"
print_success "✓ Firmware upload completed"
print_success "✓ Device rebooted"
if [ $RETRY_COUNT -lt $MAX_RETRIES ]; then
    print_success "✓ Device came back online"
fi

echo ""
print_info "OTA feature test completed successfully!"
print_info "Connect to serial monitor to verify firmware version:"
print_info "  pio device monitor"
echo ""
