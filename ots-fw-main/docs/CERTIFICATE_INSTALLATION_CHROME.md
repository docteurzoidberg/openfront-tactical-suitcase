# Installing OTS Certificate in Chrome/Chromium

This guide explains how to permanently trust the OTS device's self-signed certificate in Chrome, Edge, Brave, and other Chromium-based browsers.

## Why This Is Needed

The OTS firmware uses a self-signed SSL/TLS certificate for secure WebSocket connections (WSS). By default, browsers don't trust self-signed certificates and will show security warnings every time you connect. Installing the certificate once in your browser's trust store solves this permanently.

## Prerequisites

- OTS device powered on and connected to your network
- Device IP address or mDNS hostname (e.g., `192.168.77.153` or `ots-fw-main.local`)
- Chrome, Edge, Brave, or any Chromium-based browser

---

## Step 1: Export the Certificate from Your OTS Device

### Method A: Download from Browser (Easiest)

1. **Open your OTS device's web interface** in Chrome:
   ```
   https://192.168.77.153:3000/
   ```
   *(Replace with your device's actual IP address)*

2. **Chrome will show a security warning**: "Your connection is not private"
   - Click **"Advanced"** button
   - Click **"Proceed to 192.168.77.153 (unsafe)"**

3. **You'll see the OTS web interface** (WiFi setup page)

4. **Click the padlock icon** (or "Not secure" text) in the address bar

5. **Click "Connection is not secure"** in the dropdown

6. **Click "Certificate is not valid"** to open the certificate viewer

7. In the Certificate window:
   - **Click the "Details" tab**
   - **Click "Export" button** at the bottom
   - **Choose a location** to save the file (e.g., `~/Downloads/ots-certificate.crt`)
   - **Save as type**: "Base64-encoded ASCII, single certificate (*.crt)"
   - Click **"Save"**

8. **You now have the certificate file** saved on your computer

### Method B: Extract from Firmware Source (Alternative)

If you have access to the firmware source code:

1. Open `ots-fw-main/src/tls_creds.c`
2. Copy the contents of `ots_server_cert_pem[]` (lines starting with `-----BEGIN CERTIFICATE-----`)
3. Paste into a text file named `ots-certificate.crt`
4. Save the file

---

## Step 2: Install Certificate in Chrome

### For Chrome on Windows/Linux

1. **Open Chrome Settings**:
   - Click the three-dot menu (⋮) in the top-right corner
   - Select **"Settings"**

2. **Navigate to Security Settings**:
   - In the left sidebar, click **"Privacy and security"**
   - Click **"Security"**

3. **Open Certificate Manager**:
   - Scroll down to find **"Manage certificates"**
   - Click **"Manage certificates"**

4. **Import the Certificate**:
   - In the Certificate Manager window, click the **"Authorities"** tab
   - Click **"Import"** button
   - Browse to the `.crt` file you saved in Step 1
   - Select the file and click **"Open"**

5. **Set Trust Options**:
   - A dialog will appear: "Specify trust settings for CA certificate"
   - **Check the box**: ☑ "Trust this certificate for identifying websites"
   - Click **"OK"**

6. **Verify Installation**:
   - The certificate should now appear in the "Authorities" list
   - Look for "OTS-FW-MAIN" or "OpenFront" in the organization column

7. **Restart Chrome** for the changes to take effect

### For Chrome on macOS

Chrome on macOS uses the system keychain instead of its own certificate store.

1. **Locate the certificate file** you exported in Step 1 (e.g., `ots-certificate.crt`)

2. **Double-click the `.crt` file** in Finder
   - This will open **Keychain Access** automatically
   - The certificate will be added to your login keychain

3. **Trust the Certificate**:
   - In Keychain Access, find the certificate (search for "ots-fw-main" or "OpenFront")
   - **Double-click the certificate** to open its details
   - Expand the **"Trust"** section
   - For **"When using this certificate"**, select **"Always Trust"**
   - Close the window (you'll be asked for your password)

4. **Restart Chrome** for the changes to take effect

---

## Step 3: Verify Installation

1. **Close all Chrome windows** completely (including background processes)

2. **Reopen Chrome** and navigate to your OTS device:
   ```
   https://192.168.77.153:3000/
   ```

3. **Check the padlock icon**:
   - ✅ **Success**: You should see a **padlock icon** (not "Not secure")
   - ✅ The page should load without any security warnings
   - ✅ Click the padlock → "Connection is secure"

4. **Test the WebSocket connection**:
   - Open the game at `https://openfront.io`
   - Activate the OTS userscript (if installed)
   - The userscript should connect via `wss://192.168.77.153:3000/ws` without errors
   - Check the browser console (F12) for confirmation: "WebSocket connection established"

---

## Troubleshooting

### Certificate Not Appearing in Authorities List

**Cause**: Certificate wasn't imported correctly or Chrome didn't recognize it as a CA certificate.

**Solution**:
1. Delete the certificate from Chrome (if it appears in the wrong tab)
2. Re-export the certificate using Method A above
3. Make sure you're saving as `.crt` format, not `.pem` or `.der`
4. Try importing again in the "Authorities" tab

### Still Getting "Not Secure" Warning After Installation

**Cause**: Chrome is caching the old certificate status.

**Solution**:
1. Clear Chrome's SSL state:
   - **Windows**: Chrome Settings → Privacy and security → Clear browsing data → Advanced → Check "Cached images and files" → Clear data
   - **macOS**: Chrome → Clear Browsing Data → Advanced → Check "Cached images and files" → Clear data
2. Restart Chrome completely (close all windows + background processes)
3. Try accessing the device again

### Certificate Expired or Invalid

**Cause**: The certificate's validity period doesn't match your system clock, or the certificate was regenerated.

**Solution**:
1. Check your system date/time is correct
2. Check the certificate validity:
   - Open the certificate file in a text editor
   - The current certificate expires in 2035 (10 years from generation)
3. If the certificate was regenerated, delete the old certificate and install the new one

### Connection Works in Browser but Userscript Fails

**Cause**: The userscript is connecting to a different URL or port.

**Solution**:
1. Check the userscript's WebSocket URL setting (gear icon in userscript HUD)
2. Make sure it matches: `wss://192.168.77.153:3000/ws` (or your device's IP)
3. Try using the mDNS hostname instead: `wss://ots-fw-main.local:3000/ws`
4. Check browser console (F12) for exact WebSocket connection errors

### "ERR_CERT_AUTHORITY_INVALID" Error

**Cause**: Certificate wasn't installed in the "Authorities" tab, or trust settings weren't enabled.

**Solution**:
1. Open Chrome Certificate Manager
2. Make sure the certificate is in the **"Authorities"** tab (not "Your certificates" or "Servers")
3. Double-check the trust option is enabled: "Trust this certificate for identifying websites"
4. Delete and re-import if needed

---

## Alternative: Using mDNS Hostname

The OTS certificate is issued for the hostname `ots-fw-main.local`. Using this hostname instead of the IP address can help with certificate validation:

**Before**:
```
https://192.168.77.153:3000/
wss://192.168.77.153:3000/ws
```

**After**:
```
https://ots-fw-main.local:3000/
wss://ots-fw-main.local:3000/ws
```

**Note**: mDNS must be enabled on your network for this to work. Most modern routers support it by default.

---

## Security Considerations

### Is This Safe?

**Yes**, for local network use:
- The certificate is only trusted on your computer (not globally)
- It only works for this specific OTS device
- Your browser will still warn you about other self-signed certificates
- You can remove the certificate anytime from Chrome settings

### Should I Share This Certificate?

**No**:
- The certificate is paired with a private key stored on your OTS device
- Sharing the certificate alone doesn't pose a security risk, but it's unnecessary
- Each user should install the certificate from their own device

### What If I Sell or Give Away My OTS Device?

**Recommended**:
1. Remove the certificate from your browser (Chrome → Manage certificates → Authorities → Find certificate → Remove)
2. The new owner will need to install the certificate on their browser using this same guide

---

## Additional Resources

- **OTA Firmware Updates**: See `docs/OTA_GUIDE.md` for updating firmware (which may regenerate the certificate)
- **OTS Documentation**: See `README.md` for general device setup
- **WebSocket Protocol**: See `prompts/protocol-context.md` for technical details

---

## Summary

1. ✅ **Export certificate** from `https://device-ip:3000/` (Method A) or firmware source (Method B)
2. ✅ **Import in Chrome** → Settings → Privacy and security → Security → Manage certificates → Authorities → Import
3. ✅ **Trust for websites** → Check "Trust this certificate for identifying websites"
4. ✅ **Restart Chrome** and verify with padlock icon

**This setup is permanent** and you won't need to accept certificate warnings again unless:
- You clear all browser data (including certificates)
- The device's certificate is regenerated (rare)
- You switch to a different browser (each browser needs the certificate installed separately)

---

*Last Updated: January 2, 2026*
