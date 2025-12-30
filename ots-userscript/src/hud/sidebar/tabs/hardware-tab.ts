type HardwareDiagnosticRecord = Record<string, unknown>

export type CapturedHardwareDiagnostic = HardwareDiagnosticRecord & { timestamp: number }

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null
}

export function tryCaptureHardwareDiagnostic(text: string): CapturedHardwareDiagnostic | null {
  try {
    const parsed: unknown = JSON.parse(text)
    if (!isRecord(parsed)) return null

    const payload = isRecord(parsed.payload) ? parsed.payload : null
    const data = (payload && isRecord(payload.data) ? payload.data : null) || (isRecord(parsed.data) ? parsed.data : null)
    if (!data) return null

    const ts =
      (payload && typeof payload.timestamp === 'number' ? payload.timestamp : undefined) ||
      (typeof parsed.timestamp === 'number' ? parsed.timestamp : undefined) ||
      Date.now()

    return { ...(data as HardwareDiagnosticRecord), timestamp: ts }
  } catch {
    return null
  }
}

/**
 * Hardware tab component - displays hardware diagnostic information
 */
export class HardwareTab {
  private container: HTMLDivElement

  constructor(
    private root: HTMLElement,
    private getWsUrl: () => string,
    private setWsUrl: (url: string) => void,
    private onWsUrlChanged: () => void,
    private requestDiagnostic: () => void
  ) {
    this.container = root.querySelector('#ots-hardware-content') as HTMLDivElement
    if (!this.container) {
      throw new Error('Hardware tab container not found')
    }

    this.initializeDisplay()
  }

  /**
   * Create HTML for hardware tab
   */
  static createHTML(): string {
    return `
      <div id="ots-tab-hardware" class="ots-tab-content" style="display:none;flex:1;overflow-y:auto;padding:8px;">
        <div id="ots-hardware-content"></div>
      </div>
    `
  }

  private initializeDisplay(): void {
    this.container.innerHTML = `
      <div style="margin-bottom:16px;padding:10px;background:rgba(59,130,246,0.1);border-left:3px solid #3b82f6;border-radius:4px;">
        <div style="font-size:11px;font-weight:600;color:#93c5fd;margin-bottom:8px;">⚙️ DEVICE CONNECTION</div>
        <label style="display:flex;flex-direction:column;gap:4px;font-size:10px;color:#e5e7eb;">
          <span style="font-size:9px;font-weight:600;color:#9ca3af;letter-spacing:0.05em;">WEBSOCKET URL</span>
          <input id="ots-device-ws-url" type="text" placeholder="ws://ots-fw-main.local:3000/ws" value="${this.getWsUrl()}" style="font-size:10px;padding:6px 8px;border-radius:4px;border:1px solid rgba(148,163,184,0.4);background:rgba(15,23,42,0.7);color:#e5e7eb;outline:none;font-family:monospace;" />
          <span style="font-size:8px;color:#64748b;">Connect to firmware device or ots-server</span>
        </label>
      </div>
      <div style="margin-bottom:12px;">
        <button id="ots-refresh-device-info" style="all:unset;cursor:pointer;font-size:11px;color:#0f172a;padding:6px 12px;border-radius:4px;background:#4ade80;font-weight:600;width:100%;text-align:center;margin-bottom:12px;">🔧 Request Hardware Diagnostic</button>
      </div>
      <div style="text-align:center;color:#64748b;padding:40px 20px;">
        <div style="font-size:32px;margin-bottom:12px;">🔧</div>
        <div style="font-size:12px;margin-bottom:8px;">No diagnostic data available</div>
        <div style="font-size:10px;">Click the button above to request hardware diagnostic</div>
      </div>
    `

    this.attachListeners()
  }

  private attachListeners(): void {
    const wsUrlInput = this.container.querySelector('#ots-device-ws-url') as HTMLInputElement | null
    wsUrlInput?.addEventListener('change', () => {
      const newUrl = wsUrlInput.value.trim()
      if (newUrl) {
        this.setWsUrl(newUrl)
        this.onWsUrlChanged()
      }
    })

    const refreshBtn = this.container.querySelector('#ots-refresh-device-info') as HTMLButtonElement | null
    refreshBtn?.addEventListener('click', () => this.requestDiagnostic())
  }

  updateDisplay(diagnostic: CapturedHardwareDiagnostic | null): void {
    if (!diagnostic) return

    const data = diagnostic
    const hardware = (isRecord(data.hardware) ? data.hardware : {}) as Record<string, any>

    const componentIcons: Record<string, string> = {
      lcd: '📺',
      inputBoard: '🎮',
      outputBoard: '💡',
      adc: '📊',
      soundModule: '🔊'
    }

    const componentLabels: Record<string, string> = {
      lcd: 'LCD Display',
      inputBoard: 'Input Board',
      outputBoard: 'Output Board',
      adc: 'ADC',
      soundModule: 'Sound Module'
    }

    const getStatusIcon = (component: any) => {
      if (!component) return '❓'
      if (component.present && component.working) return '✅'
      if (component.present && !component.working) return '⚠️'
      return '❌'
    }

    const getStatusColor = (component: any) => {
      if (!component) return '#64748b'
      if (component.present && component.working) return '#4ade80'
      if (component.present && !component.working) return '#facc15'
      return '#f87171'
    }

    const getStatusText = (component: any) => {
      if (!component) return 'Unknown'
      if (component.present && component.working) return 'Working'
      if (component.present && !component.working) return 'Present (Not Working)'
      return 'Not Present'
    }

    this.container.innerHTML = `
      <div style="margin-bottom:16px;padding:10px;background:rgba(59,130,246,0.1);border-left:3px solid #3b82f6;border-radius:4px;">
        <div style="font-size:12px;font-weight:600;color:#93c5fd;margin-bottom:6px;">📡 ${(data.deviceType as string) || 'Unknown'}</div>
        <div style="font-size:10px;color:#94a3b8;line-height:1.6;font-family:monospace;">
          <div>Serial: <span style="color:#e5e7eb;">${(data.serialNumber as string) || 'Unknown'}</span></div>
          <div>Owner: <span style="color:#e5e7eb;">${(data.owner as string) || 'Unknown'}</span></div>
          <div>Version: <span style="color:#e5e7eb;">${(data.version as string) || 'Unknown'}</span></div>
        </div>
      </div>
      <div style="font-size:10px;font-weight:600;color:#9ca3af;margin-bottom:8px;letter-spacing:0.05em;">HARDWARE COMPONENTS</div>
      ${Object.keys(hardware)
        .map((key) => {
          const component = hardware[key]
          return `
          <div style="display:flex;align-items:center;gap:10px;margin-bottom:8px;padding:8px;background:rgba(255,255,255,0.03);border-radius:4px;border-left:3px solid ${getStatusColor(component)};">
            <div style="font-size:18px;flex-shrink:0;">${componentIcons[key] || '📦'}</div>
            <div style="flex:1;">
              <div style="font-size:11px;font-weight:600;color:#e5e7eb;margin-bottom:2px;">${componentLabels[key] || key}</div>
              <div style="font-size:9px;color:#94a3b8;">${getStatusText(component)}</div>
            </div>
            <div style="font-size:16px;flex-shrink:0;">${getStatusIcon(component)}</div>
          </div>
        `
        })
        .join('')}
      <div style="margin-top:12px;font-size:9px;color:#64748b;text-align:center;">
        Last updated: ${new Date(typeof data.timestamp === 'number' ? data.timestamp : Date.now()).toLocaleTimeString()}
      </div>
    `
  }
}
