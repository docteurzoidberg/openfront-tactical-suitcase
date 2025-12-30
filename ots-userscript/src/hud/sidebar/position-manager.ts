import type { SnapPosition, HudSize } from './types'

/**
 * Manages window positioning, snapping, dragging, and resizing for the sidebar HUD
 */
export class PositionManager {
  private currentPosition: SnapPosition
  private userSize: HudSize

  // Drag state
  private isDragging = false
  private dragStartX = 0
  private dragStartY = 0

  // Resize state
  private isResizing = false
  private resizeStartX = 0
  private resizeStartY = 0
  private resizeStartWidth = 0
  private resizeStartHeight = 0

  // DOM references
  private root: HTMLDivElement
  private titlebar: HTMLDivElement
  private content: HTMLDivElement | null = null
  private body: HTMLDivElement | null = null

  // Callbacks
  private onPositionChange?: (position: SnapPosition) => void
  private onSizeChange?: (size: HudSize) => void

  constructor(
    root: HTMLDivElement,
    titlebar: HTMLDivElement,
    initialPosition: SnapPosition,
    initialSize: HudSize,
    options?: {
      onPositionChange?: (position: SnapPosition) => void
      onSizeChange?: (size: HudSize) => void
    }
  ) {
    this.root = root
    this.titlebar = titlebar
    this.currentPosition = initialPosition
    this.userSize = initialSize
    this.onPositionChange = options?.onPositionChange
    this.onSizeChange = options?.onSizeChange
  }

  /**
   * Set DOM references (content and body containers)
   */
  setContainers(content: HTMLDivElement | null, body: HTMLDivElement | null) {
    this.content = content
    this.body = body
  }

  /**
   * Initialize position and attach event listeners
   */
  initialize(collapsed: boolean) {
    this.applyPosition(this.currentPosition, collapsed)
    this.attachEventListeners()
  }

  /**
   * Get current snap position
   */
  getPosition(): SnapPosition {
    return this.currentPosition
  }

  /**
   * Get current user size
   */
  getSize(): HudSize {
    return { ...this.userSize }
  }

  /**
   * Calculate snap position based on mouse coordinates
   */
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

