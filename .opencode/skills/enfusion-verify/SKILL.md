---
name: enfusion-verify
description: "Use when declaring any task done on this mod, or when planning how to prove a change works. The verification ladder for Arma Reforger / Enfusion work: repo hygiene, headless compile, in-game test suites, DebugWorld play, dedicated-server boot, log reading, and the restart + late-join proofs. Trigger on 'verify', 'prove it works', 'does it work', 'test in-game', 'how do I check', 'boot smoke', 'run the tests'. Use ONLY for verifying changes to this mod's addon, not for its own tooling."
---

# Verify

In-game proof is the only proof that counts. This is the ladder, cheapest gate first. A change is not done until it has climbed the rungs its scope requires.

**Canonical source.** The ladder and proof classes here are mirrored in `AGENTS.md` ("Verification (the ladder)"), the portable contract every AI tool reads. When they diverge, AGENTS.md wins. This skill adds the workflow detail and the MCP tooling that backs each rung.

## The rungs

### Rung 1. Repo hygiene (free, always)
```
tools\cli validate
```
Runs `tools/validation/validate-repo.ps1` (artifacts, orphan metas, duplicate GUIDs, lowercase `data/`) and `tools/validation/validate-scripts.ps1` (headless Workbench EnforceScript compile, ~seconds to minutes warm). The compile rung is **cheap: run it constantly.** Do not ask the user to compile for you; this is the agent's own gate.

### Rung 2. In-game test suites (scarce)
```
tools\cli test
```
Builds the addon and boots the dedicated server on `Worlds/DebugWorld`, runs the `EL_Test*` suite, and parses the `[ELTEST] SUMMARY` markers plus engine errors. The `EL_Test` framework writes a JUnit-style XML to `$profile:TestResults`. **This gate launches a real Reforger server and takes resources: spend it deliberately, once after a phase or fix is complete, never mid-edit, never as a baseline.** Coverage is a spine, not a surface: pure logic and boot health are covered; JIP/multiplayer, full UI flows, and save/reload round-trips are not, and stay manual.

### Rung 3. DebugWorld play (manual, the real thing)
Drive the actual feature path in the Workbench play mode or a local server on the DebugWorld. Exercise the path the feature changes: spawn the item, trigger the action, open the menu, read the observable result. This is what **principle-prove-it-works** means: the feature run, not the code path that looks similar.

### Rung 4. The logs (read them, do not trust memory)
- Script errors: `console.log` / `error.log` under `Documents/My Games/ArmaReforger/logs/<date>/`, patterns like `SCRIPT (E)`, `VME:`, `Unable to find`.
- Compile errors surface through `tools\cli validate` with `file:line`-style output.
- Workbench console during interactive sessions shows runtime errors and `Print()` output.
- **Never judge a run by console error counts.** A green run still prints engine noise; judge by the specific error patterns and by the test verdict, not by the total.

## The proof classes that survive the ladder

- **Restart.** In-session round trips prove nothing about restarts. For persistence or world-state changes, do a real save → restart → load and confirm the state came back. The restart path is the ground truth.
- **Dedicated server.** A green host run proves nothing about a dedicated server. UI code without a server-side guard, client-side assumptions in server code, missing `Replication.IsServer()` checks: these surface only on the dedicated build.
- **Late joiner.** JIP state that was never written is invisible to players who were there from the start. Test with one client joining after the state exists.

## No debugger, no hot reload

EnforceScript has no interactive debugger and no hot reload. Debug with `Print()` and the logs, then restart play mode. This mod's `EL_Test` suite is the deterministic reproduction surface for logic bugs.

## What to hand to the user

For anything the automated rungs do not cover, hand a specific manual procedure, not a general request: what to do, what to observe, what counts as pass. "Please test" with no steps is a plan for nothing.

## Script the check when you can

The strongest proof is a deterministic script a reviewer re-runs. This repo already scripts its checks. A new gate is one script dropped in `tools/validation/`, `tools/lint/`, or `tools/test/`, wired by `tools\cli`. If you caught yourself re-running the same verification by hand, that is the signal to script it per **principle-encode-lessons-in-structure**.