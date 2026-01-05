# Git Workflow - AI Assistant Guidelines

## Purpose

This prompt guides AI assistants (GitHub Copilot) on how to handle git operations, commit messages, and version control workflows for the OTS project.

## Project Git Strategy

**Trunk-Based Development**: Single `main` branch, no long-lived feature branches.

When suggesting git operations:
- Always assume working on `main` branch
- Recommend testing before committing
- Suggest atomic commits (one logical change per commit)
- Prefer rebase over merge for clean history

## Commit Message Generation

### Format (Conventional Commits)

```
<type>(<scope>): <subject>

[optional body]

[optional footer]
```

### When to Generate Commit Messages

**Always include commit message** when:
- User asks to "commit" or "save changes"
- User completes a feature implementation
- User asks "what should the commit message be?"
- Multiple files changed and user asks to finalize

**Types to use:**

| Type | Use When | Example |
|------|----------|---------|
| `feat` | New feature added | `feat(userscript): add nuke interception detection` |
| `fix` | Bug fixed | `fix(firmware): correct LED blink timing` |
| `refactor` | Code restructured, no behavior change | `refactor(server): extract WebSocket handler` |
| `docs` | Documentation only | `docs: update hardware assembly guide` |
| `style` | Formatting/style only | `style(firmware): apply clang-format` |
| `test` | Tests added/updated | `test(userscript): add tracker unit tests` |
| `chore` | Maintenance/tooling | `chore: update dependencies` |
| `perf` | Performance improvement | `perf(firmware): optimize I2C polling` |

**Scopes to use:**

- `userscript` - ots-userscript
- `firmware` - ots-fw-main
- `server` - ots-simulator (NOT ots-server)
- `hardware` - ots-hardware specs
- `protocol` - WEBSOCKET_MESSAGE_SPEC.md changes
- `docs` - documentation
- `shared` - ots-shared types

### Commit Message Rules

**Subject line:**
- Start with lowercase (after scope)
- No period at end
- Imperative mood ("add" not "added" or "adds")
- Max 72 characters
- Be specific: ❌ "fix bug" ✅ "fix LCD ghosting on screen transitions"

**Body (optional but recommended for non-trivial changes):**
- Explain WHY, not WHAT (code shows what)
- Wrap at 72 characters
- Separate from subject with blank line
- Use bullet points for multiple changes

**Footer (optional):**
- Reference issues: `Closes #42` or `Fixes #38`
- Breaking changes: `BREAKING CHANGE: renamed API endpoint`

### Example Generations

**User asks: "Commit this nuke tracking feature"**

You generate:
```bash
git add ots-userscript/src/trackers/nuke-tracker.ts
git commit -m "feat(userscript): implement nuke tracking with unitID

- Track individual nukes by unitID instead of timeouts
- Support up to 32 simultaneous nukes
- Emit NUKE_EXPLODED/NUKE_INTERCEPTED events
- Update LED state only when all nukes of type resolve

Closes #42"
```

**User asks: "Fix the I2C address issue"**

You generate:
```bash
git add ots-fw-main/include/config.h
git commit -m "fix(firmware): correct LCD I2C address from 0x3F to 0x27"
```

## Protocol Change Workflow

**CRITICAL**: When user modifies protocol, guide them through proper order.

**Always follow this sequence:**

1. **First**: Update `prompts/WEBSOCKET_MESSAGE_SPEC.md`
2. **Second**: Update `ots-shared/src/game.ts` (TypeScript types)
3. **Third**: Update `ots-fw-main/include/protocol.h` (C types)
4. **Fourth**: Update implementations (userscript, server, firmware modules)

**Suggest commits in this order:**

```bash
# Step 1: Protocol spec
git add prompts/WEBSOCKET_MESSAGE_SPEC.md
git commit -m "docs(protocol): add NUKE_INTERCEPTED event definition"

# Step 2: TypeScript types
git add ots-shared/src/game.ts
git commit -m "feat(shared): add NUKE_INTERCEPTED event type"

# Step 3: Firmware types
git add ots-fw-main/include/protocol.h ots-fw-main/src/protocol.c
git commit -m "feat(firmware): add NUKE_INTERCEPTED event enum and parser"

# Step 4: Implementations
git add ots-userscript/src/trackers/nuke-tracker.ts
git add ots-simulator/server/routes/ws.ts
git add ots-fw-main/src/nuke_module.c
git commit -m "feat: implement NUKE_INTERCEPTED handling across all components

- Userscript: Emit event when nuke deleted before arrival
- Server: Broadcast event to all connected clients
- Firmware: Clear nuke from tracker and update LED state"
```

**Never suggest updating implementations before types are in place.**

## When User Asks About Git Operations

### "How do I commit this?"

Generate appropriate commit message based on files changed and context.

### "Should I create a branch?"

Answer: "No, commit directly to `main`. This project uses trunk-based development. Test locally first, then commit and push."

### "How do I undo this commit?"

If not pushed:
```bash
git reset --soft HEAD~1  # Keep changes
# or
git reset --hard HEAD~1  # Discard changes
```

If pushed:
```bash
git revert <commit-hash>  # Create reverting commit
```

### "What files should I commit together?"

Group by logical change:
- ✅ Protocol change + type definitions + implementations (if tightly coupled)
- ✅ Bug fix + related test
- ❌ Multiple unrelated features
- ❌ Feature + unrelated typo fix

### "How do I check what changed?"

```bash
git status           # See modified files
git diff             # See changes
git diff --staged    # See staged changes
```

## Release Workflow

When user wants to create a release:

```bash
./release.sh -u -m "Release description"
```

Explain:
- Script auto-updates versions in all projects
- Creates git tag with date format (YYYY-MM-DD.N)
- Commits version changes
- User must push: `git push origin main --follow-tags`

## Anti-Patterns to Avoid

❌ **Don't suggest:**
- Creating feature branches for simple changes
- Merge commits (prefer rebase)
- Generic commit messages ("fix", "update", "wip")
- Committing without testing
- Skipping WEBSOCKET_MESSAGE_SPEC.md when changing protocol
- Using `any` type instead of proper protocol types

✅ **Do suggest:**
- Direct commits to main with good messages
- Linear history with rebase
- Specific, descriptive commit messages
- Testing before commit
- Protocol changes in proper order
- Strongly typed protocol implementations

## Quick Reference

**Good commit message examples:**
```
feat(userscript): add game end detection with victory/defeat tracking
fix(firmware): resolve WiFi reconnection timeout
refactor(server): extract hardware module components
docs(hardware): update wiring diagram for Alert Module
style(userscript): format code with prettier
test(firmware): add I2C communication tests
chore(deps): update nuxt to 4.1.0
perf(firmware): reduce LED update latency
```

**Bad commit message examples:**
```
fix bug          # ❌ Not descriptive
updated code     # ❌ No context
wip              # ❌ Not a complete change
test             # ❌ Not descriptive
minor changes    # ❌ Vague
```

## Reminder

When helping users with git operations:
1. Generate proper commit messages automatically
2. Group related changes logically
3. Follow protocol change order strictly
4. Always include `-e esp32-s3-dev` for firmware builds
5. Suggest testing before committing
6. Keep commits atomic and focused

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
- **Protocol Changes**: [`prompts/WEBSOCKET_MESSAGE_SPEC.md`](WEBSOCKET_MESSAGE_SPEC.md)

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
