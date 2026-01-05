---
layout: home

hero:
  name: "OpenFront Tactical Suitcase"
  text: "Hardware Controller for OpenFront.io"
  tagline: Physical controls for the browser-based strategy game
  actions:
    - theme: brand
      text: Quick Start
      link: /user/quick-start
    - theme: alt
      text: Downloads
      link: /downloads
    - theme: alt
      text: View on GitHub
      link: https://github.com/docteurzoidberg/openfront-tactical-suitcase

features:
  - title: Physical Controls
    details: Launch nukes, view alerts, and control troops with tactile buttons and displays
  - title: WebSocket Integration
    details: Real-time connection to OpenFront.io game via userscript and WebSocket server
  - title: Modular Hardware
    details: ESP32-S3 based controller with expandable modules for different game functions
  - title: Open Source
    details: Complete hardware specs, firmware, and server code available on GitHub
---

## What is OTS?

The OpenFront Tactical Suitcase (OTS) is a physical hardware controller for the browser-based strategy game [OpenFront.io](https://openfront.io). It bridges the gap between digital gameplay and physical interaction with dedicated buttons, LEDs, and displays.

## Components

- **ots-fw-main**: ESP32-S3 firmware for the main controller
- **ots-server**: Nuxt dashboard and WebSocket server
- **ots-userscript**: Browser userscript that bridges game state to hardware
- **ots-hardware**: Hardware specifications and module designs

## Quick Links

- [User Documentation](/user/)
- [Developer Documentation](/developer/)
- [Downloads](/downloads) - Firmware, userscript, and hardware files
- [Releases](/releases) - Release history and changelogs
- [Hardware Specifications](https://github.com/docteurzoidberg/openfront-tactical-suitcase/tree/main/ots-hardware)
