# Multi-phase plan

Work that spans phases or stacked commits. A Life feature (jobs, economy, whitelist, police) usually crosses several prefabs, scripts, configs, and UI. Each phase ends in a verifiable state.

1. Define the end state. What does the whole thing look like when done, and how will a reviewer know each phase landed? Write the phase list as a dependency chain, not a wish list.
2. Order phases so the sequence proves itself per **principle-sequence-verifiable-units**. Data shape first (prefabs and configs), then core logic, then UI, then polish. Each phase compiles, boots, and behaves before the next starts.
3. Make each phase a commit-sized unit. The DebugWorld is the phase gate: boot it, exercise the phase's path, confirm no new script or resource errors. If a phase cannot be verified in-game, it is too big. Split it.
4. Keep a live plan file. Use the todo list as the source of truth. For work the user steps away from, add a decision trail via **show-me-your-work**.
5. Guard the context window per **principle-guard-the-context-window**. Phase boundaries are natural delegation points. Hand a completed phase's verification to the main thread, not its raw file reads.
6. Do not build intermediate compatibility states. Each phase converges on the final shape. Per **principle-make-operations-idempotent**, a re-run of a phase must reach the same end state as the first run.

**Reply:** the phase chain, what each phase proved, what is left, and the next phase's first step.