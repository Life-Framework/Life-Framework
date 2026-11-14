---
name: agent-scope
description: "Use when you are an agent assigned a scope, issue, or PR in this repo. The protocol from worktree to merged PR: setup, reading, TDD with observed red-proofs, the fast test loop, shipping with wt ship, attribution, locks, cleanup, and the final report line."
---

# Agent scope

`AGENTS.md` is the contract; this is the order to apply it in.

1. **Worktree.** `tools\cli wt new <slug>`, then work only in
   `..\Life-Framework-ws-<slug>` on `ws/<slug>`. Never write into the main
   checkout. Scratch goes in `<worktree>\tmp\` with names unique to your task
   (`tmp\pr-body.md`), never a shared session dir.
2. **Read.** The issue (`gh issue view <n>`) or PR, `AGENTS.md`, the
   `docs/decisions.md` entries for your area, and the area's `Feature.md`.
3. **Scaffold, then red first.** A feature starts with
   `tools\cli feature new <Area> <Name> --kind logic|world|ui|item --issue <n>` and follows
   `tmp/feature-<name>.md` (the **life-mode** feature playbook); anything a player sees follows
   **polish**. Write the `LF_Test` (see **el-tdd**) and record the observed red in
   `// red-proof:`: for a bug fix, the run that fails on the unfixed code and passes on the
   fixed code; for new tests, one batched `tools\cli test --no-build --red <LF_Test_A,LF_Test_B>`
   (the `RedProof()` hook; every listed test must fail). Loop with
   `tools\cli test --no-build --only <LF_Test_Class>` (~10 s); run
   `tools\cli validate` as often as you like (it skips the Workbench compile; a
   compile error fails `test`). Run `--tier all` once, at the end.
   **Output is already compact.** `validate`, `test`, `session up`, `look`, `wt gate|ship`,
   `asset run`, `harness.mjs flow` and the `session.mjs <proof>` verbs print one
   `SUMMARY <cmd> ok|fail ... log=<path>` line, then only the failing items with file:line and the
   exact error. Do not pipe them through `tail`, `head` or `Select-Object`: that is how the line you
   needed got cut. The full transcript is the `log=` file under `<worktree>\tmp\logs\`; `grep -n`
   it or read a line range. `--verbose` prints everything as before
   (`docs/agent-budget.md` "Compact command output").
4. **Main.** Merge `origin/main` only when GitHub says `CONFLICTING` or you need
   code from it. `wt ship` handles the rest.
5. **Ship.** Commit (message ends with
   `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`), write the PR body
   to `tmp\pr-body.md`, then
   `tools\cli wt ship <slug> --refs <n> --body-file tmp\pr-body.md`. It
   checks the tree, merges main only on conflict, runs `validate --full` +
   build + `--tier all`, pushes, opens the PR, and queues auto-merge.
   Ship checks the body before the gate and refuses: an `addons/` change with no
   body file, a `TODO(scaffold)` left in it, no `## Manual testing` section, a
   player-visible change with no embedded picture (`tools\cli evidence` prints
   the markdown; `No screenshots: <reason>` waives it), or an open finding in
   `tmp/review-<slug>.md` (**interrogate**), which ship appends to the PR.
   The body has: what changed, one line on how it is in-world first (or why
   it needs UI; `docs/decisions.md` "In-world first"), red and green evidence
   with the observed lines, manual steps for anything automation cannot
   reach (under `## Manual testing`: it makes the issue `needs-playtest` and
   feeds the human's promotion checklist), and ends with
   `🤖 Generated with [Claude Code](https://claude.com/claude-code)`.
   Shipping does not close the issue: it becomes `in-dev` (merged, not verified)
   and stays open until a human runs `promote --confirm`. Report it as
   "merged, in-dev", not "done"; `--closes N` still works but means `Refs`.
6. **Safety.** Never `git worktree remove --force`, `rm -rf`, or
   `Remove-Item -Recurse` on a worktree or `server/profile` (junctions into
   the Steam installs). Use `tools\cli wt prune <slug>` or
   `node tools/safe-rm.mjs <dir>`.
7. **Locks.** Machine-wide locks live in `<main>\tmp\wt\locks\`. `server-2001`
   covers every test, dev, serve, host, and e2e boot (the CLI takes it; queue,
   do not delete it); `build` covers the headless Workbench build;
   `agent-box-play` covers a rendering play session. Never remove a lock whose
   owner pid is alive.
8. **Close what you opened.** Stop servers, Workbench, and game clients you
   started (`tools\cli host stop`, close the play session). Leave no process
   holding a lock.
9. **Budget.** Your agent file sets a tool-call budget (`docs/agent-budget.md`).
   At it, hand off at the next checkpoint: commit to `ws/<slug>`, write
   `tmp/handoff-<slug>.md` (at most 15 lines), and end with
   `RESUME <worktree>\tmp\handoff-<slug>.md`.
10. **Report.** Five lines: `Changed:`, `Evidence:`, `Manual:`, `Next:`, and
   last exactly `PR <full URL>, head <SHA>, checks and reviews <state>`
   (`gh pr view <n> --json statusCheckRollup,reviewDecision,mergeStateStatus`),
   or the `RESUME` line.
