# OTS Release Process

## Overview

The OTS project uses a unified release system managed by the `release.sh` script at the repository root. This script handles version updates, builds, git commits, and tagging for all three components: userscript, firmware, and server.

## Tag Format

Releases use a **date-based versioning** scheme with auto-incrementing revisions:

```
YYYY-MM-DD.N
```

- **YYYY-MM-DD**: Release date
- **N**: Revision number (1, 2, 3, ...) for multiple releases on the same day

**Examples:**
- `2025-12-20.1` - First release on December 20, 2025
- `2025-12-20.2` - Second release on the same day
- `2025-12-21.1` - First release on December 21, 2025

## Release Script Usage

### Basic Commands

```bash
# List existing releases
./release.sh -l

# Create release (manual workflow)
./release.sh -m "Release description"

# Update versions, build, commit, and tag (automatic workflow)
./release.sh -u -m "Release description"

# Release and push to remote
./release.sh -u -p -m "Release description"
```

### Options

| Option | Description |
|--------|-------------|
| `-u` | Update version strings in all projects before tagging |
| `-p` | Push commit and tag to remote repository |
| `-m <msg>` | Release description (used in git tag annotation) |
| `-l` | List all existing release tags |
| `-h` | Show help message |

### Selecting Projects

By default, all projects are included. To select specific projects:

```bash
# Release only userscript
./release.sh -u -m "Userscript fix" userscript

# Release userscript and firmware
./release.sh -u -m "Hardware updates" userscript firmware

# Release all (explicit)
./release.sh -u -m "Full release" userscript firmware server
```

## Version Management

The release script updates version strings in these files:

### Userscript
- `ots-userscript/package.json` → `"version": "YYYY-MM-DD.N"`
- `ots-userscript/src/main.user.ts` → `const VERSION = 'YYYY-MM-DD.N'`

### Firmware
- `ots-fw-main/include/config.h` → `#define OTS_FIRMWARE_VERSION "YYYY-MM-DD.N"`

### Server
- `ots-server/package.json` → `"version": "YYYY-MM-DD.N"`

## Build Process

The release script automatically builds all selected projects:

1. **Userscript**: `npm run build` in `ots-userscript/`
   - Output: `build/userscript.ots.user.js` (~76KB)
   - Version injected into Tampermonkey header

2. **Firmware**: `pio run` in `ots-fw-main/`
   - Output: `.pio/build/esp32-s3-dev/firmware.bin` (~1MB)
   - Version displayed in boot log

3. **Server**: `npm run build` in `ots-server/`
   - Output: `.output/` directory (Nuxt production build)
   - Version displayed in dashboard header

**Build Validation**: If any build fails, the release is aborted. No commit or tag is created.

## Git Workflow

When using the `-u` flag, the release script follows this workflow:

1. **Update Versions**: Replace version strings in all project files
2. **Build Projects**: Execute build commands and verify success
3. **Commit Changes**: Create commit with all version updates and build outputs
4. **Create Tag**: Annotate tag pointing to the commit
5. **Push (optional)**: Push commit and tag to origin

### Commit Format

```
chore(release): version YYYY-MM-DD.N

Projects: userscript, firmware, server
<Release description>
```

### Tag Annotation

```
OTS Release YYYY-MM-DD.N

<Release description>

Projects: userscript, firmware, server
```

## Common Scenarios

### First Release of the Day

```bash
./release.sh -u -p -m "Feature: Game end detection and LCD persistence"
```

Creates tag `2025-12-20.1`, commits changes, pushes to remote.

### Second Release (Same Day)

```bash
./release.sh -u -p -m "Hotfix: Button debouncing"
```

Creates tag `2025-12-20.2` (auto-increments revision).

### Userscript-Only Release

```bash
./release.sh -u -m "Fix: HUD position persistence" userscript
```

Updates and builds only the userscript, creates tag `2025-12-20.N`.

### Local Testing (No Push)

