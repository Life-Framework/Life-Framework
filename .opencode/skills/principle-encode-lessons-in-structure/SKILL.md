---
name: principle-encode-lessons-in-structure
description: "Use ONLY when explicitly directed by the life-mode skill or another skill, or when you catch yourself writing the same instruction a second time. Encode the rule as a validation script, lint, test, or metadata flag instead of more prose. Never auto-load for routine work."
---

# Encode Lessons in Structure

Encode the rule as a lint, metadata flag, runtime check, or script instead of more text.

**Why:** A rule written in prose is a rule that gets forgotten. A rule encoded as a script runs on every commit and cannot be forgotten. This repo already made the choice: `tools/validation/validate-repo.ps1` blocks bad commits mechanically instead of asking contributors to remember.

**Rule:**
- When you write the same instruction twice, convert it into a check that runs without you.
- Prefer the strongest enforcement: a pre-commit hook beats a CONTRIBUTING.md paragraph; a compile-time shape beats a runtime assert; a runtime assert beats a comment.
- New tools are cheap here. `tools\cli validate|lint|test` runs every script in the matching folder. A new check is one script file.
- A lesson encoded as a check should read like a contract: pass or fail, with a message that says what to fix.

**In this repo:**
- The pre-commit hook (`.githooks/`, `tools/validation/validate-repo.ps1`) already encodes: no Workbench artifacts, no MCP clones, no orphan `.meta`, no duplicate GUIDs, no lowercase `data/` paths.
- `tools/validation/validate-scripts.ps1` encodes "scripts must compile" as a headless Workbench run.
- `tools/test/test-boot-smoke.ps1` encodes "the world must boot clean" as a server boot.
- If you keep fixing the same class of bug by hand, the lesson is to add an `EL_Test` for it. That is the encode step.