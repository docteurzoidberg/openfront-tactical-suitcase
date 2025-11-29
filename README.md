# Openfront Tactical Suitcase

Monorepo for the Openfront Tactical Suitcase project, including the web UI, shared game logic, and a browser userscript HUD.

## Packages

- `ots-server/`: Nuxt-based web UI and API server.
- `ots-shared/`: Shared TypeScript game logic and types.
- `ots-userscript/`: Browser userscript HUD built with TypeScript and esbuild.

## Getting Started

From the repo root:

```bash
cd ots-server
npm install
```

### Run the web UI / server

From `ots-server/`:

```bash
npm run dev
```

The dev server runs on `http://localhost:3000` by default.

### Build shared package

From `ots-shared/`:

```bash
npm install
npm run build
```

### Build userscript

From `ots-userscript/`:

```bash
npm install
npm run build
```

The bundled userscript is output to `ots-userscript/build/`.

## Repository Structure

- `ots-server/` – Nuxt app, server routes, and UI components.
- `ots-shared/` – Reusable game engine and shared types.
- `ots-userscript/` – Userscript HUD integration and build scripts.

## License

TODO: Add license information for this project.
