// ==UserScript==
// @name         OTS UI Mockup - Sidebar Snap Test
// @namespace    http://tampermonkey.net/
// @version      0.1.0
// @description  Test new sidebar UI with border snapping
// @author       OTS Team
// @match        https://openfront.io/*
// @grant        none
// ==/UserScript==

/**
 * Snap positions for the sidebar
 */
type SnapPosition = 'top' | 'bottom' | 'left' | 'right'

/**
 * Sidebar UI with border snapping and rotation
 */
class SidebarMockup {
  private container: HTMLDivElement
  private titlebar: HTMLDivElement
  private content: HTMLDivElement
  private collapsed = true
  private currentPosition: SnapPosition = 'right'

  private isDragging = false
  private dragStartX = 0
  private dragStartY = 0

  constructor() {
    this.container = this.createContainer()
    this.titlebar = this.createTitlebar()
    this.content = this.createContent()

    this.container.appendChild(this.titlebar)
    this.container.appendChild(this.content)
    document.body.appendChild(this.container)

    this.applyPosition(this.currentPosition)
    this.updateContentVisibility()
  }

  private createContainer(): HTMLDivElement {
    const div = document.createElement('div')
    div.style.cssText = `
      position: fixed;
      display: flex;
      flex-direction: column;
      background: rgba(20, 20, 30, 0.95);
      border: 2px solid #4a9eff;
      border-radius: 8px;
      box-shadow: 0 4px 20px rgba(0, 0, 0, 0.5);
      z-index: 999999;
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
      transition: all 0.3s cubic-bezier(0.4, 0, 0.2, 1);
    `
    return div
  }

  private createTitlebar(): HTMLDivElement {
    const titlebar = document.createElement('div')
    titlebar.style.cssText = `
      background: linear-gradient(135deg, #667eea 0%, #4a9eff 100%);
      color: white;
      padding: 8px 12px;
      cursor: move;
      user-select: none;
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 8px;
      font-weight: 600;
      font-size: 12px;
      border-radius: 6px 6px 0 0;
    `

    const title = document.createElement('span')
    title.textContent = 'OTS Dashboard'
    title.style.cssText = `
      white-space: nowrap;
      overflow: hidden;
      text-overflow: ellipsis;
    `

    const buttonContainer = document.createElement('div')
    buttonContainer.style.cssText = `
      display: flex;
      gap: 4px;
      flex-shrink: 0;
    `

    const toggleBtn = this.createButton(this.collapsed ? '▼' : '▲', () => {
      this.collapsed = !this.collapsed
      toggleBtn.textContent = this.collapsed ? '▼' : '▲'
      this.updateContentVisibility()
    })

    const closeBtn = this.createButton('×', () => {
      this.container.style.display = 'none'
    })

    buttonContainer.appendChild(toggleBtn)
    buttonContainer.appendChild(closeBtn)

    titlebar.appendChild(title)
    titlebar.appendChild(buttonContainer)

    // Drag handlers
    titlebar.addEventListener('mousedown', (e) => {
      if ((e.target as HTMLElement).tagName === 'BUTTON') return
      this.isDragging = true
      this.dragStartX = e.clientX
      this.dragStartY = e.clientY
      titlebar.style.cursor = 'grabbing'
      e.preventDefault()
    })

    document.addEventListener('mousemove', (e) => {
      if (!this.isDragging) return

      const x = e.clientX
      const y = e.clientY

      // Show preview of snap position
      const snapPos = this.calculateSnapPosition(x, y)
      this.showSnapPreview(snapPos)
    })

    document.addEventListener('mouseup', (e) => {
      if (!this.isDragging) return

      this.isDragging = false
      titlebar.style.cursor = 'move'

      const x = e.clientX
      const y = e.clientY

      const newPosition = this.calculateSnapPosition(x, y)
      this.currentPosition = newPosition
      this.applyPosition(newPosition)
      this.removeSnapPreview()
    })

    return titlebar
  }

