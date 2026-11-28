# Release smoke for 0.3.0, 2026-11-28: the first 90 minutes on the release build

**Verdict: SHIP.** No crash, no VME, no stuck step, no lost money or items, no raw text key on either screen.

**Build played:** `origin/release-0.3.0` at `0f6b0f1eb` (merge of #5186), worktree `ws/release-smoke-1128`, no game code
changed. Everon pack, dedicated server, two clients.

**Session:** `session.mjs up --mission island --profile full --clients 2 --roster 2 --hold 20 --wait`, ready in 73.9 s
on the first try. One session, about 14 minutes; `session down` straight after the pull.

## Route

| # | Step | How | Result |
|---|---|---|---|
| 1 | Arrival: quiz, form R-89, hostel wake-up | `harness.mjs --client 1 flow playtest-030-first-two-hours`, step 0a | pass: $100 cash, $2,000 bank, "New in town" card, clerk waypoint |
| 2 | The clerk: register, ID card, welcome pack | same flow, step 0b | pass: cash $100 to $200, welcome-pack caption and card read right |
| 3 | First job (the starter errands): car out, apples, juice | steps 1a, 1b, 1c | pass |
| 4 | First sale: market trader buys apples and juice | step 2a | pass: "Trade Successful, Sold for $18!", HUD `+$18` |
| 5 | F2 file: Errands done 5 / 5, "What to do next" | step 2b | pass: six earning rows with $/h and distance |
| 6 | Car back to the garage | step 2d | pass: "Stored in the garage: SQ KZ 3537. Keep your key." |
| 7 | A house look: the for-sale cottage `sp-b83` | `stand HomeSign_saint_philippe_sp_b83`, `seen`, `shot ui` | pass: the client sees `housesign=saint-philippe.home.sp-b83:sold=0`, the house unowned and its door unlocked, the For Sale board shows |
| 8 | A small crime and booking: client 1 robs the fuel till, client 2 (cop) cuffs, searches, escorts, books; client 1 serves, pays bail, walks out | `session.mjs crime-playtest-proof --name smoke1128` | **pass, 16 of 16 steps** (`SUMMARY ... ok 271.2s`) |
| 9 | A medic call: client 1 on duty at the clinic, client 2 downed; call, board, pager, revive | `session.mjs downed-call` | **pass** (`SUMMARY ... ok 53.5s`) |
| 10 | Downed HUD, give-up, death screen, respawn | `session.mjs downed-flow-proof` | run 1 **fail** (proof aim, see Findings 1), run 2 **pass** (`SUMMARY ... ok 99.3s`) |

The medic roles are the proofs' own: client 1 is the medic and client 2 the patient, the other way round from the
brief. The flow is the same from both seats.

## What both screens said

- **Raw text keys:** every `#LF-`/`#EL-` key either client logged (31 distinct, from `Garage_Stored` to
  `Wanted_PlaceGrid`) has a row in the core or pack string table. The pictures read as words:
  `Bleeding out: 2:10. Hold X to give up, or wait for help.`, `You wake at the hospital. The bill is $150. What you
  carried stays with your body.`, the charge sheet (`Armed robbery Jail, 15 min`, `Carrying without a license $400`),
  the clerk's captions, the HUD toasts.
- **Script errors:** client and server logs have no `VME`, no `Virtual Machine`, no `NULL pointer`, no
  `GetCharacterControlType`. The `SCRIPT (E)` lines are the ones `docs/debug-log-triage.md` already explains: dialogue
  and booking sounds that "did not start" on a `-noSound` session client, the dispatcher voice the same way, three
  `npc loiter ... has no character controller` lines and the known dock `no destructible building` line.
- **Money:** each change in the crime and medic steps lines up with a ledger line (the till take, the $400 fine, bail,
  duty pay, the $10 revive bonus, the $100 hospital bill). The cash jumps between proofs come from the harness
  `resetplayer` (`debug-reset` in the ledger), not from play.

## Findings

No release blockers. Two notes, neither filed:

1. **`downed-flow-proof` red on its first run, green on the rerun.** Run 1: the medic stood by the body with the view at
   pitch -60 and the aim ray hit the ground (`hit=e@Eden@1.81`), so no context and no Revive row. The picture shows the
   body a metre ahead of the view, not under it. Run 2 put the medic over the body and the picture shows **Revive** as
   the only, selected row. `downed-call` revived the same way in between. Dry run 6 had the same one red. It's the
   proof's stand-and-pitch aim, not the menu. A player aims at the body.
2. **The hospital bill paper drops at the patient's feet** when their pockets are full (`bill #2: no room in the
   pockets ... it lies at their feet`). This is the designed fallback, not a loss.

## Evidence

Pictures (59, both clients) and the three console logs are in the worktree's `tmp/smoke1128/` (gitignored), with PNG
copies in `tmp/smoke1128/png/`. Proof transcripts: `tmp/logs/harness-flow-20261128-132048.log`,
`crime-playtest-proof-20261128-132435.log`, `downed-flow-proof-20261128-132910.log` (red),
`downed-call-20261128-133111.log`, `downed-flow-proof-20261128-133210.log` (green).