  /**
   * Apply position styling to the window
   */
  applyPosition(position: SnapPosition, collapsed: boolean) {
    // Reset all positioning
    this.root.style.top = ''
    this.root.style.bottom = ''
    this.root.style.left = ''
    this.root.style.right = ''
    this.root.style.transform = ''
    this.root.style.borderTop = ''
    this.root.style.borderBottom = ''
    this.root.style.borderLeft = ''
    this.root.style.borderRight = ''
    this.titlebar.style.writingMode = ''
    this.titlebar.style.transform = ''
    this.titlebar.style.borderRadius = ''
    this.titlebar.style.width = ''
    this.titlebar.style.height = ''
    this.titlebar.style.flexDirection = ''

    // Get button container for counter-rotation
    const buttonContainer = this.root.querySelector('#ots-hud-buttons') as HTMLElement
    const resizeHandle = this.root.querySelector('#ots-hud-resize-handle') as HTMLElement

    const minSize = 32 // Minimal titlebar size
    const contentWidth = collapsed ? 0 : Math.max(200, this.userSize.width)
    const contentHeight = collapsed ? 0 : Math.max(150, this.userSize.height)

    switch (position) {
      case 'top':
        this.root.style.top = '-2px'
        this.root.style.left = '50%'
        this.root.style.transform = 'translateX(-50%)'
        this.root.style.width = collapsed ? '325px' : `${Math.max(325, contentWidth)}px`
        this.root.style.height = `${minSize + contentHeight + 2}px`
        this.root.style.borderRadius = '0 0 8px 8px'
        this.root.style.borderTop = 'none'
        this.titlebar.style.borderRadius = '0 0 8px 8px'
        // Reset button rotation for horizontal
        if (buttonContainer) buttonContainer.style.transform = ''
        // Position resize handle
        if (resizeHandle) {
          resizeHandle.style.display = collapsed ? 'none' : 'block'
          resizeHandle.style.right = '0'
          resizeHandle.style.bottom = '0'
          resizeHandle.style.top = ''
          resizeHandle.style.left = ''
          resizeHandle.style.cursor = 'nwse-resize'
        }
        break

      case 'bottom':
        this.root.style.bottom = '-2px'
        this.root.style.left = '50%'
        this.root.style.transform = 'translateX(-50%)'
        this.root.style.width = collapsed ? '325px' : `${Math.max(325, contentWidth)}px`
        this.root.style.height = `${minSize + contentHeight + 2}px`
        this.root.style.borderRadius = '8px 8px 0 0'
        this.root.style.borderBottom = 'none'
        this.titlebar.style.borderRadius = '8px 8px 0 0'
        // Reset button rotation for horizontal
        if (buttonContainer) buttonContainer.style.transform = ''
        // Position resize handle
        if (resizeHandle) {
          resizeHandle.style.display = collapsed ? 'none' : 'block'
          resizeHandle.style.right = '0'
          resizeHandle.style.bottom = '0'
          resizeHandle.style.top = ''
          resizeHandle.style.left = ''
          resizeHandle.style.cursor = 'nwse-resize'
        }
        break

      case 'left':
        this.root.style.left = '-2px'
        this.root.style.top = '50%'
        this.root.style.transform = 'translateY(-50%)'
        this.root.style.width = `${minSize + contentWidth + 2}px`
        this.root.style.height = collapsed ? '250px' : `${Math.max(250, contentHeight)}px`
        this.root.style.borderRadius = '0 8px 8px 0'
        this.root.style.borderLeft = 'none'

        // Rotate titlebar for vertical orientation
        this.titlebar.style.writingMode = 'vertical-rl'
        this.titlebar.style.transform = 'rotate(180deg)'
        this.titlebar.style.borderRadius = '0 8px 8px 0'
        this.titlebar.style.width = '32px'
        this.titlebar.style.height = '100%'
        // Counter-rotate buttons to keep them horizontal
        if (buttonContainer) buttonContainer.style.transform = 'rotate(-180deg)'
        // Position resize handle
        if (resizeHandle) {
          resizeHandle.style.display = collapsed ? 'none' : 'block'
          resizeHandle.style.right = '0'
          resizeHandle.style.bottom = '0'
          resizeHandle.style.top = ''
          resizeHandle.style.left = ''
          resizeHandle.style.cursor = 'nwse-resize'
        }
        break

      case 'right':
        this.root.style.right = '-2px'
        this.root.style.top = '50%'
        this.root.style.transform = 'translateY(-50%)'
        this.root.style.width = `${minSize + contentWidth + 2}px`
        this.root.style.height = collapsed ? '250px' : `${Math.max(250, contentHeight)}px`
        this.root.style.borderRadius = '8px 0 0 8px'
        this.root.style.borderRight = 'none'

        // Rotate titlebar for vertical orientation
        this.titlebar.style.writingMode = 'vertical-rl'
        this.titlebar.style.transform = 'rotate(180deg)'
        this.titlebar.style.borderRadius = '8px 0 0 8px'
        this.titlebar.style.width = '32px'
        this.titlebar.style.height = '100%'
        // Counter-rotate buttons to keep them horizontal
        if (buttonContainer) buttonContainer.style.transform = 'rotate(-180deg)'
        // Position resize handle
        if (resizeHandle) {
          resizeHandle.style.display = collapsed ? 'none' : 'block'
          resizeHandle.style.right = '0'
          resizeHandle.style.bottom = '0'
          resizeHandle.style.top = ''
          resizeHandle.style.left = ''
          resizeHandle.style.cursor = 'nwse-resize'
        }
        break
    }

    // Adjust layout based on orientation
    if (position === 'left' || position === 'right') {
      this.root.style.flexDirection = 'row'
      if (this.content) {
        this.content.style.flex = '1'
        this.content.style.minWidth = '0'
      }
      if (this.body) {
        this.body.style.height = '100%'
      }
    } else {
      this.root.style.flexDirection = 'column'
      if (this.content) {
        this.content.style.flex = '1'
        this.content.style.minHeight = '0'
      }
      if (this.body) {
        this.body.style.height = ''
        this.body.style.flex = '1'
      }
    }

    this.currentPosition = position
  }

