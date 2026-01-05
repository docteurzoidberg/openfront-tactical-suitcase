# Documentation Guidelines - AI Prompt

**Purpose**: Guide AI assistants in creating and maintaining documentation in the `/doc` directory following VitePress markdown standards.

**Last Updated**: January 5, 2026  
**VitePress Version**: 1.x (default theme)  
**Target Audience**: AI assistants writing/editing documentation

---

## Overview

All documentation in `/doc` must be:
1. **Written in Markdown** with VitePress extensions
2. **Deployed to GitHub Pages** via `ots-website` VitePress site
3. **Synced automatically** from `/doc` to `/ots-website/user` and `/ots-website/developer`
4. **Accessible to both users and developers**

### Documentation Architecture

```
/doc/                          # Source of truth (version controlled)
  ├── user/                    # User-facing guides
  └── developer/               # Technical documentation

/ots-website/                  # VitePress site builder
  ├── user/                    # Synced from /doc/user
  ├── developer/               # Synced from /doc/developer
  ├── index.md                 # Homepage (custom)
  ├── downloads.md             # Downloads page (custom)
  └── releases.md              # Release history (custom)
```

**Important**: Always edit files in `/doc`, NOT in `/ots-website`. The sync happens during build.

---

## VitePress Markdown Extensions

### Containers (Callouts)

Use custom containers for tips, warnings, and notes:

```markdown
::: tip Quick Tip
This is a helpful hint for users.
:::

::: warning Important
This requires attention or caution.
:::

::: danger Critical
This could cause data loss or damage.
:::

::: info Note
Additional context or information.
:::

::: details Click to expand
Hidden content that can be toggled.
This is useful for optional deep-dives.
:::
```

**When to use**:
- **tip**: Shortcuts, best practices, helpful hints
- **warning**: Prerequisites, compatibility issues, common mistakes
- **danger**: Destructive operations, security concerns, data loss risks
- **info**: Background context, additional resources, related topics
- **details**: Optional technical details, verbose explanations, examples

### Code Blocks

#### Basic Code Block with Syntax Highlighting

```markdown
```bash
npm install
npm run dev
\```
```

**Supported languages**: `bash`, `typescript`, `javascript`, `c`, `cpp`, `python`, `json`, `yaml`, `toml`, `markdown`

#### Code Block with Filename

```markdown
```typescript [src/main.ts]
export function hello() {
  console.log('Hello, world!')
}
\```
```

#### Line Highlighting

```markdown
```typescript {2,4-6}
function example() {
  const important = true  // Line 2 highlighted
  const normal = false
  const highlighted1 = 1  // Lines 4-6 highlighted
  const highlighted2 = 2
  const highlighted3 = 3
}
\```
```

#### Line Numbers

```markdown
```typescript:line-numbers
function numbered() {
  return 42
}
\```
```

#### Focus Lines (Dim Others)

```markdown
```typescript
function example() {
  const focus = true  // [!code focus]
  const dimmed = false
}
\```
```

#### Diff Highlighting

```markdown
```typescript
function example() {
  const old = 'removed'  // [!code --]
  const new = 'added'    // [!code ++]
}
\```
```

### Links

#### Internal Links (Relative)

```markdown
<!-- Link to another doc page -->
See [Quick Start](quick-start.md) for setup instructions.

<!-- Link to specific section -->
See [WiFi Setup - Troubleshooting](wifi-setup.md#troubleshooting) for help.

<!-- Link to parent directory -->
See [Developer Overview](../developer/README.md) for technical docs.
```

**Important**: Use relative paths with `.md` extension. VitePress converts these automatically.

#### External Links

```markdown
<!-- Opens in new tab by default -->
Visit [OpenFront.io](https://openfront.io) to play the game.

<!-- Explicit external link -->
Download from [GitHub Releases](https://github.com/docteurzoidberg/openfront-tactical-suitcase/releases)
```

### Custom Containers (Advanced)

#### GitHub-Style Alerts

