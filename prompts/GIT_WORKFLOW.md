# Git Workflow - OTS Project

## Overview

The OTS project follows a **trunk-based development** workflow with the `main` branch as the single source of truth. All changes are committed directly to `main` with proper commit messages and testing.

## Branch Strategy

### Main Branch
- **Branch**: `main`
- **Purpose**: Production-ready code
- **Protection**: All commits should be tested before pushing
- **History**: Linear history preferred (no merge commits when possible)

### Feature Branches (Optional)
For complex features or experiments:
```bash
git checkout -b feature/nuke-tracking
# ... work on feature ...
git checkout main
git merge --squash feature/nuke-tracking
git commit -m "feat(firmware): implement nuke tracking system"
```

## Commit Message Format

Follow **Conventional Commits** specification:

```
<type>(<scope>): <subject>

<body>

<footer>
```

### Types

| Type | Purpose | Example |
|------|---------|---------|
| **feat** | New feature | `feat(userscript): add death detection polling` |
| **fix** | Bug fix | `fix(firmware): correct I2C address for LCD` |
| **refactor** | Code restructuring | `refactor(server): extract WebSocket logic` |
| **docs** | Documentation only | `docs: update hardware assembly guide` |
| **style** | Code style (formatting) | `style(firmware): format with clang-format` |
| **test** | Add/update tests | `test(userscript): add nuke tracker unit tests` |
| **chore** | Maintenance tasks | `chore(release): version 2025-12-20.1` |
| **perf** | Performance improvement | `perf(firmware): optimize LED blink timing` |
| **build** | Build system changes | `build(server): upgrade to Nuxt 4.1` |

### Scopes

| Scope | Component |
|-------|-----------|
| `userscript` | ots-userscript |
| `firmware` | ots-fw-main |
| `server` | ots-simulator |
| `hardware` | ots-hardware specs |
| `protocol` | prompts/protocol-context.md |
| `release` | Release automation |
| `docs` | Documentation |

### Examples

**Good commit messages:**
```bash
feat(userscript): implement attack ratio control via slider

- Add slider value polling every 100ms
- Send set-troops-percent command on ≥1% change
- Update HUD to show current ratio

Closes #42
```

```bash
fix(firmware): resolve LCD ghosting on screen transitions

LCD was not clearing properly between screens, causing
character overlap. Added explicit lcd_clear() call before
each screen update.

Fixes #38
```

```bash
refactor(server): extract hardware module components

- Move NukeModule to components/hardware/NukeModule.vue
- Move AlertModule to components/hardware/AlertModule.vue
- Improve reusability and maintainability
```

**Bad commit messages:**
```bash
fix stuff
updated files
wip
test
minor changes
```

## Workflow Steps

### 1. Before Starting Work

```bash
# Ensure you're on main and up to date
git checkout main
git pull origin main

# Check working directory is clean
git status
```

### 2. Make Changes

Edit files, test locally:
- **Userscript**: Build and test in browser
- **Firmware**: Flash to device and verify
- **Server**: Run dev server and test UI

### 3. Stage and Commit

```bash
# Stage specific files (preferred)
git add ots-userscript/src/main.user.ts
git add ots-userscript/package.json

# Or stage all changes
git add -A

# Commit with descriptive message
git commit -m "feat(userscript): add game end detection"
```

### 4. Push to Remote

```bash
# Push to origin
git push origin main

# If remote has changes, pull first
git pull --rebase origin main
git push origin main
```

## Special Workflows

### Protocol Changes

**CRITICAL**: Protocol changes must be synchronized across all components.

```bash
# 1. Update source of truth FIRST
git add prompts/protocol-context.md
git commit -m "docs(protocol): add NUKE_INTERCEPTED event"

# 2. Update TypeScript implementation
git add ots-shared/src/game.ts
git commit -m "feat(shared): add NUKE_INTERCEPTED event type"

# 3. Update firmware implementation
git add ots-fw-main/include/protocol.h
git add ots-fw-main/src/protocol.c
git commit -m "feat(firmware): add NUKE_INTERCEPTED event handler"

# 4. Update implementations
git add ots-userscript/src/trackers/nuke-tracker.ts
git add ots-simulator/app/composables/useGameSocket.ts
git add ots-fw-main/src/nuke_module.c
git commit -m "feat: implement NUKE_INTERCEPTED event across components"

# 5. Push all changes
git push origin main
```

