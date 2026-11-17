# Starter pack: slice card (M4/M5 slices)

One card is one job of at most 45 min / 60 calls (`docs/process/slice-card.md`). Do not read the roadmap or other PRs.

## Start
```
tools\cli slice brief <N>                      # goal, acceptance, owned files, timebox, machines
tools\cli wt new slice-<N> --wait ; gh issue edit <N> --add-label agent:working
```
Read only the card, `AGENTS.md`, the area's `Feature.md` and `docs/features/<area>.md`. Stay inside `owns`; shared
append-only files (`docs/features/<area>.md`, string table, triage) are fine; anything else: stop and report.

## Red first, then green
```
tools\cli feature new <Area> <Name> --kind logic|world|ui|item --issue <N>   # scaffold, if the card adds a feature
tools\cli test --no-build --only LF_Test_X            # the ~10 s loop
tools\cli test --no-build --red LF_Test_A,LF_Test_B   # ONE boot, new tests with no failing state; keep the RED-OK line
tools\cli validate                                    # as often as you like
tools\cli test --no-build --tier all                  # once, at the end
```
- Bug fix: the run failing on the unfixed code is the red proof; put it in `// red-proof:`, no break-and-restore run.
- Script-only change whose tests touch no new resource: `LF_RDB_REFRESH=0` skips the ~10 s resource-database refresh
  (PowerShell: `$env:LF_RDB_REFRESH = "0"`). Any new prefab or `.meta`: leave the refresh on. Never commit the rdb.
- `test ... --no-cache` forces a run; a pass on identical inputs is otherwise reused.
- Output is one `SUMMARY` line and the failing items; the full log is `tmp\logs\`. Never pipe it through `tail`.

## Ship
```
tools\cli wt ship slice-<N> --refs <N> --body-file tmp\pr-body-slice-<N>.md
tools\cli prs status
tools\cli slice record <N> --model sonnet --outcome merged|handoff|split|failed --gate pass|fail --minutes <m>
```
Body: what changed, the observed red and green lines, `## Manual testing` for anything automation cannot reach, and the
`Generated with [Claude Code]` line. A visible change carries one `look` picture (`tools\cli evidence`). Say
"merged, in-dev", never "done". At the limit, ship what passes, write `tmp/handoff-slice-<N>.md` (15 lines), say `SPLIT`
and list the rest as new cards; never extend.

## Final report
Five lines: `Changed:`, `Evidence:`, `Manual:`, `Next:`, then exactly
`PR <full URL>, head <SHA>, checks and reviews <state>` (or the `RESUME <path>` line).

## Known traps
- A new `LF_Test_*.c` needs `tools\cli regen-tests`; the registry is generated, never hand-edited.
- A string is appended to `Language/everonlife_localization.st`, then `tools\cli regen-localization`. No `EL_` names.
- `// tier: LOGIC|WORLD|PERSISTENCE` and a `// red-proof:` line sit above the class. A test never asserts a town
  coordinate or distance.
- Change a feature: update `docs/features/<area>.md` in the same PR; never add rows to `docs/features.md`.
- Persistence tests use public mutators only; an in-session round trip proves nothing about restart.
- Merge `origin/main` only when GitHub says `CONFLICTING`.
- A world prop goes in your own grid cell (`brief grid`), never a town layer.

## Environment
No Workbench on this PC and never pin a machine (`--local`, `--machine`). Run in the foreground (timeout up to
600000 ms) or `t3_background_wait`; never end the turn with a run pending. Never `--force`, `rm -rf` or
`Remove-Item -Recurse` a worktree (`wt prune`). Heavy commands refuse the main checkout.