```bash
./release.sh -u -m "Test release"
```

Creates commit and tag locally. Verify before pushing:

```bash
git log -1 --oneline
git show 2025-12-20.N --stat
git push origin HEAD && git push origin 2025-12-20.N
```

### Manual Workflow (No Version Updates)

If you've already updated versions manually:

```bash
./release.sh -m "Manual version updates"
```

Skips version replacement, only creates tag from current state.

## Verification

After running a release, verify:

```bash
# Check tag was created
git tag -n5 2025-12-20.N

# View commit with all changes
git show 2025-12-20.N --stat

# Verify working directory is clean
git status --short

# Check builds were created
ls -lh ots-userscript/build/userscript.ots.user.js
ls -lh ots-fw-main/.pio/build/esp32-s3-dev/firmware.bin
ls -ld ots-server/.output
```

## Weekly Changelog

Update `prompts/weekly_announces.md` before releasing:

```markdown
## Week of December 16-20, 2025

🎮 **Game Integration:**
- Feature description

🚀 **Firmware:**
- Hardware improvements

🛠️ **Dashboard:**
- UI updates
```

This provides a summary for Discord announcements and release notes.

## Troubleshooting

### Build Failure

If a build fails, the release is aborted:

```
❌ userscript build failed
Release aborted - build failures detected
```

**Fix**: Resolve build errors and run release script again.

### Uncommitted Changes Warning

If you have uncommitted changes without `-u` flag:

```
⚠ Warning: You have uncommitted changes.
```

**Options:**
- Use `-u` flag to automatically commit
- Commit changes manually first
- Stash changes: `git stash`

### Tag Already Exists

If tag exists locally:

```bash
git tag -d 2025-12-20.1
./release.sh -u -m "New description"
```

If tag exists on remote:

```bash
git push origin :refs/tags/2025-12-20.1
```

### Version Mismatch

If versions are out of sync, use `-u` flag to reset all:

```bash
./release.sh -u -m "Sync all versions"
```

## Best Practices

1. **Always Use `-u` Flag**: Ensures version consistency across all projects
2. **Test Before Pushing**: Use local release first, verify, then use `-p`
3. **Meaningful Descriptions**: Write clear release messages for tag annotations
4. **Update Changelog**: Keep `prompts/weekly_announces.md` current for Discord posts
5. **Build Verification**: Script handles this automatically - don't skip builds
6. **One Tag Per Release**: Don't reuse or force-push tags (breaks history)

## Integration with CI/CD

The release tags can trigger automated deployments:

- **Userscript**: Auto-publish to Tampermonkey store
- **Firmware**: OTA update distribution
- **Server**: Production deployment to hosting

Tag format enables filtering: `refs/tags/2025-*` for all 2025 releases.

## Version Display

### Userscript
Console log on load:
```
[OTS Userscript] Version 2025-12-20.1
```

Tampermonkey header:
```
// @version     2025-12-20.1
```

### Firmware
Boot log banner:
```
===========================================
OTS Firmware v2025-12-20.1
Firmware: ots-fw-main
===========================================
```

### Server
Dashboard header badge:
```
v2025-12-20.1
```

## Release Checklist

Before running release script:

- [ ] All features tested and working
- [ ] Code committed (or ready for auto-commit with `-u`)
- [ ] Weekly changelog updated
- [ ] No build errors
- [ ] Version numbers ready to be updated

After release:

- [ ] Verify tag created: `git tag -l | tail -1`
- [ ] Verify builds exist and are correct size
- [ ] Test userscript in browser
- [ ] Flash firmware to device (if hardware changes)
- [ ] Deploy server (if backend changes)
- [ ] Post weekly changelog to Discord
- [ ] Push to remote: `-p` flag or manual push

## Support

For issues with the release process:
- Check `release.sh` source code (heavily commented)
- Review git logs: `git log --oneline --decorate`
- Test with `-l` flag first (list-only, no changes)
