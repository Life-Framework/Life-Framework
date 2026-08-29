---
name: principle-prove-it-works
description: "Use ONLY when explicitly directed by the life-mode skill or another skill, or when you are about to declare a task done. Verify against the real artifact: run the feature in the DebugWorld, read the actual log line, inspect the actual diff. 'It compiles' is a proxy, not proof. Never auto-load for routine work."
---

# Prove It Works

Verify every task output by checking the real thing directly. Do not infer from proxies, self-reports, or "it compiles".

**Why:** Unverified work has unknown correctness. In a game mod, "it compiles" is the weakest possible claim: the script compiles while the prefab still points at a missing resource, the RPC still fires on the wrong side, the value still never reaches the player. The DebugWorld and the server log are the source of truth.

**Pattern:** After completing any task, ask: how do I prove this actually works?

Check the real thing, not a proxy:
- Read the actual log line, not a summary of it.
- Drive the actual feature path in the running game, not the code path that looks similar.
- Inspect the actual diff and resource tree, not the plan.
- When verification fails, suspect the observation method before suspecting the system.

Code and features:
1. Compile (necessary, not sufficient). `tools\cli validate` proves scripts compile.
2. Boot and exercise the actual feature path in the DebugWorld or the dedicated server.
3. Check the full chain: does data flow from config to component to player?
4. For networked features, verify on both sides: what the server sees and what the client sees.

Delegation: trust artifacts, not self-reports. When verifying delegated work, inspect the actual output artifact (the diff, the file contents, the log), not the delegate's summary.

**In this repo, proof looks like:**
- `tools\cli validate` passes (compiles, repo hygiene).
- `tools\cli test` boot smoke: the DebugWorld boots with no script or resource errors.
- The `EL_Test` suite passes with a fresh JUnit XML in `$profile:TestResults`.
- The feature's actual path exercised in-game, with the observable state read back (log line, UI value, inventory change).

**The restarts and the second player are proof too.** Three classes of bug survive every happy-path check:

- **In-session round trips prove nothing about restarts.** A save that commits in-session can still fail to restore after a real server restart, because a load only instantiates what the load path actually brings back. The restart path is the ground truth, not the save button.
- **A green host run proves nothing about a dedicated server.** UI code without a server-side guard, client-side assumptions in server code, missing `Replication.IsServer()` checks: these only surface on a dedicated server build.
- **A green first player proves nothing about a late joiner.** JIP state that was never written is invisible to the players who were there from the start. Test with one client joining after the state exists.

## Script the check when you can

The strongest proof is a deterministic script that re-runs the same comparison. The repo already scripts its checks: `tools/validation/`, `tools/test/`, `tools\cli`. Run the script, keep its output as the artifact, let a reviewer re-run it instead of trusting your word.