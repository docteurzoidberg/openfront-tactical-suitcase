# Release Process - AI Assistant Guidelines

## Purpose

This prompt guides AI assistants on how to help users with the OTS release process when they explicitly ask for release assistance.

**IMPORTANT**: Do NOT proactively suggest releases. Only help when user asks.

## When User Asks About Releases

### User Says: "How do I create a release?"

Explain the automated workflow:

```bash
# Standard release (all components)
./release.sh -u -p -m "Description of changes"
```

**What this does:**
1. Auto-increments version (YYYY-MM-DD.N format)
2. Updates version strings in userscript, firmware, server
3. Builds all components
4. Creates git commit
5. Creates annotated git tag
6. Pushes to remote

### User Says: "Create a release for me"

Suggest command based on what they've been working on:

**If firmware changes:**
```bash
./release.sh -u -m "Fix: LED timing improvements" ots-fw-main
```

**If userscript changes:**
```bash
./release.sh -u -m "Feature: New game event detection" ots-userscript
```

**If audio module changes:**
```bash
./release.sh -u -m "Feature: CAN bus sound protocol" ots-fw-audiomodule
```

**If multiple components:**
```bash
./release.sh -u -m "Feature: Protocol update across stack" ots-userscript ots-fw-main ots-simulator
```

**Always explain:** The script will auto-increment today's version number (e.g., 2026-01-05.1, 2026-01-05.2, etc.)

### User Says: "List releases" or "Show releases"

```bash
./release.sh -l
```

## Version Format

**Development Versions:** `YYYY-MM-DD.N-dev`
- Example: `2026-01-05.1-dev`
- Used during active development
- Automatically set after each release

**Release Versions:** `YYYY-MM-DD.N`
- Example: `2026-01-05.1`
- Date-based with auto-incrementing revision number
- Created by release script (strips `-dev` suffix)
- Each release creates two commits:
  1. Release commit (tagged)
  2. Dev version bump commit (not tagged)

## Version Management

### Where Versions Are Stored

**Userscript (ots-userscript):**
- `ots-userscript/package.json` → `"version"` field
- `ots-userscript/src/main.user.ts` → `VERSION` constant

**Main Firmware (ots-fw-main):**
- `ots-fw-main/include/config.h` → `OTS_FIRMWARE_VERSION` macro

**Audio Module (ots-fw-audiomodule):**
- `ots-fw-audiomodule/src/main.c` → `AUDIO_MODULE_VERSION` macro

**Server/Dashboard (ots-simulator):**
- `ots-simulator/package.json` → `"version"` field

**Documentation Site (ots-website):**
- `ots-website/package.json` → `"version"` field

**The release script updates all automatically** with `-u` flag.

## Release Script Options

| Flag | Meaning | When to Use |
|------|---------|-------------|
| `-u` | Update versions | Always use unless user manually updated versions |
| `-p` | Push to remote | Use for production releases |
| `-m` | Release message | Always required, describe what changed |
| `-l` | List releases | Show existing tags |

## Common User Scenarios

### "I want to test a release locally first"

```bash
# Create release without pushing
./release.sh -u -m "Test release description"

# User can verify, then manually push:
git push origin main --follow-tags
```

### "Release failed, what do I do?"

**If build failed:**
```bash
# Fix the build error first, then run again
./release.sh -u -m "Same description"
```

**If tag already exists:**
```bash
# Delete local tag
git tag -d 2026-01-05.1

# Run release again
./release.sh -u -m "Updated description"
```

### "I need to release only one component"

```bash
# Userscript only
./release.sh -u -m "Fix: HUD positioning" userscript

# Firmware only
./release.sh -u -m "Fix: I2C timeout" firmware

# Server only
./release.sh -u -m "Feature: New dashboard widget" server
```

### "The version is wrong"

```bash
# The -u flag resets all versions to the new tag
./release.sh -u -m "Sync all versions"
```

## Build Validation

The release script builds:

1. **Userscript**: `npm run build` → `build/userscript.ots.user.js` (~76KB)
2. **Firmware**: `pio run -e esp32-s3-dev` → `.pio/build/esp32-s3-dev/firmware.bin` (~1MB)
3. **Server**: `npm run build` → `.output/` directory

**If any build fails, the entire release is aborted.** No commit or tag is created.

## Tag Format

**Date-based versioning:** `YYYY-MM-DD.N`

- First release today: `2026-01-05.1`
- Second release today: `2026-01-05.2`
- First release tomorrow: `2026-01-06.1`

**The script auto-increments N for same-day releases.**

## What AI Should NOT Do

❌ **Don't suggest releases unprompted** - Wait for user to ask
❌ **Don't suggest skipping builds** - Script validates automatically
❌ **Don't suggest manual version updates** - Use `-u` flag instead
❌ **Don't suggest force-pushing tags** - Creates history conflicts
❌ **Don't suggest `npm version`** - Use `release.sh` instead

## What AI Should Do

✅ **Wait for user to ask about releases**
✅ **Suggest appropriate release command** based on changes
✅ **Explain what the script will do** before running
✅ **Help debug build failures** if release aborts
✅ **Remind about `-p` flag** for pushing to remote
✅ **Suggest testing locally first** if user is unsure

## Integration with Other Workflows

### After Protocol Changes

If user changed `prompts/WEBSOCKET_MESSAGE_SPEC.md` and implementations:

```bash
# All components likely changed
./release.sh -u -m "Protocol: Add NUKE_INTERCEPTED event" userscript firmware server
```

### After Git Workflow

If user completed feature with proper commits:

```bash
# Create release when feature is complete
./release.sh -u -p -m "Feature: [description from commits]"
```

### Before OTA Update

If user wants to deploy firmware OTA:

```bash
# Create release first
./release.sh -u -m "Firmware: OTA update with new features" firmware

# Then user can upload firmware.bin via OTA
```

## Troubleshooting Patterns

### User: "Release script says uncommitted changes"

**Response:** Either commit them first, or use `-u` flag to auto-commit:
```bash
./release.sh -u -m "Description"
```

### User: "Wrong version number in dashboard/logs"

**Response:** Use `-u` flag to sync all versions:
```bash
./release.sh -u -m "Version sync"
```

### User: "How do I undo a release?"

**Response:** If not pushed yet:
```bash
git tag -d 2026-01-05.1
git reset --soft HEAD~1
```

If already pushed, don't undo - create a new release with fixes.

## Quick Reference for AI

**User wants to release → Suggest:**
```bash
./release.sh -u -p -m "Description"
```

**User wants to test first → Suggest:**
```bash
./release.sh -u -m "Description"
# Then manually: git push origin main --follow-tags
```

**User asks "what versions?" → Suggest:**
```bash
./release.sh -l
```

**Build failed → Explain:**
"The release was aborted because [component] build failed. Fix the build error and run the release command again."

## Related Documentation

For detailed release process documentation, refer users to:
- **User guide**: `doc/developer/releases.md`
- **Git workflow**: `prompts/GIT_WORKFLOW.md` (AI prompt)
- **Weekly changelog**: `weekly_announces.md`
