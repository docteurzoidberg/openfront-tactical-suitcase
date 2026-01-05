# OTS Prompt Review Guide for GitHub Copilot

**Purpose:** Iterative review of prompt and documentation files one at a time  
**Companion File:** `PROMPT_REVIEW_PLAN.md` (tracks progress with checkboxes)

---

## How to Use This Guide

### User Instructions

1. Open `PROMPT_REVIEW_PLAN.md` and identify the next unchecked file
2. Tell Copilot: **"Review file: `<path/to/file.md>`"**
3. Copilot will execute the review workflow below
4. Answer questions about missing/unclear details
5. Approve or reject proposed changes
6. Copilot marks file as reviewed `[x]` in plan
7. Repeat for next file

### Quick Commands

- **"Review file: ots-fw-main/prompts/ALERT_MODULE_PROMPT.md"** - Start review
- **"Skip this file"** - Mark as reviewed without changes
- **"Next file"** - Auto-select next unchecked file from plan
- **"Show progress"** - Display review statistics

---

## Copilot Review Workflow

When user requests review of a specific file, follow these steps:

### Step 1: Read File Content

```markdown
Read the complete file content to understand:
- Purpose and scope
- Target audience (AI assistant, developer, both)
- Current state (draft, complete, outdated)
- Dependencies on other files
```

**Actions:**
- Use `read_file` to load complete content
- Identify file type: prompt (AI guide) vs documentation (reference)
- Note file length and structure

---

### Step 2: Code Synchronization Check

```markdown
Verify the file accurately reflects current codebase state:
- Check if referenced files/functions/modules exist
- Verify code examples match actual implementation
- Validate paths, filenames, and directory structure
- Check version numbers and dates
```

**Search Strategy:**
- Use `grep_search` to find referenced symbols
- Use `file_search` to verify file paths
- Use `semantic_search` for conceptual validation
- Use `read_file` to compare code snippets with actual code

**Questions for User:**
- "File references `module_xyz.c` - should this be `module_xyz.cpp`?"
- "Code example shows X pattern, but actual code uses Y - which is correct?"
- "Last updated date is 6 months old - has this area changed since?"

---

### Step 3: Clarity & Completeness Analysis

```markdown
Assess documentation quality:
- Is the purpose clearly stated?
- Are examples concrete and helpful?
- Is technical jargon explained?
- Are prerequisites documented?
- Are edge cases covered?
- Is error handling documented?
```

**Questions for User:**
- "Section X mentions 'configure the driver' - what specific steps?"
- "Should we add an example for scenario Y?"
- "Is the term 'ABC' clear to new developers, or should we define it?"
- "Missing information about error handling - should we document common errors?"

---

### Step 4: Redundancy Check

```markdown
Identify duplicate or overlapping content:
- Compare with similar files in same directory
- Check for content duplicated from other projects
- Look for outdated alternatives (e.g., old vs new approach)
- Identify merge opportunities
```

**Search Strategy:**
- Use `grep_search` with key phrases to find duplicates
- Compare related files (e.g., MODULE_PROMPT vs COMPONENT_PROMPT)
- Check for cross-references that could be consolidated

**Questions for User:**
- "This overlaps 60% with `other-file.md` - should we merge them?"
- "Two files document the same protocol - which is canonical?"
- "Should we move shared content to a common file?"

---

### Step 5: Propose Changes

```markdown
Based on Steps 2-4, propose specific updates:
- Corrections (fix broken paths, update code examples)
- Additions (fill gaps, add missing sections)
- Removals (delete obsolete content)
- Restructuring (improve organization)
- Merges (consolidate duplicates)
```

**Present Options:**
1. **No changes needed** - Mark as ✅ reviewed
2. **Minor fixes** - Quick updates (paths, dates, typos)
3. **Major revision** - Significant rewrite or restructure
4. **Merge with another file** - Consolidate duplicate content
5. **Mark as obsolete** - Recommend deletion

**Get User Approval:**
- Show diff preview for proposed changes
- Explain rationale for each change
- Wait for explicit approval before making edits

---

### Step 6: Apply Approved Changes

```markdown
Execute approved updates:
- Use `replace_string_in_file` for targeted edits
- Use `multi_replace_string_in_file` for multiple changes
- Update cross-references if files are merged/deleted
- Add changelog note if file has history section
```

**Commit Strategy:**
- Group related fixes: "docs: update XYZ prompt - fix paths and sync code examples"
- One file per commit OR batch similar changes
- User decides commit strategy

---

### Step 7: Mark as Reviewed

```markdown
Update PROMPT_REVIEW_PLAN.md:
- Change `[ ]` to `[x]` for completed file
- Update progress statistics
- Add notes if file was merged/deleted
```

**Auto-calculate:**
- Total reviewed count
- Percentage complete by category
- Overall progress percentage

---

## Review Checklist Template

For each file, validate these items:

### ✅ Content Accuracy
- [ ] All referenced files exist at specified paths
- [ ] Code examples match actual implementation
- [ ] Version numbers and dates are current
- [ ] Technical details are correct

### ✅ Clarity
- [ ] Purpose is clearly stated in first paragraph
- [ ] Target audience is obvious (AI, dev, both)
- [ ] Technical terms are defined or linked
- [ ] Examples are concrete and helpful

### ✅ Completeness
- [ ] Prerequisites are documented
- [ ] All features/sections are covered
- [ ] Error handling is explained
- [ ] Edge cases are mentioned

### ✅ Organization
- [ ] Structure is logical and scannable
- [ ] Headings create clear hierarchy
- [ ] Related info is grouped together
- [ ] Cross-references are useful

### ✅ Maintainability
- [ ] No duplicate content (or duplication justified)
- [ ] Cross-references are valid
- [ ] File is in correct location
- [ ] Naming follows conventions

