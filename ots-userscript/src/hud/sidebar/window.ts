export class BaseWindow {
  readonly root: HTMLDivElement
  private dragHandle: HTMLDivElement | null = null

  constructor(root: HTMLDivElement) {
    this.root = root
  }

  attachDrag(handle: HTMLDivElement, onMove?: (left: number, top: number) => void) {
    this.dragHandle = handle
    let dragging = false
    let startX = 0
    let startY = 0
    let startLeft = 0
    let startTop = 0

    handle.addEventListener('mousedown', (ev) => {
      dragging = true
      startX = ev.clientX
      startY = ev.clientY
      const rect = this.root.getBoundingClientRect()
      startLeft = rect.left
      startTop = rect.top
      ev.preventDefault()
    })

    window.addEventListener('mousemove', (ev) => {
      if (!dragging) return
      const dx = ev.clientX - startX
      const dy = ev.clientY - startY
      const nextLeft = startLeft + dx
      const nextTop = startTop + dy
      this.root.style.left = `${nextLeft}px`
      this.root.style.top = `${nextTop}px`
      this.root.style.right = 'auto'
      if (onMove) onMove(nextLeft, nextTop)
    })

    window.addEventListener('mouseup', () => {
      dragging = false
    })
  }
}

export class FloatingPanel {
  readonly panel: HTMLDivElement

  constructor(panel: HTMLDivElement) {
    this.panel = panel
  }

  attachDrag(handle: HTMLDivElement) {
    let dragging = false
    let startX = 0
    let startY = 0
    let startLeft = 0
    let startTop = 0

    handle.addEventListener('mousedown', (ev) => {
      dragging = true
      startX = ev.clientX
      startY = ev.clientY
      const rect = this.panel.getBoundingClientRect()
      startLeft = rect.left
      startTop = rect.top
      ev.preventDefault()
      ev.stopPropagation()
    })

    window.addEventListener('mousemove', (ev) => {
      if (!dragging) return
      const dx = ev.clientX - startX
      const dy = ev.clientY - startY
      this.panel.style.left = `${startLeft + dx}px`
      this.panel.style.top = `${startTop + dy}px`
      this.panel.style.right = 'auto'
    })

    window.addEventListener('mouseup', () => {
      dragging = false
    })
  }
}