  private createButton(text: string, onClick: () => void): HTMLButtonElement {
    const btn = document.createElement('button')
    btn.textContent = text
    btn.style.cssText = `
      background: rgba(255, 255, 255, 0.2);
      border: none;
      color: white;
      width: 20px;
      height: 20px;
      border-radius: 3px;
      cursor: pointer;
      font-size: 14px;
      display: flex;
      align-items: center;
      justify-content: center;
      padding: 0;
      transition: background 0.2s;
    `
    btn.addEventListener('mouseenter', () => {
      btn.style.background = 'rgba(255, 255, 255, 0.3)'
    })
    btn.addEventListener('mouseleave', () => {
      btn.style.background = 'rgba(255, 255, 255, 0.2)'
    })
    btn.addEventListener('click', onClick)
    return btn
  }

  private createContent(): HTMLDivElement {
    const content = document.createElement('div')
    content.style.cssText = `
      padding: 12px;
      color: #e0e0e0;
      font-size: 12px;
      overflow: auto;
    `

    content.innerHTML = `
      <div style="margin-bottom: 8px;">
        <strong style="color: #4a9eff;">Status:</strong>
        <span style="color: #10b981;">● Connected</span>
      </div>
      <div style="margin-bottom: 8px;">
        <strong style="color: #4a9eff;">Position:</strong>
        <span id="ots-current-position">Right</span>
      </div>
      <div style="margin-bottom: 8px;">
        <strong style="color: #4a9eff;">Events:</strong>
        <div style="font-size: 10px; color: #888; margin-top: 4px;">
          <div>GAME_START - 12:34:56</div>
          <div>NUKE_LAUNCHED - 12:35:10</div>
          <div>ALERT_ATOM - 12:35:15</div>
        </div>
      </div>
      <div style="margin-top: 12px; padding: 8px; background: rgba(255,255,255,0.05); border-radius: 4px; font-size: 10px; color: #888;">
        💡 Drag the titlebar to snap to screen edges<br>
        • Left/Right: Vertical sidebar<br>
        • Top/Bottom: Horizontal bar
      </div>
    `

    return content
  }

  private calculateSnapPosition(x: number, y: number): SnapPosition {
    const w = window.innerWidth
    const h = window.innerHeight

    const distLeft = x
    const distRight = w - x
    const distTop = y
    const distBottom = h - y

    const minDist = Math.min(distLeft, distRight, distTop, distBottom)

    if (minDist === distLeft) return 'left'
    if (minDist === distRight) return 'right'
    if (minDist === distTop) return 'top'
    return 'bottom'
  }