**Order matters:**
1. Protocol spec (`prompts/protocol-context.md`)
2. Shared types (`ots-shared`)
3. Firmware types (`protocol.h`)
4. Implementations (userscript, server, firmware modules)

### Release Workflow

Use the automated release script:

```bash
# Standard release (updates all versions, builds, commits, tags)
./release.sh -u -m "Feature: Game end detection and LCD persistence"

# Review what will be committed
git diff HEAD

# If satisfied, push
git push origin main --follow-tags
```

**Automated Release with release.sh:**

The project includes `release.sh` for unified version management across all components:

```bash
# Full automated release (userscript, firmware, server)
./release.sh -u -p -m "Release description"

# Component-specific release
./release.sh -u -m "Fix" userscript

# List existing releases
./release.sh -l

# Dry run (preview without committing)
./release.sh -u -m "Test" --dry-run
```

**What release.sh does:**
- Updates version numbers in all projects (package.json, config.h)
- Builds artifacts (userscript, firmware binaries)
- Creates git commit with version bump
- Tags with date-based format: `YYYY-MM-DD.N`
- Optionally pushes to remote
- Updates `weekly_announces.md` changelog

See [`prompts/RELEASE.md`](RELEASE.md) for full release documentation.

### Hardware Module Development

When adding a new hardware module:

```bash
# 1. Create hardware spec
git add ots-hardware/modules/new-module.md
git commit -m "docs(hardware): add new-module specification"

# 2. Implement firmware
git add ots-fw-main/include/new_module.h
git add ots-fw-main/src/new_module.c
git add ots-fw-main/src/CMakeLists.txt
git commit -m "feat(firmware): implement new-module hardware driver"

# 3. Add UI component
git add ots-simulator/app/components/hardware/NewModule.vue
git commit -m "feat(server): add new-module emulator UI"

# 4. Update protocol if needed
# (follow protocol change workflow above)
```

### Hotfix Workflow

For critical bugs in production:

```bash
# 1. Fix the bug
git add <files>
git commit -m "fix(firmware): critical I2C bus deadlock

Emergency fix for I2C bus lockup causing firmware hang.
Added timeout and retry logic.

Priority: CRITICAL"

# 2. Create hotfix release
./release.sh -u -p -m "Hotfix: I2C bus deadlock"

# Release creates tag like: 2025-12-20.2 (auto-increments)
```

## Best Practices

### Commit Frequency

✅ **Good:**
- Commit logical units of work
- One feature/fix per commit (when sensible)
- Commit when tests pass

❌ **Bad:**
- Committing broken code
- Massive commits with unrelated changes
- "WIP" commits on main branch

### Commit Size

✅ **Good:**
- 10-200 lines changed per commit (guideline)
- Related changes grouped together
- Easy to review and revert if needed

❌ **Bad:**
- 1000+ line commits mixing multiple features
- Single-line commits for trivial changes (consider squashing)

### When to Squash

Use `git commit --amend` or `git rebase -i` before pushing:
- Multiple "fix typo" commits
- "WIP" commits that should be one logical change
- Commits that should be grouped for clarity

**Example:**
```bash
# You made these commits locally:
git log --oneline
abc1234 fix typo
def5678 fix another typo
ghi9012 feat(userscript): add slider control

# Squash before pushing
git rebase -i HEAD~3
# Mark first two as "squash" (s)
# Keep "feat" commit as "pick"
```

### Git Hooks (Recommended)

Create `.git/hooks/pre-commit`:
```bash
#!/bin/bash
# Run linters/formatters before commit
cd ots-userscript && npm run lint
cd ../ots-simulator && npm run lint
# Add firmware formatting check if desired
```

## Reverting Changes

### Undo Last Commit (Not Pushed)

