# Proof health, 2026-11-27: the island proofs the X-6 run (#4836) leans on

**Commit played:** `origin/main` `6dbb681b` (compiled-ok) plus this branch's proof fixes, worktree `ws/proof-health-1127`. 0.3.0 feature freeze: tooling and test fixes only, no game code changed.
**Setup:** island sessions on this PC, `session up --mission island` (lowspec for logs-only proofs, full for the two-player police proofs), `--hold 20`, nobody queued, `session down` after each. Six sessions: A (2 clients, hours 0 to 10), B and C (1 client, fresh characters), D to F (2 clients, police and heist), one failed `--clients 4` boot (no lane free for 4 clients, see "Not run").
**Before / after (43 island proofs run at least once; `dialogue-keys-proof` and `vault-offer-proof` are not standalone proofs ; the second pass, #5047, is under "Second pass"):** first run **25 pass / 18 fail**; after this branch **38 pass / 5 fail**. Of the 13 that turned green, 7 were stale checks fixed in the proof (clothing till, export crane, truck yard, resident walk, island cop, island vault, bank door clear), and 6 were run order, a fresh-account precondition, a missing argument or one client stall (starter gates, truck haul, run plate, no-free-cars, dock stand, drill quick slot). No failure was a game bug, so there is no "RC blocker" issue; the five still red were [#5047](https://github.com/Life-Framework/Life-Framework/issues/5047) (tooling) and are green in the second pass below.

## Result per proof

Hours are the X-6 checkpoint each proof belongs to (`docs/playtest/rounds/2026-11-25-x6-dryrun2.md`); "extra" is island coverage outside the ten hours.

| Hour | Proof | First run | Now | What it was / the fix |
|---|---|---|---|---|
| 0 to 2 | `harness.mjs flow playtest-030-first-two-hours` | pass | **pass** (9 of 9, three sessions) | none |
| 0 to 2 | `starter-gates-proof` | FAIL | **pass** (fresh account) | not stale: it read the clerk's ID card, `resetplayer` from earlier proofs wiped it, and the clerk issues one per account. Fixed in the proof (rc-proof-order): it reads the turned-in arrival quest, skips the clerk when it is turned in and resets the trades itself, so no run order is needed |
| 0 to 2 | `counter-talk-proof` | pass | pass | |
| 0 to 2 | `first-talk-proof` | pass | pass | |
| 0 to 2 | `job-desk-proof` | pass | pass | |
| 0 to 2 | `shift-board-proof` | pass | pass | |
| 0 to 2 | `clothing-till-proof` | FAIL | **pass** | **stale check**: it picked up `Hat_FlatCap` from the shelf, which no longer hangs on `shelf_shop_01_2` (trousers do). Now takes the cap if it is there, else whatever is in reach; the proof is about the till |
| 0 to 2 | `fuel-kiosk-proof` | pass | pass | |
| 0 to 2 | `next-activities-proof` | pass | pass | |
| 0 to 2 | `interaction-points-proof` | pass | pass | |
| 0 to 2 | `atm-quick` | pass | pass | |
| 0 to 2 | `registrar-proof` | pass | pass | |
| 3 | `tier-tool-mine-proof` | pass | pass | |
| 3 | `mine-till-proof` | pass | pass | |
| 4 | `barkeeper-rumor-proof` | pass | pass | |
| 5 | `mining-iron-chain` | pass | pass (13 of 13) | |
| 5 | `iron-chain-proof` | pass | pass (14 of 14) | run it 5 min or more after `mining-iron-chain` |
| 6 | `casualty-call` | pass | pass | it runs `resetplayer`, which clears the pack and so the truck key (see `truck-haul-proof`) |
| 7 | `export-crane-proof` (`--board Fit_saint_philippe_dock_warehouse_export_board --spot Fit_saint_philippe_dock_dock_crane_spot`) | FAIL | **pass** (order of 40 iron bars, $1,040) | **stale**: it assumed a level-1 hauler (a character that already had DELIVERY took the order at the refusal step) and put the whole order in the pockets (a 120-plank order does not fit). Now `levelset DELIVERY 1` first, keeps the pack when the character owns a truck (the key), and when the pockets cannot hold the order it parks the owned truck at the crane spot, unlocks it and loads it with `exportorder give truck`. **The truck branch now runs live** (export-truck-branch, 11-27, one lowspec island session: `truck-check-proof`, then `export-crane-proof --board ... --spot ... --kind planks`: order of 120 planks, the pockets held 3, truck parked at the crane spot, unlocked, loaded, hand-in paid $1,018, second paid nothing). Three proof faults hid it: `--kind planks` is new (harness `exportorder draw planks` puts the plank order first, since every run drew the 40-bar one); the give line counted planks dropped at the feet as given (119 of 119), so the proof never chose the truck branch, and it now reads "brought N of M" from the short hand-in refusal; and the runner's per-proof reset cleared the pack and with it the truck key (`#EL-VehicleLock_MissingKey`), so the proof is `noReset`. No game bug. A second run in one session fails at the take step because the board's order is already held: one run per session |
| 8 | `booking-desk-proof` | pass | pass (24 steps) | |
| 8 | `outpost-north-proof` | pass | pass | |
| 9 | `level-set-proof` | pass | pass | |
| 9 | `night-light-proof` | pass | pass | |
| 10 | `truck-check-proof` | pass | pass (fresh account and a checkpoint-10 account) | the recurring price and licence failures of 11-24 to 11-26 do **not** reproduce on today's main: the price is read from the pack conf and the session's owner config, the held-licence desk line is accepted (#4939). On the checkpoint-10 character the truck's $9,600 left the bank and the licence step passed |
| 10 | `truck-haul-proof` | FAIL | **pass** (12 of 12, 350 s) | not stale: it must run **straight after** `truck-check-proof`. Anything that runs `resetplayer` in between (`casualty-call`) clears the truck key, and the unlock row answers `cannot_perform` (reason: no valid key in the pack). Back to back on the checkpoint-10 character it passes |
| 10 | `truck-yard-proof` | FAIL | **pass** | **stale check**: "no truck was sold" read the whole server log, so the truck the same session bought a minute earlier counted. Now counts only sales after the sheet opened |
| extra | `harbour-crates-proof` | pass | pass (11 of 11) | |
| extra | `plant-late-proof` | pass | pass | |
| extra | `dock-stand-proof fishmonger` / `chandler` | exit 2 (no argument) | pass | takes `<fishmonger\|chandler>`; not a fault |
| extra | `island-no-free-cars-proof` | FAIL (`no_session`, a client stall) | pass | a flake: the client stopped answering for the first 170 s of one run, the rerun passed |
| extra | `resident-island-proof` | FAIL (twice, fresh session too) | **pass** | **stale**: the proof stood by the shared residents bench, but the teller's home is now `saint-philippe.home.teller` (`Homes_saint-philippe_resident.layer`, 100 m away), so no player was within the 50 m watch radius and the server placed them instead of walking. Now reads the teller's home place id from the pack conf and the bench entity from that layer (selftest covers both reads). The server also warns `place 'BankCounter' has no anchor 'counter'; the resident stands at the place origin` at 13:00: the origin is the counter marker, so the teller still stands at the bank, not filed |
| extra | `resident-cafe-evening-proof` | pass | pass | |
| extra | `run-plate-island-proof` | FAIL (after other proofs) | **pass** (10 steps, fresh session) | state left by earlier proofs; fresh two-client session passes |
| extra | `island-cop-proof` | FAIL | **pass** (13 steps) | two **stale checks**: step 1a filled the police application, but a seeded roster account no longer sees the Apply row (`LF_PoliceApplyAction.CanBeShownScript`), so it now accepts the desk offering Clock in alone; step 5 used `LF_JailArrestAction`, which the island no longer offers (the look list has `LF_JailEscortArrestAction`: detain for escort, no sentence; the cell is the booking desk, covered by `booking-desk-proof`), so it now checks the suspect stays cuffed with no sentence. Needs a fresh session (the kit is issued once) |
| extra | `island-vault-proof` | FAIL | **pass** | two **stale checks**: the drill had to be in the hands by a quick-slot press (`hold` no longer satisfies the rob row: "Take the drill in your hands (quick slot)"), and a fresh account is in its robbery grace, so the rob was refused (`#LF-Grace_Refused`); it only used to pass after `island-cop-proof` had turned grace off. New `proofs/_grace.mjs` (`graceOff`) writes the flagship config with grace 0, as the cop proof does. One bag prop per vault: a second run in one session finds no bag |
| extra | `island-bank-door-clear-proof` | not run (hung the batch) | **pass** | same two fixes as the vault proof |
| extra | `drill-quickslot-proof --island` | FAIL without `--island` (it then aims at the annex vault) | pass with `--island` | not a fault; the flag is the island variant |
| extra | `police-phoned-call-proof` | pass | pass | |

## Second pass (#5047): the five red proofs and the heist, all green

Sessions on this PC from `ws/proof-health-1127-r2`, `--mission island --profile lowspec --hold 20`, each proof in a fresh session where it says so, `session down` after. No game code changed; none of the six was a game bug, so there is no "RC blocker" issue. One balance note is below.

| Proof | Now | Cause | Fix |
|---|---|---|---|
| `square-harbor-life-proof` | **pass** (fresh session, then twice more in a used one) | **stale check, not an atmosphere bug.** In a session without sound (`-noSound`) an emitter says `is in range ... got no voice for 5 s` once, ever (`m_bRefusalSaid`, `LF_AmbientEmitterComponent.Refused`). The proof reads the night beds after a `mark`, so in a session where the harbor night and village-night beds had already been chosen (any earlier proof at night) it found only `window open <bed>`, which it did not count. The #4957 window-edge lines are right: both night beds logged `window open` at 23:00. Reproduced: run 1 in a fresh session passed, run 2 in the same session failed the same two steps; with the fix runs 2 and 3 pass | `liveOrReopened`: a `window open` after the mark counts when an earlier line already showed that bed chosen (selftest cases) |
| `dirty-money-island` | **pass** (7 of 7) | two stale checks. The night-shift worker lives in his own house now (`saint-philippe.home.night_shift`, `Homes_saint-philippe_resident.layer`), not at the shared residents slot, so the proof stood at nothing (`stand Resident_night_shift: entity_not_found`). And the dialogue menu splits the farewell off as the `0 Goodbye` row (`d3db452da`), so `replies=` counts 3 where it used to count 4 (greet: zone, heard anything, work; work: zone, move, tip) | the doorstep is read from the pack's residents conf and the homes layer (`homeDoorOf`, selftest), the expected count is 3 |
| `patrol-island-proof` | **pass** (10 of 10) | three stale checks. The desk and armory names (`Roadmap_Police_Desk`, `Roadmap_Police_Armory`) are the grid's; the island kit rows are `police_police_kit_duty_desk` and `police_police_kit_armory`, so `use LF_DutyAction` found no target (`action_not_found`) and the kit, strip and dispatch steps cascaded. A fresh account was in its robbery grace. And the officer's call is read from `seen` (`lastdispatch=#LF-Offence_BreakIn@#AR-MapLocation_StPhillipe`), not `state`, and names the town by the base game's key spelling `StPhillipe` | names fixed, `graceOff`, `lastDispatch` + `PLACE_WORDS` (selftest) |
| `island-jail-breach-proof` | **pass** (12 of 12, fresh session, about 7 min) | three stale checks. The instant arrest (`LF_JailArrestAction`) is not offered on the island, so B went into the cell through the booking desk (`LF_BookingDeskBookAction`, the sheet, B's "Take the time" answer, as `booking-desk-proof`); a roster account has no Apply row; and the bail step: **every island sentence at one wanted is 900 s and bail is $150 a started minute, so $2,250 against a fresh account's $2,000 bank**; the game refuses it (`bail refused ... bank 2000 < bail 2250`). The proof now waits until the minutes left are affordable (about 100 s) and pays | booking-desk arrest, roster-aware apply, bail wait, comments say 900 s |
| `postal-van-proof` | **pass** (5 of 5) | stale check: `engine seat` takes the nearest vehicle within 6 m, the parked police UAZ at the depot stand; the van lies 7 to 22 m from the post-box stand (`LF_PostalRun.FindVanSpot`) | the character walks round the stand until `seen` puts the van within 5.8 m (`vanDistance`, selftest), then `engine veh_PostalVan`; the server's `entry refused` is the check |
| `island-bank-heist-proof` | **pass** (16 of 16, `ALL PASS`) | first live run, 4 clients booted at the first try (`session up --clients 4 --roster 3,4 --profile lowspec`, about 2.5 min, 18 GB of RAM free). The `graceOff` and quick-slot fixes from the first pass held. The `failures=2` in its `SUMMARY` line are the two refusals the proof expects (one officer, and the big-job lock) | none |

**Balance note, not a blocker:** a new character who robs the fuel till gets 15 minutes, and bail is $2,250, which a $2,000 starting bank cannot cover for the first 100 s. The player can wait, break out or be bailed by cash from the bank once earned. `LF_JailBreak_Everon1989.conf` (`m_iBailPerMinute 150`) is the knob if a playtest finds it harsh.

## Still not run

| Proof | State | Why |
|---|---|---|
| `dialogue-keys-proof` | N/A | a measurement tool: it needs a client already inside a talk (`flow ... ends on the clerk talk`), prints `the talk closed before every press was made` otherwise |
| `vault-offer-proof` | N/A on the island | targets the annex vault (`Annex_Bank_Vault`), a grid-world proof |
| `tracker-rejoin-proof`, `drill-scene-proof` and the shot scripts | not run | not on an X-6 hour |

## Order for Saturday's run (what the table implies)

1. The first-two-hours flow, then `starter-gates-proof` on the same character, in any order: the proof sets up its own precondition (`_preconditions.mjs`, rc-proof-order). It puts every trade back to level 1 and skips the clerk when `arrival_register` is already turned in (the gate is the quest, not the card; `resetplayer` empties the pack but the quest stays). The one thing it cannot reset is a criminal record (no harness verb clears one), so it still goes before the police and jail proofs.
2. Hour 6 `casualty-call` resets the pack; do the truck proofs on a character that has not been reset since `truck-check-proof` bought the truck: `truck-check-proof` then `truck-haul-proof` back to back (about 2 min plus 6 min), before the restart or after it (the haul proof collects an impounded truck).
3. Hour 7 `export-crane-proof` once per session, with a bought truck parked first if the order drawn is the 120 planks (`--kind planks` forces it, straight after `truck-check-proof`).
4. Hour 8 `outpost-north-proof` on a character at standing 0 (`repgrant UNDERWORLD 0`), once per session; `booking-desk-proof` with two clients; it holds the officer's handcuffs before every cuffing and grants a pair through `itemgive` when the pack has none (the duty kit is issued once per account; step 1b once flaked `NO_CUFFS` with the officer holding no cuffs yet), so a fresh or a reused officer both pass.
5. Police and crew proofs (`island-cop-proof`, `island-vault-proof`, `island-bank-door-clear-proof`) each want a fresh two-client session, or at least a character that has not used the cop kit; `island-vault-proof` and `island-bank-door-clear-proof` both take the vault's single bag.
6. `mining-iron-chain` and `iron-chain-proof` 5 minutes apart at least.
7. Two-client island proofs each want a fresh session where they rob, cuff or take a run: `postal-van-proof` (about 2 min), `patrol-island-proof` (2 min, needs the roster seed for client 2), `island-jail-breach-proof` (7 min, one run: the jail doors stay blown), then `island-bank-heist-proof` on 4 clients (`session up --clients 4 --roster 3,4`, 4 min). `square-harbor-life-proof` can run any time; a second run in one session is fine now.

### Run `ui-click-smoke` first

8. **`ui-click-smoke` goes first, right after the first-two-hours flow and before the gated proofs** (one lowspec island session, `up --mission island --profile lowspec --hold 20`, then `session.mjs ui-click-smoke`, about 4 min). It presses the first actionable row or button of the F2 Next, errand and skill lists, a shop, the licence desk, the car lot, the ATM and the charge sheet through the real widgets, and fails on any `widget_not_found` (the #5099 class: a row with no button, found by no proof for five days). It buys one cheap row at a till and the lot and runs `checkpoint 10`, so give it its own session: later proofs that want a fresh character (`starter-gates-proof`, `island-cop-proof`) must not follow it in the same session (`ui-click-smoke fresh` keeps the new character). A red smoke is a dead button: file the issue before the rest of the run.

## Files changed

Harness UI paths (2026-11-27): `activities pick|go` and `playermenu pick` press the row's widget (`direct` keeps the handler call), the new `ui-click-smoke` proof (one lowspec island session, 14 checks, all pressed) and its selftest. `agent-box/proofs/`: `clothing-till-proof`, `export-crane-proof`, `island-cop-proof`, `island-vault-proof`, `island-bank-door-clear-proof`, `island-bank-heist-proof`, `patrol-island-proof`, `resident-island-proof` (with selftest cases), `truck-yard-proof`, and the new `_grace.mjs`. Second pass (#5047): `square-harbor-life-proof`, `dirty-money-island`, `patrol-island-proof`, `island-jail-breach-proof`, `postal-van-proof` (each with a selftest case for its new pure helper).
