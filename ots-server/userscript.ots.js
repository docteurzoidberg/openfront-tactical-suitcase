// ==UserScript==
// @name         OTS Game Dashboard Bridge
// @namespace    http://tampermonkey.net/
// @version      0.1.0
// @description  Send game state and events to local Nuxt dashboard
// @author       You
// @match        https://example-game.com/*
// @grant        none
// ==/UserScript==

"use strict";
(() => {
  // src/main.user.ts
  var WS_URL = "ws://localhost:3000/ws-script";
  var socket = null;
  var reconnectTimeout = null;
  var reconnectDelay = 2e3;
  function log(...args) {
    console.log("[OTS Userscript]", ...args);
  }
  function connect() {
    if (socket && (socket.readyState === WebSocket.OPEN || socket.readyState === WebSocket.CONNECTING)) {
      return;
    }
    log("Connecting to", WS_URL);
    socket = new WebSocket(WS_URL);
    socket.addEventListener("open", () => {
      log("WebSocket connected");
      reconnectDelay = 2e3;
      sendInfo("userscript-connected", { url: window.location.href });
      const initialState = {
        timestamp: Date.now(),
        mapName: "Sample Map",
        mode: "Demo",
        score: { teamA: 5, teamB: 3 },
        players: [
          { id: "1", name: "You", clanTag: "TM", isAlly: true, score: 10 },
          { id: "2", name: "Enemy1", isAlly: false, score: 8 }
        ]
      };
      sendState(initialState);
    });
    socket.addEventListener("close", (ev) => {
      log("WebSocket closed", ev.code, ev.reason);
      scheduleReconnect();
    });
    socket.addEventListener("error", (err) => {
      log("WebSocket error", err);
    });
    socket.addEventListener("message", (event) => {
      handleServerMessage(event.data);
    });
  }
  function scheduleReconnect() {
    if (reconnectTimeout !== null) return;
    reconnectTimeout = window.setTimeout(() => {
      reconnectTimeout = null;
      reconnectDelay = Math.min(reconnectDelay * 1.5, 15e3);
      connect();
    }, reconnectDelay);
  }
  function safeSend(msg) {
    if (!socket || socket.readyState !== WebSocket.OPEN) {
      log("Cannot send, socket not open");
      return;
    }
    socket.send(JSON.stringify(msg));
  }
  function sendState(state) {
    safeSend({
      type: "state",
      payload: state
    });
  }
  function sendEvent(type, message, data) {
    safeSend({
      type: "event",
      payload: {
        type,
        timestamp: Date.now(),
        message,
        data
      }
    });
  }
  function sendInfo(message, data) {
    sendEvent("INFO", message, data);
  }
  function handleServerMessage(raw) {
    var _a;
    let msg;
    try {
      msg = JSON.parse(raw);
    } catch (e) {
      log("Invalid JSON from server", raw, e);
      return;
    }
    if (msg.type === "cmd" && typeof ((_a = msg.payload) == null ? void 0 : _a.action) === "string") {
      const { action, params } = msg.payload;
      log("Received command from dashboard:", action, params);
      if (action === "ping") {
        sendInfo("pong-from-userscript");
      }
      if (action.startsWith("focus-player:")) {
        const playerId = action.split(":")[1];
        sendInfo("focus-player-received", { playerId });
      }
    }
  }
  function demoHook() {
    const kinds = ["KILL", "DEATH", "OBJECTIVE", "INFO"];
    window.setInterval(() => {
      const kind = kinds[Math.floor(Math.random() * kinds.length)];
      sendEvent(kind, `Demo ${kind.toLowerCase()} event from userscript`, {
        random: Math.random()
      });
    }, 5e3);
  }
  (function start() {
    connect();
    demoHook();
  })();
})();
