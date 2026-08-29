---
name: principle-sequence-verifiable-units
description: "Use ONLY when explicitly directed by the life-mode skill or another skill, or when doing multi-step Enfusion work or staging commits. Break work into small units that each end in a verifiable state, check each before the next, order delivery so the sequence proves itself. Never auto-load for routine work."
---

# Sequence Work into Verifiable Units

Break work into small units that each end in a verifiable state, check each before the next, and order delivery so the sequence proves itself to a reviewer.

**Why:** The failure mode of mod development is the mega-commit: twenty files, one message, unverifiable. When each unit ends in a check, a broken unit bisects cleanly, a reviewer reads the diff like a story, and the game is never left in a half-broken state.

**Rule:**
- The smallest unit that ends in a check. For a script change, the check is `tools\cli validate` plus a boot. For a prefab change, the check is that the resource resolves and the world loads. For a logic change, the check is the `EL_Test`.
- Verify each unit before starting the next. A failing base poisons every unit stacked on it.
- Order units so each proves the previous: data shape before logic, failing test before fix, prefab before script that reads it.
- Stage commits so the repro or test lands before the fix. The diff tells the story in the order it happened.

**In this repo:**
- `tools\cli validate` is the per-unit gate for script and repo hygiene.
- `tools\cli test` (boot smoke) is the per-unit gate for world and resource health.
- `EL_Test` classes (registered automatically via `tools\cli regen-tests`, gated by `tools\cli validate`) are the per-unit gate for pure logic.
- Commit units map to feature phases: one feature folder, one working slice, one commit.