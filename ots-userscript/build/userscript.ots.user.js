// ==UserScript==
// @name         OTS Game Dashboard Bridge
// @namespace    http://tampermonkey.net/
// @version      0.1.0
// @description  Send game state and events to local Nuxt dashboard
// @author       [PUSH] DUCKDUCK
// @match        https://openfront.io/*
// @grant        GM_getValue
// @grant        GM_setValue
// ==/UserScript==

"use strict";
(() => {
  // src/hud/window.ts
  var BaseWindow = class {
    constructor(root) {
      this.dragHandle = null;
      this.root = root;
    }
    attachDrag(handle, onMove) {
      this.dragHandle = handle;
      let dragging = false;
      let startX = 0;
      let startY = 0;
      let startLeft = 0;
      let startTop = 0;
      handle.addEventListener("mousedown", (ev) => {
        dragging = true;
        startX = ev.clientX;
        startY = ev.clientY;
        const rect = this.root.getBoundingClientRect();
        startLeft = rect.left;
        startTop = rect.top;
        ev.preventDefault();
      });
      window.addEventListener("mousemove", (ev) => {
        if (!dragging) return;
        const dx = ev.clientX - startX;
        const dy = ev.clientY - startY;
        const nextLeft = startLeft + dx;
        const nextTop = startTop + dy;
        this.root.style.left = `${nextLeft}px`;
        this.root.style.top = `${nextTop}px`;
        this.root.style.right = "auto";
        if (onMove) onMove(nextLeft, nextTop);
      });
      window.addEventListener("mouseup", () => {
        dragging = false;
      });
    }
  };
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

  // src/hud/main-hud.ts
  var STORAGE_KEY_HUD_POS = "ots-hud-pos";
  var STORAGE_KEY_HUD_SIZE = "ots-hud-size";
  var STORAGE_KEY_LOG_FILTERS = "ots-hud-log-filters";
  var Hud = class {
    constructor(getWsUrl, setWsUrl, onWsUrlChanged) {
      this.getWsUrl = getWsUrl;
      this.setWsUrl = setWsUrl;
      this.onWsUrlChanged = onWsUrlChanged;
      this.root = null;
      this.closed = false;
      this.logList = null;
      this.wsDot = null;
      this.gameDot = null;
      this.body = null;
      this.settingsPanel = null;
      this.settingsInput = null;
      this.filterPanel = null;
      this.filterCheckboxes = {
        directions: { send: null, recv: null, info: null },
        events: { game: null, nukes: null, alerts: null, troops: null, system: null }
      };
      this.logFilters = {
        directions: { send: true, recv: true, info: true },
        events: { game: true, nukes: true, alerts: true, troops: true, system: true }
      };
      this.logCounter = 0;
      this.autoScroll = true;
      this.autoScrollBtn = null;
    }
    ensure() {
      if (this.closed || this.root) return;
      const root = document.createElement("div");
      root.id = "ots-userscript-hud";
      root.style.position = "fixed";
      root.style.top = "8px";
      root.style.right = "8px";
      root.style.zIndex = "2147483647";
      root.style.fontFamily = 'system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif';
      root.style.color = "#e5e7eb";
      root.innerHTML = `<div id="ots-hud-inner" style="width: 320px; height: 260px; max-height: 60vh; background: rgba(15,23,42,0.96); border-radius: 8px; box-shadow: 0 8px 20px rgba(0,0,0,0.5); border: 1px solid rgba(148,163,184,0.5); display: flex; flex-direction: column; overflow: hidden; position: relative;">
      <div id="ots-hud-header" style="cursor: move; padding: 6px 8px; display: flex; align-items: center; justify-content: space-between; gap: 6px; background: rgba(15,23,42,0.98); border-bottom: 1px solid rgba(30,64,175,0.7);">
        <div style="display:flex;align-items:center;gap:6px;min-width:0;">
          <span style="display:inline-flex;align-items:center;gap:4px;padding:2px 6px;border-radius:999px;border:1px solid rgba(148,163,184,0.7);background:rgba(15,23,42,0.9);font-size:9px;font-weight:600;letter-spacing:0.09em;text-transform:uppercase;color:#e5e7eb;white-space:nowrap;">
            <span id="ots-hud-ws-dot" style="display:inline-flex;width:8px;height:8px;border-radius:999px;background:#f97373;"></span>
            <span>WS</span>
          </span>
          <span style="display:inline-flex;align-items:center;gap:4px;padding:2px 6px;border-radius:999px;border:1px solid rgba(148,163,184,0.7);background:rgba(15,23,42,0.05);font-size:9px;font-weight:600;letter-spacing:0.09em;text-transform:uppercase;color:#e5e7eb;white-space:nowrap;">
            <span id="ots-hud-game-dot" style="display:inline-flex;width:8px;height:8px;border-radius:999px;background:#f97373;"></span>
            <span>GAME</span>
          </span>
          <span style="font-size:11px;font-weight:600;letter-spacing:0.08em;text-transform:uppercase;color:#9ca3af;white-space:nowrap;text-overflow:ellipsis;overflow:hidden;">OTS LINK</span>
        </div>
        <div style="display:flex;align-items:center;gap:4px;">
          <button id="ots-hud-toggle" style="all:unset;cursor:pointer;font-size:11px;color:#9ca3af;padding:2px 4px;border-radius:4px;border:1px solid rgba(148,163,184,0.6);background:rgba(15,23,42,0.9);">\u2013</button>
          <button id="ots-hud-settings" style="all:unset;cursor:pointer;font-size:11px;color:#9ca3af;padding:2px 4px;border-radius:4px;border:1px solid rgba(148,163,184,0.6);background:rgba(15,23,42,0.9);">\u2699</button>
          <button id="ots-hud-close" style="all:unset;cursor:pointer;font-size:11px;color:#6b7280;padding:2px 4px;border-radius:4px;">\u2715</button>
        </div>
      </div>
      <div id="ots-hud-body" style="display:flex;flex-direction:column;flex:1 1 auto;min-height:0;">
        <div id="ots-hud-log" style="flex:1 1 auto; padding: 4px 6px 6px; overflow-y:auto; overflow-x:hidden; font-size:11px; line-height:1.3; background:rgba(15,23,42,0.98);">
        </div>
        <div id="ots-hud-footer" style="padding:4px 8px;display:flex;align-items:center;justify-content:space-between;background:rgba(15,23,42,0.98);border-top:1px solid rgba(30,64,175,0.7);">
          <button id="ots-hud-filter" style="all:unset;cursor:pointer;font-size:11px;color:#9ca3af;padding:2px 6px;border-radius:4px;border:1px solid rgba(148,163,184,0.6);background:rgba(15,23,42,0.9);">\u2691 Filter</button>
          <button id="ots-hud-autoscroll" style="all:unset;cursor:pointer;font-size:9px;font-weight:600;color:#6ee7b7;padding:2px 6px;border-radius:999px;border:1px solid rgba(52,211,153,0.6);background:rgba(52,211,153,0.18);white-space:nowrap;letter-spacing:0.05em;">\u2713 Auto</button>
        </div>
      </div>
      <div id="ots-hud-settings-panel" style="position:fixed;top:64px;right:16px;min-width:260px;max-width:320px;display:none;flex-direction:column;gap:6px;padding:8px;border-radius:6px;background:rgba(15,23,42,0.98);border:1px solid rgba(59,130,246,0.6);box-shadow:0 8px 20px rgba(0,0,0,0.5);z-index:2147483647;">
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
      </div>
      <div id="ots-hud-filter-panel" style="position:fixed;top:64px;right:16px;min-width:220px;max-width:280px;display:none;flex-direction:column;gap:8px;padding:10px;border-radius:6px;background:rgba(15,23,42,0.98);border:1px solid rgba(59,130,246,0.6);box-shadow:0 8px 20px rgba(0,0,0,0.5);z-index:2147483647;">
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
          <label style="display:flex;align-items:center;gap:6px;font-size:11px;color:#e5e7eb;cursor:pointer;">
            <input type="checkbox" id="ots-hud-filter-system" checked style="cursor:pointer;" />
            <span style="color:#94a3b8;">System Events</span>
          </label>
        </div>
      </div>
      <div id="ots-hud-resize" style="position:absolute;right:2px;bottom:2px;width:12px;height:12px;cursor:se-resize;opacity:0.7;">
        <div style="position:absolute;right:1px;bottom:1px;width:10px;height:2px;background:rgba(148,163,184,0.9);transform:rotate(45deg);"></div>
        <div style="position:absolute;right:3px;bottom:3px;width:8px;height:2px;background:rgba(148,163,184,0.7);transform:rotate(45deg);"></div>
      </div>
    </div>`;
      document.body.appendChild(root);
      this.root = root;
      this.logList = root.querySelector("#ots-hud-log");
      this.body = root.querySelector("#ots-hud-body");
      const footer = root.querySelector("#ots-hud-footer");
      this.wsDot = root.querySelector("#ots-hud-ws-dot");
      this.gameDot = root.querySelector("#ots-hud-game-dot");
      this.settingsPanel = root.querySelector("#ots-hud-settings-panel");
      this.settingsInput = root.querySelector("#ots-hud-settings-ws");
      this.filterPanel = root.querySelector("#ots-hud-filter-panel");
      this.autoScrollBtn = root.querySelector("#ots-hud-autoscroll");
      this.filterCheckboxes.directions.send = root.querySelector("#ots-hud-filter-send");
      this.filterCheckboxes.directions.recv = root.querySelector("#ots-hud-filter-recv");
      this.filterCheckboxes.directions.info = root.querySelector("#ots-hud-filter-info");
      this.filterCheckboxes.events.game = root.querySelector("#ots-hud-filter-game");
      this.filterCheckboxes.events.nukes = root.querySelector("#ots-hud-filter-nukes");
      this.filterCheckboxes.events.alerts = root.querySelector("#ots-hud-filter-alerts");
      this.filterCheckboxes.events.troops = root.querySelector("#ots-hud-filter-troops");
      this.filterCheckboxes.events.system = root.querySelector("#ots-hud-filter-system");
      const savedFilters = GM_getValue(STORAGE_KEY_LOG_FILTERS, null);
      if (savedFilters) {
        this.logFilters = savedFilters;
        if (this.filterCheckboxes.directions.send) this.filterCheckboxes.directions.send.checked = savedFilters.directions.send;
        if (this.filterCheckboxes.directions.recv) this.filterCheckboxes.directions.recv.checked = savedFilters.directions.recv;
        if (this.filterCheckboxes.directions.info) this.filterCheckboxes.directions.info.checked = savedFilters.directions.info;
        if (this.filterCheckboxes.events.game) this.filterCheckboxes.events.game.checked = savedFilters.events.game;
        if (this.filterCheckboxes.events.nukes) this.filterCheckboxes.events.nukes.checked = savedFilters.events.nukes;
        if (this.filterCheckboxes.events.alerts) this.filterCheckboxes.events.alerts.checked = savedFilters.events.alerts;
        if (this.filterCheckboxes.events.troops) this.filterCheckboxes.events.troops.checked = savedFilters.events.troops;
        if (this.filterCheckboxes.events.system) this.filterCheckboxes.events.system.checked = savedFilters.events.system;
      }
      const header = root.querySelector("#ots-hud-header");
      const settingsHeader = root.querySelector("#ots-hud-settings-header");
      const filterHeader = root.querySelector("#ots-hud-filter-header");
      const inner = root.querySelector("#ots-hud-inner");
      const savedPos = GM_getValue(STORAGE_KEY_HUD_POS, null);
      if (savedPos && typeof savedPos.left === "number" && typeof savedPos.top === "number") {
        root.style.left = `${savedPos.left}px`;
        root.style.top = `${savedPos.top}px`;
        root.style.right = "auto";
      }
      const savedSize = GM_getValue(STORAGE_KEY_HUD_SIZE, null);
      if (savedSize && typeof savedSize.width === "number" && typeof savedSize.height === "number") {
        inner.style.width = `${savedSize.width}px`;
        inner.style.height = `${savedSize.height}px`;
      }
      const resizeHandle = root.querySelector("#ots-hud-resize");
      const toggleBtn = root.querySelector("#ots-hud-toggle");
      const autoScrollBtn = root.querySelector("#ots-hud-autoscroll");
      const filterBtn = root.querySelector("#ots-hud-filter");
      const filterCloseBtn = root.querySelector("#ots-hud-filter-close");
      const settingsBtn = root.querySelector("#ots-hud-settings");
      const settingsCloseBtn = root.querySelector("#ots-hud-settings-close");
      const settingsSaveBtn = root.querySelector("#ots-hud-settings-save");
      const settingsResetBtn = root.querySelector("#ots-hud-settings-reset");
      const closeBtn = root.querySelector("#ots-hud-close");
      const hudWindow = new BaseWindow(root);
      hudWindow.attachDrag(header, (left, top) => {
        const pos = { left, top };
        GM_setValue(STORAGE_KEY_HUD_POS, pos);
      });
      if (this.settingsPanel && settingsHeader) {
        const settingsWindow = new FloatingPanel(this.settingsPanel);
        settingsWindow.attachDrag(settingsHeader);
      }
      if (this.filterPanel && filterHeader) {
        const filterWindow = new FloatingPanel(this.filterPanel);
        filterWindow.attachDrag(filterHeader);
      }
      setupResize(inner, resizeHandle);
      if (this.logList) {
        this.logList.addEventListener("scroll", () => {
          if (!this.logList) return;
          const isAtBottom = this.logList.scrollHeight - this.logList.scrollTop - this.logList.clientHeight < 50;
          this.autoScroll = isAtBottom;
          this.updateAutoScrollButton();
        });
      }
      autoScrollBtn.addEventListener("click", () => {
        this.autoScroll = true;
        this.updateAutoScrollButton();
        this.scrollLogToBottom();
      });
      let collapsed = true;
      let savedHeight = (savedSize == null ? void 0 : savedSize.height) || 260;
      let savedWidth = (savedSize == null ? void 0 : savedSize.width) || 320;
      if (this.body) {
        this.body.style.display = "none";
        footer.style.display = "none";
        inner.style.height = "32px";
        inner.style.width = "250px";
        toggleBtn.textContent = "+";
      }
      toggleBtn.addEventListener("click", () => {
        if (!this.body) return;
        collapsed = !collapsed;
        this.body.style.display = collapsed ? "none" : "flex";
        footer.style.display = collapsed ? "none" : "flex";
        if (collapsed) {
          const currentHeight = parseInt(inner.style.height || "260");
          const currentWidth = parseInt(inner.style.width || "320");
          if (currentHeight > 32) {
            savedHeight = currentHeight;
          }
          if (currentWidth > 250) {
            savedWidth = currentWidth;
          }
          inner.style.height = "32px";
          inner.style.width = "250px";
        } else {
          const currentTop = parseInt(root.style.top || "8");
          const currentBottom = currentTop + 32;
          const expandedBottom = currentTop + savedHeight;
          const viewportHeight = window.innerHeight;
          if (expandedBottom > viewportHeight) {
            const newTop = Math.max(8, viewportHeight - savedHeight - 8);
            root.style.top = `${newTop}px`;
            GM_setValue(STORAGE_KEY_HUD_POS, {
              top: newTop,
              left: parseInt(root.style.left || "8")
            });
          }
          inner.style.height = `${savedHeight}px`;
          inner.style.width = `${savedWidth}px`;
        }
        toggleBtn.textContent = collapsed ? "+" : "\u2013";
      });
      filterBtn.addEventListener("click", () => {
        if (!this.filterPanel) return;
        this.filterPanel.style.display = "flex";
      });
      filterCloseBtn.addEventListener("click", () => {
        if (this.filterPanel) this.filterPanel.style.display = "none";
      });
      if (this.filterCheckboxes.directions.send) {
        this.filterCheckboxes.directions.send.addEventListener("change", (e) => {
          this.logFilters.directions.send = e.target.checked;
          GM_setValue(STORAGE_KEY_LOG_FILTERS, this.logFilters);
          this.applyFiltersToLog();
        });
      }
      if (this.filterCheckboxes.directions.recv) {
        this.filterCheckboxes.directions.recv.addEventListener("change", (e) => {
          this.logFilters.directions.recv = e.target.checked;
          GM_setValue(STORAGE_KEY_LOG_FILTERS, this.logFilters);
          this.applyFiltersToLog();
        });
      }
      if (this.filterCheckboxes.directions.info) {
        this.filterCheckboxes.directions.info.addEventListener("change", (e) => {
          this.logFilters.directions.info = e.target.checked;
          GM_setValue(STORAGE_KEY_LOG_FILTERS, this.logFilters);
          this.applyFiltersToLog();
        });
      }
      if (this.filterCheckboxes.events.game) {
        this.filterCheckboxes.events.game.addEventListener("change", (e) => {
          this.logFilters.events.game = e.target.checked;
          GM_setValue(STORAGE_KEY_LOG_FILTERS, this.logFilters);
          this.applyFiltersToLog();
        });
      }
      if (this.filterCheckboxes.events.nukes) {
        this.filterCheckboxes.events.nukes.addEventListener("change", (e) => {
          this.logFilters.events.nukes = e.target.checked;
          GM_setValue(STORAGE_KEY_LOG_FILTERS, this.logFilters);
          this.applyFiltersToLog();
        });
      }
      if (this.filterCheckboxes.events.alerts) {
        this.filterCheckboxes.events.alerts.addEventListener("change", (e) => {
          this.logFilters.events.alerts = e.target.checked;
          GM_setValue(STORAGE_KEY_LOG_FILTERS, this.logFilters);
          this.applyFiltersToLog();
        });
      }
      if (this.filterCheckboxes.events.troops) {
        this.filterCheckboxes.events.troops.addEventListener("change", (e) => {
          this.logFilters.events.troops = e.target.checked;
          GM_setValue(STORAGE_KEY_LOG_FILTERS, this.logFilters);
          this.applyFiltersToLog();
        });
      }
      if (this.filterCheckboxes.events.system) {
        this.filterCheckboxes.events.system.addEventListener("change", (e) => {
          this.logFilters.events.system = e.target.checked;
          GM_setValue(STORAGE_KEY_LOG_FILTERS, this.logFilters);
          this.applyFiltersToLog();
        });
      }
      settingsBtn.addEventListener("click", () => {
        if (!this.settingsPanel || !this.settingsInput) return;
        this.settingsPanel.style.display = "flex";
        this.settingsInput.value = this.getWsUrl();
        this.settingsInput.focus();
      });
      settingsCloseBtn.addEventListener("click", () => {
        if (this.settingsPanel) this.settingsPanel.style.display = "none";
      });
      settingsResetBtn.addEventListener("click", () => {
        if (!this.settingsInput) return;
        this.settingsInput.value = this.getWsUrl();
      });
      settingsSaveBtn.addEventListener("click", () => {
        if (!this.settingsInput) return;
        const value = this.settingsInput.value.trim();
        if (!value) return;
        this.setWsUrl(value);
        this.pushLog("info", `WS URL updated to ${value}`);
        if (this.settingsPanel) this.settingsPanel.style.display = "none";
        this.onWsUrlChanged();
      });
      closeBtn.addEventListener("click", () => {
        root.remove();
        this.root = null;
        this.logList = null;
        this.wsDot = null;
        this.gameDot = null;
        this.closed = true;
      });
    }
    setWsStatus(status) {
      this.ensure();
      if (!this.wsDot) return;
      let color = "#f97373";
      if (status === "CONNECTING") color = "#fbbf24";
      if (status === "OPEN") color = "#4ade80";
      if (status === "ERROR") color = "#f97373";
      this.wsDot.style.background = color;
    }
    setGameStatus(connected) {
      this.ensure();
      if (!this.gameDot) return;
      this.gameDot.style.background = connected ? "#4ade80" : "#f97373";
    }
    updateAutoScrollButton() {
      if (!this.autoScrollBtn) return;
      if (this.autoScroll) {
        this.autoScrollBtn.textContent = "\u2713 Auto";
        this.autoScrollBtn.style.color = "#6ee7b7";
        this.autoScrollBtn.style.background = "rgba(52,211,153,0.18)";
        this.autoScrollBtn.style.borderColor = "rgba(52,211,153,0.6)";
      } else {
        this.autoScrollBtn.textContent = "\u23F8 Paused";
        this.autoScrollBtn.style.color = "#fbbf24";
        this.autoScrollBtn.style.background = "rgba(251,191,36,0.18)";
        this.autoScrollBtn.style.borderColor = "rgba(251,191,36,0.6)";
      }
    }
    scrollLogToBottom() {
      if (!this.logList) return;
      this.logList.scrollTop = this.logList.scrollHeight;
    }
    getEventCategory(eventType) {
      if (!eventType) return "system";
      const gameEvents = ["GAME_START", "GAME_END", "GAME_SPAWNING"];
      const nukeEvents = ["NUKE_LAUNCHED", "NUKE_EXPLODED", "NUKE_INTERCEPTED"];
      const alertEvents = ["ALERT_NUKE", "ALERT_HYDRO", "ALERT_MIRV", "ALERT_LAND", "ALERT_NAVAL"];
      const troopEvents = ["TROOP_UPDATE"];
      const systemEvents = ["INFO", "ERROR", "HARDWARE_TEST"];
      if (gameEvents.includes(eventType)) return "game";
      if (nukeEvents.includes(eventType)) return "nukes";
      if (alertEvents.includes(eventType)) return "alerts";
      if (troopEvents.includes(eventType)) return "troops";
      if (systemEvents.includes(eventType)) return "system";
      return "system";
    }
    pushLog(direction, text, eventType) {
      this.ensure();
      if (!this.logList) return;
      if (!this.logFilters.directions[direction]) {
        return;
      }
      const eventCategory = this.getEventCategory(eventType);
      if (!this.logFilters.events[eventCategory]) {
        return;
      }
      const entry = {
        id: ++this.logCounter,
        ts: Date.now(),
        direction,
        text
      };
      const line = document.createElement("div");
      line.style.display = "flex";
      line.style.gap = "4px";
      line.style.marginBottom = "2px";
      line.setAttribute("data-log-direction", direction);
      line.setAttribute("data-log-category", eventCategory);
      const dirSpan = document.createElement("span");
      dirSpan.textContent = direction.toUpperCase();
      dirSpan.style.fontSize = "9px";
      dirSpan.style.fontWeight = "600";
      dirSpan.style.letterSpacing = "0.08em";
      dirSpan.style.textTransform = "uppercase";
      dirSpan.style.padding = "1px 4px";
      dirSpan.style.borderRadius = "999px";
      if (direction === "send") {
        dirSpan.style.background = "rgba(52,211,153,0.18)";
        dirSpan.style.color = "#6ee7b7";
      } else if (direction === "recv") {
        dirSpan.style.background = "rgba(59,130,246,0.22)";
        dirSpan.style.color = "#93c5fd";
      } else {
        dirSpan.style.background = "rgba(156,163,175,0.25)";
        dirSpan.style.color = "#e5e7eb";
      }
      const textSpan = document.createElement("span");
      textSpan.textContent = text;
      textSpan.style.flex = "1 1 auto";
      textSpan.style.color = "#e5e7eb";
      textSpan.style.whiteSpace = "nowrap";
      textSpan.style.textOverflow = "ellipsis";
      textSpan.style.overflow = "hidden";
      line.appendChild(dirSpan);
      line.appendChild(textSpan);
      this.logList.appendChild(line);
      if (this.autoScroll) {
        this.scrollLogToBottom();
      }
    }
    applyFiltersToLog() {
      if (!this.logList) return;
      const logLines = this.logList.querySelectorAll("[data-log-direction]");
      logLines.forEach((line) => {
        const direction = line.getAttribute("data-log-direction");
        const category = line.getAttribute("data-log-category");
        const htmlLine = line;
        if (this.logFilters.directions[direction] && this.logFilters.events[category]) {
          htmlLine.style.display = "flex";
        } else {
          htmlLine.style.display = "none";
        }
      });
    }
  };
  function setupResize(container, handle) {
    let resizing = false;
    let startX = 0;
    let startY = 0;
    let startWidth = 0;
    let startHeight = 0;
    handle.addEventListener("mousedown", (ev) => {
      resizing = true;
      startX = ev.clientX;
      startY = ev.clientY;
      const rect = container.getBoundingClientRect();
      startWidth = rect.width;
      startHeight = rect.height;
      ev.preventDefault();
      ev.stopPropagation();
    });
    window.addEventListener("mousemove", (ev) => {
      if (!resizing) return;
      const dx = ev.clientX - startX;
      const dy = ev.clientY - startY;
      const newWidth = Math.max(250, startWidth + dx);
      container.style.width = `${newWidth}px`;
      container.style.height = `${newHeight}px`;
      const size = { width: newWidth, height: newHeight };
      GM_setValue(STORAGE_KEY_HUD_SIZE, size);
    });
    window.addEventListener("mouseup", () => {
      resizing = false;
    });
  }

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
    }
    connect() {
      if (this.socket && this.socket.readyState === WebSocket.OPEN) {
        return;
      }
      if (this.socket && this.socket.readyState === WebSocket.CONNECTING) {
        return;
      }
      const url = this.getWsUrl();
      debugLog("Connecting to", url);
      this.hud.setWsStatus("CONNECTING");
      this.hud.pushLog("info", `Connecting to ${url}`);
      this.socket = new WebSocket(url);
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
        this.scheduleReconnect();
      });
      this.socket.addEventListener("error", (err) => {
        debugLog("WebSocket error", err);
        this.hud.setWsStatus("ERROR");
        this.hud.pushLog("info", "WebSocket error");
        this.scheduleReconnect();
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
      this.stopHeartbeat();
      try {
        this.socket.close(code, reason);
      } catch (e) {
      }
    }
    scheduleReconnect() {
      if (this.reconnectTimeout !== null) return;
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
    startHeartbeat() {
      this.stopHeartbeat();
      this.heartbeatInterval = window.setInterval(() => {
        this.sendInfo("heartbeat");
      }, 5e3);
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
          if (typeof game.started === "function") {
            return game.started();
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
        const myPlayerID = gameAPI.getMyPlayerID();
        if (!myPlayerID) {
          if (this.inGame || this.inSpawning) {
            this.inGame = false;
            this.inSpawning = false;
            const didWin = gameAPI.didPlayerWin();
            if (didWin === true) {
              this.ws.sendEvent("GAME_END", "Victory!", { victory: true });
              console.log("[GameBridge] Game ended - VICTORY!");
            } else if (didWin === false) {
              this.ws.sendEvent("GAME_END", "Defeat", { victory: false });
              console.log("[GameBridge] Game ended - DEFEAT");
            } else {
              this.ws.sendEvent("GAME_END", "Game ended", { victory: null });
              console.log("[GameBridge] Game ended (outcome unknown)");
            }
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
        if (gameStarted === true && !this.inGame) {
          this.inGame = true;
          this.inSpawning = false;
          this.ws.sendEvent("GAME_START", "Game started - countdown ended");
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
  (function start() {
    let currentWsUrl = loadWsUrl();
    const hud = new Hud(
      () => currentWsUrl,
      (url) => {
        currentWsUrl = url;
        saveWsUrl(url);
      },
      () => {
        ws.disconnect(4100, "URL changed");
        ws.connect();
      }
    );
    let game = null;
    const ws = new WsClient(
      hud,
      () => currentWsUrl,
      (action, params) => {
        if (game) {
          game.handleCommand(action, params);
        }
      }
    );
    game = new GameBridge(ws, hud);
    hud.ensure();
    ws.connect();
    game.init();
    window.otsShowHud = () => hud.ensure();
    window.otsWsClient = ws;
    window.otsGameBridge = game;
  })();
})();
