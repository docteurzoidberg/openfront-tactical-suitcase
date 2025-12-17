# OTS Shared Types

**Source-only TypeScript types** shared across OTS projects.

## Purpose

This folder provides the **TypeScript implementation** of types defined in `/protocol-context.md`. It serves as the single source of truth for TypeScript projects to prevent type duplication.

## Architecture

- **No build step** - projects import source files directly
- **No npm package** - just a shared folder with TypeScript source
- **Type definitions only** - no runtime code

## Used By

- **ots-server** - Nuxt server routes and composables
- **ots-userscript** - Browser userscript for OpenFront.io

## Usage

Projects import types directly from source:

```typescript
import type { GameEvent, GameState, IncomingMessage } from '../../../ots-shared/src/game'
```

## Files

- `src/game.ts` - Game state, events, WebSocket protocol types
- `src/index.ts` - Re-exports all types

## Synchronization

When updating protocol:

1. **Update source of truth**: `/protocol-context.md` (root)
2. **Update TypeScript types**: `ots-shared/src/game.ts` (this folder)
3. **Update firmware types**: `ots-fw-main/include/protocol.h` (C)

All three must stay in sync.

## Why Not Build?

The current approach (direct source imports) works perfectly for:
- ✅ Development workflow - instant type updates
- ✅ Monorepo structure - shared types without package complexity
- ✅ TypeScript projects only - no runtime distribution needed

A build step would add unnecessary complexity without benefits.
