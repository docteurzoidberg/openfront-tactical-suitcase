# Project Context – Nuxt 4 Game Dashboard + WebSocket + Userscript

You are GitHub Copilot, assisting on a **Nuxt 4 + TypeScript** project with a separate **TypeScript userscript** subproject.

The goal of this repository is:

- To build a **real-time dashboard UI** (Vue 3 + Nuxt 4 + TypeScript) for a browser game.
- The dashboard receives **game state and events** from a **userscript** running on the game website.
- Communication between userscript and dashboard happens via **WebSocket(s)** handled by **Nuxt/Nitro**.

The Nuxt app and userscript live in separate folders (`ots-server`, `ots-userscript`, `ots-shared`). You must evolve them with the specs below.

---

## High-level architecture

### 1. Nuxt 4 app (UI + server)

- Framework: **Nuxt 4**, **Vue 3**, **TypeScript**.
- Nitro is used as the **HTTP server and WebSocket server**.
- The Nuxt app exposes at least:
  - One WebSocket endpoint for the **userscript** (e.g. `/ws-script`).
  - One WebSocket endpoint for the **dashboard UI** (e.g. `/ws-ui`).
- Nitro’s **experimental websocket support** is enabled with:
```txt
repo root/
  ots-shared/             # shared TS-only folder (no package.json at root)
    src/
      game.ts             # Shared types for game state + events (imported via relative paths)

  ots-server/             # Nuxt 4 app (dashboard + Nitro server)
    copilot-project-context.md  # Detailed context for the server project
    nuxt.config.ts
    app/
      app.vue
      pages/
        index.vue         # Main dashboard page
    composables/
      useGameSocket.ts    # WebSocket client for the dashboard UI (imports ../../ots-shared/src/game)
    server/
      routes/
        ws-script.ts      # WebSocket handler for userscript (imports shared types from ../../ots-shared/src/game)
        ws-ui.ts          # WebSocket handler for dashboard UI (imports shared types from ../../ots-shared/src/game)
        api/state.get.ts  # (optional) HTTP endpoint to fetch current state

  ots-userscript/         # standalone TS userscript project (see ots-userscript/copilot-project-context.md)
    package.json
    tsconfig.json         # imports from ../ots-shared/src/game via relative path
    src/
      main.user.ts        # Tampermonkey entry, uses shared types via ../../ots-shared/src/game
    build/
      userscript.ots.user.js  # built userscript bundle (output of npm run build)
```

There is **no root-level package.json**. Each subproject is managed independently:

- To run the Nuxt app:
  - `cd ots-server`
  - `npm install`
  - `npm run dev` (or `build`, `generate`, `preview`).
- To work on the userscript:
  - See `ots-userscript/copilot-project-context.md` for detailed behavior and build/install steps.

Copilot should keep the protocol and shared types compatible across server, dashboard UI, and userscript, and avoid breaking changes when evolving code.

---

## Technologies and conventions

- **Language**: TypeScript everywhere.
- **UI**: Vue 3 with `<script setup lang="ts">`.
- **Styling**: use nuxt-ui (it alreadyincludes tailwindcss)
- **State management**: For now, use local component state or simple composables. No Vuex/Pinia unless explicitly requested.
- **WebSocket client**: Use `@vueuse/core`’s `useWebSocket` on the client side.
- **WebSocket server**: Use Nitro’s `defineWebSocketHandler` with channels (`peer.subscribe`, `peer.publish`).

Prefer **clear, strongly typed data structures** and avoid `any` whenever possible.

---

## Desired files and structure (within the Nuxt project)

Copilot should help maintain or create the following structure.

```txt
root/
  nuxt.config.ts

  types/
    game.ts        # Shared types for game state + events
  composables/
    useGameSocket.ts  # WebSocket client for the dashboard UI
  app/
    app.vue
    pages/
      index.vue      # Main dashboard page
  server/
    routes/
      ws-script.ts # WebSocket handler for userscript
      ws-ui.ts     # WebSocket handler for dashboard UI
      api/state.get.ts  # (optional) HTTP endpoint to fetch current state

  # userscript is NOT built by Nuxt, but lives in its own TS package.

  userscript/
    package.json          # small subpackage for the userscript build
    tsconfig.json         # TS config that can import from ../types/game
    src/
      main.user.ts        # main userscript entry (Tampermonkey header + code)

  userscript.ots.js       # built userscript bundle (esbuild or similar)
```

### `types/game.ts`

Define shared types for the messages exchanged via WebSocket.

Example (Copilot may refine based on usage):

```ts
export type TeamScore = {
  teamA: number
  teamB: number
}

export type PlayerInfo = {
  id: string
  name: string
  clanTag?: string
  isAlly: boolean
  score: number
}

export type GameState = {
  timestamp: number
  mapName: string
  mode: string
  players: PlayerInfo[]
  score: TeamScore
}

export type GameEventType =
  | 'KILL'
  | 'DEATH'
  | 'OBJECTIVE'
  | 'INFO'
  | 'ERROR'

export type GameEvent = {
  type: GameEventType
  timestamp: number
  message: string
  data?: unknown
}

export type IncomingMessage =
  | { type: 'state'; payload: GameState }
  | { type: 'event'; payload: GameEvent }

export type OutgoingMessage =
  | { type: 'cmd'; payload: { action: string; params?: unknown } }
  | { type: 'ack'; payload?: unknown }
```

Copilot should keep these types **central** and re-use them in server and client code.

---

## Server-side WebSocket handlers (Nitro)

Use `defineWebSocketHandler` from Nitro.

### `server/routes/ws-script.ts`

Responsibilities:

- Accept WebSocket connections from the **userscript**.
- Parse incoming JSON messages as `IncomingMessage`.
- Broadcast:
  - game events → to the `"ui"` channel
  - game state → to the `"ui"` channel + store the newest state in memory
- Optionally handle commands from UI (via channel) back to the userscript.

Implementation hints:

- Use a module-level variable to store the latest `GameState`.
- Use `peer.publish('ui', rawMessageText)` so that UI peers subscribed to `"ui"` receive the broadcast.
- Log basic connection info (`open`, `close`, `error`).

### `server/routes/ws-ui.ts`

Responsibilities:

- Accept WebSocket connections from the **dashboard UI**.
- Subscribe to channel `"ui"` to receive messages coming from `ws-script`.
- On new connection:
  - Optionally send the latest stored `GameState` as an initial message.
- Accept messages from UI:
  - Typically `OutgoingMessage` such as `{ type: 'cmd', ... }`.
  - Broadcast commands over a `"script"`
