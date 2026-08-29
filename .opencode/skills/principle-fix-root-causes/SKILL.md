---
name: principle-fix-root-causes
description: "Use ONLY when explicitly directed by the life-mode skill or another skill, or when you are debugging Enfusion code. Trace each symptom to its root cause and fix it there; reproduce first, ask why until you reach it, resist guards that silence crashes. Never auto-load for routine work."
---

# Fix Root Causes

Trace each symptom to its root cause and fix it there; reproduce first, ask why until you reach it, resist nil-check guards that silence crashes.

**Why:** A bug fix that patches the symptom is a second bug wearing a disguise. A nil-check that stops a crash hides the real question: why was the component missing? In a networked mod the cost compounds: the symptom appears on the client, the cause sits in server-side state or a replication decision one subsystem over.

**Rule:**
- Reproduce first. You cannot prove a root cause fixed what you cannot reproduce. Reproduce from the server log or in the DebugWorld.
- Ask why until you reach the mechanism. Each answer must explain the observed evidence, not just sound plausible.
- A guard that only silences a crash is a hypothesis, not a fix. If you add a nil-check, also find out why the value was nil and fix that.
- When evidence refutes a hypothesis, revert what the hypothesis motivated. Do not leave dead guards behind.
- Confirm the mechanism before planning the fix. A design grounded on a plausible-but-unconfirmed cause can be unanimously wrong.

**In this repo:**
- Script errors surface in `console.log` / `error.log` with the class and line. Start there, then read the offending `EL_` source with the error in hand.
- A component that comes back null from `EL_Component<Class T>.Find()` means the prefab does not carry it. Fix the prefab wiring, not just the call site.
- A persistence value that loads as default usually means the save path and the load path disagree on a key. Align the schema; do not default-silence the mismatch.
- Use the DebugWorld's `Tests.layer` and `EL_Test` to reproduce logic bugs deterministically before hunting in a live server.