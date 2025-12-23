# WSS (WebSocket Secure) Testing Guide

This guide covers testing the WebSocket Secure (WSS) implementation in the OTS firmware.

## Overview

The firmware supports both WS (insecure) and WSS (secure) WebSocket connections:
- **WS**: Plain WebSocket (port 3000) - for local testing only
- **WSS**: Secure WebSocket with TLS (port 3000) - required for HTTPS pages

Configuration is controlled by `WS_USE_TLS` in `include/config.h`.

## Prerequisites

1. **ESP32-S3 Development Board** connected via USB
2. **PlatformIO** installed (`pio` command available)
3. **WiFi credentials** configured in `include/config.h`
4. **Self-signed certificate** (already in `certs/` directory)

## Quick Start

### 1. Build and Upload Test Firmware

```bash
cd /home/drzoid/dev/openfront/ots/ots-fw-main

# Build with test-websocket environment (includes WSS support)
pio run -e test-websocket

# Upload to device
pio run -e test-websocket -t upload

# Monitor serial output
pio device monitor
```

### 2. Verify WSS Server Started

Look for these log messages in serial output:

```
I (3245) OTS_WS_SERVER: Starting HTTPS server (WSS) on port 3000
I (3250) OTS_WS_SERVER: HTTPS server started successfully
I (3255) OTS_WS_SERVER: Certificate: 1234 bytes
I (3260) OTS_WS_SERVER: WSS URL: wss://<device-ip>:3000/ws
W (3265) OTS_WS_SERVER: Note: Browsers will show security warning for self-signed cert
```

### 3. Find Device IP Address

Look for WiFi connection log:

```
I (5432) OTS_NETWORK: WiFi connected! IP: 192.168.1.123
I (5437) OTS_NETWORK: mDNS hostname: ots-device.local
```

Your WSS URL will be: `wss://192.168.1.123:3000/ws` or `wss://ots-device.local:3000/ws`

## Testing Methods

### Method 1: Browser Console (Recommended)