```bash
# Keep changes, undo commit
git reset --soft HEAD~1

# Discard changes, undo commit
git reset --hard HEAD~1
```

### Revert Pushed Commit

```bash
# Create new commit that undoes changes
git revert <commit-hash>
git push origin main
```

### Emergency: Force Push (USE CAREFULLY)

```bash
# If you pushed broken code and need to undo
git reset --hard HEAD~1
git push origin main --force

# ⚠️ WARNING: Only use if:
# 1. You're the only developer
# 2. No one has pulled the broken commit
# 3. It's truly broken (not just a minor bug)
```

## Tagging Strategy

### Release Tags

Automatically created by `release.sh`:
- Format: `YYYY-MM-DD.N` (e.g., `2025-12-20.1`)
- Annotated tags with description
- Points to commit with all version updates

**Manual tag creation:**
```bash
git tag -a 2025-12-20.1 -m "OTS Release 2025-12-20.1

First release

Projects: userscript, firmware, server"

git push origin 2025-12-20.1
```

### Lightweight Tags (Not Recommended)

```bash
# Don't use these for releases
git tag milestone-1
```

## Checking Out Old Versions

```bash
# List all releases
./release.sh -l

# Checkout specific release
git checkout 2025-12-20.1

# Build that version
cd ots-userscript && npm run build
cd ../ots-fw-main && pio run
cd ../ots-simulator && npm run build

# Return to latest
git checkout main
```

## Collaboration Guidelines

### Before Pushing

1. **Test your changes**
   - Userscript: Install and test in browser
   - Firmware: Flash to device and verify
   - Server: Run dev server and check UI

2. **Check git status**
   ```bash
   git status
   git diff
   ```

3. **Review commit message**
   - Clear type and scope
   - Descriptive subject
   - Body explains WHY (not just WHAT)

4. **Pull latest changes**
   ```bash
   git pull --rebase origin main
   ```

### After Pushing

1. **Verify build passes** (if CI/CD exists)
2. **Test on clean environment** if major changes
3. **Update Discord/announcements** if user-facing changes

## Troubleshooting

### Merge Conflicts

```bash
# During pull/rebase
git status  # Shows conflicted files

# Edit files, resolve conflicts
vim <conflicted-file>

# Mark as resolved
git add <conflicted-file>

# Continue rebase
git rebase --continue
```

### Accidentally Committed to Wrong Branch

```bash
# If you meant to commit to feature branch
git checkout -b feature/my-feature
git checkout main
git reset --hard HEAD~1
git checkout feature/my-feature
```

### Lost Commits

```bash
# View reflog to find lost commits
git reflog

# Restore lost commit
git cherry-pick <commit-hash>
```

## Git Configuration

### Recommended Settings

```bash
# Set your identity
git config --global user.name "Your Name"
git config --global user.email "your.email@example.com"

# Use main as default branch
git config --global init.defaultBranch main

# Enable color output
git config --global color.ui auto

# Set default editor
git config --global core.editor "vim"  # or "code --wait" for VS Code

# Set pull behavior
git config --global pull.rebase true

# Show more context in diffs
git config --global diff.context 5
```

## Resources

- **Conventional Commits**: https://www.conventionalcommits.org/
- **Git Best Practices**: https://git-scm.com/book/en/v2
- **Release Process**: [`prompts/RELEASE.md`](RELEASE.md)
- **Protocol Changes**: [`prompts/protocol-context.md`](protocol-context.md)

## Summary Checklist

Before committing:
- [ ] Changes tested locally
- [ ] Commit message follows convention
- [ ] Type and scope are correct
- [ ] Subject is clear and concise
- [ ] Body explains why (if needed)
- [ ] No debug code or console.logs
- [ ] Protocol changes synced across components

Before pushing:
- [ ] `git pull --rebase origin main`
- [ ] No merge conflicts
- [ ] Final test passed
- [ ] Commit history is clean

After release:
- [ ] Tag created and pushed
- [ ] Builds successful
- [ ] `weekly_announces.md` updated
- [ ] Discord announcement posted