```markdown
> [!NOTE]
> Highlights information that users should take into account.

> [!TIP]
> Helpful advice for doing things better or more easily.

> [!IMPORTANT]
> Key information users need to know to achieve their goal.

> [!WARNING]
> Urgent info that needs immediate user attention to avoid problems.

> [!CAUTION]
> Advises about risks or negative outcomes of certain actions.
```

**Note**: VitePress transforms these into styled callout boxes.

### Tables

```markdown
| Component | Description | Port |
|-----------|-------------|------|
| Firmware | ESP32-S3 controller | Serial |
| Dashboard | Nuxt web server | 3000 |
| Userscript | Browser extension | N/A |

<!-- Right-align columns -->
| Item | Price |
|------|------:|
| Atom | $100 |
| Hydro | $250 |
```

**Best practices**:
- Keep tables simple (< 5 columns)
- Use right-align for numbers
- Add descriptive headers
- Consider using details container for large tables

### Emoji (Optional)

Use sparingly for visual organization:

```markdown
## 📚 Documentation Sections
## 🚀 Quick Start
## 🛠️ Developer Guide
## ⚠️ Troubleshooting
```

**Approved emoji**:
- 📚 Documentation / Reading
- 🚀 Quick Start / Launch
- 🛠️ Development / Tools
- ⚙️ Configuration / Settings
- 📋 Prerequisites / Checklist
- ✅ Success / Complete
- ❌ Error / Failed
- ⚠️ Warning / Caution
- 💡 Tips / Ideas
- 🔗 Links / Resources

### Badges (Custom HTML)

```markdown
<Badge type="tip" text="NEW" />
<Badge type="warning" text="BETA" />
<Badge type="danger" text="DEPRECATED" />
<Badge type="info" text="v1.0" />
```

Use for version indicators or status labels.

---

## Document Structure Standards

### User Documentation (`/doc/user`)

**Target Audience**: Device owners, players, non-technical users

**Writing Style**:
- Clear, concise, beginner-friendly
- Step-by-step instructions
- Visual descriptions (no code unless necessary)
- Focus on "how to use" not "how it works"

**Required Sections**:
1. **Overview** (1-2 sentences explaining the topic)
2. **Prerequisites** (what they need before starting)
3. **Step-by-Step Guide** (numbered instructions)
4. **Troubleshooting** (common issues + fixes)
5. **Next Steps** (links to related guides)

**Example Structure**:

```markdown
# WiFi Setup

Configure your OTS device to connect to your home network.

::: tip Quick Tip
Most home WiFi networks work out of the box. This guide covers custom setups.
:::

## Prerequisites

- OTS device powered on
- WiFi network name (SSID) and password
- Computer or phone with WiFi

## Step-by-Step Instructions

### 1. Access WiFi Settings

1. Power on your OTS device
2. Wait for the LINK LED to blink rapidly (10 seconds)
3. Look for WiFi network named `OTS-Setup-XXXX`

::: warning Important
The setup WiFi appears only if the device can't connect to a saved network.
:::

### 2. Connect to Setup Network

[Continue with detailed steps...]

## Troubleshooting

### LED Not Blinking

**Problem**: LINK LED stays off or solid.

**Solutions**:
1. Check power supply (LED should illuminate)
2. Try power cycling the device
3. Verify power switch is ON

[More troubleshooting items...]

## Next Steps

- [Userscript Installation](userscript-install.md) - Install browser extension
- [Quick Start Guide](quick-start.md) - Complete device setup
- [Troubleshooting](troubleshooting.md) - More solutions
```

### Developer Documentation (`/doc/developer`)

**Target Audience**: Developers, makers, contributors

**Writing Style**:
- Technical, precise, complete
- Code examples and command-line instructions
- Architectural explanations
- Assumes programming knowledge

**Required Sections**:
1. **Overview** (what this doc covers)
2. **Prerequisites** (tools, knowledge, dependencies)
3. **Concepts** (theory, architecture, design)
4. **Implementation** (code examples, commands)
5. **Testing** (verification steps)
6. **Reference** (API docs, links, related files)

**Example Structure**:

```markdown
# Adding a Hardware Module

Create a new hardware module with buttons, LEDs, and game state integration.

## Prerequisites

- Development environment set up ([Getting Started](getting-started.md))
- Understanding of [Module System Architecture](architecture/module-system.md)
- Basic C programming knowledge
- PlatformIO installed

## Module System Overview

Hardware modules implement the `hardware_module_t` interface:

```c
typedef struct {
    esp_err_t (*init)(void);
    void (*update)(void);
    void (*handle_event)(game_event_type_t event, const char* data);
    esp_err_t (*get_status)(module_status_t* status);
    void (*shutdown)(void);
} hardware_module_t;
\```

::: info Architecture Note
All modules share the I2C bus with MCP23017 expanders at 0x20 (inputs) and 0x21 (outputs).
See [I2C Architecture](hardware/i2c-architecture.md) for details.
:::

## Implementation Steps

### 1. Create Module Files

Create `include/my_module.h` and `src/my_module.c`:

```c [include/my_module.h]
#ifndef MY_MODULE_H
#define MY_MODULE_H

#include "hardware_module.h"

// Pin definitions
#define MY_MODULE_BUTTON_BOARD 0
#define MY_MODULE_BUTTON_PIN   4

hardware_module_t* my_module_get(void);

#endif
\```

[Continue with implementation details...]

## Testing

Test your module in isolation before integration:

```bash
cd ots-fw-main
pio run -e esp32-s3-dev -t upload && pio device monitor
\```

Expected output:

```
[MY_MODULE] Initialized successfully
[MY_MODULE] Button pressed
[WS] Sent event: MY_MODULE_ACTIVATED
\```

## Reference

**Related Files**:
- `src/module_manager.c` - Module registration
- `include/hardware_module.h` - Base interface
- `prompts/MODULE_DEVELOPMENT_PROMPT.md` - AI development guide

**Similar Modules**:
- [Nuke Module](../hardware/modules/nuke-module.md) - Button + LED example
- [Alert Module](../hardware/modules/alert-module.md) - LED-only example
```

---

## Writing Guidelines

### Headings

Use descriptive, hierarchical headings:

```markdown
# Page Title (H1 - Only once per page)

## Major Section (H2 - Main topics)

### Subsection (H3 - Details)

#### Minor Detail (H4 - Rarely needed)
```

**Rules**:
- Only ONE H1 per document (the page title)
- H2 for major sections
- H3 for subsections within H2
- H4 sparingly (consider list instead)
- Never skip levels (H1 → H3 without H2)

### Tone and Voice

**User Documentation**:
- Second person ("You will see...")
- Active voice ("Click the button")
- Friendly, encouraging tone
- Avoid jargon

**Developer Documentation**:
- Direct, instructional ("Implement the interface")
- Technical terms explained once
- Assume programming literacy
- Precise, unambiguous language

### Code Examples

**Always include**:
- Syntax highlighting language
- Context (what file, where in code)
- Expected output or result
- Explanation of non-obvious parts

**Example**:

```markdown
Update the event handler in `src/my_module.c`:

```c
static void my_module_handle_event(game_event_type_t event, const char* data) {
    if (event == GAME_EVENT_NUKE_LAUNCHED) {
        // Flash LED on nuke launch
        module_io_set_led(MY_MODULE_LED, true);
        vTaskDelay(pdMS_TO_TICKS(500));
        module_io_set_led(MY_MODULE_LED, false);
    }
}
\```

This code:
1. Checks for nuke launch event
2. Turns LED on
3. Waits 500ms
4. Turns LED off
```

### Command-Line Examples

Show full commands with context:

```markdown
Build and upload firmware to the device:

```bash
cd ots-fw-main
pio run -e esp32-s3-dev -t upload && pio device monitor
\```

**Expected output**:
```
Wrote 1048576 bytes at 0x00010000 in 92.3 seconds
Hard resetting via RTS pin...
[MAIN] Firmware initialized
\```
```

### Cross-References

Link to related documentation:

```markdown
::: tip See Also
- [WebSocket Protocol](websocket-protocol.md) - Message format reference
- [Module Development](workflows/add-hardware-module.md) - Create new modules
- [I2C Architecture](hardware/i2c-architecture.md) - Bus configuration
:::
```

