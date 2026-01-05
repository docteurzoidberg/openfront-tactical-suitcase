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
2. Strips `-dev` suffix from all project versions
3. Updates version strings in all 5 projects (ots-userscript, ots-fw-main, ots-fw-audiomodule, ots-simulator, ots-website)
4. Builds all components
5. Creates git commit (tagged)
6. Auto-bumps to next dev version (N+1-dev)
7. Creates second commit (not tagged)
8. Optionally pushes to remote

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

### Command Syntax

```bash
./release.sh [OPTIONS] [PROJECT...]
```

**Pattern for AI agents:**
```bash
./release.sh -u -m "Description" [project1] [project2] ...
```

**Key rules:**
- `-m "message"` is REQUIRED (except with `-l` or `-h`)
- `-u` is REQUIRED to update version files (almost always needed)
- `-p` is OPTIONAL (adds push to remote after tagging)
- Projects are OPTIONAL (defaults to `all` if omitted)
- Multiple projects can be specified: `ots-userscript ots-fw-main`

**Most common usage:**
```bash
./release.sh -u -m "Brief description of changes" all
```

### Available Projects

| Project Name | Short Alias | Updates |
|--------------|-------------|----------|
| `ots-userscript` | `userscript`, `us` | Userscript for browser |
| `ots-fw-main` | `firmware`, `fw` | Main ESP32-S3 controller |
| `ots-fw-audiomodule` | `audiomodule`, `am` | ESP32-A1S audio module |
| `ots-simulator` | `server`, `sv` | Dashboard/backend server |
| `ots-website` | `website`, `ws` | VitePress documentation |
| `all` | (default) | All 5 projects |

### Command Flags

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
./release.sh -u -m "Fix: HUD positioning" ots-userscript

# Main firmware only
./release.sh -u -m "Fix: I2C timeout" ots-fw-main

# Audio module only
./release.sh -u -m "Fix: CAN bus stability" ots-fw-audiomodule

# Server/dashboard only
./release.sh -u -m "Feature: New dashboard widget" ots-simulator

# Website only
./release.sh -u -m "Docs: Update installation guide" ots-website
```

### "The version is wrong"

```bash
# The -u flag resets all versions to the new tag
./release.sh -u -m "Sync all versions"
```

## Build Validation

The release script builds all selected projects:

1. **ots-userscript**: `npm run build` → `build/userscript.ots.user.js` (~76KB)
2. **ots-fw-main**: `pio run -e esp32-s3-dev` → `.pio/build/esp32-s3-dev/firmware.bin` (~1MB)
3. **ots-fw-audiomodule**: `pio run -e esp32-a1s-espidf` → `.pio/build/esp32-a1s-espidf/firmware.bin` (~800KB)
4. **ots-simulator**: `npm run build` → `.output/` directory
5. **ots-website**: `npm run build` → `.vitepress/dist/` directory

**If any build fails:**
- The entire release is aborted
- No commit or tag is created
- **Last 30 lines of build output are displayed** for debugging
- Version files remain updated (can retry after fixing)

**Build output visibility:**
The script now captures and displays build errors automatically, making it easy to identify issues without manually rebuilding.

## Tag Format

**Date-based versioning:** `YYYY-MM-DD.N`

- First release today: `2026-01-05.1`
- Second release today: `2026-01-05.2`
- First release tomorrow: `2026-01-06.1`

**The script auto-increments N for same-day releases.**

## How AI Should Run Release Script

### Step-by-Step Process

When user requests a release, follow this pattern:

1. **Determine which projects changed**
   - Check recent commits or user description
   - Default to `all` if unsure

2. **Construct the command**
   ```bash
   ./release.sh -u -m "Brief description" [projects]
   ```

3. **Execute and monitor**
   - Run the command
   - If build fails, the script will show error output
   - Help user fix the error and retry

4. **Verify success**
   - Script will show: ✓ All builds successful
   - Script will show: ✓ Tag created: YYYY-MM-DD.N
   - Two commits created (release + dev bump)

### Example Decision Tree

**User says:** "I fixed a bug in the userscript"
**AI runs:** `./release.sh -u -m "Fix: HUD positioning bug" ots-userscript`

**User says:** "I updated the protocol across everything"
**AI runs:** `./release.sh -u -m "Protocol: Add new event types" all`

**User says:** "Make a release"
**AI runs:** `./release.sh -u -m "Release: [summarize recent changes]" all`

**User says:** "Release and push to GitHub"
**AI runs:** `./release.sh -u -p -m "Release: [description]" all`

## What AI Should NOT Do

❌ **Don't suggest releases unprompted** - Wait for user to ask
❌ **Don't omit `-u` flag** - Almost always required for version updates
❌ **Don't omit `-m` message** - Always required (script will fail without it)
❌ **Don't suggest skipping builds** - Script validates automatically
❌ **Don't suggest manual version updates** - Use `-u` flag instead
❌ **Don't suggest force-pushing tags** - Creates history conflicts
❌ **Don't suggest `npm version`** - Use `release.sh` instead
❌ **Don't run without checking recent changes** - Message should be meaningful

## What AI Should Do

✅ **Wait for user to ask about releases**
✅ **Always use `-u` flag** unless user explicitly says versions already updated
✅ **Always use `-m` flag** with meaningful description
✅ **Suggest appropriate release command** based on changes
✅ **Run the command directly** (don't just suggest it)
✅ **Monitor build output** and help debug if failures occur
✅ **Verify two commits created** (release + dev bump)
✅ **Remind about `-p` flag** if user wants to push immediately
✅ **Default to `all` projects** unless user specifies otherwise

## Integration with Other Workflows

### After Protocol Changes

If user changed `prompts/WEBSOCKET_MESSAGE_SPEC.md` and implementations:

```bash
# All components likely changed
./release.sh -u -m "Protocol: Add NUKE_INTERCEPTED event" ots-userscript ots-fw-main ots-simulator
```

If CAN protocol changed (`prompts/CANBUS_MESSAGE_SPEC.md`):

```bash
# Main firmware and audio module
./release.sh -u -m "CAN Protocol: Add new sound commands" ots-fw-main ots-fw-audiomodule
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

### User: "What's the development workflow with versions?"

**Response:** The release script manages this automatically:

1. **During development**: All projects have `-dev` suffix (e.g., `2026-01-05.2-dev`)
2. **Create release**: `./release.sh -u -m "..."` strips `-dev` and tags
3. **Auto-bump**: Script automatically bumps to next dev version (`2026-01-05.3-dev`)
4. **Result**: Two commits created:
   - Release commit (tagged: `2026-01-05.2`)
   - Dev version bump (untagged: `2026-01-05.3-dev`)

This keeps development versions clearly distinguished from release versions.

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
