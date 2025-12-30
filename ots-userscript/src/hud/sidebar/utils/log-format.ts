import type { JsonLike } from './types'

// Counter for unique IDs
let formatIdCounter = 0

// Track IDs that need event listeners attached
const pendingToggleIds: string[] = []

export function escapeHtml(text: string): string {
  const div = document.createElement('div')
  div.textContent = text
  return div.innerHTML
}

export function formatJson(obj: JsonLike, indent = 0): string {
  const indentStr = '  '.repeat(indent)
  const nextIndent = '  '.repeat(indent + 1)

  if (obj === null) return '<span style="color:#f87171;">null</span>'
  if (obj === undefined) return '<span style="color:#f87171;">undefined</span>'
  if (typeof obj === 'boolean') return `<span style="color:#a78bfa;">${obj}</span>`
  if (typeof obj === 'number') return `<span style="color:#fbbf24;">${obj}</span>`
  if (typeof obj === 'string') return `<span style="color:#86efac;">"${escapeHtml(obj)}"</span>`

  if (Array.isArray(obj)) {
    if (obj.length === 0) return '<span style="color:#cbd5e1;">[]</span>'

    const id = `json-arr-${formatIdCounter++}`
    pendingToggleIds.push(id)
    const items = obj.map(item => `${nextIndent}${formatJson(item, indent + 1)}`).join(',\n')

    // Create collapsible array
    const result = `<span style="color:#cbd5e1;">[</span><span id="${id}-toggle" style="cursor:pointer;color:#9ca3af;margin:0 4px;" data-json-toggle="${id}">▼</span><span id="${id}-summary" style="display:none;color:#9ca3af;">${obj.length} items</span>
<span id="${id}-content">${items}
${indentStr}</span><span style="color:#cbd5e1;">]</span>`

    return result
  }

  if (typeof obj === 'object') {
    const record = obj as Record<string, JsonLike>
    const keys = Object.keys(record)
    if (keys.length === 0) return '<span style="color:#cbd5e1;">{}</span>'

    const id = `json-obj-${formatIdCounter++}`
    pendingToggleIds.push(id)
    const items = keys
      .map((key) => {
        const value = formatJson(record[key], indent + 1)
        return `${nextIndent}<span style="color:#60a5fa;">"${escapeHtml(key)}"</span><span style="color:#cbd5e1;">:</span> ${value}`
      })
      .join(',\n')

    // Create collapsible object
    const result = `<span style="color:#cbd5e1;">{</span><span id="${id}-toggle" style="cursor:pointer;color:#9ca3af;margin:0 4px;" data-json-toggle="${id}">▼</span><span id="${id}-summary" style="display:none;color:#9ca3af;">${keys.length} keys</span>
<span id="${id}-content">${items}
${indentStr}</span><span style="color:#cbd5e1;">}</span>`

    return result
  }

  return String(obj)
}

export function attachJsonEventListeners() {
  // Attach event listeners to all pending toggle elements
  const idsToAttach = [...pendingToggleIds]
  pendingToggleIds.length = 0 // Clear the array

  idsToAttach.forEach(id => {
    const toggle = document.getElementById(`${id}-toggle`)
    if (toggle && !toggle.dataset.listenerAttached) {
      toggle.dataset.listenerAttached = 'true'
      toggle.addEventListener('click', (e) => {
        e.stopPropagation()
        const summary = document.getElementById(`${id}-summary`)
        const content = document.getElementById(`${id}-content`)

        if (summary && content) {
          const isCollapsed = content.style.display === 'none'
          if (isCollapsed) {
            content.style.display = ''
            summary.style.display = 'none'
            toggle.textContent = '▼'
          } else {
            content.style.display = 'none'
            summary.style.display = 'inline'
            toggle.textContent = '▶'
          }
        }
      })
    }
  })
}


