---
name: principle-make-operations-idempotent
description: "Use ONLY when explicitly directed by the life-mode skill or another skill, or when you are designing persistence, save/load, respawn, or loops that run amid disconnects and retries in Enfusion. Converge to the same end state. Never auto-load for routine work."
---

# Make Operations Idempotent

Converge to the same end state regardless of partial prior runs.

**Why:** Server code runs amid chaos. Players disconnect mid-transaction, saves crash half-written, respawns double-fire, RPCs retry. An operation that assumes it runs exactly once corrupts state on the second run. An idempotent operation converges: the tenth run leaves the same state as the first.

**Rule:**
- An operation that creates or mutates persistent state must be safe to run twice.
- Guard with the state, not with a flag that resets. "If this account already has the transaction, skip" survives restarts; "if a bool is set, skip" does not.
- Respawn, teleport, and load are the classic offenders. Verify the player is in the target state before applying it, and verify they are not already there.
- Persistence writes should converge: writing the current model to disk twice is the same as once.
- When a retry is possible, make the retry harmless rather than suppressing it.

**In this repo:**
- The persistence layer (`Feature/*/Persistence/EL_*Persistence`) is the highest-stakes surface. Load must be re-runnable and write must be re-runnable.
- `EL_GameModeRoleplay` lazily creates managers on `OnPlayerConnected`. A second connect must not create a second manager instance for the same player.
- Money and inventory mutations on disconnect are the risky path: a player who disconnects mid-purchase should end in a consistent state whether the transaction completed or not, and never doubled.