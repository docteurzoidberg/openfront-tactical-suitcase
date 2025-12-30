import type { JsonLike } from './types'

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
  if (typeof obj === 'boolean') return `<span style="color:#c084fc;">${obj}</span>`
  if (typeof obj === 'number') return `<span style="color:#facc15;">${obj}</span>`
  if (typeof obj === 'string') return `<span style="color:#4ade80;">\"${escapeHtml(obj)}\"</span>`

  if (Array.isArray(obj)) {
    if (obj.length === 0) return '<span style="color:#94a3b8;">[]</span>'
    const items = obj.map(item => `${nextIndent}${formatJson(item, indent + 1)}`).join(',\n')
    return `<span style="color:#94a3b8;">[</span>\n${items}\n${indentStr}<span style="color:#94a3b8;">]</span>`
  }

  if (typeof obj === 'object') {
    const record = obj as Record<string, JsonLike>
    const keys = Object.keys(record)
    if (keys.length === 0) return '<span style="color:#94a3b8;">{}</span>'
    const items = keys
      .map((key) => {
        const value = formatJson(record[key], indent + 1)
        return `${nextIndent}<span style="color:#60a5fa;">\"${escapeHtml(key)}\"</span><span style="color:#94a3b8;">:</span> ${value}`
      })
      .join(',\n')
    return `<span style="color:#94a3b8;">{</span>\n${items}\n${indentStr}<span style="color:#94a3b8;">}</span>`
  }

  return String(obj)
}
