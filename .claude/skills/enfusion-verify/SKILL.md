---
name: enfusion-verify
description: "Use before declaring Life Framework work done, or to plan or perform proof: validation, tests, runtime behavior, dedicated server, persistence, networking, UI, or late join. Compilation alone is never proof."
---

# Verify

Done means the changed behavior was observed. Use the smallest gate that can
prove it; `AGENTS.md` defines the ladder and the proof classes.

1. Inspect the real diff and artifacts, not your summary of them. Run
   `tools\cli validate` for every code or resource change.
2. Run the relevant `LF_Test` tier in a worktree: `tools\cli test --no-build
   --tier fast` for logic, `--tier all` for world/runtime changes and before
   calling a phase complete. `tools\cli logs <pattern>` greps the run.
3. Exercise the actual feature path: DebugWorld play mode, the agent box
   (`node agent-box/harness.mjs flow <name>`), or the dedicated server.
4. Read the specific log line, assertion, or UI value. Error counts alone are
   not evidence; check `docs/debug-log-triage.md` before chasing a line.
5. For persistence, networking, or JIP changes add the matching proof class:
   a real save, restart, and load; a dedicated server rather than a host; a
   late joiner. In-session round trips prove nothing about restarts.
6. Textures are verified visually only; the dedicated server has no renderer. Two things a Workbench `look` cannot
   prove: a sign's text (blank plate) and a light at 22:00; both are a full-profile session (**polish**, "What
   `look` cannot show" and "Night").
7. Read a proof or flow by its `PASS`/`FAIL` verdict and `FAIL <step>` lines. `failures=` counts failed steps;
   `verb-errors=` counts harness answers retried by design. Session discipline (`--hold 20`, `session down`
   straight after, never a proof twice inside 5 minutes) is in **agent-scope** "Sessions". `down` deletes the client
   pictures: `session.mjs pull <dir>` first.
8. Pick the existing proof before writing one (`agent-box/README.md`, "Proofs added 2026-11-27"): `crime-playtest-proof`
   (crime and police loop, two clients), `downed-call` and `downed-flow-proof` (downed, medic call, HUD, action order),
   `clinic-bed-proof`, `lot-sheet-proof` (`lotsheet` verb), `map-search-proof` (`maplegend search`), `jip-sweep-proof`
   (late joiner: `seen` plate, door, house, flag; `peers worn=`).

If automation cannot cover the path, write the exact manual actions and pass
criteria into the PR description. Report what was proven and what was not.
