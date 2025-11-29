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
      this.logCounter = 0;
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
      root.innerHTML = `<div id="ots-hud-inner" style="width: 380px; height: 260px; max-height: 60vh; background: rgba(15,23,42,0.96); border-radius: 8px; box-shadow: 0 8px 20px rgba(0,0,0,0.5); border: 1px solid rgba(148,163,184,0.5); display: flex; flex-direction: column; overflow: hidden; position: relative;">
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
          <span style="font-size:11px;font-weight:600;letter-spacing:0.08em;text-transform:uppercase;color:#9ca3af;white-space:nowrap;text-overflow:ellipsis;overflow:hidden;">Openfront Tactical Suitcase link</span>
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
      <div id="ots-hud-resize" style="position:absolute;right:2px;bottom:2px;width:12px;height:12px;cursor:se-resize;opacity:0.7;">
        <div style="position:absolute;right:1px;bottom:1px;width:10px;height:2px;background:rgba(148,163,184,0.9);transform:rotate(45deg);"></div>
        <div style="position:absolute;right:3px;bottom:3px;width:8px;height:2px;background:rgba(148,163,184,0.7);transform:rotate(45deg);"></div>
      </div>
    </div>`;
      document.body.appendChild(root);
      this.root = root;
      this.logList = root.querySelector("#ots-hud-log");
      this.body = root.querySelector("#ots-hud-body");
      this.wsDot = root.querySelector("#ots-hud-ws-dot");
      this.gameDot = root.querySelector("#ots-hud-game-dot");
      this.settingsPanel = root.querySelector("#ots-hud-settings-panel");
      this.settingsInput = root.querySelector("#ots-hud-settings-ws");
      const header = root.querySelector("#ots-hud-header");
      const settingsHeader = root.querySelector("#ots-hud-settings-header");
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
      setupResize(inner, resizeHandle);
      let collapsed = false;
      toggleBtn.addEventListener("click", () => {
        if (!this.body) return;
        collapsed = !collapsed;
        this.body.style.display = collapsed ? "none" : "flex";
        if (collapsed) {
          inner.style.height = "32px";
        } else {
          inner.style.height = "";
        }
        toggleBtn.textContent = collapsed ? "+" : "\u2013";
      });
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
    pushLog(direction, text) {
      this.ensure();
      if (!this.logList) return;
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
      this.logList.scrollTop = this.logList.scrollHeight;
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
      const newWidth = Math.max(220, startWidth + dx);
      const newHeight = Math.max(140, startHeight + dy);
      container.style.width = `${newWidth}px`;
      container.style.height = `${newHeight}px`;
      const size = { width: newWidth, height: newHeight };
      GM_setValue(STORAGE_KEY_HUD_SIZE, size);
    });
    window.addEventListener("mouseup", () => {
      resizing = false;
    });
  }

  // src/websocket/client.ts
  function debugLog(...args) {
    console.log("[OTS Userscript]", ...args);
  }
  var WsClient = class {
    constructor(hud, getWsUrl) {
      this.hud = hud;
      this.getWsUrl = getWsUrl;
      this.socket = null;
      this.reconnectTimeout = null;
      this.reconnectDelay = 2e3;
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
        this.sendInfo("userscript-connected", { url: window.location.href });
      });
      this.socket.addEventListener("close", (ev) => {
        debugLog("WebSocket closed", ev.code, ev.reason);
        this.hud.setWsStatus("DISCONNECTED");
        this.hud.pushLog("info", `WebSocket closed (${ev.code} ${ev.reason || ""})`);
        this.scheduleReconnect();
      });
      this.socket.addEventListener("error", (err) => {
        debugLog("WebSocket error", err);
        this.hud.setWsStatus("ERROR");
        this.hud.pushLog("info", "WebSocket error");
        this.scheduleReconnect();
      });
      this.socket.addEventListener("message", (event) => {
        this.hud.pushLog("recv", typeof event.data === "string" ? event.data : "[binary message]");
        this.handleServerMessage(event.data);
      });
    }
    disconnect(code, reason) {
      if (!this.socket) return;
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
      if (!this.socket || this.socket.readyState !== WebSocket.OPEN) {
        debugLog("Cannot send, socket not open");
        this.hud.pushLog("info", "Cannot send, socket not open");
        return;
      }
      const json = JSON.stringify(msg);
      this.hud.pushLog("send", json);
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
        }
        if (action.startsWith("focus-player:")) {
          const playerId = action.split(":")[1];
          this.sendInfo("focus-player-received", { playerId });
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
  function waitForGameInstance(controlPanel, callback, checkInterval = 100) {
    const existing = controlPanel.game;
    if (existing) {
      callback(existing);
      return;
    }
    const intervalId = window.setInterval(() => {
      const game = controlPanel.game;
      if (game) {
        window.clearInterval(intervalId);
        callback(game);
      }
    }, checkInterval);
  }

  // src/utils/logger.ts
  function debugLog2(...args) {
    console.log("[OTS Userscript]", ...args);
  }

  // src/game/openfront-bridge.ts
  var CONTROL_PANEL_SELECTOR = "control-panel";
  var GAME_UPDATE_EVENT = "openfront:game-update";
  var GAME_UPDATE_TYPE_UNIT = 1;
  var gameUpdateListenerAttached = false;
  var initialPayloadSent = false;
  function hookGameUpdates(game) {
    if (!game) {
      debugLog2("[OTS] No GameView instance to hook.");
      return;
    }
    const proto = Object.getPrototypeOf(game);
    if (!proto || typeof proto.update !== "function") {
      debugLog2("[OTS] GameView prototype missing update method; skipping hook.");
      return;
    }
    if (proto.__openfrontGameHooked) {
      return;
    }
    const originalUpdate = proto.update;
    proto.update = function patchedGameUpdate(...args) {
      const result = originalUpdate.apply(this, args);
      window.dispatchEvent(
        new CustomEvent(GAME_UPDATE_EVENT, {
          detail: { game: this, payload: args[0] }
        })
      );
      return result;
    };
    Object.defineProperty(proto, "__openfrontGameHooked", {
      value: true,
      configurable: true
    });
    debugLog2("[OTS] Hooked GameView.update().");
    attachGameUpdateListener();
  }
  function attachGameUpdateListener() {
    if (gameUpdateListenerAttached) {
      return;
    }
    window.addEventListener(GAME_UPDATE_EVENT, handleGameUpdate);
    gameUpdateListenerAttached = true;
  }
  function handleGameUpdate(event) {
    var _a, _b, _c;
    const game = (_a = event == null ? void 0 : event.detail) == null ? void 0 : _a.game;
    const viewData = (_b = event == null ? void 0 : event.detail) == null ? void 0 : _b.payload;
    if (!game || !viewData || !viewData.updates) {
      return;
    }
    const ws = window.otsWsClient;
    if (!ws) {
      return;
    }
    if (!initialPayloadSent) {
      ws.sendEvent("INFO", "of-initial-gameview-payload", {
        game,
        payload: viewData
      });
      initialPayloadSent = true;
    }
    const myPlayer = typeof game.myPlayer === "function" ? game.myPlayer() : null;
    const mySmallID = typeof (myPlayer == null ? void 0 : myPlayer.smallID) === "function" ? myPlayer.smallID() : void 0;
    if (mySmallID === void 0) {
      return;
    }
    const unitUpdates = (_c = viewData.updates[GAME_UPDATE_TYPE_UNIT]) != null ? _c : [];
    const myUnits = unitUpdates.filter((u) => u.ownerID === mySmallID);
    if (!myUnits.length) return;
    ws.sendEvent("INFO", "of-game-unit-updates", {
      count: myUnits.length,
      sample: myUnits.slice(0, 3).map((u) => ({
        id: u.id,
        type: u.unitType,
        ownerID: u.ownerID,
        troops: u.troops,
        underConstruction: u.underConstruction,
        isActive: u.isActive
      }))
    });
  }
  var GameBridge = class {
    constructor(ws, hud) {
      this.ws = ws;
      this.hud = hud;
    }
    init() {
      waitForElement(CONTROL_PANEL_SELECTOR, (controlPanel) => {
        debugLog2("[OTS] control panel detected", controlPanel);
        waitForGameInstance(controlPanel, (game) => {
          ;
          window.openfrontControlPanelGame = game;
          debugLog2("[OTS] GameView ready", game);
          this.ws.sendEvent("INFO", "of-game-instance-detected", game);
          this.hud.setGameStatus(true);
          hookGameUpdates(game);
        });
      });
    }
  };

  // src/storage/config.ts
  var DEFAULT_WS_URL = "ws://localhost:3000/ws-script";
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
    const ws = new WsClient(hud, () => currentWsUrl);
    const game = new GameBridge(ws, hud);
    hud.ensure();
    ws.connect();
    game.init();
    window.otsShowHud = () => hud.ensure();
    window.otsWsClient = ws;
  })();
})();