### Images and Diagrams

Reference images in `/doc/images`:

```markdown
![WiFi Setup Screen](/images/wifi-setup-screen.png)
*Figure 1: WiFi configuration interface*
```

**Image guidelines**:
- PNG format preferred
- Max width: 800px
- Include alt text
- Add caption if needed

### Lists

**Unordered Lists** (no specific order):

```markdown
Features:
- WebSocket connection
- Hardware module control
- Game event tracking
```

**Ordered Lists** (sequential steps):

```markdown
Setup procedure:
1. Clone repository
2. Install dependencies
3. Build firmware
```

**Nested Lists**:

```markdown
Project structure:
- Firmware
  - Main controller (ESP32-S3)
  - Audio module (ESP32-A1S)
- Software
  - Dashboard (Nuxt)
  - Userscript (TypeScript)
```

---

## Document Templates

### User Guide Template

```markdown
# [Feature Name]

[One-sentence description of what this feature does]

::: tip Quick Tip
[Helpful hint or shortcut]
:::

## Prerequisites

- [Required item 1]
- [Required item 2]

## How to Use

### Step 1: [Action Name]

1. [Detailed instruction]
2. [Next instruction]
3. [Final instruction]

::: warning Important
[Cautionary note about this step]
:::

### Step 2: [Next Action]

[Continue with steps...]

## Troubleshooting

### [Common Issue 1]

**Problem**: [Description of what user sees]

**Solution**:
1. [Fix step 1]
2. [Fix step 2]

### [Common Issue 2]

[More issues...]

## Next Steps

- [Related guide 1](link.md)
- [Related guide 2](link.md)
```

### Developer Guide Template

```markdown
# [Technical Topic]

[Brief overview and purpose]

## Prerequisites

- [Tool or knowledge requirement 1]
- [Tool or knowledge requirement 2]

## Concepts

### [Key Concept 1]

[Explanation with diagram if needed]

```[language]
[Code example illustrating concept]
\```

### [Key Concept 2]

[More concepts...]

## Implementation

### Step 1: [Setup Task]

```bash
[Command to run]
\```

**Expected output**:
```
[Sample output]
\```

### Step 2: [Code Task]

Create `[filename]`:

```[language] [filename]
[Full code example]
\```

[Explanation of code]

## Testing

Verify implementation:

```bash
[Test command]
\```

Expected results:
- [Expected behavior 1]
- [Expected behavior 2]

## Reference

**Related Files**:
- `[path/to/file]` - [Description]

**Related Documentation**:
- [Link to related doc](link.md)

**External Resources**:
- [Link to library docs](https://example.com)
```

---

## File Naming Conventions

### General Rules

- Use lowercase
- Use hyphens for spaces (kebab-case)
- Be descriptive but concise
- Include context in subdirectories

### Examples

✅ **Good**:
- `quick-start.md`
- `wifi-setup.md`
- `add-hardware-module.md`
- `websocket-protocol.md`

❌ **Bad**:
- `QS.md` (not descriptive)
- `wifi_setup.md` (use hyphens not underscores)
- `How to Set Up WiFi.md` (no spaces, no capitals)
- `guide.md` (too generic)

### Special Files

- `README.md` - Directory index (always capitalized)
- `CONTRIBUTING.md` - Contribution guide (always capitalized)
- `FAQ.md` - Frequently asked questions (always capitalized)

---

## VitePress Configuration

### Sidebar Navigation

Edit `/ots-website/.vitepress/config.ts` to add new pages:

```typescript
sidebar: {
  '/user/': [
    {
      text: 'User Guide',
      items: [
        { text: 'Overview', link: '/user/' },
        { text: 'Quick Start', link: '/user/quick-start' },
        { text: 'New Guide', link: '/user/new-guide' },  // Add here
      ]
    }
  ]
}
```

**Rules**:
- Add ALL new pages to sidebar
- Group related pages together
- Keep order logical (general → specific)
- Use descriptive text (not filename)


## Checklist for New Documentation

Before submitting new documentation, verify:

