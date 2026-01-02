(function () {
  /** @typedef {{ ssid: string, rssi: number, auth: string }} ScanAp */

  /** @typedef {{ mode: string, ip: string, hasCredentials: boolean, savedSsid: string, serialNumber: string, ownerName: string, ownerConfigured: boolean, firmwareVersion: string }} DeviceInfo */

  const el = {
    statusPill: document.getElementById('statusPill'),
    footerInfo: document.getElementById('footerInfo'),

    onboarding: document.getElementById('onboarding'),
    onboardingMsg: document.getElementById('onboardingMsg'),
    ownerStep: document.getElementById('ownerStep'),
    wifiStep: document.getElementById('wifiStep'),
    ownerName: document.getElementById('ownerName'),
    saveOwnerBtn: document.getElementById('saveOwnerBtn'),

    tabs: document.getElementById('tabs'),
    tabWifi: document.getElementById('tab-wifi'),
    tabDevice: document.getElementById('tab-device'),
    tabAbout: document.getElementById('tab-about'),

    devSerial: document.getElementById('devSerial'),
    devOwner: document.getElementById('devOwner'),
    devFw: document.getElementById('devFw'),
    devMode: document.getElementById('devMode'),
    devIp: document.getElementById('devIp'),

    wifiSetupTemplate: document.getElementById('wifiSetupTemplate'),
    wifiSetupPane: document.getElementById('wifiSetupPane'),
    wifiSetupPane2: document.getElementById('wifiSetupPane2'),

    factoryResetBtn: document.getElementById('factoryResetBtn'),
    factoryResetMsg: document.getElementById('factoryResetMsg'),

    otaFile: document.getElementById('otaFile'),
    otaUploadBtn: document.getElementById('otaUploadBtn'),
    otaMsg: document.getElementById('otaMsg'),
    otaProgress: document.getElementById('otaProgress'),
    otaProgressFill: document.getElementById('otaProgressFill'),
    otaProgressText: document.getElementById('otaProgressText'),
  }

  /** @type {DeviceInfo | null} */
  let latestDevice = null

  function setOnboardingMsg(text) {
    if (el.onboardingMsg) el.onboardingMsg.textContent = text
  }

  function setStatus(text, kind) {
    el.statusPill.textContent = text
    if (kind === 'ok') el.statusPill.style.borderColor = 'rgba(92, 255, 176, 0.55)'
    else if (kind === 'warn') el.statusPill.style.borderColor = 'rgba(255, 207, 92, 0.55)'
    else if (kind === 'bad') el.statusPill.style.borderColor = 'rgba(255, 84, 104, 0.55)'
    else el.statusPill.style.borderColor = ''
  }

  async function httpJson(url) {
    const res = await fetch(url, { cache: 'no-store' })
    if (!res.ok) throw new Error('HTTP ' + res.status)
    return await res.json()
  }

  async function httpFormPost(url, data) {
    const body = new URLSearchParams(data)
    const res = await fetch(url, {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body,
    })
    const text = await res.text()
    if (!res.ok) throw new Error(text || 'HTTP ' + res.status)
    return text
  }

  function setHidden(node, hidden) {
    if (!node) return
    node.hidden = !!hidden
  }

  function activateTab(tabId) {
    // Hide all panels
    if (el.tabWifi) el.tabWifi.hidden = true
    if (el.tabDevice) el.tabDevice.hidden = true
    if (el.tabAbout) el.tabAbout.hidden = true

    // Show selected panel
    if (tabId === 'wifi' && el.tabWifi) el.tabWifi.hidden = false
    else if (tabId === 'device' && el.tabDevice) el.tabDevice.hidden = false
    else if (tabId === 'about' && el.tabAbout) el.tabAbout.hidden = false

    // Update tab button active state
    const buttons = el.tabs ? Array.from(el.tabs.querySelectorAll('.tab')) : []
    for (const b of buttons) {
      const isActive = b.getAttribute('data-tab') === tabId
      b.classList.toggle('active', isActive)
    }
  }

  function fillDeviceInfo(d) {
    if (!d) return
    if (el.devSerial) el.devSerial.textContent = d.serialNumber || '-'
    if (el.devOwner) el.devOwner.textContent = d.ownerName || '-'
    if (el.devFw) el.devFw.textContent = d.firmwareVersion || '-'
    if (el.devMode) el.devMode.textContent = d.mode || '-'
    if (el.devIp) el.devIp.textContent = d.ip || '-'
  }

  function setTopStatus(d) {
    const mode = (d && d.mode) || 'unknown'
    const ip = (d && d.ip) || ''
    const ssid = (d && d.savedSsid) || ''

    if (mode === 'portal') setStatus('PORTAL', 'warn')
    else if (mode === 'normal') setStatus('NORMAL', 'ok')
    else setStatus(String(mode).toUpperCase(), 'warn')

    el.footerInfo.textContent = `mode=${mode} ip=${ip || '-'} savedSsid=${ssid || '-'}`
  }

  function buildWifiSetup(pane, device) {
    if (!pane || !el.wifiSetupTemplate) return null
    pane.innerHTML = ''

    const frag = el.wifiSetupTemplate.content.cloneNode(true)
    pane.appendChild(frag)

    const root = pane
    const ssidSelect = root.querySelector('[data-role="ssidSelect"]')
    const ssidInput = root.querySelector('[data-role="ssid"]')
    const passwordInput = root.querySelector('[data-role="password"]')
    const scanBtn = root.querySelector('[data-role="scanBtn"]')
    const saveBtn = root.querySelector('[data-role="saveBtn"]')
    const clearBtn = root.querySelector('[data-role="clearBtn"]')
    const msgEl = root.querySelector('[data-role="msg"]')
    const scanHint = root.querySelector('[data-role="scanHint"]')

    function setMsg(text) {
      if (msgEl) msgEl.textContent = text
    }

    function fillSelect(aps) {
      if (!ssidSelect) return
      ssidSelect.innerHTML = ''

      const empty = document.createElement('option')
      empty.value = ''
      empty.textContent = 'Select network…'
      ssidSelect.appendChild(empty)

      for (const ap of aps) {
        const opt = document.createElement('option')
        opt.value = ap.ssid
        const rssi = typeof ap.rssi === 'number' ? ap.rssi : 0
        opt.textContent = `${ap.ssid}  (${rssi} dBm, ${ap.auth})`
        ssidSelect.appendChild(opt)
      }
    }

    async function scan() {
      if (!scanBtn || !scanHint) return
      scanBtn.disabled = true
      scanHint.textContent = 'Scanning…'
      setMsg('')

      try {
        const data = await httpJson('/api/scan')
        const aps = Array.isArray(data.aps) ? data.aps : []
        fillSelect(aps)
        scanHint.textContent = aps.length ? `Found ${aps.length} network(s)` : 'No networks found'
      } catch (e) {
        scanHint.textContent = 'Scan failed'
        setMsg(String(e && e.message ? e.message : e))
      } finally {
        scanBtn.disabled = false
      }
    }

    async function save() {
      if (!saveBtn || !ssidInput || !passwordInput) return
      saveBtn.disabled = true
      setMsg('Saving…')

      const ssid = (ssidInput.value || '').trim()
      const password = passwordInput.value || ''

      try {
        if (!ssid) throw new Error('SSID is required')
        const text = await httpFormPost('/wifi', { ssid, password })
        setMsg(text.trim() || 'Saved. Rebooting…')
      } catch (e) {
        setMsg(String(e && e.message ? e.message : e))
        saveBtn.disabled = false
      }
    }

    async function clearCreds() {
      if (!clearBtn) return
      if (!confirm('Clear stored WiFi credentials and reboot?')) return

      clearBtn.disabled = true
      setMsg('Clearing…')

      try {
        const text = await httpFormPost('/wifi/clear', {})
        setMsg(text.trim() || 'Cleared. Rebooting…')
      } catch (e) {
        setMsg(String(e && e.message ? e.message : e))
        clearBtn.disabled = false
      }
    }

    // Wire
    if (ssidSelect && ssidInput) {
      ssidSelect.addEventListener('change', function () {
        const selected = ssidSelect.value
        if (selected) ssidInput.value = selected
      })
    }
    if (scanBtn) scanBtn.addEventListener('click', scan)
    if (saveBtn) saveBtn.addEventListener('click', save)
    if (clearBtn) clearBtn.addEventListener('click', clearCreds)

    // Init
    fillSelect([])
    if (ssidInput && device && device.savedSsid && !ssidInput.value) ssidInput.value = device.savedSsid

    return {
      scan,
    }
  }

  async function refreshDevice() {
    try {
      /** @type {DeviceInfo} */
      const d = await httpJson('/device')
      latestDevice = d
      setTopStatus(d)
      fillDeviceInfo(d)

      // Two distinct modes: onboarding vs normal
      const needsOnboarding = !d.ownerConfigured || !d.hasCredentials || d.mode === 'portal'
      
      if (needsOnboarding) {
        // ONBOARDING MODE: Show wizard, hide tabs
        setHidden(el.onboarding, false)
        setHidden(el.tabs, true)
        setHidden(el.tabWifi, true)
        setHidden(el.tabDevice, true)
        setHidden(el.tabAbout, true)

        // Step 1: Owner name (skip if already configured)
        setHidden(el.ownerStep, !!d.ownerConfigured)
        // Step 2: WiFi setup
        setHidden(el.wifiStep, false)

        buildWifiSetup(el.wifiSetupPane, d)
      } else {
        // NORMAL MODE: Hide wizard, show tabs
        setHidden(el.onboarding, true)
        setHidden(el.tabs, false)

        // Build WiFi setup in tab panel
        buildWifiSetup(el.wifiSetupPane2, d)
        
        // Show wifi tab by default
        activateTab('wifi')
      }
    } catch (e) {
      setStatus('OFFLINE', 'bad')
      el.footerInfo.textContent = 'device unavailable'
    }
  }

  async function saveOwner() {
    if (!el.saveOwnerBtn || !el.ownerName) return
    el.saveOwnerBtn.disabled = true
    setOnboardingMsg('Saving…')

    const ownerName = (el.ownerName.value || '').trim()
    try {
      if (!ownerName) throw new Error('Name is required')
      const text = await httpFormPost('/device', { ownerName })
      setOnboardingMsg(text.trim() || 'Saved.')
      // Refresh so we can proceed to WiFi step.
      await refreshDevice()
    } catch (e) {
      setOnboardingMsg(String(e && e.message ? e.message : e))
      el.saveOwnerBtn.disabled = false
    }
  }

  async function factoryReset() {
    if (!el.factoryResetBtn || !el.factoryResetMsg) return
    if (!confirm('Factory reset will erase WiFi credentials and owner name. Continue?')) return

    el.factoryResetBtn.disabled = true
    el.factoryResetMsg.textContent = 'Resetting...'

    try {
      const text = await httpFormPost('/factory-reset', {})
      el.factoryResetMsg.textContent = '✓ Factory reset complete!'
      
      // Show instructions after successful reset
      setTimeout(() => {
        el.factoryResetMsg.innerHTML = 
          '✓ Factory reset complete!<br><br>' +
          '<strong>Next steps:</strong><br>' +
          '1. Device is rebooting into captive portal mode<br>' +
          '2. Disconnect from current WiFi<br>' +
          '3. Connect to the device\'s WiFi network (OTS-XXXXXX)<br>' +
          '4. The setup page will open automatically'
      }, 1000)
    } catch (e) {
      el.factoryResetMsg.textContent = String(e && e.message ? e.message : e)
      el.factoryResetBtn.disabled = false
    }
  }

  function setOtaMsg(text) {
    if (el.otaMsg) el.otaMsg.textContent = text
  }

  function setOtaProgress(percent) {
    if (el.otaProgressFill) el.otaProgressFill.style.width = percent + '%'
    if (el.otaProgressText) el.otaProgressText.textContent = Math.round(percent) + '%'
  }

  async function uploadOta() {
    if (!el.otaUploadBtn || !el.otaFile) return

    const file = el.otaFile.files[0]
    if (!file) {
      setOtaMsg('Please select a firmware file')
      return
    }

    if (!file.name.endsWith('.bin')) {
      setOtaMsg('Invalid file type. Please select a .bin file')
      return
    }

    el.otaUploadBtn.disabled = true
    setOtaMsg('Uploading firmware...')
    setHidden(el.otaProgress, false)
    setOtaProgress(0)

    try {
      await new Promise((resolve, reject) => {
        const xhr = new XMLHttpRequest()

        xhr.upload.addEventListener('progress', (e) => {
          if (e.lengthComputable) {
            const percent = (e.loaded / e.total) * 100
            setOtaProgress(percent)
          }
        })

        xhr.addEventListener('load', () => {
          if (xhr.status === 200) {
            resolve(xhr.responseText)
          } else {
            reject(new Error('Upload failed: HTTP ' + xhr.status))
          }
        })

        xhr.addEventListener('error', () => {
          reject(new Error('Network error during upload'))
        })

        xhr.addEventListener('abort', () => {
          reject(new Error('Upload aborted'))
        })

        xhr.open('POST', '/ota/upload', true)
        xhr.send(file)
      })

      setOtaProgress(100)
      setOtaMsg('✓ Upload complete! Device is rebooting...')
      
      // Reset UI after delay
      setTimeout(() => {
        setHidden(el.otaProgress, true)
        setOtaProgress(0)
        if (el.otaFile) el.otaFile.value = ''
        el.otaUploadBtn.disabled = true
        setOtaMsg('Device will be back online shortly. Refresh page after ~30 seconds.')
      }, 2000)
    } catch (e) {
      setOtaMsg('✗ ' + String(e && e.message ? e.message : e))
      setHidden(el.otaProgress, true)
      setOtaProgress(0)
      el.otaUploadBtn.disabled = false
    }
  }

  // Wire UI
  if (el.saveOwnerBtn) el.saveOwnerBtn.addEventListener('click', saveOwner)
  if (el.factoryResetBtn) el.factoryResetBtn.addEventListener('click', factoryReset)
  if (el.otaFile) {
    el.otaFile.addEventListener('change', function() {
      if (el.otaUploadBtn) {
        el.otaUploadBtn.disabled = !el.otaFile.files.length
      }
    })
  }
  if (el.otaUploadBtn) el.otaUploadBtn.addEventListener('click', uploadOta)
  if (el.tabs) {
    el.tabs.addEventListener('click', function (ev) {
      const target = ev.target
      if (!target || !target.getAttribute) return
      const tabId = target.getAttribute('data-tab')
      if (tabId) activateTab(tabId)
    })
  }

  // Display build hash if available
  if (window.OTS_BUILD_HASH && window.OTS_BUILD_HASH !== '__OTS_BUILD_HASH__') {
    const buildHashEl = document.getElementById('buildHash')
    if (buildHashEl) {
      buildHashEl.textContent = `Build: ${window.OTS_BUILD_HASH}`
    }
  }

  // Boot
  refreshDevice()
})()
