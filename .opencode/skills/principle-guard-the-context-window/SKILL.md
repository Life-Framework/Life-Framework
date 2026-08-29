---
name: principle-guard-the-context-window
description: "Use ONLY when explicitly directed by the life-mode skill or another skill, or when your context is filling up during Enfusion work. Route bulk reads and sweeps to subagents; keep summaries in the main thread, not raw payloads. Never auto-load for routine work."
---

# Guard the Context Window

Route bulk to subagents; keep summaries in the main thread, not raw payloads.

**Why:** Enfusion work is context-heavy. A single `.et` prefab can be hundreds of lines. An `api_search` on a broad term returns many classes. A `game_read` of a vanilla class is large. Reading all of it into the main thread crowds out the reasoning that actually matters.

**Rule:**
- When a task needs many reads (a tree of prefabs, a family of classes, a sweep across a feature folder), spawn a subagent to do the reads and return a tight summary with file paths.
- Pass file paths to subagents, not inlined file contents.
- Keep the main thread on decisions, not on raw payloads. The summary is the artifact that informs the decision; the paths are how you verify.
- Reserve full reads for the specific file you are about to edit.
- A large refactor across many files is a delegation boundary, not a stamina test.

**In this repo:**
- `prefab` inspect on an inherited `.et` pulls the whole ancestry chain. Let a subagent summarize the merged component set instead of pasting it all into the thread.
- A feature sweep (read every `EL_*` in a folder to plan a refactor) is a subagent job.
- Prefer the Workbench MCP index tools (`wb_read_props` style resolved reads, `find-references`) that return only the resolved value over raw file dumps.