  /**
   * Show snap preview overlay
   */
  private showSnapPreview(position: SnapPosition) {
    let preview = document.getElementById('ots-snap-preview')
    if (!preview) {
      preview = document.createElement('div')
      preview.id = 'ots-snap-preview'
      preview.style.cssText = `
        position: fixed;
        background: rgba(59, 130, 246, 0.25);
        border: 2px dashed rgba(59, 130, 246, 0.6);
        z-index: 2147483646;
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
        preview.style.bottom = ''
        preview.style.right = ''
        preview.style.transform = 'translateX(-50%)'
        preview.style.width = '300px'
        preview.style.height = `${previewSize}px`
        break
      case 'bottom':
        preview.style.bottom = '0'
        preview.style.left = '50%'
        preview.style.top = ''
        preview.style.right = ''
        preview.style.transform = 'translateX(-50%)'
        preview.style.width = '300px'
        preview.style.height = `${previewSize}px`
        break
      case 'left':
        preview.style.left = '0'
        preview.style.top = '50%'
        preview.style.bottom = ''
        preview.style.right = ''
        preview.style.transform = 'translateY(-50%)'
        preview.style.width = `${previewSize}px`
        preview.style.height = '300px'
        break
      case 'right':
        preview.style.right = '0'
        preview.style.top = '50%'
        preview.style.left = ''
        preview.style.bottom = ''
        preview.style.transform = 'translateY(-50%)'
        preview.style.width = `${previewSize}px`
        preview.style.height = '300px'
        break
    }
  }

  /**
   * Remove snap preview overlay
   */
  private removeSnapPreview() {
    const preview = document.getElementById('ots-snap-preview')
    if (preview) {
      preview.remove()
    }
  }

  /**
   * Update arrow indicators based on position and collapsed state
   */
  updateArrows(collapsed: boolean) {
    const arrowLeft = this.root.querySelector('#ots-hud-arrow-left') as HTMLElement
    const arrowRight = this.root.querySelector('#ots-hud-arrow-right') as HTMLElement
    if (!arrowLeft || !arrowRight) return

    const position = this.currentPosition

    if (position === 'left') {
      // Left edge: outward = left, inward = right
      arrowLeft.textContent = collapsed ? '◀' : '▶'
      arrowRight.textContent = collapsed ? '◀' : '▶'
    } else if (position === 'right') {
      // Right edge: outward = right, inward = left
      arrowLeft.textContent = collapsed ? '▶' : '◀'
      arrowRight.textContent = collapsed ? '▶' : '◀'
    } else if (position === 'top') {
      // Top edge: outward = down, inward = up
      arrowLeft.textContent = collapsed ? '▼' : '▲'
      arrowRight.textContent = collapsed ? '▼' : '▲'
    } else if (position === 'bottom') {
      // Bottom edge: outward = up, inward = down
      arrowLeft.textContent = collapsed ? '▲' : '▼'
      arrowRight.textContent = collapsed ? '▲' : '▼'
    }
  }

  /**
   * Attach drag and resize event listeners
   */
  private attachEventListeners() {
    // Titlebar drag
    this.titlebar.addEventListener('mousedown', (e) => {
      // Don't drag if clicking on buttons
      if ((e.target as HTMLElement).tagName === 'BUTTON' || (e.target as HTMLElement).closest('button')) return
      // Mark as potential drag (will be confirmed if mouse moves > threshold)
      this.isDragging = false // Start as false, will be set to true if mouse moves enough
      this.dragStartX = e.clientX
      this.dragStartY = e.clientY
      e.preventDefault()
    })

    // Resize handle
    const resizeHandle = this.root.querySelector('#ots-hud-resize-handle') as HTMLElement
    if (resizeHandle) {
      resizeHandle.addEventListener('mouseenter', () => {
        resizeHandle.style.opacity = '1'
      })
      resizeHandle.addEventListener('mouseleave', () => {
        if (!this.isResizing) resizeHandle.style.opacity = '0.5'
      })
      resizeHandle.addEventListener('mousedown', (e: MouseEvent) => {
        e.stopPropagation()
        e.preventDefault()
        this.isResizing = true
        this.resizeStartX = e.clientX
        this.resizeStartY = e.clientY
        this.resizeStartWidth = this.userSize.width
        this.resizeStartHeight = this.userSize.height
        resizeHandle.style.opacity = '1'
        // Disable transitions and optimize for resize
        this.root.style.transition = 'none'
        this.root.style.willChange = 'width, height'
      })
    }

    // Global mouse move
    document.addEventListener('mousemove', (e) => {
      this.handleMouseMove(e)
    })

    // Global mouse up
    document.addEventListener('mouseup', (e) => {
      this.handleMouseUp(e)
    })
  }

  /**
   * Handle mouse move for drag and resize
   */
  private handleMouseMove(e: MouseEvent) {
    // Handle resize
    if (this.isResizing) {
      e.preventDefault()
      e.stopPropagation()

      const minSize = 200 // Minimum content size
      const minTitlebar = 32
      const pos = this.currentPosition

      if (pos === 'left' || pos === 'right') {
        // Horizontal resize
        const deltaX = pos === 'left' ? (e.clientX - this.resizeStartX) : (this.resizeStartX - e.clientX)
        const newWidth = Math.max(minSize, this.resizeStartWidth + deltaX)

        // Vertical resize
        const deltaY = e.clientY - this.resizeStartY
        const newHeight = Math.max(minSize, this.resizeStartHeight + deltaY)

        // Update stored size
        this.userSize.width = newWidth
        this.userSize.height = newHeight

        // Direct dimension update for smoother resize
        this.root.style.width = `${minTitlebar + newWidth + 2}px`
        this.root.style.height = `${newHeight}px`
      } else {
        // Horizontal resize for top/bottom (centered, so double the delta)
        const deltaX = (e.clientX - this.resizeStartX) * 2
        const newWidth = Math.max(minSize, this.resizeStartWidth + deltaX)

        // Vertical resize
        const deltaY = pos === 'top' ? (e.clientY - this.resizeStartY) : (this.resizeStartY - e.clientY)
        const newHeight = Math.max(minSize, this.resizeStartHeight + deltaY)

        // Update stored size
        this.userSize.width = newWidth
        this.userSize.height = newHeight

        // Direct dimension update for smoother resize
        this.root.style.width = `${newWidth}px`
        this.root.style.height = `${minTitlebar + newHeight + 2}px`
      }
      return
    }

    // Handle drag (check for threshold first)
    if (this.dragStartX !== 0 || this.dragStartY !== 0) {
      const dx = Math.abs(e.clientX - this.dragStartX)
      const dy = Math.abs(e.clientY - this.dragStartY)
      const dragThreshold = 5 // pixels

      // Only activate drag if mouse moved beyond threshold
      if (dx > dragThreshold || dy > dragThreshold) {
        if (!this.isDragging) {
          this.isDragging = true
          this.titlebar.style.cursor = 'grabbing'
        }
        const snapPos = this.calculateSnapPosition(e.clientX, e.clientY)
        this.showSnapPreview(snapPos)
      }
    }
  }

  /**
   * Handle mouse up for drag and resize
   */
  private handleMouseUp(e: MouseEvent) {
    // Handle resize end
    if (this.isResizing) {
      this.isResizing = false
      const resizeHandle = this.root.querySelector('#ots-hud-resize-handle') as HTMLElement
      if (resizeHandle) resizeHandle.style.opacity = '0.5'
      // Re-enable transitions and remove optimization hint
      this.root.style.transition = 'width 0.3s cubic-bezier(0.4, 0, 0.2, 1), height 0.3s cubic-bezier(0.4, 0, 0.2, 1)'
      this.root.style.willChange = 'auto'

      // Notify size change
      if (this.onSizeChange) {
        this.onSizeChange(this.userSize)
      }
      return
    }

    // Handle drag end
    if (this.isDragging) {
      this.isDragging = false
      this.dragStartX = 0
      this.dragStartY = 0
      this.titlebar.style.cursor = 'move'
      const newPosition = this.calculateSnapPosition(e.clientX, e.clientY)
      this.currentPosition = newPosition

      // Notify position change
      if (this.onPositionChange) {
        this.onPositionChange(newPosition)
      }

      this.removeSnapPreview()
      return
    }

    // Reset drag tracking if it was just a click
    if (this.dragStartX !== 0 || this.dragStartY !== 0) {
      this.dragStartX = 0
      this.dragStartY = 0
    }
  }
}