---

## Common Issues & Fixes

### Issue: Broken File References

**Example:** `See module_xyz.c for implementation`  
**Search:** `file_search("module_xyz.c")`  
**Fix:** Update to correct path or remove if file deleted

### Issue: Outdated Code Examples

**Example:** Shows old API usage  
**Search:** `grep_search("old_function_name")`  
**Compare:** Read actual code with `read_file`  
**Fix:** Update example to match current code

### Issue: Duplicate Documentation

**Example:** Two files explain same module  
**Search:** `grep_search("module description")` across files  
**Fix:** Merge into single canonical doc, update references

### Issue: Vague Instructions

**Example:** "Configure the settings appropriately"  
**Ask User:** "What specific settings? Can we list them?"  
**Fix:** Add concrete steps or example configuration

### Issue: Missing Context

**Example:** Assumes knowledge of protocol details  
**Ask User:** "Should we add protocol overview or link to spec?"  
**Fix:** Add prerequisite section with links

---

## Special File Types

### Prompt Files (for AI)

**Review Focus:**
- Are instructions clear and unambiguous?
- Do examples show expected input/output?
- Are edge cases documented?
- Is context sufficient for AI to act independently?

**Example Issues:**
- "Vague instruction: 'handle errors properly' → Be specific"
- "Missing example: Show complete code snippet with error handling"

### Project Context Files (copilot-project-context.md)

**Review Focus:**
- Does it accurately describe current project state?
- Are key files and patterns documented?
- Is build/test workflow clear?
- Are conventions and anti-patterns listed?

**Example Issues:**
- "References deleted file XYZ.md → Remove or update"
- "Build command outdated → Verify with platformio.ini"

### Component Documentation (COMPONENT_PROMPT.md)

**Review Focus:**
- Hardware details: pins, protocols, addresses
- API reference: public functions, structs, enums
- Usage examples: initialization, typical calls
- Dependencies: other components, external libs

**Example Issues:**
- "Pin assignment changed in code but not in doc"
- "Missing example of typical initialization sequence"

### Module Prompts (MODULE_NAME_PROMPT.md)

**Review Focus:**
- Hardware specification (buttons, LEDs, displays)
- Event handling (what events trigger what actions)
- Protocol integration (messages in/out)
- Implementation checklist

**Example Issues:**
- "Event type renamed in protocol but not here"
- "Pin mapping doesn't match module_io.h"

---

## Progress Tracking

After each review, update `PROMPT_REVIEW_PLAN.md`:

```markdown
### Example Update

Before:
- [ ] `ots-fw-main/prompts/ALERT_MODULE_PROMPT.md` - Alert module specification

After:
- [x] `ots-fw-main/prompts/ALERT_MODULE_PROMPT.md` - Alert module specification
  ✅ Reviewed 2026-01-05: Updated pin mappings, fixed event type names

Update statistics:
**Progress:** 1/72 files reviewed (1.4%)
```

---

## Example Review Session

### User:
```
Review file: ots-fw-main/prompts/NUKE_MODULE_PROMPT.md
```

### Copilot Response:

**📖 Reading file...**
- File: `ots-fw-main/prompts/NUKE_MODULE_PROMPT.md` (850 lines)
- Purpose: Hardware module specification for nuke launch buttons
- Last updated: December 9, 2025

**🔍 Code Synchronization Check...**
- ✅ Referenced file `nuke_module.c` exists at expected path
- ✅ Pin definitions match `module_io.h`
- ⚠️ Event type `NUKE_LAUNCHED` found, but doc shows `ATOM_LAUNCHED`
- ✅ Hardware specs match PCB design in `ots-hardware/modules/nuke-module.md`

**❓ Question for you:**
The code uses event type `NUKE_LAUNCHED` for all three nuke types, but this doc shows separate events `ATOM_LAUNCHED`, `HYDRO_LAUNCHED`, `MIRV_LAUNCHED`. Which is correct? Should we:
1. Update code to use three separate events (match doc)
2. Update doc to use single event with data field (match code)
3. Something else?

*(User responds, Copilot continues with remaining steps...)*

---

## Tips for Efficient Review

1. **Batch similar files** - Review all MODULE_PROMPTs together to spot patterns
2. **Start with copilot-project-context.md files** - These are high-value for AI
3. **Check WEBSOCKET_MESSAGE_SPEC.md early** - It's the source of truth for messages
4. **Use semantic search liberally** - Don't guess if code examples are current
5. **Ask user for clarification** - Don't assume intent, especially for ambiguous sections
6. **Document non-obvious decisions** - Add notes to plan file for future reference

---

## When to Skip or Delete

### Skip if:
- File is auto-generated (managed_components/)
- File is part of external dependency
- File is intentionally outdated (historical record)
- File references `doc/` folder (user documentation synced to ots-website, outside review scope)

### Recommend deletion if:
- Content is 100% duplicated elsewhere
- Feature was removed and doc is obsolete
- Temporary file (e.g., migration notes after migration complete)
- Implementation notes for completed work

### Recommend merge if:
- Two files cover >70% same content
- One file is subset of another
- Split causes confusion (e.g., part 1 vs part 2)

---

## Final Notes

- **Be systematic:** Follow steps 1-7 for every file
- **Be thorough:** Don't skip validation steps to save time
- **Be clear:** Ask specific questions, propose concrete changes
- **Be efficient:** Use multi_replace for batch edits
- **Be persistent:** Some files need multiple iterations

**Goal:** By the end, every prompt file is accurate, clear, complete, and synchronized with code.

---

*Use this guide with `PROMPT_REVIEW_PLAN.md` to systematically improve documentation quality across the entire OTS project.*