1. Open any HTTPS website (e.g., https://openfront.io)
2. Open browser DevTools (F12)
3. Go to Console tab
4. Run this test script:

```javascript
// WSS connection test
const ws = new WebSocket('wss://192.168.1.123:3000/ws');

ws.onopen = () => {
    console.log('✅ WSS Connected!');
    
    // Send handshake
    ws.send(JSON.stringify({
        type: 'handshake',
        clientType: 'test-client'
    }));
    
    // Send test event
    setTimeout(() => {
        ws.send(JSON.stringify({
            type: 'event',
            payload: {
                type: 'INFO',
                timestamp: Date.now(),
                message: 'Browser test message'
            }
        }));
    }, 1000);
};

ws.onmessage = (event) => {
    console.log('📨 Received:', event.data);
};

ws.onerror = (error) => {
    console.error('❌ WSS Error:', error);
};

ws.onclose = () => {
    console.log('🔌 WSS Disconnected');
};
```

**Expected browser warnings:**
- "The certificate is not trusted" - This is normal for self-signed certificates
- Click "Advanced" → "Accept Risk and Continue"

### Method 2: wscat (Command Line)

Install wscat:
```bash
npm install -g wscat
```

Test WSS connection:
```bash
# Connect to WSS endpoint (may need to accept cert warning)
wscat -c wss://192.168.1.123:3000/ws --no-check

# Once connected, send handshake:
{"type":"handshake","clientType":"wscat"}

# Send test event:
{"type":"event","payload":{"type":"INFO","timestamp":1234567890,"message":"wscat test"}}
```

### Method 3: Python Script

Create `test_wss.py`:

```python
#!/usr/bin/env python3
import asyncio
import websockets
import json
import ssl

async def test_wss():
    # Disable SSL certificate verification (for self-signed cert)
    ssl_context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
    ssl_context.check_hostname = False
    ssl_context.verify_mode = ssl.CERT_NONE
    
    uri = "wss://192.168.1.123:3000/ws"  # Replace with your device IP
    
    async with websockets.connect(uri, ssl=ssl_context) as websocket:
        print("✅ Connected to WSS server")
        
        # Send handshake
        handshake = {
            "type": "handshake",
            "clientType": "python-test"
        }
        await websocket.send(json.dumps(handshake))
        print(f"📤 Sent: {handshake}")
        
        # Send test event
        event = {
            "type": "event",
            "payload": {
                "type": "INFO",
                "timestamp": 1234567890,
                "message": "Python test message"
            }
        }
        await websocket.send(json.dumps(event))
        print(f"📤 Sent: {event}")
        
        # Receive responses
        try:
            while True:
                message = await asyncio.wait_for(websocket.recv(), timeout=5.0)
                print(f"📨 Received: {message}")
        except asyncio.TimeoutError:
            print("⏱️ No more messages")

if __name__ == "__main__":
    asyncio.run(test_wss())
```

Run:
```bash
pip3 install websockets
python3 test_wss.py
```

### Method 4: curl (HTTP Health Check)

Test HTTPS endpoint:
```bash
# Test if HTTPS server is running (will fail handshake but shows server is alive)
curl -k https://192.168.1.123:3000/ws
```

## Serial Monitor Output

Watch for these events in `pio device monitor`:

**Client connects:**
```
I (12345) OTS_WS_SERVER: WebSocket handshake from client
I (12346) OTS_WS_SERVER: Client connected (fd=54), total clients: 1
I (12347) OTS_EVENT_DISPATCHER: Event posted: INTERNAL_EVENT_WS_CONNECTED
```

**Message received:**
```
I (12500) OTS_WS_SERVER: Received packet: {"type":"event","payload":{...}}
I (12501) OTS_WS_SERVER: Message parsed successfully
```

**Client disconnects:**
```
I (15000) OTS_WS_SERVER: Client disconnected (fd=54), total clients: 0
I (15001) OTS_EVENT_DISPATCHER: Event posted: INTERNAL_EVENT_WS_DISCONNECTED
```

## Troubleshooting

### Certificate Warnings in Browser

**Problem**: Browser shows "Your connection is not private"

**Solution**: This is expected for self-signed certificates.
1. Click "Advanced"
2. Click "Accept Risk and Continue" or "Proceed to site"
3. Certificate is valid for 10 years

### Connection Refused

**Problem**: `ECONNREFUSED` or connection timeout

**Check**:
1. Device is on same WiFi network
2. Firewall not blocking port 3000
3. Serial monitor shows "HTTPS server started successfully"
4. Ping device IP: `ping 192.168.1.123`
5. Test mDNS: `ping ots-device.local`

### Certificate Not Trusted

**Problem**: "SSL certificate problem: self signed certificate"

**For wscat**:
```bash
wscat -c wss://192.168.1.123:3000/ws --no-check
```

**For curl**:
```bash
curl -k https://192.168.1.123:3000/ws
```

**For Python**: Use `ssl.CERT_NONE` (see Method 3 above)

### ESP32 Crashes or Reboots

**Check**:
1. Serial output for stack traces
2. Heap memory: Watch for "Failed to allocate memory"
3. Certificate size: Should be ~1200-1400 bytes
4. Max clients: Default is 4 simultaneous connections

**Serial debug commands**:
```bash
# Filter for errors only
pio device monitor --filter esp32_exception_decoder --filter log2file

# Full verbose logging
pio device monitor
```

## Testing Checklist

- [ ] Firmware builds with WSS enabled (`WS_USE_TLS=1`)
- [ ] Serial shows "HTTPS server started successfully"
- [ ] Serial shows certificate size (should be ~1200-1400 bytes)
- [ ] Browser console can connect (with cert warning accepted)
- [ ] Handshake message sent and acknowledged
- [ ] Event messages sent from browser received by firmware
- [ ] Serial shows "Client connected" and "Client disconnected"
- [ ] Multiple clients can connect simultaneously (up to 4)
- [ ] Connection works from HTTPS pages (e.g., openfront.io)
- [ ] mDNS hostname works: `wss://ots-device.local:3000/ws`

## Advanced Testing

### Load Testing (Multiple Clients)

```javascript
// Connect 4 clients simultaneously
const clients = [];
for (let i = 0; i < 4; i++) {
    const ws = new WebSocket('wss://192.168.1.123:3000/ws');
    ws.onopen = () => console.log(`Client ${i} connected`);
    clients.push(ws);
}

// Send messages from all clients
setTimeout(() => {
    clients.forEach((ws, i) => {
        ws.send(JSON.stringify({
            type: 'event',
            payload: {
                type: 'INFO',
                message: `Message from client ${i}`
            }
        }));
    });
}, 2000);
```

### Stress Testing (Rapid Messages)

```javascript
const ws = new WebSocket('wss://192.168.1.123:3000/ws');
ws.onopen = () => {
    // Send 100 messages rapidly
    for (let i = 0; i < 100; i++) {
        ws.send(JSON.stringify({
            type: 'event',
            payload: {
                type: 'INFO',
                message: `Stress test message ${i}`
            }
        }));
    }
};
```

Watch serial for:
- No crashes
- All messages received
- No memory allocation failures

## Regenerating Certificate

If you need to regenerate the SSL certificate:

```bash
cd /home/drzoid/dev/openfront/ots/ots-fw-main/certs
./generate_cert.sh

# Rebuild firmware to embed new certificate
cd ..
pio run -e test-websocket -t upload
```

## Switching Between WS and WSS

Edit `include/config.h`:

```c
// For WSS (secure - works with HTTPS pages)
#define WS_USE_TLS 1

// For WS (insecure - localhost only)
#define WS_USE_TLS 0
```

Then rebuild:
```bash
pio run -e test-websocket -t upload
```

## Integration with OTS Dashboard

Once WSS is working, test with the full OTS system:

1. **Start OTS Server** (in separate terminal):
   ```bash
   cd /home/drzoid/dev/openfront/ots/ots-server
   npm run dev
   ```

2. **Device connects to server**: Set `WS_SERVER_HOST` in firmware config

3. **Dashboard UI**: Open http://localhost:3000

4. **Userscript**: Install from `ots-userscript/build/userscript.ots.user.js`

## Next Steps

After WSS testing is successful:

1. **Hardware Integration**: Test with physical buttons and LEDs
2. **Module Testing**: Verify Nuke, Alert, Troops modules
3. **Protocol Testing**: Send/receive all event types
4. **OTA Testing**: Test firmware updates over WiFi

## References

- **Protocol Specification**: `/prompts/protocol-context.md`
- **Firmware Context**: `copilot-project-context.md`
- **Certificate Generation**: `certs/generate_cert.sh`
- **WebSocket Server**: `src/ws_server.c`

---

**Last Updated**: December 23, 2025
