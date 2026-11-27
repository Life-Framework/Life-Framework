---
name: agent-scope
description: "Use when you are an agent assigned a scope, issue, or PR in this repo. The protocol from worktree to merged PR: setup, reading, TDD with observed red-proofs, the fast test loop, shipping with wt ship, attribution, locks, cleanup, and the final report line."
---

# Agent scope

`AGENTS.md` is the contract; this is the order to apply it in.

**A background run never wakes a subagent or delegated task.** Ending your turn
while `test`, `e2e` or `session` runs in the background ends the task
unfinished. Run it in the foreground (timeout up to 600000 ms) or call
`t3_background_wait` before ending the turn.

1. **Worktree.** `tools\cli wt new <slug>` is your FIRST command: no `git pull`
   before it (it fetches origin itself). Then work only in the path it prints, on
   `ws/<slug>`. Never run git in the main checkout and never write into it; the
   hook refuses both. Scratch goes in `<worktree>\tmp\` with names unique to your task
   (`tmp\pr-body.md`), never a shared session dir.
2. **Read.** The issue (`gh issue view <n>`; for an old bug run `tools\cli issue-check <n>`
   first and read the PRs it cites, since it is often already fixed) or PR, `AGENTS.md`, the
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
5. **Ship.** Commit (end the message with the attribution line your session
   gives), write the PR body to `tmp\pr-body-<slug>.md`, then
   `tools\cli wt ship <slug> --refs <n> --body-file tmp\pr-body-<slug>.md`. It
   checks the tree, prints the tests your diff can break that GitHub's
   `validate` never runs (run that `test --no-build --only ...` line before you
   ship; pack data prints the whole pack leg), runs a seconds-long preflight
   (merge-tree conflicts, `validate` of the changed inputs), pushes, opens the PR and turns auto-merge
   on at once. There is no pre-merge gate: GitHub's `validate` is the check and
   the main guard compiles and tests `main` afterwards (`docs/worktrees.md`
   "Ship"). Do not poll for the merge; `tools\cli prs status` once. The async
   ship queue is retired. Ship checks the body and refuses: an `addons/` change with no
   body file, a `TODO(scaffold)` left in it, no `## Manual testing` section, a
   player-visible change with no embedded picture (`look`, `look item` or
   `wb_preview` pictures kept with `tools\cli evidence`, which prints the
   markdown; no session needed; `No screenshots: <reason>` waives it), or an open finding in
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
   Shipping is inside it. When you are near the limit (about 60 of 80 calls) and the work
   passes its tests, ship what you have (step 5): list the checks you did not reach under
   `## Manual testing` or as follow-up issues, and do not keep polishing. Only when the work is
   not yet shippable, hand off at the next checkpoint: commit to `ws/<slug>`, write
   `tmp/handoff-<slug>.md` (at most 15 lines), and end with
   `RESUME <worktree>\tmp\handoff-<slug>.md`. A fresh agent that only ships gets the
   ship-only brief in `docs/agent-budget.md`.
10. **Report.** Five lines: `Changed:`, `Evidence:`, `Manual:`, `Next:`, and
   last exactly `PR <full URL>, head <SHA>, checks and reviews <state>`
   (`gh pr view <n> --json statusCheckRollup,reviewDecision,mergeStateStatus`),
   or the `RESUME` line.

## Lessons from the 0.3.0 weeks (2026-11-24 to 27)

- **A string edit updates every test that pins the old text.** #4993 (a US English pass) turned `main` red: a
  reworded row, and `LF_Test_ChargeSheetScreen` still asserted the old words. Before shipping one, grep the old text
  in `addons/*/Scripts/Game/Tests` and `tools/validation`, fix the hits, then `test --no-build --only` those tests.
  Assert through `LF_Terms.Text("#Key")` where you can, not a copy of the words.
- **One slot per island-layer branch.** A `Place_*.layer` changes only in a `town-draft-<place>-<slot>` worktree (a
  follow-up on a slot with an open PR: `...-<slot>-<word>`), one slot per PR (`brief town`). PRs that append to the
  same `docs/features/<area>.md` go CONFLICTING on GitHub (no merge driver there); the `pr-autoheal` workflow merges
  main in within minutes, `prs heal` is the fallback (six night-light PRs, 11-25). Do not hand-merge the append.
- **Sessions.** Prepare every edit first. A proof over about 10 minutes needs `session up --hold 20` (the pool closes a
  session at 10 to 12 minutes when anyone waits). Proof and shots in that one session, `session down` straight after.
  In a proof's `SUMMARY`, `failures=` counts failed steps; `verb-errors=` counts harness answers retried by design
  (`widget_not_found`, `not_carried`); the verdict is the `PASS` or `FAIL` line. Never run a proof twice inside 5
  minutes without a restart (rocks and smelters do not reset). A proof failing on a stale expectation (a changed first
  lot row, a character who already holds the licence) is a proof to fix, not a feature bug.
  `session down` deletes each client's screenshot folders: `session.mjs pull <dir>` first, or the pictures are gone.
  `session up` answering `GAME UPDATE PENDING` is Steam, not your change: stop, never retry it (each retry starts a
  server that blocks the update), run `tools\cli game-update --check` and tell the lead (`docs/worktrees.md` "Game
  update guard"). The proofs for a feature are listed in `agent-box/README.md`.
