# Starter pack: slice card (M4/M5 slices)

One card is one job of at most 45 min / 60 calls (`docs/process/slice-card.md`). Do not read the roadmap or other PRs.

## Start
```
tools\cli slice brief <N>                      # goal, acceptance, owned files, timebox, machines
tools\cli wt new slice-<N> --wait ; gh issue edit <N> --add-label agent:working
```
Before building an old bug, run `tools\cli issue-check <N>` and read the cited PRs: today three of five were already fixed.
Read only the card, `AGENTS.md`, the area's `Feature.md` and `docs/features/<area>.md`. Stay inside `owns`; shared
append-only files (`docs/features/<area>.md`, string table, triage) are fine; anything else: stop and report.

## Red first, then green
```
tools\cli feature new <Area> <Name> --kind logic|world|ui|item --issue <N>   # scaffold, if the card adds a feature
tools\cli test --no-build --only LF_Test_X            # the ~10 s loop
tools\cli test --no-build --red LF_Test_A,LF_Test_B   # ONE boot, new tests with no failing state; keep the RED-OK line
tools\cli validate                                    # first run reuses checks other worktrees passed
tools\cli test --no-build --tier all                  # T2 only, once, at the end; T0/T1 skip it
```
- Batch: one `--only A,B,C` run covers every test you touched (each run is a server boot plus queue, ~1-3 min).
  Fix compile errors from the first run's `SCRIPT (E)` lines all at once before running again.
- `regen-tests` and `regen-localization` once, right before you commit, not after every edit.
- Bug fix: the run failing on the unfixed code is the red proof; put it in `// red-proof:`, no break-and-restore run.
- Script-only change whose tests touch no new resource: `LF_RDB_REFRESH=0` skips the ~10 s resource-database refresh
  (PowerShell: `$env:LF_RDB_REFRESH = "0"`). Any new prefab or `.meta`: leave the refresh on. Never commit the rdb.
- `test ... --no-cache` forces a run; a pass on identical inputs is otherwise reused.
- Output is one `SUMMARY` line and the failing items; the full log is `tmp\logs\`. Never pipe it through `tail`.

## Ship
```
tools\cli wt ship slice-<N> --refs <N> --body-file tmp\pr-body-slice-<N>.md --commit "<subject>"
tools\cli slice record <N> --model sonnet --outcome merged --gate pass|fail
```
- `--commit` commits anything left (git add -A) first; without it a dirty worktree is refused.
- Ship merges `origin/main` only when `git merge-tree` reports a conflict, and regenerates a conflict that is only in the
  language confs. Only a conflict in your own files comes back to you.
- `slice record` computes minutes from the worktree's creation time; do not estimate them. `--minutes N` is kept only as `agentMinutes` on the row.
- `slice record --outcome merged` with the PR still open records `shipped` and exits 0. Then stop: never poll
  `gh pr checks`, `Wait-Process` or sleep for the merge; auto-merge and the main guard finish it.
Body: what changed, the observed red and green lines, `## Manual testing` for anything automation cannot reach, and the
`Generated with [Claude Code]` line. A visible change carries one `look` picture (`tools\cli evidence`). Say
"merged, in-dev", never "done". At the limit, ship what passes, write `tmp/handoff-slice-<N>.md` (15 lines), say `SPLIT`
and list the rest as new cards; never extend.

- `wt ship` prints "the tests this diff can break" before the preflight (GitHub's `validate` runs no game tests): run that
  `test --no-build --only ...` line first. Any pack data change prints the whole pack leg (~40 s); #5096 went red on
  `everon/mannequins` that way.

## Final report
Five lines: `Changed:`, `Evidence:`, `Manual:`, `Next:`, then exactly
`PR <full URL>, head <SHA>, checks and reviews <state>` (or the `RESUME <path>` line).

## Known traps
- There is no test registry: a new `LF_Test_*.c` with its own `[LF_TestInfo]` is found by reflection; `tools\cli regen-tests` only checks it.
- A string is appended to `Language/everonlife_localization.st`, then `tools\cli regen-localization`. No `EL_` names.
  Changing an existing string: grep the old text in `Tests/` and `tools/validation` and fix every pin (#4993 turned `main` red).
- Under the 0.3.0 freeze a card is a fix, wording or readiness job; a new feature is refused (`life-mode` "Under a feature freeze").
- `[LF_TestInfo(LF_TestTier.LOGIC|WORLD|PERSISTENCE)]` and a `// red-proof:` line sit above the class. A test never asserts a town
  coordinate or distance.
- Change a feature: update `docs/features/<area>.md` in the same PR; never add rows to `docs/features.md`.
- Persistence tests use public mutators only; an in-session round trip proves nothing about restart.
- Merge `origin/main` only when GitHub says `CONFLICTING`.
- A world prop goes in your own grid cell (`brief grid`), never a town layer.
- Data checks `validate` runs for you: a prop with an action needs an `RplComponent` (`validate-actions-have-rpl`); a
  price lives in one pack file (`validate-shop-price-owners`); a mannequin wears only items its outlet sells
  (`validate-mannequin-outfits`).
- Harness: `hold weapon` re-draws the first slotted weapon and `itemdrop` cannot clear a weapon-slot item (fresh
  `session down` + `up` per tool); `input SwitchCameraType` does not reach third person (`agent-box/README.md`).

- Proofs: look in `agent-box/README.md` ("Proofs added 2026-11-27") before writing one. `session down` deletes client
  screenshots: `session.mjs pull <dir>` first. `GAME UPDATE PENDING` from `session up`: do not retry, `tools\cli game-update --check`.
  `validate-no-escaped-quote-targets`: no `\"` inside a string-table Target.

## Environment
No Workbench on the maintainer's PC (Gaming-Box); on lf-testbox (PC1) it is allowed through the pool (`tools\cli status` says which you are on). Never pin a machine (`--local`, `--machine`). Run in the foreground (timeout up to
600000 ms) or `t3_background_wait`; never end the turn with a run pending. Never `--force`, `rm -rf` or
`Remove-Item -Recurse` a worktree (`wt prune`). Heavy commands refuse the main checkout.
