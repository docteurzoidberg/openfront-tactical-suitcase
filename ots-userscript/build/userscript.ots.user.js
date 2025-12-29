// ==UserScript==
// @name         OTS Game Dashboard Bridge
// @namespace    http://tampermonkey.net/
// @version      2025-12-20.1
// @description  Send game state and events to OTS controller
// @author       [PUSH] DUCKDUCK
// @author       DeloVan
// @author       [PUSH] Nono
// @author       [PUSH] Rime
// @match        https://openfront.io/*
// @grant        GM_getValue
// @grant        GM_setValue
// ==/UserScript==

"use strict";
(() => {
  // src/hud/window.ts
  var FloatingPanel = class {
    constructor(panel) {
      this.panel = panel;
    }
    attachDrag(handle) {
      let dragging = false;
      let startX = 0;
      let startY = 0;
      let startLeft = 0;
      let startTop = 0;
      handle.addEventListener("mousedown", (ev) => {
        dragging = true;
        startX = ev.clientX;
        startY = ev.clientY;
        const rect = this.panel.getBoundingClientRect();
        startLeft = rect.left;
        startTop = rect.top;
        ev.preventDefault();
        ev.stopPropagation();
      });
      window.addEventListener("mousemove", (ev) => {
        if (!dragging) return;
        const dx = ev.clientX - startX;
        const dy = ev.clientY - startY;
        this.panel.style.left = `${startLeft + dx}px`;
        this.panel.style.top = `${startTop + dy}px`;
        this.panel.style.right = "auto";
      });
      window.addEventListener("mouseup", () => {
        dragging = false;
      });
    }
  };

  // src/hud/sidebar-hud.ts
  var STORAGE_KEY_HUD_SIZE = "ots-hud-size";
  var STORAGE_KEY_HUD_SNAP = "ots-hud-snap-position";
  var STORAGE_KEY_HUD_COLLAPSED = "ots-hud-collapsed";
  var STORAGE_KEY_LOG_FILTERS = "ots-hud-log-filters";
  var Hud = class {
    constructor(getWsUrl, setWsUrl, onWsUrlChanged, sendCommand, onSoundTest) {
      this.getWsUrl = getWsUrl;
      this.setWsUrl = setWsUrl;
      this.onWsUrlChanged = onWsUrlChanged;
      this.sendCommand = sendCommand;
      this.onSoundTest = onSoundTest;
      this.root = null;
      this.titlebar = null;
      this.content = null;
      this.closed = false;
      this.collapsed = false;
      this.currentPosition = "right";
      this.isDragging = false;
      this.dragStartX = 0;
      this.dragStartY = 0;
      this.userSize = { width: 320, height: 260 };
      this.isResizing = false;
      this.resizeStartX = 0;
      this.resizeStartY = 0;
      this.resizeStartWidth = 0;
      this.resizeStartHeight = 0;
      this.logList = null;
      this.wsDot = null;
      this.gameDot = null;
      this.body = null;
      this.settingsPanel = null;
      this.settingsInput = null;
      this.filterPanel = null;
      this.filterCheckboxes = {
        directions: { send: null, recv: null, info: null },
        events: { game: null, nukes: null, alerts: null, troops: null, sounds: null, system: null, heartbeat: null }
      };
      this.logFilters = {
        directions: { send: true, recv: true, info: true },
        events: { game: true, nukes: true, alerts: true, troops: true, sounds: true, system: true, heartbeat: false }
      };
      this.logCounter = 0;
      this.autoScroll = true;
      this.autoScrollBtn = null;
      this.activeTab = "logs";
      this.hardwareDiagnostic = null;
      this.soundToggles = {
        "game_start": true,
        "game_player_death": true,
        "game_victory": true,
        "game_defeat": true
      };
      this.collapsed = GM_getValue(STORAGE_KEY_HUD_COLLAPSED, true);
      this.currentPosition = GM_getValue(STORAGE_KEY_HUD_SNAP, "right");
      this.userSize = GM_getValue(STORAGE_KEY_HUD_SIZE, { width: 320, height: 260 });
    }
    ensure() {
      if (this.closed || this.root) return;
      this.createHud();
      this.setupEventListeners();
      this.applyPosition(this.currentPosition);
      this.updateContentVisibility();
    }
    createHud() {
      const root = document.createElement("div");
      root.id = "ots-userscript-hud";
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
    `;
      const titlebar = document.createElement("div");
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
    `;
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
        <span id="ots-hud-arrow-left" style="display:inline-flex;align-items:center;justify-content:center;font-size:8px;color:#94a3b8;line-height:1;width:10px;height:10px;overflow:hidden;flex-shrink:0;">\u25B6</span>
        <span style="font-size:11px;font-weight:600;letter-spacing:0.02em;color:#cbd5e1;white-space:nowrap;">OTS Dashboard</span>
        <span id="ots-hud-arrow-right" style="display:inline-flex;align-items:center;justify-content:center;font-size:8px;color:#94a3b8;line-height:1;width:10px;height:10px;overflow:hidden;flex-shrink:0;">\u25C0</span>
      </div>
      <div id="ots-hud-buttons" style="display:flex;align-items:center;gap:2px;flex-shrink:0;">
        <button id="ots-hud-settings" style="all:unset;cursor:pointer;display:flex;align-items:center;justify-content:center;font-size:13px;color:white;width:18px;height:18px;border-radius:3px;background:rgba(255,255,255,0.2);transition:background 0.2s;">\u2699</button>
        <button id="ots-hud-close" style="all:unset;cursor:pointer;display:flex;align-items:center;justify-content:center;font-size:13px;color:white;width:18px;height:18px;border-radius:3px;background:rgba(255,255,255,0.2);transition:background 0.2s;">\xD7</button>
      </div>
    `;
      const content = document.createElement("div");
      content.id = "ots-hud-content";
      content.style.cssText = `
      flex: 1;
      min-height: 0;
      display: flex;
      flex-direction: column;
      overflow: hidden;
    `;
      content.innerHTML = `
      <div id="ots-hud-tabs" style="display:flex;gap:2px;padding:6px 8px;background:rgba(10,10,15,0.6);border-bottom:1px solid rgba(255,255,255,0.1);">
        <button class="ots-tab-btn" data-tab="logs" style="all:unset;cursor:pointer;font-size:10px;font-weight:600;color:#e5e7eb;padding:4px 12px;border-radius:4px;background:rgba(59,130,246,0.3);transition:all 0.2s;">Logs</button>
        <button class="ots-tab-btn" data-tab="hardware" style="all:unset;cursor:pointer;font-size:10px;font-weight:600;color:#9ca3af;padding:4px 12px;border-radius:4px;background:rgba(255,255,255,0.05);transition:all 0.2s;">Hardware</button>
        <button class="ots-tab-btn" data-tab="sound" style="all:unset;cursor:pointer;font-size:10px;font-weight:600;color:#9ca3af;padding:4px 12px;border-radius:4px;background:rgba(255,255,255,0.05);transition:all 0.2s;">Sound</button>
      </div>
      <div id="ots-hud-body" style="display:flex;flex-direction:column;height:100%;overflow:hidden;">
        <div id="ots-tab-logs" class="ots-tab-content" style="flex:1;display:flex;flex-direction:column;min-height:0;">
          <div id="ots-hud-log" style="flex:1;overflow-y:auto;overflow-x:hidden;padding:8px;font-size:11px;line-height:1.4;background:rgba(10,10,15,0.8);"></div>
          <div id="ots-hud-footer" style="flex:none;padding:6px 8px;display:flex;align-items:center;justify-content:space-between;background:rgba(10,10,15,0.8);border-top:1px solid rgba(255,255,255,0.1);">
            <button id="ots-hud-filter" style="all:unset;cursor:pointer;font-size:11px;color:#e0e0e0;padding:3px 8px;border-radius:4px;background:rgba(255,255,255,0.1);transition:background 0.2s;">\u2691 Filter</button>
            <button id="ots-hud-autoscroll" style="all:unset;cursor:pointer;font-size:10px;font-weight:600;color:#6ee7b7;padding:3px 8px;border-radius:12px;background:rgba(52,211,153,0.2);white-space:nowrap;transition:background 0.2s;">\u2713 Auto</button>
          </div>
        </div>
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
    `;
      root.appendChild(titlebar);
      root.appendChild(content);
      document.body.appendChild(root);
      this.createSettingsPanel();
      this.createFilterPanel();
      this.root = root;
      this.titlebar = titlebar;
      this.content = content;
      this.logList = root.querySelector("#ots-hud-log");
      this.body = root.querySelector("#ots-hud-body");
      this.wsDot = root.querySelector("#ots-hud-ws-dot");
      this.gameDot = root.querySelector("#ots-hud-game-dot");
      this.autoScrollBtn = root.querySelector("#ots-hud-autoscroll");
      const savedFilters = GM_getValue(STORAGE_KEY_LOG_FILTERS, null);
      if (savedFilters) {
        this.logFilters = savedFilters;
        this.restoreFilterStates(savedFilters);
      }
    }
    createSettingsPanel() {
      const panel = document.createElement("div");
      panel.id = "ots-hud-settings-panel";
      panel.style.cssText = `
      position:fixed;min-width:260px;max-width:320px;display:none;flex-direction:column;gap:6px;padding:8px;border-radius:6px;background:rgba(15,23,42,0.98);border:1px solid rgba(59,130,246,0.6);box-shadow:0 8px 20px rgba(0,0,0,0.5);z-index:2147483647;
    `;
      panel.innerHTML = `
      <div id="ots-hud-settings-header" style="display:flex;align-items:center;justify-content:space-between;gap:6px;cursor:move;">
        <span style="font-size:11px;font-weight:600;letter-spacing:0.08em;text-transform:uppercase;color:#9ca3af;">Settings</span>
        <button id="ots-hud-settings-close" style="all:unset;cursor:pointer;font-size:11px;color:#6b7280;padding:2px 4px;border-radius:4px;">\u2715</button>
      </div>
      <label style="display:flex;flex-direction:column;gap:4px;font-size:11px;color:#e5e7eb;">
        <span>WebSocket URL</span>
        <input id="ots-hud-settings-ws" type="text" style="font-size:11px;padding:4px 6px;border-radius:4px;border:1px solid rgba(148,163,184,0.8);background:rgba(15,23,42,0.9);color:#e5e7eb;outline:none;" />
      </label>
      <div style="display:flex;justify-content:flex-end;gap:6px;margin-top:4px;">
        <button id="ots-hud-settings-reset" style="all:unset;cursor:pointer;font-size:11px;color:#9ca3af;padding:3px 6px;border-radius:4px;border:1px solid rgba(148,163,184,0.5);background:rgba(15,23,42,0.9);">Reset</button>
        <button id="ots-hud-settings-save" style="all:unset;cursor:pointer;font-size:11px;color:#0f172a;padding:3px 8px;border-radius:4px;background:#4ade80;font-weight:600;">Save</button>
      </div>
    `;
      document.body.appendChild(panel);
      this.settingsPanel = panel;
      this.settingsInput = panel.querySelector("#ots-hud-settings-ws");
    }
    createFilterPanel() {
      const panel = document.createElement("div");
      panel.id = "ots-hud-filter-panel";
      panel.style.cssText = `
      position:fixed;min-width:220px;max-width:280px;display:none;flex-direction:column;gap:8px;padding:10px;border-radius:6px;background:rgba(15,23,42,0.98);border:1px solid rgba(59,130,246,0.6);box-shadow:0 8px 20px rgba(0,0,0,0.5);z-index:2147483647;
    `;
      panel.innerHTML = `
      <div id="ots-hud-filter-header" style="display:flex;align-items:center;justify-content:space-between;gap:6px;cursor:move;">
        <span style="font-size:11px;font-weight:600;letter-spacing:0.08em;text-transform:uppercase;color:#9ca3af;">Log Filters</span>
        <button id="ots-hud-filter-close" style="all:unset;cursor:pointer;font-size:11px;color:#6b7280;padding:2px 4px;border-radius:4px;">\u2715</button>
      </div>
      <div style="border-bottom:1px solid rgba(148,163,184,0.3);padding-bottom:6px;">
        <div style="font-size:10px;font-weight:600;color:#9ca3af;margin-bottom:6px;letter-spacing:0.05em;">DIRECTION</div>
        <label style="display:flex;align-items:center;gap:6px;font-size:11px;color:#e5e7eb;cursor:pointer;margin-bottom:4px;">
          <input type="checkbox" id="ots-hud-filter-send" checked style="cursor:pointer;" />
          <span style="display:inline-flex;font-size:9px;font-weight:600;letter-spacing:0.08em;text-transform:uppercase;padding:1px 4px;border-radius:999px;background:rgba(52,211,153,0.18);color:#6ee7b7;">SEND</span>
        </label>
        <label style="display:flex;align-items:center;gap:6px;font-size:11px;color:#e5e7eb;cursor:pointer;margin-bottom:4px;">
          <input type="checkbox" id="ots-hud-filter-recv" checked style="cursor:pointer;" />
          <span style="display:inline-flex;font-size:9px;font-weight:600;letter-spacing:0.08em;text-transform:uppercase;padding:1px 4px;border-radius:999px;background:rgba(59,130,246,0.22);color:#93c5fd;">RECV</span>
        </label>
        <label style="display:flex;align-items:center;gap:6px;font-size:11px;color:#e5e7eb;cursor:pointer;">
          <input type="checkbox" id="ots-hud-filter-info" checked style="cursor:pointer;" />
          <span style="display:inline-flex;font-size:9px;font-weight:600;letter-spacing:0.08em;text-transform:uppercase;padding:1px 4px;border-radius:999px;background:rgba(156,163,175,0.25);color:#e5e7eb;">INFO</span>
        </label>
      </div>
      <div>
        <div style="font-size:10px;font-weight:600;color:#9ca3af;margin-bottom:6px;letter-spacing:0.05em;">EVENT TYPE</div>
        <label style="display:flex;align-items:center;gap:6px;font-size:11px;color:#e5e7eb;cursor:pointer;margin-bottom:4px;">
          <input type="checkbox" id="ots-hud-filter-game" checked style="cursor:pointer;" />
          <span style="color:#a78bfa;">Game Events</span>
        </label>
        <label style="display:flex;align-items:center;gap:6px;font-size:11px;color:#e5e7eb;cursor:pointer;margin-bottom:4px;">
          <input type="checkbox" id="ots-hud-filter-nukes" checked style="cursor:pointer;" />
          <span style="color:#f87171;">Nuke Events</span>
        </label>
        <label style="display:flex;align-items:center;gap:6px;font-size:11px;color:#e5e7eb;cursor:pointer;margin-bottom:4px;">
          <input type="checkbox" id="ots-hud-filter-alerts" checked style="cursor:pointer;" />
          <span style="color:#fb923c;">Alert Events</span>
        </label>
        <label style="display:flex;align-items:center;gap:6px;font-size:11px;color:#e5e7eb;cursor:pointer;margin-bottom:4px;">
          <input type="checkbox" id="ots-hud-filter-troops" checked style="cursor:pointer;" />
          <span style="color:#34d399;">Troop Events</span>
        </label>
        <label style="display:flex;align-items:center;gap:6px;font-size:11px;color:#e5e7eb;cursor:pointer;margin-bottom:4px;">
          <input type="checkbox" id="ots-hud-filter-sounds" checked style="cursor:pointer;" />
          <span style="color:#06b6d4;">Sound Events</span>
        </label>
        <label style="display:flex;align-items:center;gap:6px;font-size:11px;color:#e5e7eb;cursor:pointer;margin-bottom:4px;">
          <input type="checkbox" id="ots-hud-filter-system" checked style="cursor:pointer;" />
          <span style="color:#94a3b8;">System Events</span>
        </label>
        <label style="display:flex;align-items:center;gap:6px;font-size:11px;color:#e5e7eb;cursor:pointer;">
          <input type="checkbox" id="ots-hud-filter-heartbeat" style="cursor:pointer;" />
          <span style="color:#64748b;">Heartbeat</span>
        </label>
      </div>
    `;
      document.body.appendChild(panel);
      this.filterPanel = panel;
      this.filterCheckboxes.directions.send = panel.querySelector("#ots-hud-filter-send");
      this.filterCheckboxes.directions.recv = panel.querySelector("#ots-hud-filter-recv");
      this.filterCheckboxes.directions.info = panel.querySelector("#ots-hud-filter-info");
      this.filterCheckboxes.events.game = panel.querySelector("#ots-hud-filter-game");
      this.filterCheckboxes.events.nukes = panel.querySelector("#ots-hud-filter-nukes");
      this.filterCheckboxes.events.alerts = panel.querySelector("#ots-hud-filter-alerts");
      this.filterCheckboxes.events.troops = panel.querySelector("#ots-hud-filter-troops");
      this.filterCheckboxes.events.sounds = panel.querySelector("#ots-hud-filter-sounds");
      this.filterCheckboxes.events.system = panel.querySelector("#ots-hud-filter-system");
      this.filterCheckboxes.events.heartbeat = panel.querySelector("#ots-hud-filter-heartbeat");
    }
    restoreFilterStates(filters) {
      if (this.filterCheckboxes.directions.send) this.filterCheckboxes.directions.send.checked = filters.directions.send;
      if (this.filterCheckboxes.directions.recv) this.filterCheckboxes.directions.recv.checked = filters.directions.recv;
      if (this.filterCheckboxes.directions.info) this.filterCheckboxes.directions.info.checked = filters.directions.info;
      if (this.filterCheckboxes.events.game) this.filterCheckboxes.events.game.checked = filters.events.game;
      if (this.filterCheckboxes.events.nukes) this.filterCheckboxes.events.nukes.checked = filters.events.nukes;
      if (this.filterCheckboxes.events.alerts) this.filterCheckboxes.events.alerts.checked = filters.events.alerts;
      if (this.filterCheckboxes.events.troops) this.filterCheckboxes.events.troops.checked = filters.events.troops;
      if (this.filterCheckboxes.events.sounds) this.filterCheckboxes.events.sounds.checked = filters.events.sounds;
      if (this.filterCheckboxes.events.system) this.filterCheckboxes.events.system.checked = filters.events.system;
      if (this.filterCheckboxes.events.heartbeat) this.filterCheckboxes.events.heartbeat.checked = filters.events.heartbeat;
    }
    setupEventListeners() {
      var _a, _b;
      if (!this.root || !this.titlebar) return;
      const titlebarButtons = this.root.querySelectorAll("#ots-hud-buttons button");
      titlebarButtons.forEach((btn) => {
        btn.addEventListener("mouseenter", () => {
          ;
          btn.style.background = "rgba(255, 255, 255, 0.3)";
        });
        btn.addEventListener("mouseleave", () => {
          ;
          btn.style.background = "rgba(255, 255, 255, 0.2)";
        });
      });
      const closeBtn = this.root.querySelector("#ots-hud-close");
      closeBtn == null ? void 0 : closeBtn.addEventListener("click", () => {
        this.close();
      });
      const settingsBtn = this.root.querySelector("#ots-hud-settings");
      settingsBtn == null ? void 0 : settingsBtn.addEventListener("click", () => {
        this.toggleSettings();
      });
      const filterBtn = this.root.querySelector("#ots-hud-filter");
      if (filterBtn) {
        filterBtn.addEventListener("mouseenter", () => {
          filterBtn.style.background = "rgba(255, 255, 255, 0.15)";
        });
        filterBtn.addEventListener("mouseleave", () => {
          filterBtn.style.background = "rgba(255, 255, 255, 0.1)";
        });
        filterBtn.addEventListener("click", () => {
          this.toggleFilter();
        });
      }
      (_a = this.autoScrollBtn) == null ? void 0 : _a.addEventListener("click", () => {
        this.autoScroll = !this.autoScroll;
        this.updateAutoScrollBtn();
      });
      this.titlebar.addEventListener("click", (e) => {
        if (e.target.tagName === "BUTTON" || e.target.closest("button")) return;
        this.collapsed = !this.collapsed;
        GM_setValue(STORAGE_KEY_HUD_COLLAPSED, this.collapsed);
        this.updateContentVisibility();
        this.updateArrows();
      });
      this.titlebar.addEventListener("mousedown", (e) => {
        if (e.target.tagName === "BUTTON" || e.target.closest("button")) return;
        this.isDragging = false;
        this.dragStartX = e.clientX;
        this.dragStartY = e.clientY;
        e.preventDefault();
      });
      const resizeHandle = this.root.querySelector("#ots-hud-resize-handle");
      if (resizeHandle) {
        resizeHandle.addEventListener("mouseenter", () => {
          resizeHandle.style.opacity = "1";
        });
        resizeHandle.addEventListener("mouseleave", () => {
          if (!this.isResizing) resizeHandle.style.opacity = "0.5";
        });
        resizeHandle.addEventListener("mousedown", (e) => {
          if (this.collapsed) return;
          e.stopPropagation();
          e.preventDefault();
          this.isResizing = true;
          this.resizeStartX = e.clientX;
          this.resizeStartY = e.clientY;
          this.resizeStartWidth = this.userSize.width;
          this.resizeStartHeight = this.userSize.height;
          resizeHandle.style.opacity = "1";
          if (this.root) {
            this.root.style.transition = "none";
            this.root.style.willChange = "width, height";
          }
        });
      }
      document.addEventListener("mousemove", (e) => {
        if (this.isResizing && this.root) {
          e.preventDefault();
          e.stopPropagation();
          const minSize = 200;
          const minTitlebar = 32;
          const pos = this.currentPosition;
          if (pos === "left" || pos === "right") {
            const deltaX = pos === "left" ? e.clientX - this.resizeStartX : this.resizeStartX - e.clientX;
            const newWidth = Math.max(minSize, this.resizeStartWidth + deltaX);
            const deltaY = e.clientY - this.resizeStartY;
            const newHeight = Math.max(minSize, this.resizeStartHeight + deltaY);
            this.userSize.width = newWidth;
            this.userSize.height = newHeight;
            this.root.style.width = `${minTitlebar + newWidth + 2}px`;
            this.root.style.height = `${newHeight}px`;
          } else {
            const deltaX = (e.clientX - this.resizeStartX) * 2;
            const newWidth = Math.max(minSize, this.resizeStartWidth + deltaX);
            const deltaY = pos === "top" ? e.clientY - this.resizeStartY : this.resizeStartY - e.clientY;
            const newHeight = Math.max(minSize, this.resizeStartHeight + deltaY);
            this.userSize.width = newWidth;
            this.userSize.height = newHeight;
            this.root.style.width = `${newWidth}px`;
            this.root.style.height = `${minTitlebar + newHeight + 2}px`;
          }
          return;
        }
        if (this.dragStartX !== 0 || this.dragStartY !== 0) {
          const dx = Math.abs(e.clientX - this.dragStartX);
          const dy = Math.abs(e.clientY - this.dragStartY);
          const dragThreshold = 5;
          if (dx > dragThreshold || dy > dragThreshold) {
            if (!this.isDragging) {
              this.isDragging = true;
              this.titlebar.style.cursor = "grabbing";
            }
            const snapPos = this.calculateSnapPosition(e.clientX, e.clientY);
            this.showSnapPreview(snapPos);
          }
        }
      });
      document.addEventListener("mouseup", (e) => {
        var _a2;
        if (this.isResizing) {
          this.isResizing = false;
          GM_setValue(STORAGE_KEY_HUD_SIZE, this.userSize);
          const resizeHandle2 = (_a2 = this.root) == null ? void 0 : _a2.querySelector("#ots-hud-resize-handle");
          if (resizeHandle2) resizeHandle2.style.opacity = "0.5";
          if (this.root) {
            this.root.style.transition = "width 0.3s cubic-bezier(0.4, 0, 0.2, 1), height 0.3s cubic-bezier(0.4, 0, 0.2, 1)";
            this.root.style.willChange = "auto";
          }
          return;
        }
        if (this.isDragging) {
          this.isDragging = false;
          this.dragStartX = 0;
          this.dragStartY = 0;
          this.titlebar.style.cursor = "move";
          const newPosition = this.calculateSnapPosition(e.clientX, e.clientY);
          this.currentPosition = newPosition;
          GM_setValue(STORAGE_KEY_HUD_SNAP, newPosition);
          this.applyPosition(newPosition);
          this.removeSnapPreview();
          return;
        }
        if (this.dragStartX !== 0 || this.dragStartY !== 0) {
          this.dragStartX = 0;
          this.dragStartY = 0;
        }
      });
      this.setupSettingsPanelEvents();
      const tabBtns = (_b = this.root) == null ? void 0 : _b.querySelectorAll(".ots-tab-btn");
      tabBtns == null ? void 0 : tabBtns.forEach((btn) => {
        btn.addEventListener("click", () => {
          const tab = btn.dataset.tab;
          this.switchTab(tab);
        });
      });
      this.initializeSoundToggles();
      this.initializeHardwareDisplay();
      this.setupFilterPanelEvents();
    }
    updateArrows() {
      var _a, _b;
      const arrowLeft = (_a = this.root) == null ? void 0 : _a.querySelector("#ots-hud-arrow-left");
      const arrowRight = (_b = this.root) == null ? void 0 : _b.querySelector("#ots-hud-arrow-right");
      if (!arrowLeft || !arrowRight) return;
      const collapsed = this.collapsed;
      const position = this.currentPosition;
      if (position === "left") {
        arrowLeft.textContent = collapsed ? "\u25C0" : "\u25B6";
        arrowRight.textContent = collapsed ? "\u25C0" : "\u25B6";
      } else if (position === "right") {
        arrowLeft.textContent = collapsed ? "\u25B6" : "\u25C0";
        arrowRight.textContent = collapsed ? "\u25B6" : "\u25C0";
      } else if (position === "top") {
        arrowLeft.textContent = collapsed ? "\u25BC" : "\u25B2";
        arrowRight.textContent = collapsed ? "\u25BC" : "\u25B2";
      } else if (position === "bottom") {
        arrowLeft.textContent = collapsed ? "\u25B2" : "\u25BC";
        arrowRight.textContent = collapsed ? "\u25B2" : "\u25BC";
      }
    }
    setupSettingsPanelEvents() {
      if (!this.settingsPanel || !this.settingsInput) return;
      const closeBtn = this.settingsPanel.querySelector("#ots-hud-settings-close");
      closeBtn == null ? void 0 : closeBtn.addEventListener("click", () => {
        this.settingsPanel.style.display = "none";
      });
      const resetBtn = this.settingsPanel.querySelector("#ots-hud-settings-reset");
      resetBtn == null ? void 0 : resetBtn.addEventListener("click", () => {
        this.settingsInput.value = "ws://localhost:3000/ws";
      });
      const saveBtn = this.settingsPanel.querySelector("#ots-hud-settings-save");
      saveBtn == null ? void 0 : saveBtn.addEventListener("click", () => {
        const url = this.settingsInput.value.trim();
        if (url) {
          this.setWsUrl(url);
          this.logInfo(`WS URL updated to ${url}`);
          this.settingsPanel.style.display = "none";
          this.onWsUrlChanged();
        }
      });
      const header = this.settingsPanel.querySelector("#ots-hud-settings-header");
      const settingsFloatingPanel = new FloatingPanel(this.settingsPanel);
      settingsFloatingPanel.attachDrag(header);
    }
    setupFilterPanelEvents() {
      if (!this.filterPanel) return;
      const closeBtn = this.filterPanel.querySelector("#ots-hud-filter-close");
      closeBtn == null ? void 0 : closeBtn.addEventListener("click", () => {
        this.filterPanel.style.display = "none";
      });
      Object.entries(this.filterCheckboxes.directions).forEach(([key, checkbox]) => {
        checkbox == null ? void 0 : checkbox.addEventListener("change", () => {
          this.logFilters.directions[key] = checkbox.checked;
          GM_setValue(STORAGE_KEY_LOG_FILTERS, this.logFilters);
          this.refilterLogs();
        });
      });
      Object.entries(this.filterCheckboxes.events).forEach(([key, checkbox]) => {
        checkbox == null ? void 0 : checkbox.addEventListener("change", () => {
          this.logFilters.events[key] = checkbox.checked;
          GM_setValue(STORAGE_KEY_LOG_FILTERS, this.logFilters);
          this.refilterLogs();
        });
      });
      const header = this.filterPanel.querySelector("#ots-hud-filter-header");
      const filterFloatingPanel = new FloatingPanel(this.filterPanel);
      filterFloatingPanel.attachDrag(header);
    }
    calculateSnapPosition(x, y) {
      const w = window.innerWidth;
      const h = window.innerHeight;
      const distLeft = x;
      const distRight = w - x;
      const distTop = y;
      const distBottom = h - y;
      const minDist = Math.min(distLeft, distRight, distTop, distBottom);
      if (minDist === distLeft) return "left";
      if (minDist === distRight) return "right";
      if (minDist === distTop) return "top";
      return "bottom";
    }
    applyPosition(position) {
      if (!this.root || !this.titlebar) return;
      this.root.style.top = "";
      this.root.style.bottom = "";
      this.root.style.left = "";
      this.root.style.right = "";
      this.root.style.transform = "";
      this.root.style.borderTop = "";
      this.root.style.borderBottom = "";
      this.root.style.borderLeft = "";
      this.root.style.borderRight = "";
      this.titlebar.style.writingMode = "";
      this.titlebar.style.transform = "";
      this.titlebar.style.borderRadius = "";
      this.titlebar.style.width = "";
      this.titlebar.style.height = "";
      this.titlebar.style.flexDirection = "";
      const buttonContainer = this.root.querySelector("#ots-hud-buttons");
      const resizeHandle = this.root.querySelector("#ots-hud-resize-handle");
      const minSize = 32;
      const contentWidth = this.collapsed ? 0 : Math.max(200, this.userSize.width);
      const contentHeight = this.collapsed ? 0 : Math.max(150, this.userSize.height);
      switch (position) {
        case "top":
          this.root.style.top = "-2px";
          this.root.style.left = "50%";
          this.root.style.transform = "translateX(-50%)";
          this.root.style.width = this.collapsed ? "325px" : `${Math.max(325, contentWidth)}px`;
          this.root.style.height = `${minSize + contentHeight + 2}px`;
          this.root.style.borderRadius = "0 0 8px 8px";
          this.root.style.borderTop = "none";
          this.titlebar.style.borderRadius = "0 0 8px 8px";
          if (buttonContainer) buttonContainer.style.transform = "";
          if (resizeHandle) {
            resizeHandle.style.display = this.collapsed ? "none" : "block";
            resizeHandle.style.right = "0";
            resizeHandle.style.bottom = "0";
            resizeHandle.style.top = "";
            resizeHandle.style.left = "";
            resizeHandle.style.cursor = "nwse-resize";
          }
          break;
        case "bottom":
          this.root.style.bottom = "-2px";
          this.root.style.left = "50%";
          this.root.style.transform = "translateX(-50%)";
          this.root.style.width = this.collapsed ? "325px" : `${Math.max(325, contentWidth)}px`;
          this.root.style.height = `${minSize + contentHeight + 2}px`;
          this.root.style.borderRadius = "8px 8px 0 0";
          this.root.style.borderBottom = "none";
          this.titlebar.style.borderRadius = "8px 8px 0 0";
          if (buttonContainer) buttonContainer.style.transform = "";
          if (resizeHandle) {
            resizeHandle.style.display = this.collapsed ? "none" : "block";
            resizeHandle.style.right = "0";
            resizeHandle.style.bottom = "0";
            resizeHandle.style.top = "";
            resizeHandle.style.left = "";
            resizeHandle.style.cursor = "nwse-resize";
          }
          break;
        case "left":
          this.root.style.left = "-2px";
          this.root.style.top = "50%";
          this.root.style.transform = "translateY(-50%)";
          this.root.style.width = `${minSize + contentWidth + 2}px`;
          this.root.style.height = this.collapsed ? "250px" : `${Math.max(250, contentHeight)}px`;
          this.root.style.borderRadius = "0 8px 8px 0";
          this.root.style.borderLeft = "none";
          this.titlebar.style.writingMode = "vertical-rl";
          this.titlebar.style.transform = "rotate(180deg)";
          this.titlebar.style.borderRadius = "0 8px 8px 0";
          this.titlebar.style.width = "32px";
          this.titlebar.style.height = "100%";
          if (buttonContainer) buttonContainer.style.transform = "rotate(-180deg)";
          if (resizeHandle) {
            resizeHandle.style.display = this.collapsed ? "none" : "block";
            resizeHandle.style.right = "0";
            resizeHandle.style.bottom = "0";
            resizeHandle.style.top = "";
            resizeHandle.style.left = "";
            resizeHandle.style.cursor = "nwse-resize";
          }
          break;
        case "right":
          this.root.style.right = "-2px";
          this.root.style.top = "50%";
          this.root.style.transform = "translateY(-50%)";
          this.root.style.width = `${minSize + contentWidth + 2}px`;
          this.root.style.height = this.collapsed ? "250px" : `${Math.max(250, contentHeight)}px`;
          this.root.style.borderRadius = "8px 0 0 8px";
          this.root.style.borderRight = "none";
          this.titlebar.style.writingMode = "vertical-rl";
          this.titlebar.style.transform = "rotate(180deg)";
          this.titlebar.style.borderRadius = "8px 0 0 8px";
          this.titlebar.style.width = "32px";
          this.titlebar.style.height = "100%";
          if (buttonContainer) buttonContainer.style.transform = "rotate(-180deg)";
          if (resizeHandle) {
            resizeHandle.style.display = this.collapsed ? "none" : "block";
            resizeHandle.style.right = "0";
            resizeHandle.style.bottom = "0";
            resizeHandle.style.top = "";
            resizeHandle.style.left = "";
            resizeHandle.style.cursor = "nwse-resize";
          }
          break;
      }
      if (position === "left" || position === "right") {
        this.root.style.flexDirection = "row";
        if (this.content) {
          this.content.style.flex = "1";
          this.content.style.minWidth = "0";
        }
        if (this.body) {
          this.body.style.height = "100%";
        }
      } else {
        this.root.style.flexDirection = "column";
        if (this.content) {
          this.content.style.flex = "1";
          this.content.style.minHeight = "0";
        }
        if (this.body) {
          this.body.style.height = "";
          this.body.style.flex = "1";
        }
      }
      this.updateArrows();
    }
    showSnapPreview(position) {
      let preview = document.getElementById("ots-snap-preview");
      if (!preview) {
        preview = document.createElement("div");
        preview.id = "ots-snap-preview";
        preview.style.cssText = `
        position: fixed;
        background: rgba(59, 130, 246, 0.25);
        border: 2px dashed rgba(59, 130, 246, 0.6);
        z-index: 2147483646;
        pointer-events: none;
        transition: all 0.2s;
      `;
        document.body.appendChild(preview);
      }
      const previewSize = 100;
      switch (position) {
        case "top":
          preview.style.top = "0";
          preview.style.left = "50%";
          preview.style.bottom = "";
          preview.style.right = "";
          preview.style.transform = "translateX(-50%)";
          preview.style.width = "300px";
          preview.style.height = `${previewSize}px`;
          break;
        case "bottom":
          preview.style.bottom = "0";
          preview.style.left = "50%";
          preview.style.top = "";
          preview.style.right = "";
          preview.style.transform = "translateX(-50%)";
          preview.style.width = "300px";
          preview.style.height = `${previewSize}px`;
          break;
        case "left":
          preview.style.left = "0";
          preview.style.top = "50%";
          preview.style.bottom = "";
          preview.style.right = "";
          preview.style.transform = "translateY(-50%)";
          preview.style.width = `${previewSize}px`;
          preview.style.height = "300px";
          break;
        case "right":
          preview.style.right = "0";
          preview.style.top = "50%";
          preview.style.left = "";
          preview.style.bottom = "";
          preview.style.transform = "translateY(-50%)";
          preview.style.width = `${previewSize}px`;
          preview.style.height = "300px";
          break;
      }
    }
    removeSnapPreview() {
      const preview = document.getElementById("ots-snap-preview");
      if (preview) {
        preview.remove();
      }
    }
    updateContentVisibility() {
      if (!this.content) return;
      if (this.collapsed) {
        this.content.style.display = "none";
      } else {
        this.content.style.display = "flex";
      }
      this.applyPosition(this.currentPosition);
    }
    positionPanelNextToHud(panel) {
      if (!this.root || !panel) return;
      const hudRect = this.root.getBoundingClientRect();
      const gap = 8;
      panel.style.top = "";
      panel.style.bottom = "";
      panel.style.left = "";
      panel.style.right = "";
      switch (this.currentPosition) {
        case "right":
          panel.style.top = `${hudRect.top}px`;
          panel.style.right = `${window.innerWidth - hudRect.left + gap}px`;
          break;
        case "left":
          panel.style.top = `${hudRect.top}px`;
          panel.style.left = `${hudRect.right + gap}px`;
          break;
        case "top":
          panel.style.top = `${hudRect.bottom + gap}px`;
          panel.style.left = `${hudRect.left}px`;
          break;
        case "bottom":
          panel.style.bottom = `${window.innerHeight - hudRect.top + gap}px`;
          panel.style.left = `${hudRect.left}px`;
          break;
      }
    }
    requestDiagnostic() {
      if (this.sendCommand) {
        this.sendCommand("hardware-diagnostic", {});
        this.pushLog("info", "Hardware diagnostic requested");
      } else {
        this.pushLog("info", "Cannot send diagnostic request: no WebSocket connection");
      }
    }
    toggleSettings() {
      if (!this.settingsPanel || !this.settingsInput || !this.root) return;
      if (this.settingsPanel.style.display === "none" || this.settingsPanel.style.display === "") {
        this.settingsInput.value = this.getWsUrl();
        this.positionPanelNextToHud(this.settingsPanel);
        this.settingsPanel.style.display = "flex";
        if (this.filterPanel) this.filterPanel.style.display = "none";
      } else {
        this.settingsPanel.style.display = "none";
      }
    }
    toggleFilter() {
      if (!this.filterPanel || !this.root) return;
      if (this.filterPanel.style.display === "none" || this.filterPanel.style.display === "") {
        this.positionPanelNextToHud(this.filterPanel);
        this.filterPanel.style.display = "flex";
        if (this.settingsPanel) this.settingsPanel.style.display = "none";
      } else {
        this.filterPanel.style.display = "none";
      }
    }
    updateAutoScrollBtn() {
      if (!this.autoScrollBtn) return;
      if (this.autoScroll) {
        this.autoScrollBtn.style.color = "#6ee7b7";
        this.autoScrollBtn.style.borderColor = "rgba(52,211,153,0.6)";
        this.autoScrollBtn.style.background = "rgba(52,211,153,0.18)";
        this.autoScrollBtn.textContent = "\u2713 Auto";
      } else {
        this.autoScrollBtn.style.color = "#9ca3af";
        this.autoScrollBtn.style.borderColor = "rgba(148,163,184,0.6)";
        this.autoScrollBtn.style.background = "rgba(15,23,42,0.9)";
        this.autoScrollBtn.textContent = "\u2717 Auto";
      }
    }
    refilterLogs() {
      if (!this.logList) return;
      const entries = Array.from(this.logList.children);
      entries.forEach((entry) => {
        const shouldShow = this.shouldShowLogEntry(
          entry.dataset.direction,
          entry.dataset.eventType,
          entry.dataset.messageText
        );
        entry.style.display = shouldShow ? "block" : "none";
      });
    }
    shouldShowLogEntry(direction, eventType, messageText) {
      if (!this.logFilters.directions[direction]) return false;
      if (!eventType) return true;
      if (eventType === "INFO" && messageText && messageText.includes('"heartbeat"')) {
        return this.logFilters.events.heartbeat;
      }
      if (eventType.startsWith("GAME_") || eventType === "GAME_SPAWNING") {
        return this.logFilters.events.game;
      } else if (eventType.includes("NUKE") || eventType.includes("HYDRO") || eventType.includes("MIRV")) {
        return this.logFilters.events.nukes;
      } else if (eventType.startsWith("ALERT_")) {
        return this.logFilters.events.alerts;
      } else if (eventType.includes("TROOP")) {
        return this.logFilters.events.troops;
      } else if (eventType === "SOUND_PLAY") {
        return this.logFilters.events.sounds;
      } else if (eventType === "INFO" || eventType === "ERROR" || eventType === "HARDWARE_TEST") {
        return this.logFilters.events.system;
      }
      return true;
    }
    switchTab(tab) {
      var _a, _b, _c;
      this.activeTab = tab;
      const tabBtns = (_a = this.root) == null ? void 0 : _a.querySelectorAll(".ots-tab-btn");
      tabBtns == null ? void 0 : tabBtns.forEach((btn) => {
        const btnTab = btn.dataset.tab;
        if (btnTab === tab) {
          ;
          btn.style.background = "rgba(59,130,246,0.3)";
          btn.style.color = "#e5e7eb";
        } else {
          ;
          btn.style.background = "rgba(255,255,255,0.05)";
          btn.style.color = "#9ca3af";
        }
      });
      const tabContents = (_b = this.root) == null ? void 0 : _b.querySelectorAll(".ots-tab-content");
      tabContents == null ? void 0 : tabContents.forEach((content) => {
        ;
        content.style.display = "none";
      });
      const activeContent = (_c = this.root) == null ? void 0 : _c.querySelector(`#ots-tab-${tab}`);
      if (activeContent) {
        activeContent.style.display = "flex";
      }
      if (tab === "hardware") {
        this.updateHardwareDisplay();
      }
    }
    initializeSoundToggles() {
      var _a;
      const container = (_a = this.root) == null ? void 0 : _a.querySelector("#ots-sound-toggles");
      if (!container) return;
      const soundIds = ["game_start", "game_player_death", "game_victory", "game_defeat"];
      const soundLabels = {
        "game_start": "Game Start",
        "game_player_death": "Player Death",
        "game_victory": "Victory",
        "game_defeat": "Defeat"
      };
      container.innerHTML = soundIds.map((id) => `
      <div style="display:flex;align-items:center;gap:8px;margin-bottom:6px;padding:6px 8px;border-radius:4px;background:rgba(255,255,255,0.05);transition:background 0.2s;" data-sound-row="${id}">
        <label style="display:flex;align-items:center;gap:8px;font-size:11px;color:#e5e7eb;cursor:pointer;flex:1;">
          <input type="checkbox" data-sound-toggle="${id}" ${this.soundToggles[id] ? "checked" : ""} style="cursor:pointer;" />
          <span style="flex:1;">${soundLabels[id]}</span>
          <code style="font-size:9px;color:#94a3b8;font-family:monospace;">${id}</code>
        </label>
        <button data-sound-test="${id}" style="all:unset;cursor:pointer;font-size:11px;padding:4px 10px;border-radius:4px;background:rgba(59,130,246,0.3);color:#93c5fd;font-weight:600;transition:background 0.2s;white-space:nowrap;">\u25B6 Test</button>
      </div>
    `).join("");
      container.querySelectorAll("[data-sound-toggle]").forEach((checkbox) => {
        checkbox.addEventListener("change", (e) => {
          const soundId = e.target.dataset.soundToggle;
          this.soundToggles[soundId] = e.target.checked;
          this.logInfo(`Sound ${soundId}: ${this.soundToggles[soundId] ? "enabled" : "disabled"}`);
        });
      });
      container.querySelectorAll("[data-sound-test]").forEach((button) => {
        button.addEventListener("click", (e) => {
          const soundId = button.dataset.soundTest;
          this.testSound(soundId);
        });
        button.addEventListener("mouseenter", () => {
          button.style.background = "rgba(59,130,246,0.5)";
        });
        button.addEventListener("mouseleave", () => {
          button.style.background = "rgba(59,130,246,0.3)";
        });
      });
      container.querySelectorAll("[data-sound-row]").forEach((row) => {
        row.addEventListener("mouseenter", () => {
          ;
          row.style.background = "rgba(255,255,255,0.08)";
        });
        row.addEventListener("mouseleave", () => {
          ;
          row.style.background = "rgba(255,255,255,0.05)";
        });
      });
    }
    testSound(soundId) {
      this.logInfo(`Testing sound: ${soundId}`);
      if (this.onSoundTest) {
        this.onSoundTest(soundId);
      }
    }
    initializeHardwareDisplay() {
      var _a;
      const container = (_a = this.root) == null ? void 0 : _a.querySelector("#ots-hardware-content");
      if (!container) return;
      container.innerHTML = `
      <div style="margin-bottom:16px;padding:10px;background:rgba(59,130,246,0.1);border-left:3px solid #3b82f6;border-radius:4px;">
        <div style="font-size:11px;font-weight:600;color:#93c5fd;margin-bottom:8px;">\u2699\uFE0F DEVICE CONNECTION</div>
        <label style="display:flex;flex-direction:column;gap:4px;font-size:10px;color:#e5e7eb;">
          <span style="font-size:9px;font-weight:600;color:#9ca3af;letter-spacing:0.05em;">WEBSOCKET URL</span>
          <input id="ots-device-ws-url" type="text" placeholder="ws://ots-fw-main.local:3000/ws" value="${this.getWsUrl()}" style="font-size:10px;padding:6px 8px;border-radius:4px;border:1px solid rgba(148,163,184,0.4);background:rgba(15,23,42,0.7);color:#e5e7eb;outline:none;font-family:monospace;" />
          <span style="font-size:8px;color:#64748b;">Connect to firmware device or ots-server</span>
        </label>
      </div>
      <div style="margin-bottom:12px;">
        <button id="ots-refresh-device-info" style="all:unset;cursor:pointer;font-size:11px;color:#0f172a;padding:6px 12px;border-radius:4px;background:#4ade80;font-weight:600;width:100%;text-align:center;margin-bottom:12px;">\u{1F527} Request Hardware Diagnostic</button>
      </div>
      <div style="text-align:center;color:#64748b;padding:40px 20px;">
        <div style="font-size:32px;margin-bottom:12px;">\u{1F527}</div>
        <div style="font-size:12px;margin-bottom:8px;">No diagnostic data available</div>
        <div style="font-size:10px;">Click the button above to request hardware diagnostic</div>
      </div>
    `;
      const wsUrlInput = container.querySelector("#ots-device-ws-url");
      wsUrlInput == null ? void 0 : wsUrlInput.addEventListener("change", () => {
        const newUrl = wsUrlInput.value.trim();
        if (newUrl) {
          this.setWsUrl(newUrl);
          this.onWsUrlChanged();
          this.logInfo(`Device WS URL updated to ${newUrl}`);
        }
      });
      const refreshBtn = container.querySelector("#ots-refresh-device-info");
      refreshBtn == null ? void 0 : refreshBtn.addEventListener("click", () => {
        this.requestDiagnostic();
      });
    }
    updateHardwareDisplay() {
      var _a;
      const container = (_a = this.root) == null ? void 0 : _a.querySelector("#ots-hardware-content");
      if (!container) return;
      if (!this.hardwareDiagnostic) {
        this.initializeHardwareDisplay();
        return;
      }
      const data = this.hardwareDiagnostic;
      const hardware = data.hardware || {};
      const componentIcons = {
        lcd: "\u{1F4FA}",
        inputBoard: "\u{1F3AE}",
        outputBoard: "\u{1F4A1}",
        adc: "\u{1F4CA}",
        soundModule: "\u{1F50A}"
      };
      const componentLabels = {
        lcd: "LCD Display",
        inputBoard: "Input Board",
        outputBoard: "Output Board",
        adc: "ADC",
        soundModule: "Sound Module"
      };
      const getStatusIcon = (component) => {
        if (!component) return "\u2753";
        if (component.present && component.working) return "\u2705";
        if (component.present && !component.working) return "\u26A0\uFE0F";
        return "\u274C";
      };
      const getStatusColor = (component) => {
        if (!component) return "#64748b";
        if (component.present && component.working) return "#4ade80";
        if (component.present && !component.working) return "#facc15";
        return "#f87171";
      };
      const getStatusText = (component) => {
        if (!component) return "Unknown";
        if (component.present && component.working) return "Working";
        if (component.present && !component.working) return "Present (Not Working)";
        return "Not Present";
      };
      container.innerHTML = `
      <div style="margin-bottom:16px;padding:10px;background:rgba(59,130,246,0.1);border-left:3px solid #3b82f6;border-radius:4px;">
        <div style="font-size:12px;font-weight:600;color:#93c5fd;margin-bottom:6px;">\u{1F4E1} ${data.deviceType || "Unknown"}</div>
        <div style="font-size:10px;color:#94a3b8;line-height:1.6;font-family:monospace;">
          <div>Serial: <span style="color:#e5e7eb;">${data.serialNumber || "Unknown"}</span></div>
          <div>Owner: <span style="color:#e5e7eb;">${data.owner || "Unknown"}</span></div>
          <div>Version: <span style="color:#e5e7eb;">${data.version || "Unknown"}</span></div>
        </div>
      </div>
      <div style="font-size:10px;font-weight:600;color:#9ca3af;margin-bottom:8px;letter-spacing:0.05em;">HARDWARE COMPONENTS</div>
      ${Object.keys(hardware).map((key) => {
        const component = hardware[key];
        return `
          <div style="display:flex;align-items:center;gap:10px;margin-bottom:8px;padding:8px;background:rgba(255,255,255,0.03);border-radius:4px;border-left:3px solid ${getStatusColor(component)};">
            <div style="font-size:18px;flex-shrink:0;">${componentIcons[key] || "\u{1F4E6}"}</div>
            <div style="flex:1;">
              <div style="font-size:11px;font-weight:600;color:#e5e7eb;margin-bottom:2px;">${componentLabels[key] || key}</div>
              <div style="font-size:9px;color:#94a3b8;">${getStatusText(component)}</div>
            </div>
            <div style="font-size:16px;flex-shrink:0;">${getStatusIcon(component)}</div>
          </div>
        `;
      }).join("")}
      <div style="margin-top:12px;font-size:9px;color:#64748b;text-align:center;">
        Last updated: ${new Date(data.timestamp || Date.now()).toLocaleTimeString()}
      </div>
    `;
    }
    isSoundEnabled(soundId) {
      return this.soundToggles[soundId] !== false;
    }
    close() {
      this.closed = true;
      if (this.root) {
        this.root.style.display = "none";
      }
      if (this.settingsPanel) {
        this.settingsPanel.style.display = "none";
      }
      if (this.filterPanel) {
        this.filterPanel.style.display = "none";
      }
    }
    setWsStatus(status) {
      if (!this.wsDot) return;
      switch (status) {
        case "CONNECTING":
          this.wsDot.style.background = "#facc15";
          break;
        case "OPEN":
          this.wsDot.style.background = "#4ade80";
          break;
        case "DISCONNECTED":
        case "ERROR":
          this.wsDot.style.background = "#f97373";
          break;
      }
    }
    setGameStatus(connected) {
      if (!this.gameDot) return;
      this.gameDot.style.background = connected ? "#4ade80" : "#f97373";
    }
    logSend(text) {
      this.addLog("send", text);
    }
    logRecv(text, eventType) {
      var _a, _b;
      if (eventType === "HARDWARE_DIAGNOSTIC") {
        try {
          const parsed = JSON.parse(text);
          console.log("[OTS HUD] Received HARDWARE_DIAGNOSTIC:", parsed);
          const data = ((_a = parsed == null ? void 0 : parsed.payload) == null ? void 0 : _a.data) || (parsed == null ? void 0 : parsed.data);
          if (data) {
            this.hardwareDiagnostic = {
              ...data,
              timestamp: ((_b = parsed == null ? void 0 : parsed.payload) == null ? void 0 : _b.timestamp) || (parsed == null ? void 0 : parsed.timestamp) || Date.now()
            };
            console.log("[OTS HUD] Hardware diagnostic captured:", this.hardwareDiagnostic);
            this.logInfo("Hardware diagnostic data captured - switch to Hardware tab to view");
            if (this.activeTab === "hardware") {
              this.updateHardwareDisplay();
            }
          } else {
            console.warn("[OTS HUD] No data found in HARDWARE_DIAGNOSTIC message:", parsed);
          }
        } catch (e) {
          console.error("[OTS HUD] Failed to parse HARDWARE_DIAGNOSTIC:", e, text);
        }
      }
      this.addLog("recv", text, eventType);
    }
    logInfo(text) {
      this.addLog("info", text);
    }
    // Alias for compatibility with WsClient
    pushLog(direction, text, eventType) {
      var _a, _b;
      if (direction === "recv" && eventType === "HARDWARE_DIAGNOSTIC") {
        try {
          const parsed = JSON.parse(text);
          console.log("[OTS HUD] Received HARDWARE_DIAGNOSTIC:", parsed);
          const data = ((_a = parsed == null ? void 0 : parsed.payload) == null ? void 0 : _a.data) || (parsed == null ? void 0 : parsed.data);
          if (data) {
            this.hardwareDiagnostic = {
              ...data,
              timestamp: ((_b = parsed == null ? void 0 : parsed.payload) == null ? void 0 : _b.timestamp) || (parsed == null ? void 0 : parsed.timestamp) || Date.now()
            };
            console.log("[OTS HUD] Hardware diagnostic captured:", this.hardwareDiagnostic);
            this.logInfo("Hardware diagnostic data captured - switch to Hardware tab to view");
            if (this.activeTab === "hardware") {
              this.updateHardwareDisplay();
            }
          } else {
            console.warn("[OTS HUD] No data found in HARDWARE_DIAGNOSTIC message:", parsed);
          }
        } catch (e) {
          console.error("[OTS HUD] Failed to parse HARDWARE_DIAGNOSTIC:", e, text);
        }
      }
      this.addLog(direction, text, eventType);
    }
    addLog(direction, text, eventType) {
      if (!this.logList) return;
      if (!this.shouldShowLogEntry(direction, eventType, text)) return;
      const id = ++this.logCounter;
      const ts = Date.now();
      const entry = document.createElement("div");
      entry.dataset.id = String(id);
      entry.dataset.direction = direction;
      entry.dataset.messageText = text;
      if (eventType) entry.dataset.eventType = eventType;
      entry.style.cssText = "margin-bottom:3px;padding:2px 4px;border-radius:3px;background:rgba(15,23,42,0.7);border-left:2px solid;word-break:break-word;";
      const time = new Date(ts).toLocaleTimeString("en-US", { hour12: false, hour: "2-digit", minute: "2-digit", second: "2-digit" });
      let badge = "";
      let borderColor = "";
      switch (direction) {
        case "send":
          badge = '<span style="display:inline-flex;font-size:9px;font-weight:600;letter-spacing:0.08em;text-transform:uppercase;padding:1px 4px;border-radius:999px;background:rgba(52,211,153,0.18);color:#6ee7b7;margin-right:4px;">SEND</span>';
          borderColor = "#6ee7b7";
          break;
        case "recv":
          badge = '<span style="display:inline-flex;font-size:9px;font-weight:600;letter-spacing:0.08em;text-transform:uppercase;padding:1px 4px;border-radius:999px;background:rgba(59,130,246,0.22);color:#93c5fd;margin-right:4px;">RECV</span>';
          borderColor = "#93c5fd";
          break;
        case "info":
          badge = '<span style="display:inline-flex;font-size:9px;font-weight:600;letter-spacing:0.08em;text-transform:uppercase;padding:1px 4px;border-radius:999px;background:rgba(156,163,175,0.25);color:#e5e7eb;margin-right:4px;">INFO</span>';
          borderColor = "#9ca3af";
          break;
      }
      entry.style.borderLeftColor = borderColor;
      let jsonData = null;
      let summary = text;
      try {
        jsonData = JSON.parse(text);
        if (jsonData.type === "event" && jsonData.payload) {
          const eventType2 = jsonData.payload.type || "Unknown";
          const message = jsonData.payload.message;
          summary = `Event: ${eventType2}${message ? " - " + message : ""}`;
        } else if (jsonData.type === "cmd" && jsonData.payload) {
          const action = jsonData.payload.action || "Unknown";
          summary = `Command: ${action}`;
        } else if (jsonData.type) {
          summary = `${jsonData.type}`;
        } else {
          summary = "JSON Message (click to expand)";
        }
      } catch (e) {
      }
      if (jsonData) {
        entry.innerHTML = `
        <div style="display:flex;align-items:center;gap:4px;margin-bottom:1px;">
          <span style="font-size:9px;color:#6b7280;font-family:monospace;">${time}</span>${badge}
        </div>
        <div class="log-summary" style="font-size:11px;color:#e5e7eb;line-height:1.3;cursor:pointer;user-select:none;display:flex;align-items:center;gap:4px;">
          <span style="color:#9ca3af;">\u25B6</span>
          <span>${this.escapeHtml(summary)}</span>
        </div>
        <div class="log-details" style="display:none;margin-top:4px;padding:8px;background:rgba(0,0,0,0.4);border-radius:4px;font-family:monospace;font-size:10px;line-height:1.5;overflow-x:auto;">
          ${this.formatJson(jsonData)}
        </div>
      `;
        const summaryEl = entry.querySelector(".log-summary");
        const detailsEl = entry.querySelector(".log-details");
        const arrow = summaryEl == null ? void 0 : summaryEl.querySelector("span:first-child");
        summaryEl == null ? void 0 : summaryEl.addEventListener("click", () => {
          const isExpanded = detailsEl.style.display !== "none";
          detailsEl.style.display = isExpanded ? "none" : "block";
          if (arrow) arrow.textContent = isExpanded ? "\u25B6" : "\u25BC";
        });
      } else {
        entry.innerHTML = `
        <div style="display:flex;align-items:center;gap:4px;margin-bottom:1px;">
          <span style="font-size:9px;color:#6b7280;font-family:monospace;">${time}</span>${badge}
        </div>
        <div style="font-size:11px;color:#e5e7eb;line-height:1.3;">${this.escapeHtml(text)}</div>
      `;
      }
      this.logList.appendChild(entry);
      if (this.autoScroll) {
        this.logList.scrollTop = this.logList.scrollHeight;
      }
      while (this.logList.children.length > 100) {
        this.logList.removeChild(this.logList.firstChild);
      }
    }
    formatJson(obj, indent = 0) {
      const indentStr = "  ".repeat(indent);
      const nextIndent = "  ".repeat(indent + 1);
      if (obj === null) return '<span style="color:#f87171;">null</span>';
      if (obj === void 0) return '<span style="color:#f87171;">undefined</span>';
      if (typeof obj === "boolean") return `<span style="color:#c084fc;">${obj}</span>`;
      if (typeof obj === "number") return `<span style="color:#facc15;">${obj}</span>`;
      if (typeof obj === "string") return `<span style="color:#4ade80;">"${this.escapeHtml(obj)}"</span>`;
      if (Array.isArray(obj)) {
        if (obj.length === 0) return '<span style="color:#94a3b8;">[]</span>';
        const items = obj.map((item) => `${nextIndent}${this.formatJson(item, indent + 1)}`).join(",\n");
        return `<span style="color:#94a3b8;">[</span>
${items}
${indentStr}<span style="color:#94a3b8;">]</span>`;
      }
      if (typeof obj === "object") {
        const keys = Object.keys(obj);
        if (keys.length === 0) return '<span style="color:#94a3b8;">{}</span>';
        const items = keys.map((key) => {
          const value = this.formatJson(obj[key], indent + 1);
          return `${nextIndent}<span style="color:#60a5fa;">"${this.escapeHtml(key)}"</span><span style="color:#94a3b8;">:</span> ${value}`;
        }).join(",\n");
        return `<span style="color:#94a3b8;">{</span>
${items}
${indentStr}<span style="color:#94a3b8;">}</span>`;
      }
      return String(obj);
    }
    escapeHtml(text) {
      const div = document.createElement("div");
      div.textContent = text;
      return div.innerHTML;
    }
  };

  // ../ots-shared/src/game.ts
  var PROTOCOL_CONSTANTS = {
    // WebSocket configuration
    DEFAULT_WS_URL: "ws://localhost:3000/ws",
    DEFAULT_WS_PORT: 3e3,
    // Client types for handshake
    CLIENT_TYPE_UI: "ui",
    CLIENT_TYPE_USERSCRIPT: "userscript",
    CLIENT_TYPE_FIRMWARE: "firmware",
    // Standard INFO event messages
    INFO_MESSAGE_USERSCRIPT_CONNECTED: "userscript-connected",
    INFO_MESSAGE_USERSCRIPT_DISCONNECTED: "userscript-disconnected",
    INFO_MESSAGE_NUKE_SENT: "Nuke sent",
    // Heartbeat configuration
    HEARTBEAT_INTERVAL_MS: 5e3,
    RECONNECT_DELAY_MS: 2e3,
    RECONNECT_MAX_DELAY_MS: 15e3
  };

  // src/websocket/client.ts
  function debugLog(...args) {
    console.log("[OTS Userscript]", ...args);
  }
  var WsClient = class {
    constructor(hud, getWsUrl, onCommand) {
      this.hud = hud;
      this.getWsUrl = getWsUrl;
      this.onCommand = onCommand;
      this.socket = null;
      this.reconnectTimeout = null;
      this.reconnectDelay = 2e3;
      this.heartbeatInterval = null;
      this.shouldReconnect = true;
    }
    connect() {
      if (this.socket && this.socket.readyState === WebSocket.OPEN) {
        return;
      }
      if (this.socket && this.socket.readyState === WebSocket.CONNECTING) {
        return;
      }
      if (this.reconnectTimeout !== null) {
        window.clearTimeout(this.reconnectTimeout);
        this.reconnectTimeout = null;
      }
      const url = this.getWsUrl();
      debugLog("Connecting to", url);
      this.hud.setWsStatus("CONNECTING");
      this.hud.pushLog("info", `Connecting to ${url}`);
      this.shouldReconnect = true;
      try {
        this.socket = new WebSocket(url);
      } catch (error) {
        debugLog("Failed to create WebSocket:", error);
        this.hud.setWsStatus("ERROR");
        this.hud.pushLog("info", `Failed to create WebSocket: ${error}`);
        this.scheduleReconnect();
        return;
      }
      this.socket.addEventListener("open", () => {
        debugLog("WebSocket connected");
        this.hud.setWsStatus("OPEN");
        this.hud.pushLog("info", "WebSocket connected");
        this.reconnectDelay = 2e3;
        this.safeSend({ type: "handshake", clientType: "userscript" });
        this.sendInfo(PROTOCOL_CONSTANTS.INFO_MESSAGE_USERSCRIPT_CONNECTED, { url: window.location.href });
        this.startHeartbeat();
      });
      this.socket.addEventListener("close", (ev) => {
        debugLog("WebSocket closed", ev.code, ev.reason);
        this.hud.setWsStatus("DISCONNECTED");
        this.hud.pushLog("info", `WebSocket closed (${ev.code} ${ev.reason || ""})`);
        this.stopHeartbeat();
        if (this.shouldReconnect) {
          this.scheduleReconnect();
        }
      });
      this.socket.addEventListener("error", (err) => {
        debugLog("WebSocket error", err);
        this.hud.setWsStatus("ERROR");
        this.hud.pushLog("info", "WebSocket error - will retry connection");
      });
      this.socket.addEventListener("message", (event) => {
        var _a;
        let eventType;
        if (typeof event.data === "string") {
          try {
            const parsed = JSON.parse(event.data);
            if (parsed.type === "event" && ((_a = parsed.payload) == null ? void 0 : _a.type)) {
              eventType = parsed.payload.type;
            }
          } catch (e) {
          }
        }
        this.hud.pushLog("recv", typeof event.data === "string" ? event.data : "[binary message]", eventType);
        this.handleServerMessage(event.data);
      });
    }
    disconnect(code, reason) {
      if (!this.socket) return;
      this.shouldReconnect = false;
      this.stopHeartbeat();
      if (this.reconnectTimeout !== null) {
        window.clearTimeout(this.reconnectTimeout);
        this.reconnectTimeout = null;
      }
      try {
        this.socket.close(code, reason);
      } catch (e) {
      }
    }
    scheduleReconnect() {
      if (this.reconnectTimeout !== null) return;
      if (!this.shouldReconnect) return;
      debugLog(`Scheduling reconnect in ${this.reconnectDelay}ms`);
      this.reconnectTimeout = window.setTimeout(() => {
        this.reconnectTimeout = null;
        this.reconnectDelay = Math.min(this.reconnectDelay * 1.5, 15e3);
        if (this.socket && this.socket.readyState !== WebSocket.OPEN) {
          try {
            this.socket.close();
          } catch (e) {
          }
          this.socket = null;
        }
        debugLog("Attempting to reconnect...");
        this.connect();
      }, this.reconnectDelay);
    }
    safeSend(msg) {
      var _a;
      if (!this.socket || this.socket.readyState !== WebSocket.OPEN) {
        debugLog("Cannot send, socket not open");
        this.hud.pushLog("info", "Cannot send, socket not open");
        return;
      }
      const json = JSON.stringify(msg);
      let eventType;
      if (typeof msg === "object" && msg !== null) {
        const msgObj = msg;
        if (msgObj.type === "event" && ((_a = msgObj.payload) == null ? void 0 : _a.type)) {
          eventType = msgObj.payload.type;
        }
      }
      this.hud.pushLog("send", json, eventType);
      this.socket.send(json);
    }
    sendState(state) {
      this.safeSend({
        type: "state",
        payload: state
      });
    }
    sendEvent(type, message, data) {
      this.safeSend({
        type: "event",
        payload: {
          type,
          timestamp: Date.now(),
          message,
          data
        }
      });
    }
    sendInfo(message, data) {
      this.sendEvent("INFO", message, data);
    }
    sendCommand(action, params) {
      this.safeSend({
        type: "cmd",
        payload: {
          action,
          params
        }
      });
    }
    startHeartbeat() {
      this.stopHeartbeat();
      this.heartbeatInterval = window.setInterval(() => {
        this.sendInfo("heartbeat");
      }, 15e3);
    }
    stopHeartbeat() {
      if (this.heartbeatInterval !== null) {
        window.clearInterval(this.heartbeatInterval);
        this.heartbeatInterval = null;
      }
    }
    handleServerMessage(raw) {
      var _a;
      if (typeof raw !== "string") {
        debugLog("Non-text message from server", raw);
        this.hud.pushLog("info", "Non-text message from server");
        return;
      }
      let msg;
      try {
        msg = JSON.parse(raw);
      } catch (e) {
        debugLog("Invalid JSON from server", raw, e);
        this.hud.pushLog("info", "Invalid JSON from server");
        return;
      }
      if (msg.type === "cmd" && typeof ((_a = msg.payload) == null ? void 0 : _a.action) === "string") {
        const { action, params } = msg.payload;
        debugLog("Received command from dashboard:", action, params);
        if (action === "ping") {
          this.sendInfo("pong-from-userscript");
        } else if (this.onCommand) {
          this.onCommand(action, params);
        } else {
          debugLog("No command handler registered for:", action);
        }
      }
    }
  };

  // src/utils/dom.ts
  function waitForElement(selector, callback) {
    const existing = document.querySelector(selector);
    if (existing) {
      callback(existing);
      return;
    }
    const observer = new MutationObserver(() => {
      const el = document.querySelector(selector);
      if (el) {
        observer.disconnect();
        callback(el);
      }
    });
    observer.observe(document.documentElement || document.body, {
      childList: true,
      subtree: true
    });
  }

  // src/game/game-api.ts
  function getGameView() {
    try {
      const eventsDisplay = document.querySelector("events-display");
      if (eventsDisplay && eventsDisplay.game) {
        return eventsDisplay.game;
      }
    } catch (e) {
    }
    return null;
  }
  function createGameAPI() {
    let cachedGame = null;
    let lastCheck = 0;
    const CACHE_DURATION = 1e3;
    const getGame = () => {
      const now = Date.now();
      if (!cachedGame || now - lastCheck > CACHE_DURATION) {
        cachedGame = getGameView();
        lastCheck = now;
      }
      return cachedGame;
    };
    return {
      isValid() {
        const game = getGame();
        return game != null;
      },
      getMyPlayer() {
        try {
          const game = getGame();
          if (!game || typeof game.myPlayer !== "function") return null;
          return game.myPlayer();
        } catch (e) {
          return null;
        }
      },
      getMyPlayerID() {
        try {
          const myPlayer = this.getMyPlayer();
          if (!myPlayer || typeof myPlayer.id !== "function") return null;
          return myPlayer.id();
        } catch (e) {
          return null;
        }
      },
      getMySmallID() {
        try {
          const myPlayer = this.getMyPlayer();
          if (!myPlayer || typeof myPlayer.smallID !== "function") return null;
          return myPlayer.smallID();
        } catch (e) {
          return null;
        }
      },
      getUnits(...types) {
        try {
          const game = getGame();
          if (!game || typeof game.units !== "function") return [];
          return game.units(...types) || [];
        } catch (e) {
          return [];
        }
      },
      getTicks() {
        try {
          const game = getGame();
          if (!game || typeof game.ticks !== "function") return null;
          return game.ticks();
        } catch (e) {
          return null;
        }
      },
      getX(tile) {
        try {
          const game = getGame();
          if (!game || typeof game.x !== "function") return null;
          return game.x(tile);
        } catch (e) {
          return null;
        }
      },
      getY(tile) {
        try {
          const game = getGame();
          if (!game || typeof game.y !== "function") return null;
          return game.y(tile);
        } catch (e) {
          return null;
        }
      },
      getOwner(tile) {
        try {
          const game = getGame();
          if (!game || typeof game.owner !== "function") return null;
          return game.owner(tile);
        } catch (e) {
          return null;
        }
      },
      getUnit(unitId) {
        try {
          const game = getGame();
          if (!game || typeof game.unit !== "function") return null;
          return game.unit(unitId);
        } catch (e) {
          return null;
        }
      },
      getPlayerBySmallID(smallID) {
        try {
          const game = getGame();
          if (!game || typeof game.playerBySmallID !== "function") return null;
          return game.playerBySmallID(smallID);
        } catch (e) {
          return null;
        }
      },
      getCurrentTroops() {
        try {
          const myPlayer = this.getMyPlayer();
          if (!myPlayer || typeof myPlayer.troops !== "function") return null;
          return myPlayer.troops();
        } catch (e) {
          return null;
        }
      },
      getMaxTroops() {
        try {
          const game = getGame();
          const myPlayer = this.getMyPlayer();
          if (!game || !myPlayer) return null;
          if (typeof game.config !== "function") return null;
          const config = game.config();
          if (!config || typeof config.maxTroops !== "function") return null;
          return config.maxTroops(myPlayer);
        } catch (e) {
          return null;
        }
      },
      getAttackRatio() {
        const controlPanel = document.querySelector("control-panel");
        if (controlPanel && controlPanel.uiState && typeof controlPanel.uiState.attackRatio === "number") {
          const ratio = controlPanel.uiState.attackRatio;
          if (ratio >= 0 && ratio <= 1) {
            return ratio;
          }
        }
        throw new Error("Unable to access attackRatio from UIState");
      },
      getTroopsToSend() {
        try {
          const currentTroops = this.getCurrentTroops();
          if (currentTroops === null) return null;
          const attackRatio = this.getAttackRatio();
          return Math.floor(currentTroops * attackRatio);
        } catch (e) {
          return null;
        }
      },
      isGameStarted() {
        try {
          const game = getGame();
          if (!game) return null;
          if (typeof game.inSpawnPhase === "function") {
            return !game.inSpawnPhase();
          }
          if (typeof game.ticks === "function") {
            const ticks = game.ticks();
            return ticks !== null && ticks > 0;
          }
          return null;
        } catch (error) {
          console.error("[GameAPI] Error checking game started:", error);
          return null;
        }
      },
      hasSpawned() {
        try {
          const myPlayer = this.getMyPlayer();
          if (!myPlayer) return null;
          if (typeof myPlayer.hasSpawned === "function") {
            return myPlayer.hasSpawned();
          }
          return null;
        } catch (error) {
          console.error("[GameAPI] Error checking hasSpawned:", error);
          return null;
        }
      },
      getUpdatesSinceLastTick() {
        try {
          const game = getGame();
          if (!game || typeof game.updatesSinceLastTick !== "function") return null;
          return game.updatesSinceLastTick();
        } catch (error) {
          console.error("[GameAPI] Error getting updates:", error);
          return null;
        }
      },
      didPlayerWin() {
        try {
          const game = window.game;
          if (!game) return null;
          const myPlayer = game.myPlayer();
          if (!myPlayer) return null;
          const gameOver = game.gameOver();
          if (!gameOver) return null;
          if (myPlayer.eliminated()) {
            return false;
          }
          return true;
        } catch (error) {
          console.error("[GameAPI] Error checking win status:", error);
          return null;
        }
      }
    };
  }

  // src/game/nuke-tracker.ts
  var NukeTracker = class {
    constructor() {
      this.trackedNukes = /* @__PURE__ */ new Map();
      this.eventCallbacks = [];
    }
    onEvent(callback) {
      this.eventCallbacks.push(callback);
    }
    emitEvent(event) {
      this.eventCallbacks.forEach((cb) => {
        try {
          cb(event);
        } catch (e) {
          console.error("[NukeTracker] Error in event callback:", e);
        }
      });
    }
    /**
     * Get target player ID from a tile
     */
    getTargetPlayerID(gameAPI, targetTile) {
      if (!targetTile) return null;
      try {
        const owner = gameAPI.getOwner(targetTile);
        if (owner && typeof owner.isPlayer === "function" && owner.isPlayer()) {
          return owner.id();
        }
      } catch (e) {
      }
      return null;
    }
    /**
     * Detect new nuke launches (both incoming and outgoing)
     */
    detectLaunches(gameAPI, myPlayerID) {
      const nukeTypes = ["Atom Bomb", "Hydrogen Bomb", "MIRV", "MIRV Warhead"];
      const allNukes = gameAPI.getUnits(...nukeTypes);
      for (const nuke of allNukes) {
        try {
          const nukeId = typeof nuke.id === "function" ? nuke.id() : null;
          if (!nukeId || this.trackedNukes.has(nukeId)) continue;
          const owner = typeof nuke.owner === "function" ? nuke.owner() : null;
          const ownerID = owner && typeof owner.id === "function" ? owner.id() : "unknown";
          const targetTile = typeof nuke.targetTile === "function" ? nuke.targetTile() : null;
          const targetPlayerID = this.getTargetPlayerID(gameAPI, targetTile);
          const nukeType = typeof nuke.type === "function" ? nuke.type() : "Unknown";
          const isIncoming = targetPlayerID === myPlayerID;
          const isOutgoing = ownerID === myPlayerID;
          if (isIncoming || isOutgoing) {
            const tracked = {
              unitID: nukeId,
              type: nukeType,
              ownerID,
              ownerName: owner && typeof owner.name === "function" ? owner.name() : "Unknown",
              targetTile,
              targetPlayerID: targetPlayerID || "unknown",
              launchedTick: gameAPI.getTicks() || 0,
              hasReachedTarget: false,
              reported: false,
              isOutgoing
            };
            this.trackedNukes.set(nukeId, tracked);
            this.reportLaunch(tracked, gameAPI);
          }
        } catch (e) {
          console.error("[NukeTracker] Error processing nuke:", e);
        }
      }
    }
    /**
     * Detect nuke explosions or interceptions
     */
    detectExplosions(gameAPI) {
      for (const [unitID, tracked] of this.trackedNukes.entries()) {
        try {
          const unit = gameAPI.getUnit(unitID);
          if (!unit) {
            if (!tracked.reported) {
              const intercepted = !tracked.hasReachedTarget;
              this.reportExplosion(tracked, intercepted, gameAPI);
              tracked.reported = true;
            }
            this.trackedNukes.delete(unitID);
            continue;
          }
          const reachedTarget = typeof unit.reachedTarget === "function" ? unit.reachedTarget() : false;
          if (reachedTarget && !tracked.hasReachedTarget) {
            tracked.hasReachedTarget = true;
            this.reportExplosion(tracked, false, gameAPI);
            tracked.reported = true;
          }
          tracked.hasReachedTarget = reachedTarget;
        } catch (e) {
          console.error("[NukeTracker] Error checking nuke explosion:", e);
        }
      }
    }
    reportLaunch(tracked, gameAPI) {
      const coordinates = {
        x: gameAPI.getX(tracked.targetTile),
        y: gameAPI.getY(tracked.targetTile)
      };
      if (tracked.isOutgoing) {
        let nukeTypeShort = "atom";
        if (tracked.type.includes("Hydrogen")) {
          nukeTypeShort = "hydro";
        } else if (tracked.type.includes("MIRV")) {
          nukeTypeShort = "mirv";
        }
        const event = {
          type: "NUKE_LAUNCHED",
          timestamp: Date.now(),
          message: `${tracked.type} launched`,
          data: {
            nukeType: nukeTypeShort,
            nukeUnitID: tracked.unitID,
            targetTile: tracked.targetTile,
            targetPlayerID: tracked.targetPlayerID,
            tick: tracked.launchedTick,
            coordinates
          }
        };
        this.emitEvent(event);
        console.log("[NukeTracker] Player launched:", tracked.type, "at", coordinates);
      } else {
        let eventType = "ALERT_NUKE";
        let message = "Incoming nuclear strike detected!";
        if (tracked.type.includes("Hydrogen")) {
          eventType = "ALERT_HYDRO";
          message = "Incoming hydrogen bomb detected!";
        } else if (tracked.type.includes("MIRV")) {
          eventType = "ALERT_MIRV";
          message = "Incoming MIRV strike detected!";
        }
        const event = {
          type: eventType,
          timestamp: Date.now(),
          message,
          data: {
            nukeType: tracked.type,
            launcherPlayerID: tracked.ownerID,
            launcherPlayerName: tracked.ownerName,
            nukeUnitID: tracked.unitID,
            targetTile: tracked.targetTile,
            targetPlayerID: tracked.targetPlayerID,
            tick: tracked.launchedTick,
            coordinates
          }
        };
        this.emitEvent(event);
      }
    }
    reportExplosion(tracked, intercepted, gameAPI) {
      if (tracked.isOutgoing) {
        const eventType = intercepted ? "NUKE_INTERCEPTED" : "NUKE_EXPLODED";
        const event = {
          type: eventType,
          timestamp: Date.now(),
          message: intercepted ? `${tracked.type} intercepted` : `${tracked.type} exploded`,
          data: {
            nukeType: tracked.type,
            unitID: tracked.unitID,
            targetTile: tracked.targetTile,
            targetPlayerID: tracked.targetPlayerID,
            tick: gameAPI.getTicks() || 0,
            isOutgoing: true
          }
        };
        this.emitEvent(event);
        console.log("[NukeTracker] Player nuke", intercepted ? "intercepted" : "landed", ":", tracked.type);
      } else {
        const eventType = intercepted ? "NUKE_INTERCEPTED" : "NUKE_EXPLODED";
        const message = intercepted ? "Nuclear weapon intercepted" : "Nuclear weapon exploded";
        const event = {
          type: eventType,
          timestamp: Date.now(),
          message,
          data: {
            nukeType: tracked.type,
            unitID: tracked.unitID,
            ownerID: tracked.ownerID,
            ownerName: tracked.ownerName,
            targetTile: tracked.targetTile,
            tick: gameAPI.getTicks() || 0
          }
        };
        this.emitEvent(event);
        console.log("[NukeTracker] Incoming nuke", intercepted ? "intercepted" : "exploded", ":", tracked.type);
      }
    }
    clear() {
      this.trackedNukes.clear();
    }
  };

  // src/game/boat-tracker.ts
  var BoatTracker = class {
    constructor() {
      this.trackedBoats = /* @__PURE__ */ new Map();
      this.eventCallbacks = [];
    }
    onEvent(callback) {
      this.eventCallbacks.push(callback);
    }
    emitEvent(event) {
      this.eventCallbacks.forEach((cb) => {
        try {
          cb(event);
        } catch (e) {
          console.error("[BoatTracker] Error in event callback:", e);
        }
      });
    }
    /**
     * Get target player ID from a tile
     */
    getTargetPlayerID(gameAPI, targetTile) {
      if (!targetTile) return null;
      try {
        const owner = gameAPI.getOwner(targetTile);
        if (owner && typeof owner.isPlayer === "function" && owner.isPlayer()) {
          return owner.id();
        }
      } catch (e) {
      }
      return null;
    }
    /**
     * Detect new boat launches targeting the current player
     */
    detectLaunches(gameAPI, myPlayerID) {
      const transportShips = gameAPI.getUnits("Transport");
      for (const boat of transportShips) {
        try {
          const boatId = typeof boat.id === "function" ? boat.id() : null;
          if (!boatId || this.trackedBoats.has(boatId)) continue;
          const targetTile = typeof boat.targetTile === "function" ? boat.targetTile() : null;
          const targetPlayerID = this.getTargetPlayerID(gameAPI, targetTile);
          if (targetPlayerID === myPlayerID) {
            const owner = typeof boat.owner === "function" ? boat.owner() : null;
            const troops = typeof boat.troops === "function" ? boat.troops() : 0;
            const tracked = {
              unitID: boatId,
              ownerID: owner && typeof owner.id === "function" ? owner.id() : "unknown",
              ownerName: owner && typeof owner.name === "function" ? owner.name() : "Unknown",
              troops,
              targetTile,
              targetPlayerID,
              launchedTick: gameAPI.getTicks() || 0,
              hasReachedTarget: false,
              reported: false
            };
            this.trackedBoats.set(boatId, tracked);
            this.reportLaunch(tracked, gameAPI);
          }
        } catch (e) {
          console.error("[BoatTracker] Error processing boat:", e);
        }
      }
    }
    /**
     * Detect boat arrivals or destructions
     */
    detectArrivals(gameAPI) {
      for (const [unitID, tracked] of this.trackedBoats.entries()) {
        try {
          const unit = gameAPI.getUnit(unitID);
          if (!unit) {
            if (!tracked.reported) {
              this.reportArrival(tracked, true, gameAPI);
              tracked.reported = true;
            }
            this.trackedBoats.delete(unitID);
            continue;
          }
          const reachedTarget = typeof unit.reachedTarget === "function" ? unit.reachedTarget() : false;
          if (reachedTarget && !tracked.hasReachedTarget) {
            tracked.hasReachedTarget = true;
            this.reportArrival(tracked, false, gameAPI);
            tracked.reported = true;
          }
          tracked.hasReachedTarget = reachedTarget;
        } catch (e) {
          console.error("[BoatTracker] Error checking boat arrival:", e);
        }
      }
    }
    reportLaunch(tracked, gameAPI) {
      const coordinates = {
        x: gameAPI.getX(tracked.targetTile),
        y: gameAPI.getY(tracked.targetTile)
      };
      const event = {
        type: "ALERT_NAVAL",
        timestamp: Date.now(),
        message: "Naval invasion detected!",
        data: {
          type: "boat",
          attackerPlayerID: tracked.ownerID,
          attackerPlayerName: tracked.ownerName,
          transportShipUnitID: tracked.unitID,
          troops: tracked.troops,
          targetTile: tracked.targetTile,
          targetPlayerID: tracked.targetPlayerID,
          tick: tracked.launchedTick,
          coordinates
        }
      };
      this.emitEvent(event);
    }
    reportArrival(tracked, destroyed, gameAPI) {
      if (destroyed) {
        console.log("[BoatTracker] Boat destroyed:", tracked.unitID);
      } else {
        console.log("[BoatTracker] Boat arrived:", tracked.unitID);
      }
    }
    clear() {
      this.trackedBoats.clear();
    }
  };

  // src/game/land-tracker.ts
  var LandAttackTracker = class {
    constructor() {
      this.trackedAttacks = /* @__PURE__ */ new Map();
      this.eventCallbacks = [];
    }
    onEvent(callback) {
      this.eventCallbacks.push(callback);
    }
    emitEvent(event) {
      this.eventCallbacks.forEach((cb) => {
        try {
          cb(event);
        } catch (e) {
          console.error("[LandAttackTracker] Error in event callback:", e);
        }
      });
    }
    /**
     * Get player by smallID
     */
    getPlayerBySmallID(gameAPI, smallID) {
      if (!smallID || smallID === 0) return null;
      try {
        return gameAPI.getPlayerBySmallID(smallID);
      } catch (e) {
        return null;
      }
    }
    /**
     * Detect new land attack launches targeting the current player
     */
    detectLaunches(gameAPI) {
      const myPlayer = gameAPI.getMyPlayer();
      if (!myPlayer) return;
      try {
        const incomingAttacks = typeof myPlayer.incomingAttacks === "function" ? myPlayer.incomingAttacks() : [];
        const mySmallID = typeof myPlayer.smallID === "function" ? myPlayer.smallID() : null;
        if (!mySmallID) return;
        for (const attack of incomingAttacks) {
          try {
            if (attack.targetID === mySmallID && !this.trackedAttacks.has(attack.id)) {
              const attacker = this.getPlayerBySmallID(gameAPI, attack.attackerID);
              if (attacker) {
                const tracked = {
                  attackID: attack.id,
                  attackerID: attack.attackerID,
                  attackerPlayerID: typeof attacker.id === "function" ? attacker.id() : "unknown",
                  attackerPlayerName: typeof attacker.name === "function" ? attacker.name() : "Unknown",
                  troops: attack.troops || 0,
                  targetPlayerID: typeof myPlayer.id === "function" ? myPlayer.id() : "unknown",
                  launchedTick: gameAPI.getTicks() || 0,
                  retreating: attack.retreating || false,
                  reported: false
                };
                this.trackedAttacks.set(attack.id, tracked);
                this.reportLaunch(tracked, gameAPI);
              }
            }
          } catch (e) {
            console.error("[LandAttackTracker] Error processing attack:", e);
          }
        }
      } catch (e) {
        console.error("[LandAttackTracker] Error getting incoming attacks:", e);
      }
    }
    /**
     * Detect land attack completions (when attacks disappear from incomingAttacks)
     */
    detectCompletions(gameAPI) {
      const myPlayer = gameAPI.getMyPlayer();
      if (!myPlayer) return;
      try {
        const incomingAttacks = typeof myPlayer.incomingAttacks === "function" ? myPlayer.incomingAttacks() : [];
        const activeAttackIDs = new Set(incomingAttacks.map((a) => a.id));
        for (const [attackID, tracked] of this.trackedAttacks.entries()) {
          if (!activeAttackIDs.has(attackID)) {
            if (!tracked.reported) {
              const wasCancelled = tracked.retreating;
              this.reportComplete(tracked, wasCancelled, gameAPI);
              tracked.reported = true;
            }
            this.trackedAttacks.delete(attackID);
          } else {
            const currentAttack = incomingAttacks.find((a) => a.id === attackID);
            if (currentAttack) {
              tracked.retreating = currentAttack.retreating || false;
            }
          }
        }
      } catch (e) {
        console.error("[LandAttackTracker] Error checking attack completions:", e);
      }
    }
    reportLaunch(tracked, gameAPI) {
      const event = {
        type: "ALERT_LAND",
        timestamp: Date.now(),
        message: "Land invasion detected!",
        data: {
          type: "land",
          attackerPlayerID: tracked.attackerPlayerID,
          attackerPlayerName: tracked.attackerPlayerName,
          attackID: tracked.attackID,
          troops: tracked.troops,
          targetPlayerID: tracked.targetPlayerID,
          tick: tracked.launchedTick
        }
      };
      this.emitEvent(event);
    }
    reportComplete(tracked, wasCancelled, gameAPI) {
      if (wasCancelled) {
        console.log("[LandAttackTracker] Land attack cancelled:", tracked.attackID);
      } else {
        console.log("[LandAttackTracker] Land attack complete:", tracked.attackID);
      }
    }
    clear() {
      this.trackedAttacks.clear();
    }
  };

  // src/game/troop-monitor.ts
  var TroopMonitor = class {
    constructor(gameAPI, ws) {
      this.gameAPI = gameAPI;
      this.ws = ws;
      this.pollInterval = null;
      this.lastCurrentTroops = null;
      this.lastMaxTroops = null;
      this.lastAttackRatio = null;
      this.lastTroopsToSend = null;
      // Track localStorage for attack ratio changes
      this.storageListener = null;
    }
    /**
     * Start monitoring for changes
     * Polls at game tick rate (100ms) but only sends when values actually change
     */
    start() {
      if (this.pollInterval) return;
      console.log("[TroopMonitor] Starting change detection");
      this.pollInterval = window.setInterval(() => {
        this.checkForChanges();
      }, 100);
      this.storageListener = (event) => {
        if (event.key === "settings.attackRatio" && event.newValue !== event.oldValue) {
          console.log("[TroopMonitor] Attack ratio changed via localStorage");
          this.checkForChanges(true);
        }
      };
      window.addEventListener("storage", this.storageListener);
      this.interceptAttackRatioChanges();
      this.checkForChanges(true);
    }
    /**
     * Stop monitoring
     */
    stop() {
      if (this.pollInterval) {
        clearInterval(this.pollInterval);
        this.pollInterval = null;
      }
      if (this.storageListener) {
        window.removeEventListener("storage", this.storageListener);
        this.storageListener = null;
      }
      console.log("[TroopMonitor] Stopped");
    }
    /**
     * Check if any values have changed and send update if they have
     */
    checkForChanges(forceRatioUpdate = false) {
      if (!this.gameAPI.isValid()) {
        if (this.lastCurrentTroops !== null || this.lastMaxTroops !== null) {
          this.lastCurrentTroops = null;
          this.lastMaxTroops = null;
          this.lastAttackRatio = null;
          this.lastTroopsToSend = null;
        }
        return;
      }
      const currentTroops = this.gameAPI.getCurrentTroops();
      const maxTroops = this.gameAPI.getMaxTroops();
      const attackRatio = this.gameAPI.getAttackRatio();
      const troopsToSend = this.gameAPI.getTroopsToSend();
      const troopsChanged = currentTroops !== this.lastCurrentTroops;
      const maxChanged = maxTroops !== this.lastMaxTroops;
      const ratioChanged = attackRatio !== this.lastAttackRatio || forceRatioUpdate;
      const toSendChanged = troopsToSend !== this.lastTroopsToSend;
      if (troopsChanged || maxChanged || ratioChanged || toSendChanged) {
        const changes = [];
        if (troopsChanged) changes.push(`troops: ${this.lastCurrentTroops} \u2192 ${currentTroops}`);
        if (maxChanged) changes.push(`max: ${this.lastMaxTroops} \u2192 ${maxTroops}`);
        if (ratioChanged) changes.push(`ratio: ${this.lastAttackRatio} \u2192 ${attackRatio}`);
        if (toSendChanged) changes.push(`toSend: ${this.lastTroopsToSend} \u2192 ${troopsToSend}`);
        console.log("[TroopMonitor] Changes detected:", changes.join(", "));
        this.lastCurrentTroops = currentTroops;
        this.lastMaxTroops = maxTroops;
        this.lastAttackRatio = attackRatio;
        this.lastTroopsToSend = troopsToSend;
        this.sendUpdate();
      }
    }
    /**
     * Send current troop data to WebSocket
     */
    sendUpdate() {
      const data = {
        currentTroops: this.lastCurrentTroops !== null ? Math.floor(this.lastCurrentTroops / 10) : null,
        maxTroops: this.lastMaxTroops !== null ? Math.floor(this.lastMaxTroops / 10) : null,
        attackRatio: this.lastAttackRatio,
        attackRatioPercent: this.lastAttackRatio !== null ? Math.round(this.lastAttackRatio * 100) : null,
        troopsToSend: this.lastTroopsToSend !== null ? Math.floor(this.lastTroopsToSend / 10) : null,
        timestamp: Date.now()
      };
      this.ws.sendEvent("TROOP_UPDATE", "Troop data changed", data);
      console.log("[TroopMonitor] Sent update:", data);
    }
    /**
     * Watch DOM input element for attack ratio changes
     */
    interceptAttackRatioChanges() {
      const self = this;
      const attachInputListener = () => {
        const attackRatioInput = document.getElementById("attack-ratio");
        if (attackRatioInput) {
          console.log("[TroopMonitor] Found #attack-ratio input, attaching listeners");
          attackRatioInput.addEventListener("input", (event) => {
            const target = event.target;
            console.log("[TroopMonitor] \u2713 Attack ratio input changed:", target.value, "%");
            self.checkForChanges(true);
          });
          attackRatioInput.addEventListener("change", (event) => {
            const target = event.target;
            console.log("[TroopMonitor] \u2713 Attack ratio change completed:", target.value, "%");
            self.checkForChanges(true);
          });
        } else {
          setTimeout(attachInputListener, 500);
        }
      };
      attachInputListener();
    }
    /**
     * Get current troop data snapshot
     */
    getCurrentData() {
      return {
        currentTroops: this.lastCurrentTroops,
        maxTroops: this.lastMaxTroops,
        attackRatio: this.lastAttackRatio,
        attackRatioPercent: this.lastAttackRatio !== null ? Math.round(this.lastAttackRatio * 100) : null,
        troopsToSend: this.lastTroopsToSend
      };
    }
  };

  // src/game/openfront-bridge.ts
  var CONTROL_PANEL_SELECTOR = "control-panel";
  var POLL_INTERVAL_MS = 100;
  var GameBridge = class {
    constructor(ws, hud) {
      this.ws = ws;
      this.hud = hud;
      this.pollInterval = null;
      this.gameConnected = false;
      this.inGame = false;
      this.inSpawning = false;
      this.gameAPI = createGameAPI();
      this.hasProcessedWin = false;
      this.nukeTracker = new NukeTracker();
      this.boatTracker = new BoatTracker();
      this.landTracker = new LandAttackTracker();
      this.troopMonitor = new TroopMonitor(this.gameAPI, this.ws);
      this.nukeTracker.onEvent((event) => this.handleTrackerEvent(event));
      this.boatTracker.onEvent((event) => this.handleTrackerEvent(event));
      this.landTracker.onEvent((event) => this.handleTrackerEvent(event));
    }
    init() {
      waitForElement(CONTROL_PANEL_SELECTOR, () => {
        console.log("[GameBridge] Control panel detected, waiting for game instance...");
        this.waitForGameAndStart();
      });
    }
    waitForGameAndStart() {
      const checkInterval = setInterval(() => {
        const game = getGameView();
        if (game) {
          clearInterval(checkInterval);
          console.log("[GameBridge] Game instance detected, starting trackers");
          this.gameConnected = true;
          this.hud.setGameStatus(true);
          this.ws.sendEvent("INFO", "Game instance detected", { timestamp: Date.now() });
          this.startPolling();
        }
      }, 100);
    }
    pollForGameEnd(gameAPI) {
      try {
        const myPlayer = gameAPI.getMyPlayer();
        if (!myPlayer) return;
        const updates = gameAPI.getUpdatesSinceLastTick();
        if (updates && Object.keys(updates).length > 0) {
          const updateTypes = Object.keys(updates).filter((key) => updates[key] && updates[key].length > 0);
          if (updateTypes.length > 0) {
            console.log("[GameBridge] Update types this tick:", updateTypes.join(", "));
          }
        }
        if (updates && updates[13]) {
          const winUpdates = updates[13];
          if (winUpdates && winUpdates.length > 0) {
            console.log("[GameBridge] Win updates detected (count: " + winUpdates.length + "):", JSON.stringify(winUpdates));
            if (this.hasProcessedWin) {
              console.log("[GameBridge] Already processed win, skipping");
              return;
            }
            console.log("[GameBridge] Inspecting all Win updates:");
            winUpdates.forEach((update, index) => {
              console.log(`[GameBridge] Win update [${index}]:`, update);
              console.log(`[GameBridge] Win update [${index}] keys:`, Object.keys(update));
            });
            let winUpdate = null;
            let winnerType = null;
            let winnerId = null;
            for (const update of winUpdates) {
              if (update.winner) {
                [winnerType, winnerId] = update.winner;
                winUpdate = update;
                console.log("[GameBridge] Found winner in update.winner:", winnerType, winnerId);
                break;
              } else if (update.winnerType !== void 0 && update.winnerId !== void 0) {
                winnerType = update.winnerType;
                winnerId = update.winnerId;
                winUpdate = update;
                console.log("[GameBridge] Found winner in separate fields:", winnerType, winnerId);
                break;
              } else if (update.emoji && update.emoji.winner) {
                [winnerType, winnerId] = update.emoji.winner;
                winUpdate = update;
                console.log("[GameBridge] Found winner in emoji.winner:", winnerType, winnerId);
                break;
              } else if (update.team !== void 0) {
                winnerType = "team";
                winnerId = update.team;
                winUpdate = update;
                console.log("[GameBridge] Found winner in update.team:", winnerId);
                break;
              } else if (update.player !== void 0) {
                winnerType = "player";
                winnerId = update.player;
                winUpdate = update;
                console.log("[GameBridge] Found winner in update.player:", winnerId);
                break;
              } else if (update.emoji && update.emoji.recipientID !== void 0) {
                winnerType = "team";
                winnerId = update.emoji.recipientID;
                winUpdate = update;
                console.log("[GameBridge] Guessing winner from emoji.recipientID:", winnerId);
                break;
              }
            }
            console.log(`[GameBridge] Final extracted winner - type: ${winnerType}, id: ${winnerId}`);
            if (winnerType && winnerId !== null && winnerId !== void 0) {
              if (winnerType === "team") {
                const myTeam = myPlayer.team ? myPlayer.team() : null;
                const mySmallID = myPlayer.smallID ? myPlayer.smallID() : null;
                console.log(`[GameBridge] My team: ${myTeam}, My smallID: ${mySmallID}, Winner ID: ${winnerId}`);
                if (myTeam !== null) {
                  if (winnerId === myTeam) {
                    this.ws.sendEvent("GAME_END", "Your team won!", { victory: true, phase: "game-won", method: "team-victory", myTeam, winnerId });
                    if (this.hud.isSoundEnabled("game_victory")) {
                      this.ws.sendEvent("SOUND_PLAY", "Victory sound", { soundId: "game_victory", priority: "high" });
                    }
                    console.log("[GameBridge] \u2713 Your team won!");
                  } else {
                    this.ws.sendEvent("GAME_END", `Team ${winnerId} won`, { victory: false, phase: "game-lost", method: "team-defeat", myTeam, winnerId });
                    if (this.hud.isSoundEnabled("game_defeat")) {
                      this.ws.sendEvent("SOUND_PLAY", "Defeat sound", { soundId: "game_defeat", priority: "high" });
                    }
                    console.log(`[GameBridge] \u2717 Team ${winnerId} won (you are team ${myTeam})`);
                  }
                } else {
                  console.warn("[GameBridge] myPlayer.team() returned null, assuming defeat");
                  this.ws.sendEvent("GAME_END", `Team ${winnerId} won`, { victory: false, phase: "game-lost", method: "team-defeat-fallback", myTeam: null, winnerId });
                  if (this.hud.isSoundEnabled("game_defeat")) {
                    this.ws.sendEvent("SOUND_PLAY", "Defeat sound", { soundId: "game_defeat", priority: "high" });
                  }
                  console.log(`[GameBridge] \u2717 Team ${winnerId} won (unable to determine your team)`);
                }
              } else if (winnerType === "player") {
                const myClientID = myPlayer.clientID ? myPlayer.clientID() : null;
                const mySmallID = myPlayer.smallID ? myPlayer.smallID() : null;
                console.log(`[GameBridge] My clientID: ${myClientID}, My smallID: ${mySmallID}, Winner ID: ${winnerId}`);
                if (myClientID !== null && winnerId === myClientID) {
                  this.ws.sendEvent("GAME_END", "You won!", { victory: true, phase: "game-won", method: "solo-victory", myClientID, winnerId });
                  if (this.hud.isSoundEnabled("game_victory")) {
                    this.ws.sendEvent("SOUND_PLAY", "Victory sound", { soundId: "game_victory", priority: "high" });
                  }
                  console.log("[GameBridge] \u2713 You won!");
                } else {
                  this.ws.sendEvent("GAME_END", "Another player won", { victory: false, phase: "game-lost", method: "solo-defeat", myClientID, winnerId });
                  if (this.hud.isSoundEnabled("game_defeat")) {
                    this.ws.sendEvent("SOUND_PLAY", "Defeat sound", { soundId: "game_defeat", priority: "high" });
                  }
                  console.log("[GameBridge] \u2717 Another player won");
                }
              } else {
                console.warn("[GameBridge] Unknown winner type:", winnerType);
              }
              this.hasProcessedWin = true;
              this.inGame = false;
              this.inSpawning = false;
              return;
            } else {
              console.warn("[GameBridge] Win update has no winner field:", winUpdate);
            }
          }
        }
        if (this.hasProcessedWin || !this.inGame) return;
        const isAlive = myPlayer.isAlive ? myPlayer.isAlive() : true;
        const hasSpawned = myPlayer.hasSpawned ? myPlayer.hasSpawned() : false;
        const game = getGameView();
        const inSpawnPhase = game && game.inSpawnPhase ? game.inSpawnPhase() : false;
        if (!isAlive && !inSpawnPhase && hasSpawned) {
          this.ws.sendEvent("GAME_END", "You died", { victory: false, phase: "game-lost", reason: "death" });
          if (this.hud.isSoundEnabled("game_player_death")) {
            this.ws.sendEvent("SOUND_PLAY", "Player death sound", { soundId: "game_player_death", priority: "high" });
          }
          console.log("[GameBridge] \u2717 You died!");
          this.hasProcessedWin = true;
          this.inGame = false;
          this.inSpawning = false;
          return;
        }
        const winResult = gameAPI.didPlayerWin();
        if (winResult === null) return;
        if (winResult === true) {
          this.ws.sendEvent("GAME_END", "Victory!", { victory: true, phase: "game-won", method: "gameOver-fallback" });
          if (this.hud.isSoundEnabled("game_victory")) {
            this.ws.sendEvent("SOUND_PLAY", "Victory sound", { soundId: "game_victory", priority: "high" });
          }
          console.log("[GameBridge] \u2713 Game ended - VICTORY! (fallback method)");
        } else {
          this.ws.sendEvent("GAME_END", "Defeat", { victory: false, phase: "game-lost", method: "gameOver-fallback" });
          if (this.hud.isSoundEnabled("game_defeat")) {
            this.ws.sendEvent("SOUND_PLAY", "Defeat sound", { soundId: "game_defeat", priority: "high" });
          }
          console.log("[GameBridge] \u2717 Game ended - DEFEAT (fallback method)");
        }
        this.hasProcessedWin = true;
        this.inGame = false;
        this.inSpawning = false;
      } catch (error) {
        console.error("[GameBridge] Error polling game end:", error);
      }
    }
    startPolling() {
      if (this.pollInterval) return;
      const gameAPI = createGameAPI();
      this.pollInterval = window.setInterval(() => {
        if (!gameAPI.isValid()) {
          if (this.gameConnected) {
            this.gameConnected = false;
            this.hud.setGameStatus(false);
            this.clearTrackers();
          }
          return;
        }
        if (!this.gameConnected) {
          this.gameConnected = true;
          this.hud.setGameStatus(true);
        }
        this.pollForGameEnd(gameAPI);
        const myPlayerID = gameAPI.getMyPlayerID();
        if (!myPlayerID) {
          if (this.inGame || this.inSpawning) {
            this.inGame = false;
            this.inSpawning = false;
            this.hasProcessedWin = false;
            console.log("[GameBridge] Player no longer in game");
          }
          this.clearTrackers();
          return;
        }
        const gameStarted = gameAPI.isGameStarted();
        if (!this.inSpawning && !this.inGame && gameStarted === false) {
          this.inSpawning = true;
          this.ws.sendEvent("GAME_SPAWNING", "Spawn countdown active", { spawning: true });
          console.log("[GameBridge] Spawning phase - countdown active");
          this.clearTrackers();
        }
        if (gameStarted === true && !this.inGame && !this.hasProcessedWin) {
          this.inGame = true;
          this.inSpawning = false;
          this.ws.sendEvent("GAME_START", "Game started - countdown ended");
          if (this.hud.isSoundEnabled("game_start")) {
            this.ws.sendEvent("SOUND_PLAY", "Play game start sound", {
              soundId: "game_start",
              priority: "high",
              interrupt: true
            });
          }
          console.log("[GameBridge] Game started - spawn countdown ended");
          this.clearTrackers();
          this.troopMonitor.start();
        }
        try {
          this.nukeTracker.detectLaunches(gameAPI, myPlayerID);
          this.nukeTracker.detectExplosions(gameAPI);
          this.boatTracker.detectLaunches(gameAPI, myPlayerID);
          this.boatTracker.detectArrivals(gameAPI);
          this.landTracker.detectLaunches(gameAPI);
          this.landTracker.detectCompletions(gameAPI);
        } catch (e) {
          console.error("[GameBridge] Error in polling loop:", e);
        }
      }, POLL_INTERVAL_MS);
    }
    handleTrackerEvent(event) {
      this.ws.sendEvent(event.type, event.message || "", event.data);
    }
    clearTrackers() {
      this.nukeTracker.clear();
      this.boatTracker.clear();
      this.landTracker.clear();
      this.troopMonitor.stop();
    }
    handleCommand(action, params) {
      console.log("[GameBridge] Received command:", action, params);
      if (action === "send-nuke") {
        this.handleSendNuke(params);
      } else if (action === "set-attack-ratio") {
        this.handleSetAttackRatio(params);
      } else if (action === "ping") {
        console.log("[GameBridge] Ping received");
      } else {
        console.warn("[GameBridge] Unknown command:", action);
        this.ws.sendEvent("ERROR", `Unknown command: ${action}`, { action, params });
      }
    }
    handleSetAttackRatio(params) {
      const ratio = params == null ? void 0 : params.ratio;
      if (typeof ratio !== "number" || ratio < 0 || ratio > 1) {
        console.error("[GameBridge] set-attack-ratio command missing or invalid ratio parameter (expected 0-1)");
        this.ws.sendEvent("ERROR", "set-attack-ratio failed: invalid ratio", { params });
        return;
      }
      const percentage = Math.round(ratio * 100);
      console.log(`[GameBridge] Setting attack ratio to ${ratio} (${percentage}%)`);
      const attackRatioInput = document.getElementById("attack-ratio");
      if (attackRatioInput) {
        attackRatioInput.value = percentage.toString();
        attackRatioInput.dispatchEvent(new Event("input", { bubbles: true }));
        attackRatioInput.dispatchEvent(new Event("change", { bubbles: true }));
        console.log("[GameBridge] \u2713 Attack ratio slider updated to", percentage, "%");
        this.ws.sendEvent("INFO", "Attack ratio updated", { ratio, percentage });
        setTimeout(() => {
          this.troopMonitor["checkForChanges"](true);
        }, 100);
      } else {
        console.error("[GameBridge] #attack-ratio input not found");
        this.ws.sendEvent("ERROR", "set-attack-ratio failed: input not found", { ratio });
      }
    }
    handleSendNuke(params) {
      const nukeType = params == null ? void 0 : params.nukeType;
      if (!nukeType) {
        console.error("[GameBridge] send-nuke command missing nukeType parameter");
        this.ws.sendEvent("ERROR", "send-nuke failed: missing nukeType", { params });
        return;
      }
      let unitTypeName;
      if (nukeType === "atom") {
        unitTypeName = "Atom Bomb";
      } else if (nukeType === "hydro") {
        unitTypeName = "Hydrogen Bomb";
      } else if (nukeType === "mirv") {
        unitTypeName = "MIRV";
      } else {
        console.error("[GameBridge] Invalid nuke type:", nukeType);
        this.ws.sendEvent("ERROR", "send-nuke failed: invalid nuke type", { nukeType });
        return;
      }
      console.log(`[GameBridge] Preparing to launch ${unitTypeName}`);
      try {
        const buildMenu = document.querySelector("build-menu");
        if (!buildMenu || !buildMenu.game || !buildMenu.eventBus) {
          throw new Error("Build menu, game instance, or event bus not available");
        }
        const game = buildMenu.game;
        const eventBus = buildMenu.eventBus;
        const myPlayer = game.myPlayer();
        if (!myPlayer) {
          throw new Error("No player found - you may not have spawned yet");
        }
        console.log("[GameBridge] Getting buildable units for", unitTypeName);
        myPlayer.actions(null).then((actions) => {
          const buildableUnit = actions.buildableUnits.find((bu) => bu.type === unitTypeName);
          if (!buildableUnit) {
            console.error("[GameBridge] Buildable unit not found for type:", unitTypeName);
            console.log("[GameBridge] Available units:", actions.buildableUnits.map((bu) => bu.type));
            this.ws.sendEvent("ERROR", "send-nuke failed: unit not available", { nukeType, unitTypeName });
            return;
          }
          if (!buildableUnit.canBuild) {
            console.error("[GameBridge] Cannot build unit - no missile silo available or insufficient gold");
            this.ws.sendEvent("ERROR", "send-nuke failed: cannot build (need missile silo and gold)", { nukeType });
            return;
          }
          console.log("[GameBridge] Found buildable unit:", unitTypeName, "can build from:", buildableUnit.canBuild);
          const controlPanel = document.querySelector("control-panel");
          if (!controlPanel || !controlPanel.uiState) {
            throw new Error("Control panel or UIState not available");
          }
          console.log("[GameBridge] \u{1F3AF} Activating ghost structure targeting mode for:", unitTypeName);
          controlPanel.uiState.ghostStructure = unitTypeName;
          class GhostStructureChangedEvent {
            constructor(ghostStructure) {
              this.ghostStructure = ghostStructure;
            }
          }
          eventBus.emit(new GhostStructureChangedEvent(unitTypeName));
          console.log("[GameBridge] \u2713 Ghost targeting mode activated - click on the map to select target");
          this.ws.sendEvent("INFO", `${unitTypeName} targeting mode activated`, {
            nukeType,
            unitTypeName,
            message: "Click on a tile on the map to target the nuke (ghost structure active)",
            timestamp: Date.now()
          });
          const checkLaunchInterval = setInterval(() => {
            if (controlPanel.uiState.ghostStructure === null) {
              clearInterval(checkLaunchInterval);
              console.log("[GameBridge] \u2713 Nuke targeting completed, command sent");
              this.ws.sendEvent("INFO", `${unitTypeName} launch command completed`, {
                nukeType,
                unitTypeName,
                timestamp: Date.now()
              });
            }
          }, 100);
        }).catch((err) => {
          console.error("[GameBridge] Failed to get player actions:", err);
          this.ws.sendEvent("ERROR", "send-nuke failed: could not get player actions", {
            nukeType,
            error: String(err)
          });
        });
      } catch (e) {
        console.error("[GameBridge] Failed to open build menu:", e);
        this.ws.sendEvent("ERROR", "send-nuke failed: could not open build menu", {
          nukeType,
          error: String(e)
        });
      }
    }
    stop() {
      if (this.pollInterval) {
        clearInterval(this.pollInterval);
        this.pollInterval = null;
      }
      this.clearTrackers();
      this.gameConnected = false;
      this.hud.setGameStatus(false);
    }
  };

  // src/storage/config.ts
  var DEFAULT_WS_URL = PROTOCOL_CONSTANTS.DEFAULT_WS_URL;
  var STORAGE_KEY_WS_URL = "ots-ws-url";
  function loadWsUrl() {
    const saved = GM_getValue(STORAGE_KEY_WS_URL, null);
    if (typeof saved === "string" && saved.trim()) {
      return saved;
    }
    return DEFAULT_WS_URL;
  }
  function saveWsUrl(url) {
    GM_setValue(STORAGE_KEY_WS_URL, url);
  }

  // src/main.user.ts
  var VERSION = "2025-12-20.1";
  (function start() {
    console.log(`[OTS Userscript] Version ${VERSION}`);
    let currentWsUrl = loadWsUrl();
    let ws;
    const hud = new Hud(
      () => currentWsUrl,
      (url) => {
        currentWsUrl = url;
        saveWsUrl(url);
      },
      () => {
        ws.disconnect(4100, "URL changed");
        ws.connect();
      },
      (action, params) => {
        if (ws) {
          ws.sendCommand(action, params);
        }
      },
      (soundId) => {
        if (ws) {
          ws.sendEvent("SOUND_PLAY", `Test sound: ${soundId}`, {
            soundId,
            priority: "high",
            test: true
          });
        }
      }
    );
    let game = null;
    ws = new WsClient(
      hud,
      () => currentWsUrl,
      (action, params) => {
        if (game) {
          game.handleCommand(action, params);
        }
      }
    );
    game = new GameBridge(ws, hud);
    function initialize() {
      console.log("[OTS Userscript] Initializing...");
      hud.ensure();
      ws.connect();
      game.init();
      window.otsShowHud = () => hud.ensure();
      window.otsWsClient = ws;
      window.otsGameBridge = game;
    }
    if (document.readyState === "loading") {
      document.addEventListener("DOMContentLoaded", initialize);
    } else {
      initialize();
    }
  })();
})();
