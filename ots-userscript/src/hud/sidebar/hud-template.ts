import { LogsTab } from './tabs/logs-tab'
import { TabManager } from './tab-manager'

/**
 * HUD Template Generator
 * 
 * Generates the complete HTML structure for the OTS HUD, including:
 * - Root container with styling
 * - Titlebar with status indicators, title, and buttons
 * - Content area with tab manager and tab content panels
 * - Resize handle
 */
export class HudTemplate {
  /**
   * Create the root HUD container element
   */
  static createRoot(): HTMLDivElement {
    const root = document.createElement('div')
    root.id = 'ots-userscript-hud'
    root.style.cssText = `
      position: fixed;
      display: flex;
      flex-direction: column;
      background: rgba(20, 20, 30, 0.95);
      border: 2px solid rgba(30,64,175,0.8);
      border-radius: 8px;
      box-shadow: 0 4px 20px rgba(0, 0, 0, 0.5);
      z-index: 2147483647;
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
      transition: width 0.3s cubic-bezier(0.4, 0, 0.2, 1), height 0.3s cubic-bezier(0.4, 0, 0.2, 1);
    `
    return root
  }

  /**
   * Create the titlebar element with status indicators and buttons
   */
  static createTitlebar(): HTMLDivElement {
    const titlebar = document.createElement('div')
    titlebar.style.cssText = `
      background: linear-gradient(135deg, #1e293b 0%, #0f172a 100%);
      color: white;
      padding: 4px 6px;
      cursor: move;
      user-select: none;
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 4px;
      font-weight: 600;
      font-size: 12px;
      border-radius: 6px 6px 0 0;
      position: relative;
    `

    titlebar.innerHTML = `
      <div style="display:flex;align-items:center;gap:4px;flex-shrink:0;">
        <span style="display:inline-flex;align-items:center;gap:3px;padding:2px 4px;border-radius:10px;background:rgba(0,0,0,0.3);font-size:10px;font-weight:600;letter-spacing:0.05em;color:#e5e7eb;white-space:nowrap;">
          <span id="ots-hud-ws-dot" style="display:inline-flex;width:6px;height:6px;border-radius:999px;background:#f87171;"></span>
          <span>WS</span>
        </span>
        <span style="display:inline-flex;align-items:center;gap:3px;padding:2px 4px;border-radius:10px;background:rgba(0,0,0,0.3);font-size:10px;font-weight:600;letter-spacing:0.05em;color:#e5e7eb;white-space:nowrap;">
          <span id="ots-hud-game-dot" style="display:inline-flex;width:6px;height:6px;border-radius:999px;background:#f87171;"></span>
          <span>GAME</span>
        </span>
      </div>
      <div id="ots-hud-title-content" style="position:absolute;left:50%;top:50%;transform:translate(-50%,-50%);display:flex;align-items:center;gap:4px;white-space:nowrap;">
        <span id="ots-hud-arrow-left" style="display:inline-flex;align-items:center;justify-content:center;font-size:8px;color:#94a3b8;line-height:1;width:10px;height:10px;overflow:hidden;flex-shrink:0;">▶</span>
        <span style="font-size:11px;font-weight:600;letter-spacing:0.02em;color:#cbd5e1;white-space:nowrap;">OTS Dashboard</span>
        <span id="ots-hud-arrow-right" style="display:inline-flex;align-items:center;justify-content:center;font-size:8px;color:#94a3b8;line-height:1;width:10px;height:10px;overflow:hidden;flex-shrink:0;">◀</span>
      </div>
      <div id="ots-hud-buttons" style="display:flex;align-items:center;gap:2px;flex-shrink:0;">
        <button id="ots-hud-settings" style="all:unset;cursor:pointer;display:flex;align-items:center;justify-content:center;font-size:13px;color:white;width:18px;height:18px;border-radius:3px;background:rgba(255,255,255,0.2);transition:background 0.2s;">⚙</button>
        <button id="ots-hud-close" style="all:unset;cursor:pointer;display:flex;align-items:center;justify-content:center;font-size:13px;color:white;width:18px;height:18px;border-radius:3px;background:rgba(255,255,255,0.2);transition:background 0.2s;">×</button>
      </div>
    `

    return titlebar
  }

  /**
   * Create the content container element
   */
  static createContent(): HTMLDivElement {
    const content = document.createElement('div')
    content.id = 'ots-hud-content'
    content.style.cssText = `
      flex: 1;
      min-height: 0;
      display: flex;
      flex-direction: column;
      overflow: hidden;
    `
    return content
  }

  /**
   * Generate the inner HTML for the content area, including tabs and panels
   * @param tabManager - The TabManager instance for generating tab HTML
   */
  static createContentHTML(tabManager: TabManager): string {
    return `
      ${tabManager.createTabsHTML()}
      <div id="ots-hud-body" style="display:flex;flex-direction:column;height:100%;overflow:hidden;">
        ${LogsTab.createHTML()}
        <div id="ots-tab-hardware" class="ots-tab-content" style="flex:1;display:none;overflow-y:auto;padding:12px;background:rgba(10,10,15,0.8);">
          <div id="ots-hardware-content" style="font-size:11px;color:#e5e7eb;"></div>
        </div>
        <div id="ots-tab-sound" class="ots-tab-content" style="flex:1;display:none;overflow-y:auto;padding:12px;background:rgba(10,10,15,0.8);">
          <div style="font-size:11px;color:#e5e7eb;"><div style="font-size:10px;font-weight:600;color:#9ca3af;margin-bottom:8px;letter-spacing:0.05em;">SOUND EVENT TOGGLES</div><div id="ots-sound-toggles"></div></div>
        </div>
      </div>
      <div id="ots-hud-resize-handle" style="position:absolute;width:20px;height:20px;cursor:nwse-resize;display:none;opacity:0.5;transition:opacity 0.2s;">
        <svg width="20" height="20" viewBox="0 0 20 20" style="width:100%;height:100%;">
          <path d="M20,10 L10,20 M20,5 L5,20 M20,0 L0,20" stroke="#94a3b8" stroke-width="2" fill="none"/>
        </svg>
      </div>
    `
  }
}
