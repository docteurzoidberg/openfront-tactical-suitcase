// ==UserScript==
// @name         OTS Game Dashboard Bridge
// @namespace    http://tampermonkey.net/
// @version      2026-01-10.1
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
  // src/hud/sidebar/position-manager.ts
  var PositionManager = class {
    constructor(root, titlebar, initialPosition, initialSize, options) {
      // Drag state
      this.isDragging = false;
      this.dragStartX = 0;
      this.dragStartY = 0;
      // Resize state
      this.isResizing = false;
      this.resizeStartX = 0;
      this.resizeStartY = 0;
      this.resizeStartWidth = 0;
      this.resizeStartHeight = 0;
      this.content = null;
      this.body = null;
      this.root = root;
      this.titlebar = titlebar;
      this.currentPosition = initialPosition;
      this.userSize = initialSize;
      this.onPositionChange = options == null ? void 0 : options.onPositionChange;
      this.onSizeChange = options == null ? void 0 : options.onSizeChange;
    }
    /**
     * Set DOM references (content and body containers)
     */
    setContainers(content, body) {
      this.content = content;
      this.body = body;
    }
    /**
     * Initialize position and attach event listeners
     */
    initialize(collapsed) {
      this.applyPosition(this.currentPosition, collapsed);
      this.attachEventListeners();
    }
    /**
     * Get current snap position
     */
    getPosition() {
      return this.currentPosition;
    }
    /**
     * Get current user size
     */
    getSize() {
      return { ...this.userSize };
    }
    /**
     * Calculate snap position based on mouse coordinates
     */
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
    /**
     * Apply position styling to the window
     */
    applyPosition(position, collapsed) {
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
      const contentWidth = collapsed ? 0 : Math.max(200, this.userSize.width);
      const contentHeight = collapsed ? 0 : Math.max(150, this.userSize.height);
      switch (position) {
        case "top":
          this.root.style.top = "-2px";
          this.root.style.left = "50%";
          this.root.style.transform = "translateX(-50%)";
          this.root.style.width = collapsed ? "325px" : `${Math.max(325, contentWidth)}px`;
          this.root.style.height = `${minSize + contentHeight + 2}px`;
          this.root.style.borderRadius = "0 0 8px 8px";
          this.root.style.borderTop = "none";
          this.titlebar.style.borderRadius = "0 0 8px 8px";
          if (buttonContainer) buttonContainer.style.transform = "";
          if (resizeHandle) {
            resizeHandle.style.display = collapsed ? "none" : "block";
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
          this.root.style.width = collapsed ? "325px" : `${Math.max(325, contentWidth)}px`;
          this.root.style.height = `${minSize + contentHeight + 2}px`;
          this.root.style.borderRadius = "8px 8px 0 0";
          this.root.style.borderBottom = "none";
          this.titlebar.style.borderRadius = "8px 8px 0 0";
          if (buttonContainer) buttonContainer.style.transform = "";
          if (resizeHandle) {
            resizeHandle.style.display = collapsed ? "none" : "block";
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
          this.root.style.height = collapsed ? "250px" : `${Math.max(250, contentHeight)}px`;
          this.root.style.borderRadius = "0 8px 8px 0";
          this.root.style.borderLeft = "none";
          this.titlebar.style.writingMode = "vertical-rl";
          this.titlebar.style.transform = "rotate(180deg)";
          this.titlebar.style.borderRadius = "0 8px 8px 0";
          this.titlebar.style.width = "32px";
          this.titlebar.style.height = "100%";
          if (buttonContainer) buttonContainer.style.transform = "rotate(-180deg)";
          if (resizeHandle) {
            resizeHandle.style.display = collapsed ? "none" : "block";
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
          this.root.style.height = collapsed ? "250px" : `${Math.max(250, contentHeight)}px`;
          this.root.style.borderRadius = "8px 0 0 8px";
          this.root.style.borderRight = "none";
          this.titlebar.style.writingMode = "vertical-rl";
          this.titlebar.style.transform = "rotate(180deg)";
          this.titlebar.style.borderRadius = "8px 0 0 8px";
          this.titlebar.style.width = "32px";
          this.titlebar.style.height = "100%";
          if (buttonContainer) buttonContainer.style.transform = "rotate(-180deg)";
          if (resizeHandle) {
            resizeHandle.style.display = collapsed ? "none" : "block";
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
      this.currentPosition = position;
    }
    /**
     * Show snap preview overlay
     */
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
    /**
     * Remove snap preview overlay
     */
    removeSnapPreview() {
      const preview = document.getElementById("ots-snap-preview");
      if (preview) {
        preview.remove();
      }
    }
    /**
     * Update arrow indicators based on position and collapsed state
     */
    updateArrows(collapsed) {
      const arrowLeft = this.root.querySelector("#ots-hud-arrow-left");
      const arrowRight = this.root.querySelector("#ots-hud-arrow-right");
      if (!arrowLeft || !arrowRight) return;
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
    /**
     * Attach drag and resize event listeners
     */
    attachEventListeners() {
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
          e.stopPropagation();
          e.preventDefault();
          this.isResizing = true;
          this.resizeStartX = e.clientX;
          this.resizeStartY = e.clientY;
          this.resizeStartWidth = this.userSize.width;
          this.resizeStartHeight = this.userSize.height;
          resizeHandle.style.opacity = "1";
          this.root.style.transition = "none";
          this.root.style.willChange = "width, height";
        });
      }
      document.addEventListener("mousemove", (e) => {
        this.handleMouseMove(e);
      });
      document.addEventListener("mouseup", (e) => {
        this.handleMouseUp(e);
      });
    }
    /**
     * Handle mouse move for drag and resize
     */
    handleMouseMove(e) {
      if (this.isResizing) {
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
    }
    /**
     * Handle mouse up for drag and resize
     */
    handleMouseUp(e) {
      if (this.isResizing) {
        this.isResizing = false;
        const resizeHandle = this.root.querySelector("#ots-hud-resize-handle");
        if (resizeHandle) resizeHandle.style.opacity = "0.5";
        this.root.style.transition = "width 0.3s cubic-bezier(0.4, 0, 0.2, 1), height 0.3s cubic-bezier(0.4, 0, 0.2, 1)";
        this.root.style.willChange = "auto";
        if (this.onSizeChange) {
          this.onSizeChange(this.userSize);
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
        if (this.onPositionChange) {
          this.onPositionChange(newPosition);
        }
        this.removeSnapPreview();
        return;
      }
      if (this.dragStartX !== 0 || this.dragStartY !== 0) {
        this.dragStartX = 0;
        this.dragStartY = 0;
      }
    }
  };

  // src/hud/sidebar/window.ts
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

  // src/hud/sidebar/settings-panel.ts
  var SettingsPanel = class {
    constructor(onClose, onSave) {
      this.onClose = onClose;
      this.onSave = onSave;
      this.panel = this.createPanel();
      this.input = this.panel.querySelector("#ots-hud-settings-ws");
      const header = this.panel.querySelector("#ots-hud-settings-header");
      this.floatingPanel = new FloatingPanel(this.panel);
      this.floatingPanel.attachDrag(header);
      this.attachListeners();
      document.body.appendChild(this.panel);
    }
    /**
     * Create the settings panel HTML structure
     */
    createPanel() {
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
      return panel;
    }
    /**
     * Attach event listeners to buttons
     */
    attachListeners() {
      const closeBtn = this.panel.querySelector("#ots-hud-settings-close");
      closeBtn == null ? void 0 : closeBtn.addEventListener("click", () => {
        this.hide();
        this.onClose();
      });
      const resetBtn = this.panel.querySelector("#ots-hud-settings-reset");
      resetBtn == null ? void 0 : resetBtn.addEventListener("click", () => {
        this.input.value = "ws://localhost:3000/ws";
      });
      const saveBtn = this.panel.querySelector("#ots-hud-settings-save");
      saveBtn == null ? void 0 : saveBtn.addEventListener("click", () => {
        const url = this.input.value.trim();
        if (url) {
          this.hide();
          this.onSave(url);
        }
      });
    }
    /**
     * Show the settings panel with current URL
     */
    show(currentUrl) {
      this.input.value = currentUrl;
      this.panel.style.display = "flex";
    }
    /**
     * Hide the settings panel
     */
    hide() {
      this.panel.style.display = "none";
    }
    /**
     * Check if panel is currently visible
     */
    isVisible() {
      return this.panel.style.display === "flex";
    }
    /**
     * Position the panel next to the HUD
     * @param hudRect - Bounding rectangle of the HUD
     * @param position - Current HUD position (right, left, top, bottom)
     */
    positionNextToHud(hudRect, position) {
      const gap = 8;
      this.panel.style.top = "";
      this.panel.style.bottom = "";
      this.panel.style.left = "";
      this.panel.style.right = "";
      switch (position) {
        case "right":
          this.panel.style.top = `${hudRect.top}px`;
          this.panel.style.right = `${window.innerWidth - hudRect.left + gap}px`;
          break;
        case "left":
          this.panel.style.top = `${hudRect.top}px`;
          this.panel.style.left = `${hudRect.right + gap}px`;
          break;
        case "top":
          this.panel.style.top = `${hudRect.bottom + gap}px`;
          this.panel.style.left = `${hudRect.left}px`;
          break;
        case "bottom":
          this.panel.style.bottom = `${window.innerHeight - hudRect.top + gap}px`;
          this.panel.style.left = `${hudRect.left}px`;
          break;
      }
    }
    /**
     * Get the panel element (for positioning)
     */
    getElement() {
      return this.panel;
    }
  };

  // src/hud/sidebar/filter-panel.ts
  var FilterPanel = class {
    constructor(onClose, onFilterChange, initialFilters) {
      this.onClose = onClose;
      this.onFilterChange = onFilterChange;
      this.panel = this.createPanel();
      this.checkboxes = {
        directions: {
          send: this.panel.querySelector("#ots-hud-filter-send"),
          recv: this.panel.querySelector("#ots-hud-filter-recv"),
          info: this.panel.querySelector("#ots-hud-filter-info")
        },
        events: {
          game: this.panel.querySelector("#ots-hud-filter-game"),
          nukes: this.panel.querySelector("#ots-hud-filter-nukes"),
          alerts: this.panel.querySelector("#ots-hud-filter-alerts"),
          troops: this.panel.querySelector("#ots-hud-filter-troops"),
          sounds: this.panel.querySelector("#ots-hud-filter-sounds"),
          system: this.panel.querySelector("#ots-hud-filter-system"),
          heartbeat: this.panel.querySelector("#ots-hud-filter-heartbeat")
        }
      };
      this.restoreFilterStates(initialFilters);
      const header = this.panel.querySelector("#ots-hud-filter-header");
      this.floatingPanel = new FloatingPanel(this.panel);
      this.floatingPanel.attachDrag(header);
      this.attachListeners();
      document.body.appendChild(this.panel);
    }
    /**
     * Create the filter panel HTML structure
     */
    createPanel() {
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
      return panel;
    }
    /**
     * Attach event listeners to checkboxes and close button
     */
    attachListeners() {
      const closeBtn = this.panel.querySelector("#ots-hud-filter-close");
      closeBtn == null ? void 0 : closeBtn.addEventListener("click", () => {
        this.hide();
        this.onClose();
      });
      Object.entries(this.checkboxes.directions).forEach(([key, checkbox]) => {
        checkbox.addEventListener("change", () => {
          this.onFilterChange(this.getCurrentFilters());
        });
      });
      Object.entries(this.checkboxes.events).forEach(([key, checkbox]) => {
        checkbox.addEventListener("change", () => {
          this.onFilterChange(this.getCurrentFilters());
        });
      });
    }
    /**
     * Restore filter checkbox states from LogFilters
     */
    restoreFilterStates(filters) {
      this.checkboxes.directions.send.checked = filters.directions.send;
      this.checkboxes.directions.recv.checked = filters.directions.recv;
      this.checkboxes.directions.info.checked = filters.directions.info;
      this.checkboxes.events.game.checked = filters.events.game;
      this.checkboxes.events.nukes.checked = filters.events.nukes;
      this.checkboxes.events.alerts.checked = filters.events.alerts;
      this.checkboxes.events.troops.checked = filters.events.troops;
      this.checkboxes.events.sounds.checked = filters.events.sounds;
      this.checkboxes.events.system.checked = filters.events.system;
      this.checkboxes.events.heartbeat.checked = filters.events.heartbeat;
    }
    /**
     * Get current filter state from checkboxes
     */
    getCurrentFilters() {
      return {
        directions: {
          send: this.checkboxes.directions.send.checked,
          recv: this.checkboxes.directions.recv.checked,
          info: this.checkboxes.directions.info.checked
        },
        events: {
          game: this.checkboxes.events.game.checked,
          nukes: this.checkboxes.events.nukes.checked,
          alerts: this.checkboxes.events.alerts.checked,
          troops: this.checkboxes.events.troops.checked,
          sounds: this.checkboxes.events.sounds.checked,
          system: this.checkboxes.events.system.checked,
          heartbeat: this.checkboxes.events.heartbeat.checked
        }
      };
    }
    /**
     * Show the filter panel
     */
    show() {
      this.panel.style.display = "flex";
    }
    /**
     * Hide the filter panel
     */
    hide() {
      this.panel.style.display = "none";
    }
    /**
     * Check if panel is currently visible
     */
    isVisible() {
      return this.panel.style.display === "flex";
    }
    /**
     * Position the panel next to the HUD
     * @param hudRect - Bounding rectangle of the HUD
     * @param position - Current HUD position (right, left, top, bottom)
     */
    positionNextToHud(hudRect, position) {
      const gap = 8;
      this.panel.style.top = "";
      this.panel.style.bottom = "";
      this.panel.style.left = "";
      this.panel.style.right = "";
      switch (position) {
        case "right":
          this.panel.style.top = `${hudRect.top}px`;
          this.panel.style.right = `${window.innerWidth - hudRect.left + gap}px`;
          break;
        case "left":
          this.panel.style.top = `${hudRect.top}px`;
          this.panel.style.left = `${hudRect.right + gap}px`;
          break;
        case "top":
          this.panel.style.top = `${hudRect.bottom + gap}px`;
          this.panel.style.left = `${hudRect.left}px`;
          break;
        case "bottom":
          this.panel.style.bottom = `${window.innerHeight - hudRect.top + gap}px`;
          this.panel.style.left = `${hudRect.left}px`;
          break;
      }
    }
    /**
     * Get the panel element (for positioning)
     */
    getElement() {
      return this.panel;
    }
  };

  // src/hud/sidebar/utils/log-filtering.ts
  function shouldShowLogEntry(filters, direction, eventType, messageText) {
    if (!filters.directions[direction]) return false;
    if (!eventType) return true;
    if (eventType === "INFO" && messageText && messageText.includes('"heartbeat"')) {
      return filters.events.heartbeat;
    }
    if (eventType.startsWith("GAME_") || eventType === "GAME_SPAWNING") {
      return filters.events.game;
    } else if (eventType.includes("NUKE") || eventType.includes("HYDRO") || eventType.includes("MIRV")) {
      return filters.events.nukes;
    } else if (eventType.startsWith("ALERT_")) {
      return filters.events.alerts;
    } else if (eventType.includes("TROOP")) {
      return filters.events.troops;
    } else if (eventType === "SOUND_PLAY") {
      return filters.events.sounds;
    } else if (eventType === "INFO" || eventType === "ERROR" || eventType === "HARDWARE_TEST") {
      return filters.events.system;
    }
    return true;
  }

  // src/hud/sidebar/utils/log-format.ts
  var formatIdCounter = 0;
  var pendingToggleIds = [];
  function escapeHtml(text) {
    const div = document.createElement("div");
    div.textContent = text;
    return div.innerHTML;
  }
  function formatJson(obj, indent = 0) {
    const indentStr = "  ".repeat(indent);
    const nextIndent = "  ".repeat(indent + 1);
    if (obj === null) return '<span style="color:#f87171;">null</span>';
    if (obj === void 0) return '<span style="color:#f87171;">undefined</span>';
    if (typeof obj === "boolean") return `<span style="color:#a78bfa;">${obj}</span>`;
    if (typeof obj === "number") return `<span style="color:#fbbf24;">${obj}</span>`;
    if (typeof obj === "string") return `<span style="color:#86efac;">"${escapeHtml(obj)}"</span>`;
    if (Array.isArray(obj)) {
      if (obj.length === 0) return '<span style="color:#cbd5e1;">[]</span>';
      const id = `json-arr-${formatIdCounter++}`;
      pendingToggleIds.push(id);
      const items = obj.map((item) => `${nextIndent}${formatJson(item, indent + 1)}`).join(",\n");
      const result = `<span style="color:#cbd5e1;">[</span><span id="${id}-toggle" style="cursor:pointer;color:#9ca3af;margin:0 4px;" data-json-toggle="${id}">\u25BC</span><span id="${id}-summary" style="display:none;color:#9ca3af;">${obj.length} items</span>
<span id="${id}-content">${items}
${indentStr}</span><span style="color:#cbd5e1;">]</span>`;
      return result;
    }
    if (typeof obj === "object") {
      const record = obj;
      const keys = Object.keys(record);
      if (keys.length === 0) return '<span style="color:#cbd5e1;">{}</span>';
      const id = `json-obj-${formatIdCounter++}`;
      pendingToggleIds.push(id);
      const items = keys.map((key) => {
        const value = formatJson(record[key], indent + 1);
        return `${nextIndent}<span style="color:#60a5fa;">"${escapeHtml(key)}"</span><span style="color:#cbd5e1;">:</span> ${value}`;
      }).join(",\n");
      const result = `<span style="color:#cbd5e1;">{</span><span id="${id}-toggle" style="cursor:pointer;color:#9ca3af;margin:0 4px;" data-json-toggle="${id}">\u25BC</span><span id="${id}-summary" style="display:none;color:#9ca3af;">${keys.length} keys</span>
<span id="${id}-content">${items}
${indentStr}</span><span style="color:#cbd5e1;">}</span>`;
      return result;
    }
    return String(obj);
  }
  function attachJsonEventListeners() {
    const idsToAttach = [...pendingToggleIds];
    pendingToggleIds.length = 0;
    idsToAttach.forEach((id) => {
      const toggle = document.getElementById(`${id}-toggle`);
      if (toggle && !toggle.dataset.listenerAttached) {
        toggle.dataset.listenerAttached = "true";
        toggle.addEventListener("click", (e) => {
          e.stopPropagation();
          const summary = document.getElementById(`${id}-summary`);
          const content = document.getElementById(`${id}-content`);
          if (summary && content) {
            const isCollapsed = content.style.display === "none";
            if (isCollapsed) {
              content.style.display = "";
              summary.style.display = "none";
              toggle.textContent = "\u25BC";
            } else {
              content.style.display = "none";
              summary.style.display = "inline";
              toggle.textContent = "\u25B6";
            }
          }
        });
      }
    });
  }

  // src/hud/sidebar/constants.ts
  var MAX_LOG_ENTRIES = 100;
  var DEFAULT_LOG_FILTERS = {
    directions: { send: true, recv: true, info: true },
    events: {
      game: true,
      nukes: true,
      alerts: true,
      troops: true,
      sounds: true,
      system: true,
      heartbeat: false
    }
  };
  var DEFAULT_SOUND_TOGGLES = {
    "game_start": true,
    "game_player_death": true,
    "game_victory": true,
    "game_defeat": true
  };

  // src/hud/sidebar/tabs/logs-tab.ts
  var LogsTab = class {
    constructor(onFilterClick, initialFilters) {
      this.onFilterClick = onFilterClick;
      this.logCounter = 0;
      this.autoScroll = true;
      this.logFilters = initialFilters;
      this.logList = document.getElementById("ots-hud-log");
      this.autoScrollBtn = document.getElementById("ots-hud-autoscroll");
      this.filterBtn = document.getElementById("ots-hud-filter");
      if (!this.logList || !this.autoScrollBtn || !this.filterBtn) {
        throw new Error("Logs tab DOM elements not found");
      }
      this.attachListeners();
      this.updateAutoScrollBtn();
    }
    /**
     * Create HTML for logs tab
     */
    static createHTML() {
      return `
      <div id="ots-tab-logs" class="ots-tab-content" style="flex:1;display:flex;flex-direction:column;min-height:0;">
        <div id="ots-hud-log" style="flex:1;overflow-y:auto;overflow-x:hidden;padding:8px;font-size:11px;line-height:1.4;background:rgba(10,10,15,0.8);"></div>
        <div id="ots-hud-footer" style="flex:none;padding:6px 8px;display:flex;align-items:center;justify-content:space-between;background:rgba(10,10,15,0.8);border-top:1px solid rgba(255,255,255,0.1);">
          <button id="ots-hud-filter" style="all:unset;cursor:pointer;font-size:11px;color:#e0e0e0;padding:3px 8px;border-radius:4px;background:rgba(255,255,255,0.1);transition:background 0.2s;">\u2691 Filter</button>
          <button id="ots-hud-autoscroll" style="all:unset;cursor:pointer;font-size:10px;font-weight:600;color:#6ee7b7;padding:3px 8px;border-radius:12px;background:rgba(52,211,153,0.2);white-space:nowrap;transition:background 0.2s;">\u2713 Auto</button>
        </div>
      </div>
    `;
    }
    /**
     * Attach event listeners
     */
    attachListeners() {
      this.autoScrollBtn.addEventListener("click", () => {
        this.autoScroll = !this.autoScroll;
        this.updateAutoScrollBtn();
      });
      this.filterBtn.addEventListener("mouseenter", () => {
        this.filterBtn.style.background = "rgba(255, 255, 255, 0.2)";
      });
      this.filterBtn.addEventListener("mouseleave", () => {
        this.filterBtn.style.background = "rgba(255, 255, 255, 0.1)";
      });
      this.filterBtn.addEventListener("click", () => {
        this.onFilterClick();
      });
    }
    /**
     * Update auto-scroll button appearance
     */
    updateAutoScrollBtn() {
      if (this.autoScroll) {
        this.autoScrollBtn.textContent = "\u2713 Auto";
        this.autoScrollBtn.style.color = "#6ee7b7";
        this.autoScrollBtn.style.background = "rgba(52,211,153,0.2)";
      } else {
        this.autoScrollBtn.textContent = "\u2717 Auto";
        this.autoScrollBtn.style.color = "#9ca3af";
        this.autoScrollBtn.style.background = "rgba(100,116,139,0.2)";
      }
    }
    /**
     * Update filters (from external source)
     */
    updateFilters(filters) {
      this.logFilters = filters;
    }
    /**
     * Add a log entry
     */
    pushLog(direction, text, eventType, jsonData) {
      if (!this.logList) return;
      if (!shouldShowLogEntry(this.logFilters, direction, eventType, text)) {
        return;
      }
      this.logCounter++;
      const entry = document.createElement("div");
      entry.dataset.logId = `${this.logCounter}`;
      entry.dataset.direction = direction;
      if (eventType) entry.dataset.eventType = eventType;
      entry.style.cssText = "margin-bottom:6px;";
      let directionColor = "#93c5fd";
      let directionLabel = "SEND";
      if (direction === "recv") {
        directionColor = "#86efac";
        directionLabel = "RECV";
      } else if (direction === "info") {
        directionColor = "#fcd34d";
        directionLabel = "INFO";
      }
      const entryId = `ots-log-entry-${this.logCounter}`;
      const summaryParts = [];
      summaryParts.push(`<span style="display:inline-block;font-weight:600;font-size:9px;padding:1px 4px;border-radius:3px;background:${directionColor};color:#0f172a;margin-right:6px;">${directionLabel}</span>`);
      if (eventType) {
        let eventColor = "#60a5fa";
        if (eventType.startsWith("ALERT_")) eventColor = "#f87171";
        else if (eventType.includes("NUKE")) eventColor = "#fb923c";
        else if (eventType.includes("TROOPS")) eventColor = "#4ade80";
        else if (eventType === "SOUND_PLAY") eventColor = "#fbbf24";
        else if (eventType.includes("HARDWARE")) eventColor = "#06b6d4";
        summaryParts.push(`<span style="display:inline-block;font-size:9px;padding:1px 4px;border-radius:3px;background:${eventColor};color:white;margin-right:6px;">${eventType}</span>`);
      }
      let summary = text;
      if (jsonData !== void 0) {
        const jsonObj = jsonData;
        if (jsonObj.type === "event" && jsonObj.payload) {
          summary = jsonObj.payload.message || jsonObj.payload.type || "Event";
        } else if (jsonObj.type === "cmd" && jsonObj.payload) {
          summary = `Command: ${jsonObj.payload.action || "unknown"}`;
        } else if (jsonObj.type === "state") {
          summary = "State update";
        } else if (jsonObj.type === "handshake") {
          summary = `Handshake: ${jsonObj.clientType || "unknown"}`;
        } else {
          summary = text.length > 50 ? text.substring(0, 50) + "..." : text;
        }
      }
      summaryParts.push(`<span style="color:#d1d5db;">${escapeHtml(summary)}</span>`);
      if (jsonData !== void 0) {
        summaryParts.push(`<span id="ots-arrow-${this.logCounter}" style="display:inline-block;margin-left:6px;font-size:10px;color:#9ca3af;">\u25B6</span>`);
      }
      const lines = [];
      lines.push(`<div id="${entryId}-summary" style="cursor:${jsonData !== void 0 ? "pointer" : "default"};">${summaryParts.join("")}</div>`);
      if (jsonData !== void 0) {
        lines.push(`<div id="${entryId}-json" style="display:none;margin-top:6px;padding:8px;background:rgba(0,0,0,0.3);border-radius:4px;">`);
        lines.push(`<pre style="margin:0;font-size:10px;line-height:1.4;white-space:pre-wrap;word-break:break-all;">${formatJson(jsonData)}</pre>`);
        lines.push(`</div>`);
        setTimeout(() => {
          const summaryEl = document.getElementById(`${entryId}-summary`);
          const jsonEl = document.getElementById(`${entryId}-json`);
          const arrowEl = document.getElementById(`ots-arrow-${this.logCounter}`);
          if (summaryEl && jsonEl && arrowEl) {
            summaryEl.addEventListener("click", () => {
              const isHidden = jsonEl.style.display === "none";
              if (isHidden) {
                jsonEl.style.display = "block";
                arrowEl.textContent = "\u25BC";
              } else {
                jsonEl.style.display = "none";
                arrowEl.textContent = "\u25B6";
              }
            });
          }
          attachJsonEventListeners();
        }, 0);
      }
      entry.innerHTML = lines.join("");
      this.logList.appendChild(entry);
      while (this.logList.children.length > MAX_LOG_ENTRIES) {
        this.logList.removeChild(this.logList.firstChild);
      }
      if (this.autoScroll) {
        this.logList.scrollTop = this.logList.scrollHeight;
      }
    }
    /**
     * Clear all logs
     */
    clearLogs() {
      if (this.logList) {
        this.logList.innerHTML = "";
        this.logCounter = 0;
      }
    }
    /**
     * Refilter existing logs
     */
    refilterLogs() {
      if (!this.logList) return;
      Array.from(this.logList.children).forEach((child) => {
        const entry = child;
        const direction = entry.dataset.direction;
        const eventType = entry.dataset.eventType;
        const messageText = entry.textContent || "";
        if (direction && shouldShowLogEntry(this.logFilters, direction, eventType, messageText)) {
          entry.style.display = "";
        } else {
          entry.style.display = "none";
        }
      });
    }
  };

  // src/hud/sidebar/hud-template.ts
  var HudTemplate = class {
    /**
     * Create the root HUD container element
     */
    static createRoot() {
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
      return root;
    }
    /**
     * Create the titlebar element with status indicators and buttons
     */
    static createTitlebar() {
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
      return titlebar;
    }
    /**
     * Create the content container element
     */
    static createContent() {
      const content = document.createElement("div");
      content.id = "ots-hud-content";
      content.style.cssText = `
      flex: 1;
      min-height: 0;
      display: flex;
      flex-direction: column;
      overflow: hidden;
    `;
      return content;
    }
    /**
     * Generate the inner HTML for the content area, including tabs and panels
     * @param tabManager - The TabManager instance for generating tab HTML
     */
    static createContentHTML(tabManager) {
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
    `;
    }
  };

  // src/hud/sidebar/tab-manager.ts
  var TabManager = class {
    constructor(options) {
      this.container = options.container;
      this.tabs = options.tabs;
      this.activeTab = options.defaultTab;
      this.onTabChange = options.onTabChange;
    }
    /**
     * Generate HTML for tab navigation buttons
     */
    createTabsHTML() {
      return `
      <div id="ots-hud-tabs" style="display:flex;gap:2px;padding:6px 8px;background:rgba(10,10,15,0.6);border-bottom:1px solid rgba(255,255,255,0.1);">
        ${this.tabs.map((tab) => {
        const isActive = tab.id === this.activeTab;
        const bgColor = isActive ? "rgba(59,130,246,0.3)" : "rgba(255,255,255,0.05)";
        const textColor = isActive ? "#e5e7eb" : "#9ca3af";
        return `<button class="ots-tab-btn" data-tab="${tab.id}" style="all:unset;cursor:pointer;font-size:10px;font-weight:600;color:${textColor};padding:4px 12px;border-radius:4px;background:${bgColor};transition:all 0.2s;">${tab.label}</button>`;
      }).join("")}
      </div>
    `;
    }
    /**
     * Attach event listeners to tab buttons
     */
    attachListeners() {
      const tabBtns = this.container.querySelectorAll(".ots-tab-btn");
      tabBtns.forEach((btn) => {
        btn.addEventListener("click", () => {
          const tabId = btn.dataset.tab;
          this.switchTab(tabId);
        });
        btn.addEventListener("mouseenter", () => {
          const isActive = btn.dataset.tab === this.activeTab;
          if (!isActive) {
            ;
            btn.style.background = "rgba(255,255,255,0.1)";
          }
        });
        btn.addEventListener("mouseleave", () => {
          this.updateTabStyles();
        });
      });
    }
    /**
     * Switch to a different tab
     */
    switchTab(tabId) {
      if (this.activeTab === tabId) return;
      this.activeTab = tabId;
      this.updateTabStyles();
      this.updateContentVisibility();
      if (this.onTabChange) {
        this.onTabChange(tabId);
      }
    }
    /**
     * Update visual styles of tab buttons
     */
    updateTabStyles() {
      const tabBtns = this.container.querySelectorAll(".ots-tab-btn");
      tabBtns.forEach((btn) => {
        const btnTab = btn.dataset.tab;
        const isActive = btnTab === this.activeTab;
        btn.style.background = isActive ? "rgba(59,130,246,0.3)" : "rgba(255,255,255,0.05)";
        btn.style.color = isActive ? "#e5e7eb" : "#9ca3af";
      });
    }
    /**
     * Show active tab content, hide others
     */
    updateContentVisibility() {
      this.tabs.forEach((tab) => {
        const content = this.container.querySelector(`#${tab.contentId}`);
        if (content) {
          content.style.display = tab.id === this.activeTab ? "flex" : "none";
        }
      });
    }
    /**
     * Get the currently active tab
     */
    getActiveTab() {
      return this.activeTab;
    }
    /**
     * Set active tab programmatically
     */
    setActiveTab(tabId) {
      this.switchTab(tabId);
    }
  };

  // src/hud/sidebar/tabs/hardware-tab.ts
  function isRecord(value) {
    return typeof value === "object" && value !== null;
  }
  function tryCaptureHardwareDiagnostic(text) {
    try {
      const parsed = JSON.parse(text);
      if (!isRecord(parsed)) return null;
      const payload = isRecord(parsed.payload) ? parsed.payload : null;
      const data = (payload && isRecord(payload.data) ? payload.data : null) || (isRecord(parsed.data) ? parsed.data : null);
      if (!data) return null;
      const ts = (payload && typeof payload.timestamp === "number" ? payload.timestamp : void 0) || (typeof parsed.timestamp === "number" ? parsed.timestamp : void 0) || Date.now();
      return { ...data, timestamp: ts };
    } catch (e) {
      return null;
    }
  }
  var HardwareTab = class {
    constructor(root, getWsUrl, setWsUrl, onWsUrlChanged, requestDiagnostic) {
      this.root = root;
      this.getWsUrl = getWsUrl;
      this.setWsUrl = setWsUrl;
      this.onWsUrlChanged = onWsUrlChanged;
      this.requestDiagnostic = requestDiagnostic;
      this.container = root.querySelector("#ots-hardware-content");
      if (!this.container) {
        throw new Error("Hardware tab container not found");
      }
      this.initializeDisplay();
    }
    /**
     * Create HTML for hardware tab
     */
    static createHTML() {
      return `
      <div id="ots-tab-hardware" class="ots-tab-content" style="display:none;flex:1;overflow-y:auto;padding:8px;">
        <div id="ots-hardware-content"></div>
      </div>
    `;
    }
    initializeDisplay() {
      this.container.innerHTML = `
      <div style="margin-bottom:16px;padding:10px;background:rgba(59,130,246,0.1);border-left:3px solid #3b82f6;border-radius:4px;">
        <div style="font-size:11px;font-weight:600;color:#93c5fd;margin-bottom:8px;">\u2699\uFE0F DEVICE CONNECTION</div>
        <label style="display:flex;flex-direction:column;gap:4px;font-size:10px;color:#e5e7eb;">
          <span style="font-size:9px;font-weight:600;color:#9ca3af;letter-spacing:0.05em;">WEBSOCKET URL</span>
          <input id="ots-device-ws-url" type="text" placeholder="ws://ots-fw-main.local:3000/ws" value="${this.getWsUrl()}" style="font-size:10px;padding:6px 8px;border-radius:4px;border:1px solid rgba(148,163,184,0.4);background:rgba(15,23,42,0.7);color:#e5e7eb;outline:none;font-family:monospace;" />
          <span style="font-size:8px;color:#64748b;">Connect to firmware device or ots-simulator</span>
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
      this.attachListeners();
    }
    attachListeners() {
      const wsUrlInput = this.container.querySelector("#ots-device-ws-url");
      wsUrlInput == null ? void 0 : wsUrlInput.addEventListener("change", () => {
        const newUrl = wsUrlInput.value.trim();
        if (newUrl) {
          this.setWsUrl(newUrl);
          this.onWsUrlChanged();
        }
      });
      const refreshBtn = this.container.querySelector("#ots-refresh-device-info");
      refreshBtn == null ? void 0 : refreshBtn.addEventListener("click", () => this.requestDiagnostic());
    }
    updateDisplay(diagnostic) {
      if (!diagnostic) return;
      const data = diagnostic;
      const hardware = isRecord(data.hardware) ? data.hardware : {};
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
      this.container.innerHTML = `
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
        Last updated: ${new Date(typeof data.timestamp === "number" ? data.timestamp : Date.now()).toLocaleTimeString()}
      </div>
    `;
    }
  };

  // src/hud/sidebar/tabs/sound-tab.ts
  var SoundTab = class {
    constructor(root, soundToggles, logInfo, onSoundTest) {
      this.root = root;
      this.soundToggles = soundToggles;
      this.logInfo = logInfo;
      this.onSoundTest = onSoundTest;
      this.container = root.querySelector("#ots-sound-toggles");
      if (!this.container) {
        throw new Error("Sound tab container not found");
      }
      this.initializeToggles();
    }
    /**
     * Create HTML for sound tab
     */
    static createHTML() {
      return `
      <div id="ots-tab-sound" class="ots-tab-content" style="display:none;flex:1;overflow-y:auto;padding:8px;">
        <div id="ots-sound-toggles"></div>
      </div>
    `;
    }
    initializeToggles() {
      const soundIds = ["game_start", "game_player_death", "game_victory", "game_defeat"];
      const soundLabels = {
        game_start: "Game Start",
        game_player_death: "Player Death",
        game_victory: "Victory",
        game_defeat: "Defeat"
      };
      this.container.innerHTML = soundIds.map(
        (id) => `
      <div style="display:flex;align-items:center;gap:8px;margin-bottom:6px;padding:6px 8px;border-radius:4px;background:rgba(255,255,255,0.05);transition:background 0.2s;" data-sound-row="${id}">
        <label style="display:flex;align-items:center;gap:8px;font-size:11px;color:#e5e7eb;cursor:pointer;flex:1;">
          <input type="checkbox" data-sound-toggle="${id}" ${this.soundToggles[id] ? "checked" : ""} style="cursor:pointer;" />
          <span style="flex:1;">${soundLabels[id]}</span>
          <code style="font-size:9px;color:#94a3b8;font-family:monospace;">${id}</code>
        </label>
        <button data-sound-test="${id}" style="all:unset;cursor:pointer;font-size:11px;padding:4px 10px;border-radius:4px;background:rgba(59,130,246,0.3);color:#93c5fd;font-weight:600;transition:background 0.2s;white-space:nowrap;">\u25B6 Test</button>
      </div>
    `
      ).join("");
      this.attachListeners();
    }
    attachListeners() {
      this.container.querySelectorAll("[data-sound-toggle]").forEach((checkbox) => {
        checkbox.addEventListener("change", (e) => {
          const input = e.target;
          const soundId = input.dataset.soundToggle;
          this.soundToggles[soundId] = input.checked;
          this.logInfo(`Sound ${soundId}: ${this.soundToggles[soundId] ? "enabled" : "disabled"}`);
        });
      });
      this.container.querySelectorAll("[data-sound-test]").forEach((button) => {
        button.addEventListener("click", () => {
          var _a;
          const soundId = button.dataset.soundTest;
          this.logInfo(`Testing sound: ${soundId}`);
          (_a = this.onSoundTest) == null ? void 0 : _a.call(this, soundId);
        });
        button.addEventListener("mouseenter", () => {
          ;
          button.style.background = "rgba(59,130,246,0.5)";
        });
        button.addEventListener("mouseleave", () => {
          ;
          button.style.background = "rgba(59,130,246,0.3)";
        });
      });
      this.container.querySelectorAll("[data-sound-row]").forEach((row) => {
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
  };

  // src/storage/keys.ts
  var STORAGE_KEYS = {
    WS_URL: "ots-ws-url",
    HUD_COLLAPSED: "ots-hud-collapsed",
    HUD_SNAP: "ots-hud-snap",
    HUD_SIZE: "ots-hud-size",
    LOG_FILTERS: "ots-hud-log-filters",
    SOUND_TOGGLES: "ots-hud-sound-toggles"
  };

  // src/hud/sidebar-hud.ts
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
      this.positionManager = null;
      this.wsDot = null;
      this.gameDot = null;
      this.body = null;
      this.settingsPanel = null;
      this.filterPanel = null;
      this.logFilters = DEFAULT_LOG_FILTERS;
      // Tab components
      this.tabManager = null;
      this.logsTab = null;
      this.hardwareTab = null;
      this.soundTab = null;
      this.hardwareDiagnostic = null;
      this.soundToggles = DEFAULT_SOUND_TOGGLES;
      this.collapsed = GM_getValue(STORAGE_KEYS.HUD_COLLAPSED, true);
    }
    ensure() {
      if (this.closed || this.root) return;
      this.createHud();
      this.setupEventListeners();
      if (this.root && this.titlebar && this.positionManager) {
        this.positionManager.setContainers(this.content, this.body);
        this.positionManager.initialize(this.collapsed);
        this.positionManager.updateArrows(this.collapsed);
      }
      this.updateContentVisibility();
    }
    createHud() {
      const root = HudTemplate.createRoot();
      const titlebar = HudTemplate.createTitlebar();
      const content = HudTemplate.createContent();
      this.tabManager = new TabManager({
        container: content,
        tabs: [
          { id: "logs", label: "Logs", contentId: "ots-tab-logs" },
          { id: "hardware", label: "Hardware", contentId: "ots-tab-hardware" },
          { id: "sound", label: "Sound", contentId: "ots-tab-sound" }
        ],
        defaultTab: "logs",
        onTabChange: (tabId) => this.handleTabChange(tabId)
      });
      content.innerHTML = HudTemplate.createContentHTML(this.tabManager);
      root.appendChild(titlebar);
      root.appendChild(content);
      document.body.appendChild(root);
      const savedFilters = GM_getValue(STORAGE_KEYS.LOG_FILTERS, null);
      if (savedFilters) {
        this.logFilters = savedFilters;
      }
      this.settingsPanel = new SettingsPanel(
        () => {
        },
        // onClose callback (no action needed)
        (url) => {
          this.setWsUrl(url);
          this.logInfo(`WS URL updated to ${url}`);
          this.onWsUrlChanged();
        }
      );
      this.filterPanel = new FilterPanel(
        () => {
        },
        // onClose callback (no action needed)
        (filters) => {
          var _a;
          this.logFilters = filters;
          GM_setValue(STORAGE_KEYS.LOG_FILTERS, this.logFilters);
          (_a = this.logsTab) == null ? void 0 : _a.updateFilters(this.logFilters);
          this.refilterLogs();
        },
        this.logFilters
      );
      this.root = root;
      this.titlebar = titlebar;
      this.content = content;
      this.body = root.querySelector("#ots-hud-body");
      this.wsDot = root.querySelector("#ots-hud-ws-dot");
      this.gameDot = root.querySelector("#ots-hud-game-dot");
      this.logsTab = new LogsTab(
        () => this.toggleFilter(),
        this.logFilters
      );
      const savedPosition = GM_getValue(STORAGE_KEYS.HUD_SNAP, "right");
      const savedSize = GM_getValue(STORAGE_KEYS.HUD_SIZE, { width: 320, height: 260 });
      this.positionManager = new PositionManager(
        root,
        titlebar,
        savedPosition,
        savedSize,
        {
          onPositionChange: (position) => {
            var _a, _b;
            GM_setValue(STORAGE_KEYS.HUD_SNAP, position);
            (_a = this.positionManager) == null ? void 0 : _a.applyPosition(position, this.collapsed);
            (_b = this.positionManager) == null ? void 0 : _b.updateArrows(this.collapsed);
          },
          onSizeChange: (size) => {
            GM_setValue(STORAGE_KEYS.HUD_SIZE, size);
          }
        }
      );
      if (this.tabManager) {
        this.tabManager.attachListeners();
      }
    }
    setupEventListeners() {
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
      this.titlebar.addEventListener("click", (e) => {
        if (e.target.tagName === "BUTTON" || e.target.closest("button")) return;
        this.collapsed = !this.collapsed;
        GM_setValue(STORAGE_KEYS.HUD_COLLAPSED, this.collapsed);
        this.updateContentVisibility();
        if (this.positionManager) {
          this.positionManager.updateArrows(this.collapsed);
        }
      });
      if (this.root) {
        this.hardwareTab = new HardwareTab(
          this.root,
          this.getWsUrl,
          (url) => {
            this.setWsUrl(url);
            this.logInfo(`Device WS URL updated to ${url}`);
          },
          this.onWsUrlChanged,
          () => this.requestDiagnostic()
        );
        this.soundTab = new SoundTab(
          this.root,
          this.soundToggles,
          (t) => this.logInfo(t),
          this.onSoundTest
        );
      }
    }
    updateContentVisibility() {
      if (!this.content) return;
      if (this.collapsed) {
        this.content.style.display = "none";
      } else {
        this.content.style.display = "flex";
      }
      if (this.positionManager) {
        this.positionManager.applyPosition(this.positionManager.getPosition(), this.collapsed);
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
      if (!this.settingsPanel || !this.root || !this.positionManager) return;
      if (!this.settingsPanel.isVisible()) {
        const hudRect = this.root.getBoundingClientRect();
        const currentPosition = this.positionManager.getPosition();
        this.settingsPanel.positionNextToHud(hudRect, currentPosition);
        this.settingsPanel.show(this.getWsUrl());
        if (this.filterPanel) this.filterPanel.hide();
      } else {
        this.settingsPanel.hide();
      }
    }
    toggleFilter() {
      if (!this.filterPanel || !this.root || !this.positionManager) return;
      if (!this.filterPanel.isVisible()) {
        const hudRect = this.root.getBoundingClientRect();
        const currentPosition = this.positionManager.getPosition();
        this.filterPanel.positionNextToHud(hudRect, currentPosition);
        this.filterPanel.show();
        if (this.settingsPanel) this.settingsPanel.hide();
      } else {
        this.filterPanel.hide();
      }
    }
    refilterLogs() {
      var _a;
      (_a = this.logsTab) == null ? void 0 : _a.refilterLogs();
    }
    /**
     * Handle tab change callback from TabManager
     */
    handleTabChange(tabId) {
      if (tabId === "hardware" && this.hardwareTab && this.hardwareDiagnostic) {
        this.hardwareTab.updateDisplay(this.hardwareDiagnostic);
      }
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
        this.settingsPanel.hide();
      }
      if (this.filterPanel) {
        this.filterPanel.hide();
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
    logSend(text, eventType, jsonData) {
      var _a;
      (_a = this.logsTab) == null ? void 0 : _a.pushLog("send", text, eventType, jsonData);
    }
    logRecv(text, eventType, jsonData) {
      var _a, _b, _c;
      if (eventType === "HARDWARE_DIAGNOSTIC") {
        const captured = tryCaptureHardwareDiagnostic(text);
        if (captured) {
          console.log("[OTS HUD] Hardware diagnostic captured:", captured);
          this.hardwareDiagnostic = captured;
          this.logInfo("Hardware diagnostic data captured - switch to Hardware tab to view");
          if (((_a = this.tabManager) == null ? void 0 : _a.getActiveTab()) === "hardware") {
            (_b = this.hardwareTab) == null ? void 0 : _b.updateDisplay(this.hardwareDiagnostic);
          }
        } else {
          console.warn("[OTS HUD] Failed to capture HARDWARE_DIAGNOSTIC:", text);
        }
      }
      (_c = this.logsTab) == null ? void 0 : _c.pushLog("recv", text, eventType, jsonData);
    }
    logInfo(text) {
      var _a;
      (_a = this.logsTab) == null ? void 0 : _a.pushLog("info", text);
    }
    // Alias for compatibility with WsClient
    pushLog(direction, text, eventType, jsonData) {
      var _a, _b, _c;
      if (direction === "recv" && eventType === "HARDWARE_DIAGNOSTIC") {
        const captured = tryCaptureHardwareDiagnostic(text);
        if (captured) {
          console.log("[OTS HUD] Hardware diagnostic captured:", captured);
          this.hardwareDiagnostic = captured;
          this.logInfo("Hardware diagnostic data captured - switch to Hardware tab to view");
          if (((_a = this.tabManager) == null ? void 0 : _a.getActiveTab()) === "hardware") {
            (_b = this.hardwareTab) == null ? void 0 : _b.updateDisplay(this.hardwareDiagnostic);
          }
        } else {
          console.warn("[OTS HUD] Failed to capture HARDWARE_DIAGNOSTIC:", text);
        }
      }
      (_c = this.logsTab) == null ? void 0 : _c.pushLog(direction, text, eventType, jsonData);
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

  // src/game/constants.ts
  var CONTROL_PANEL_SELECTOR = "control-panel";
  var GAME_POLL_INTERVAL_MS = 100;
  var GAME_INSTANCE_CHECK_INTERVAL_MS = 100;
  var API_CACHE_DURATION_MS = 1e3;
  var GAME_UPDATE_TYPE = {
    WIN: 13
    // GameUpdateType.Win = 13
    // Add other types as needed
  };
  var WS_CLOSE_CODE_URL_CHANGED = 4100;
  var DEFAULT_RECONNECT_DELAY_MS = 2e3;

  // src/websocket/client.ts
  function isRecord2(value) {
    return typeof value === "object" && value !== null;
  }
  function parseWsMessage(raw) {
    try {
      const parsed = JSON.parse(raw);
      if (!isRecord2(parsed)) return null;
      if (typeof parsed.type !== "string") return null;
      return parsed;
    } catch (e) {
      return null;
    }
  }
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
      this.reconnectDelay = DEFAULT_RECONNECT_DELAY_MS;
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
        this.reconnectDelay = DEFAULT_RECONNECT_DELAY_MS;
        const handshake = {
          type: "handshake",
          clientType: PROTOCOL_CONSTANTS.CLIENT_TYPE_USERSCRIPT
        };
        this.safeSend(handshake);
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
        let eventType;
        let parsedData = void 0;
        if (typeof event.data === "string") {
          const parsed = parseWsMessage(event.data);
          if ((parsed == null ? void 0 : parsed.type) === "event") {
            eventType = parsed.payload.type;
          }
          parsedData = parsed;
        }
        this.hud.pushLog("recv", typeof event.data === "string" ? event.data : "[binary message]", eventType, parsedData);
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
      if (!this.socket || this.socket.readyState !== WebSocket.OPEN) {
        debugLog("Cannot send, socket not open");
        this.hud.pushLog("info", "Cannot send, socket not open");
        return;
      }
      const json = JSON.stringify(msg);
      let eventType;
      if (isRecord2(msg) && msg.type === "event") {
        const payload = msg.payload;
        if (isRecord2(payload) && typeof payload.type === "string") {
          eventType = payload.type;
        }
      }
      this.hud.pushLog("send", json, eventType, msg);
      this.socket.send(json);
    }
    sendState(state) {
      const msg = { type: "state", payload: state };
      this.safeSend(msg);
    }
    sendEvent(type, message, data) {
      const msg = {
        type: "event",
        payload: {
          type,
          timestamp: Date.now(),
          message,
          data
        }
      };
      this.safeSend(msg);
    }
    sendInfo(message, data) {
      this.sendEvent("INFO", message, data);
    }
    sendCommand(action, params) {
      const msg = { type: "cmd", payload: { action, params } };
      this.safeSend(msg);
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
      const msg = parseWsMessage(raw);
      if (!msg) {
        debugLog("Invalid JSON from server", raw);
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

  // src/utils/logger.ts
  function createLogger(scope) {
    const prefix = `[${scope}]`;
    return {
      log: (...args) => console.log(prefix, ...args),
      warn: (...args) => console.warn(prefix, ...args),
      error: (...args) => console.error(prefix, ...args),
      /**
       * Log with custom level
       */
      logLevel: (level, ...args) => {
        console[level](prefix, ...args);
      },
      /**
       * Log success message with checkmark
       */
      success: (...args) => {
        console.log(`${prefix} \u2713`, ...args);
      },
      /**
       * Log failure message with cross
       */
      failure: (...args) => {
        console.log(`${prefix} \u2717`, ...args);
      },
      /**
       * Log debug message (only if conditions are met)
       */
      debug: (condition, ...args) => {
        if (condition) {
          console.log(`${prefix} [DEBUG]`, ...args);
        }
      }
    };
  }

  // src/game/game-api.ts
  function isFn(value) {
    return typeof value === "function";
  }
  function isRecord3(value) {
    return typeof value === "object" && value !== null;
  }
  function getGameView() {
    try {
      const eventsDisplay = document.querySelector("events-display");
      if (eventsDisplay && isRecord3(eventsDisplay) && "game" in eventsDisplay) {
        const maybeGame = eventsDisplay.game;
        if (maybeGame && isRecord3(maybeGame)) {
          return maybeGame;
        }
      }
    } catch (e) {
    }
    return null;
  }
  function createGameAPI() {
    let cachedGame = null;
    let lastCheck = 0;
    const CACHE_DURATION = API_CACHE_DURATION_MS;
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
          if (!game || !isFn(game.myPlayer)) return null;
          const myPlayer = game.myPlayer();
          return isRecord3(myPlayer) ? myPlayer : null;
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
          if (!game || !isFn(game.units)) return [];
          const units = game.units(...types);
          return Array.isArray(units) ? units : [];
        } catch (e) {
          return [];
        }
      },
      getTicks() {
        try {
          const game = getGame();
          if (!game || !isFn(game.ticks)) return null;
          const ticks = game.ticks();
          return typeof ticks === "number" ? ticks : null;
        } catch (e) {
          return null;
        }
      },
      getX(tile) {
        try {
          const game = getGame();
          if (!game || !isFn(game.x)) return null;
          const x = game.x(tile);
          return typeof x === "number" ? x : null;
        } catch (e) {
          return null;
        }
      },
      getY(tile) {
        try {
          const game = getGame();
          if (!game || !isFn(game.y)) return null;
          const y = game.y(tile);
          return typeof y === "number" ? y : null;
        } catch (e) {
          return null;
        }
      },
      getOwner(tile) {
        try {
          const game = getGame();
          if (!game || !isFn(game.owner)) return null;
          const owner = game.owner(tile);
          return isRecord3(owner) ? owner : null;
        } catch (e) {
          return null;
        }
      },
      getUnit(unitId) {
        try {
          const game = getGame();
          if (!game || !isFn(game.unit)) return null;
          const unit = game.unit(unitId);
          return isRecord3(unit) ? unit : null;
        } catch (e) {
          return null;
        }
      },
      getPlayerBySmallID(smallID) {
        try {
          const game = getGame();
          if (!game || !isFn(game.playerBySmallID)) return null;
          const player = game.playerBySmallID(smallID);
          return isRecord3(player) ? player : null;
        } catch (e) {
          return null;
        }
      },
      getCurrentTroops() {
        try {
          const myPlayer = this.getMyPlayer();
          if (!myPlayer || !isFn(myPlayer.troops)) return null;
          const troops = myPlayer.troops();
          return typeof troops === "number" ? troops : null;
        } catch (e) {
          return null;
        }
      },
      getMaxTroops() {
        try {
          const game = getGame();
          const myPlayer = this.getMyPlayer();
          if (!game || !myPlayer) return null;
          if (!isFn(game.config)) return null;
          const config = game.config();
          if (!isRecord3(config) || !isFn(config.maxTroops)) return null;
          const maxTroops = config.maxTroops(myPlayer);
          return typeof maxTroops === "number" ? maxTroops : null;
        } catch (e) {
          return null;
        }
      },
      getAttackRatio() {
        const controlPanel = document.querySelector("control-panel");
        const uiState = isRecord3(controlPanel) ? controlPanel.uiState : void 0;
        const ratio = isRecord3(uiState) ? uiState.attackRatio : void 0;
        if (typeof ratio === "number") {
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
          if (isFn(game.inSpawnPhase)) {
            const inSpawnPhase = game.inSpawnPhase();
            return typeof inSpawnPhase === "boolean" ? !inSpawnPhase : null;
          }
          if (isFn(game.ticks)) {
            const ticks = game.ticks();
            return typeof ticks === "number" ? ticks > 0 : null;
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
          if (isFn(myPlayer.hasSpawned)) {
            const spawned = myPlayer.hasSpawned();
            return typeof spawned === "boolean" ? spawned : null;
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
          if (!game || !isFn(game.updatesSinceLastTick)) return null;
          const updates = game.updatesSinceLastTick();
          return isRecord3(updates) ? updates : null;
        } catch (error) {
          console.error("[GameAPI] Error getting updates:", error);
          return null;
        }
      },
      didPlayerWin() {
        try {
          const game = getGame();
          if (!game || !isFn(game.myPlayer)) return null;
          const myPlayerUnknown = game.myPlayer();
          if (!isRecord3(myPlayerUnknown)) return null;
          const myPlayer = myPlayerUnknown;
          if (!isFn(game.gameOver)) return null;
          const gameOver = game.gameOver();
          if (gameOver !== true) return null;
          if (isFn(myPlayer.eliminated) && myPlayer.eliminated() === true) {
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

  // src/game/trackers/nuke-tracker.ts
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

  // src/game/trackers/boat-tracker.ts
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

  // src/game/trackers/land-tracker.ts
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
          setTimeout(attachInputListener, INPUT_LISTENER_RETRY_DELAY_MS);
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
  var logger = createLogger("GameBridge");
  function isRecord4(value) {
    return typeof value === "object" && value !== null;
  }
  function isSetAttackRatioParams(params) {
    return isRecord4(params) && typeof params.ratio === "number";
  }
  function isSendNukeParams(params) {
    return isRecord4(params) && (params.nukeType === "atom" || params.nukeType === "hydro" || params.nukeType === "mirv");
  }
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
      }, GAME_INSTANCE_CHECK_INTERVAL_MS);
    }
    pollForGameEnd(gameAPI) {
      try {
        const myPlayer = gameAPI.getMyPlayer();
        if (!myPlayer) return;
        const updates = gameAPI.getUpdatesSinceLastTick();
        if (updates && Object.keys(updates).length > 0) {
          const updateTypes = Object.keys(updates).filter((key) => {
            const value = updates[key];
            return Array.isArray(value) && value.length > 0;
          });
          if (updateTypes.length > 0) {
            console.log("[GameBridge] Update types this tick:", updateTypes.join(", "));
          }
        }
        if (updates && Array.isArray(updates[GAME_UPDATE_TYPE.WIN])) {
          const winUpdates = updates[GAME_UPDATE_TYPE.WIN];
          if (winUpdates.length > 0) {
            console.log("[GameBridge] Win updates detected (count: " + winUpdates.length + "):", JSON.stringify(winUpdates));
            if (this.hasProcessedWin) {
              console.log("[GameBridge] Already processed win, skipping");
              return;
            }
            console.log("[GameBridge] Inspecting all Win updates:");
            winUpdates.forEach((update, index) => {
              console.log(`[GameBridge] Win update [${index}]:`, update);
              if (typeof update === "object" && update !== null) {
                console.log(`[GameBridge] Win update [${index}] keys:`, Object.keys(update));
              }
            });
            let winUpdate = null;
            let winnerType = null;
            let winnerId = null;
            for (const update of winUpdates) {
              const u = typeof update === "object" && update !== null ? update : null;
              if (u && Array.isArray(u.winner)) {
                ;
                [winnerType, winnerId] = u.winner;
                winUpdate = u;
                console.log("[GameBridge] Found winner in update.winner:", winnerType, winnerId);
                break;
              } else if (u && u.winnerType !== void 0 && u.winnerId !== void 0) {
                winnerType = u.winnerType;
                winnerId = u.winnerId;
                winUpdate = u;
                console.log("[GameBridge] Found winner in separate fields:", winnerType, winnerId);
                break;
              } else if (u && typeof u.emoji === "object" && u.emoji !== null && Array.isArray(u.emoji.winner)) {
                const emoji = u.emoji;
                [winnerType, winnerId] = emoji.winner;
                winUpdate = u;
                console.log("[GameBridge] Found winner in emoji.winner:", winnerType, winnerId);
                break;
              } else if (u && u.team !== void 0) {
                winnerType = "team";
                winnerId = u.team;
                winUpdate = u;
                console.log("[GameBridge] Found winner in update.team:", winnerId);
                break;
              } else if (u && u.player !== void 0) {
                winnerType = "player";
                winnerId = u.player;
                winUpdate = u;
                console.log("[GameBridge] Found winner in update.player:", winnerId);
                break;
              } else if (u && typeof u.emoji === "object" && u.emoji !== null && u.emoji.recipientID !== void 0) {
                winnerType = "team";
                winnerId = u.emoji.recipientID;
                winUpdate = u;
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
          logger.failure("You died!");
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
      }, GAME_POLL_INTERVAL_MS);
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
      logger.log("Received command:", action, params);
      if (action === "send-nuke") {
        if (!isSendNukeParams(params)) {
          logger.error("send-nuke command missing nukeType parameter");
          this.ws.sendEvent("ERROR", "send-nuke failed: missing nukeType", { params });
          return;
        }
        this.handleSendNuke(params);
      } else if (action === "set-attack-ratio") {
        if (!isSetAttackRatioParams(params)) {
          logger.error("set-attack-ratio command missing or invalid ratio parameter (expected 0-1)");
          this.ws.sendEvent("ERROR", "set-attack-ratio failed: invalid ratio", { params });
          return;
        }
        this.handleSetAttackRatio(params);
      } else if (action === "ping") {
        logger.log("Ping received");
      } else {
        logger.warn("Unknown command:", action);
        this.ws.sendEvent("ERROR", `Unknown command: ${action}`, { action, params });
      }
    }
    handleSetAttackRatio(params) {
      const ratio = params.ratio;
      if (ratio < 0 || ratio > 1) {
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
      const nukeType = params.nukeType;
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

  // src/game/victory-handler.ts
  var logger2 = createLogger("VictoryHandler");

  // src/storage/config.ts
  var DEFAULT_WS_URL = PROTOCOL_CONSTANTS.DEFAULT_WS_URL;
  function loadWsUrl() {
    const saved = GM_getValue(STORAGE_KEYS.WS_URL, null);
    if (typeof saved === "string" && saved.trim()) {
      return saved;
    }
    return DEFAULT_WS_URL;
  }
  function saveWsUrl(url) {
    GM_setValue(STORAGE_KEYS.WS_URL, url);
  }

  // src/main.user.ts
  var VERSION = "2026-01-10.1";
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
        ws.disconnect(WS_CLOSE_CODE_URL_CHANGED, "URL changed");
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