  private applyPosition(position: SnapPosition) {
    this.currentPosition = position

    // Update position indicator in content
    const posSpan = document.getElementById('ots-current-position')
    if (posSpan) {
      posSpan.textContent = position.charAt(0).toUpperCase() + position.slice(1)
    }

    // Reset all positioning
    this.container.style.top = ''
    this.container.style.bottom = ''
    this.container.style.left = ''
    this.container.style.right = ''
    this.container.style.transform = ''
    this.titlebar.style.writingMode = ''
    this.titlebar.style.transform = ''
    this.titlebar.style.borderRadius = ''
    this.titlebar.style.width = ''
    this.titlebar.style.height = ''
    this.titlebar.style.flexDirection = ''

    const contentHeight = this.collapsed ? 0 : 200

    switch (position) {
      case 'top':
        this.container.style.top = '0'
        this.container.style.left = '50%'
        this.container.style.transform = 'translateX(-50%)'
        this.container.style.width = '400px'
        this.container.style.height = `${40 + contentHeight}px`
        this.container.style.borderRadius = '0 0 8px 8px'
        this.titlebar.style.borderRadius = '0 0 6px 6px'
        break

      case 'bottom':
        this.container.style.bottom = '0'
        this.container.style.left = '50%'
        this.container.style.transform = 'translateX(-50%)'
        this.container.style.width = '400px'
        this.container.style.height = `${40 + contentHeight}px`
        this.container.style.borderRadius = '8px 8px 0 0'
        this.titlebar.style.borderRadius = '6px 6px 0 0'
        break

      case 'left':
        this.container.style.left = '0'
        this.container.style.top = '50%'
        this.container.style.transform = 'translateY(-50%)'
        this.container.style.width = `${40 + (this.collapsed ? 0 : 300)}px`
        this.container.style.height = '400px'
        this.container.style.borderRadius = '0 8px 8px 0'

        // Rotate titlebar for vertical orientation
        this.titlebar.style.writingMode = 'vertical-rl'
        this.titlebar.style.transform = 'rotate(180deg)'
        this.titlebar.style.borderRadius = '0 6px 6px 0'
        this.titlebar.style.width = '40px'
        this.titlebar.style.height = '100%'
        break

      case 'right':
        this.container.style.right = '0'
        this.container.style.top = '50%'
        this.container.style.transform = 'translateY(-50%)'
        this.container.style.width = `${40 + (this.collapsed ? 0 : 300)}px`
        this.container.style.height = '400px'
        this.container.style.borderRadius = '8px 0 0 8px'

        // Rotate titlebar for vertical orientation
        this.titlebar.style.writingMode = 'vertical-rl'
        this.titlebar.style.transform = 'rotate(180deg)'
        this.titlebar.style.borderRadius = '6px 0 0 6px'
        this.titlebar.style.width = '40px'
        this.titlebar.style.height = '100%'
        break
    }

    // Adjust layout based on orientation
    if (position === 'left' || position === 'right') {
      this.container.style.flexDirection = 'row'
      this.content.style.flex = '1'
      this.content.style.minWidth = '0'
    } else {
      this.container.style.flexDirection = 'column'
      this.content.style.flex = ''
      this.content.style.minWidth = ''
    }
  }

  private showSnapPreview(position: SnapPosition) {
    // Visual preview of where it will snap
    let preview = document.getElementById('ots-snap-preview')
    if (!preview) {
      preview = document.createElement('div')
      preview.id = 'ots-snap-preview'
      preview.style.cssText = `
        position: fixed;
        background: rgba(74, 158, 255, 0.3);
        border: 2px dashed #4a9eff;
        z-index: 999998;
        pointer-events: none;
        transition: all 0.2s;
      `
      document.body.appendChild(preview)
    }

    const previewSize = 100

    switch (position) {
      case 'top':
        preview.style.top = '0'
        preview.style.left = '50%'
        preview.style.transform = 'translateX(-50%)'
        preview.style.width = '300px'
        preview.style.height = `${previewSize}px`
        break
      case 'bottom':
        preview.style.bottom = '0'
        preview.style.top = ''
        preview.style.left = '50%'
        preview.style.transform = 'translateX(-50%)'
        preview.style.width = '300px'
        preview.style.height = `${previewSize}px`
        break
      case 'left':
        preview.style.left = '0'
        preview.style.top = '50%'
        preview.style.transform = 'translateY(-50%)'
        preview.style.width = `${previewSize}px`
        preview.style.height = '300px'
        break
      case 'right':
        preview.style.right = '0'
        preview.style.left = ''
        preview.style.top = '50%'
        preview.style.transform = 'translateY(-50%)'
        preview.style.width = `${previewSize}px`
        preview.style.height = '300px'
        break
    }
  }

  private removeSnapPreview() {
    const preview = document.getElementById('ots-snap-preview')
    if (preview) {
      preview.remove()
    }
  }

  private updateContentVisibility() {
    if (this.collapsed) {
      this.content.style.display = 'none'
    } else {
      this.content.style.display = 'block'
    }

    // Re-apply position to update dimensions
    this.applyPosition(this.currentPosition)
  }
}

// Initialize when page is ready
; (function () {
  console.log('[OTS Mockup] Initializing sidebar test...')

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => {
      new SidebarMockup()
    })
  } else {
    new SidebarMockup()
  }
})()