### Content Quality
- [ ] Clear, concise writing appropriate for audience
- [ ] All technical terms defined or linked
- [ ] Code examples tested and working
- [ ] Commands show full context (directory, expected output)
- [ ] Screenshots/images added to `/doc/images` if needed

### Structure
- [ ] Follows appropriate template (user or developer)
- [ ] Single H1 heading (page title)
- [ ] Logical heading hierarchy (no skipped levels)
- [ ] Proper use of containers (tip, warning, danger, info)
- [ ] Cross-references to related documentation

### Markdown Syntax
- [ ] VitePress-compatible markdown (no unsupported extensions)
- [ ] Relative links with `.md` extension
- [ ] Code blocks with language specification
- [ ] Properly formatted tables
- [ ] No raw HTML (use VitePress markdown features)

### Integration
- [ ] File created in `/doc` (NOT `/ots-website`)
- [ ] Added to sidebar in `/ots-website/.vitepress/config.ts`
- [ ] Links from related documents updated
- [ ] Filename follows naming conventions
- [ ] Front matter added if needed

### Testing
- [ ] Build VitePress site locally to verify rendering
- [ ] Check all internal links work
- [ ] Verify code syntax highlighting
- [ ] Test on mobile viewport (responsive)

**Build and test**:
```bash
cd ots-website
npm install
npm run dev
# Open http://localhost:5173 and navigate to new page
```

---

## Common Mistakes to Avoid

### ❌ Editing in `/ots-website` directly
Files in `/ots-website/user` and `/ots-website/developer` are synced copies. Edit the source in `/doc` instead.

### ❌ Using absolute paths
```markdown
[Link](/user/quick-start.md)  # Don't use leading slash
```

✅ Use relative paths:
```markdown
[Link](quick-start.md)
[Link](../developer/getting-started.md)
```

### ❌ Forgetting sidebar entry
New pages won't appear in navigation without sidebar configuration.

### ❌ Multiple H1 headings
```markdown
# Page Title       # ❌ First H1
# Another Title    # ❌ Second H1 (don't do this)
```

✅ Use H2 for sections:
```markdown
# Page Title       # ✅ Only H1
## Section 1        # ✅ H2 for sections
## Section 2        # ✅ H2 for sections
```

### ❌ Broken code blocks
```markdown
```bash
npm install
```                # ❌ Backticks not aligned
\```

✅ Properly closed:
```markdown
```bash
npm install
\```               # ✅ Aligned closing backticks
```

### ❌ Overusing containers
```markdown
::: tip
This is a tip
:::
::: warning
This is a warning
:::
::: tip
Another tip
:::
```

✅ Use sparingly:
```markdown
Instructions here...

::: tip
One helpful tip for the entire section
:::

More instructions...
```

---

## Examples of Excellent Documentation

### User Documentation Example

See [`/doc/user/quick-start.md`](../doc/user/quick-start.md):
- Clear step-by-step flow
- Visual status indicators (LED states)
- Troubleshooting for common issues
- Links to next steps

### Developer Documentation Example

See [`/doc/developer/websocket-protocol.md`](../doc/developer/websocket-protocol.md):
- Complete technical reference
- Code examples in multiple languages
- Architecture diagrams
- Testing strategies

---

## VitePress Resources

**Official Documentation**:
- [VitePress Guide](https://vitepress.dev/guide/what-is-vitepress)
- [Markdown Extensions](https://vitepress.dev/guide/markdown)
- [Default Theme](https://vitepress.dev/reference/default-theme-config)

**Quick References**:
- [Container Syntax](https://vitepress.dev/guide/markdown#custom-containers)
- [Code Blocks](https://vitepress.dev/guide/markdown#syntax-highlighting-in-code-blocks)
- [Frontmatter](https://vitepress.dev/reference/frontmatter-config)

---

## Questions or Issues?

When unsure about documentation structure:
1. Check existing similar documentation for patterns
2. Refer to templates in this guide
3. Review `/doc/STRUCTURE.md` for intended organization
4. Build locally with VitePress to test rendering

**Remember**: Documentation is version controlled. It's safe to experiment and iterate. Focus on clarity and usefulness for the target audience.
