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

## Third pass: `drive-haul-proof` in the RC order (`ws/haul-in-rc-order`)

`drive-haul-proof` failed its three driven legs (1a, 2a, 3a) in RC dry runs 4, 5 and 6 and passed 13 of 13 standalone, on a head that had the drive-harness fixes. Reproduced in one lowspec island session (`ui-click-smoke`, then the truck proofs, then the haul), and it was two stale preconditions in the proof order, not a drive-harness fault and not a game bug. No game code changed.

| What the RC order changes | Evidence | Fix |
|---|---|---|
| **A different truck.** `truck-check-proof` banks $12,000 and clicks the first Buy button the sheet shows. A fresh account is left too poor for the M923A1 ($9,600) and gets the Ural ($7,600); the account `ui-click-smoke` leaves at checkpoint 10 already had bank money, so the same click buys the **M923A1**. Every green standalone run was a Ural (`LF_Ural4320_transport_CIV`), every red RC run an `EL_M923A1_transport`. | RC-order session, no `truck-haul-proof` between the two proofs: the M923A1 leaves the yard (`yard_to_gate` arrived, `yard_to_woodlot` close 984 m, 9 m off the pad because the placed Ural stands on it), then `to_tree_1` stuck 44.7 m in 85 s, `why=stalled_blocked_by_HouseAddon_ShedMetal_01_Long_at_0.5m_middle`, `to_tree_2` `lost_vehicle`. (kept as `tmp/drive-haul-m923a1-repro.log`) | `truck-check-proof --truck Ural` pins the yard row (`truckRowIndex`, the server's `purchased` line is the check: it fails when the truck bought is not the one asked for). Without the flag it still buys the first row the account can pay and now prints which. The driven M923A1 in the woodlot sheds was **not fixed** at that point (fixed in the fourth pass below): the proof says so in a `NOTE` when it drives one, and the manual truck drive in the RC page still covers a player's M923A1 |
| **A truck set down by `parkat`.** `truck-haul-proof` runs just before and leaves the truck at 4333,10632 (8 m from the saw, between the wood piles and the sheds, off the road), so the haul proof skipped its yard-to-woodlot leg ("the truck already stands at the woodlot"). From that pose `drive road` answers `no_road_path`, and the straight drive stalls at once: with the Ural **3 m in 117 s**, `why=creep_stalled_blocked_by_WoodPile_01_A_at_0.4m_right`, on every leg (1a, 2a, and 3a for `enter_seat_refused` after the lost vehicle). | same session, Ural pinned, `truck-haul-proof` first: `STUCK 4 of 4 driven legs` (`tmp/logs/drive-haul-proof-20261127-232244.log`) | The haul proof sets a truck that stands in the woodlot but not on the saw pad (12 m, `restageNeeded`) back on the yard pad with `parkat Fit_ne_compound_truckyard_lot 22` and drives leg 0 as it does after the truck check alone. A truck this proof drove to the pad is left where it is. The arrival checks are unchanged: all of 0b, 1a, 2a and 3a still need `arrived` or `close` |

Order that passes (one lowspec session, `up --mission island --profile lowspec --hold 20`, nobody queued, `session down` after): `ui-click-smoke`, `truck-check-proof --truck Ural`, `truck-haul-proof`, `drive-haul-proof` (**PASS 13 of 13**, `SUMMARY drive-haul-proof ok 1037.3s`): `yard_to_gate` arrived 42.6 m, `yard_to_woodlot` arrived 973.5 m (3.5 m off the pad), `to_tree_1` arrived 474.5 m, `to_tree_2` arrived, `to_tree_3` close (13.6 m, way shut), `tree_to_saw` arrived 318.3 m, `saw_to_merchant` arrived 242.6 m, `STUCK 0 of 7`; 6 logs, +$195 in 7.7 min, $1,514/h. Both pieces were needed: the Ural without the restage stuck at 3 m, and the M923A1 without `truck-haul-proof` (so no `parkat` pose) stuck in the sheds. The M923A1 with the restage was not run.

Two side findings: (1) a character that a failed haul leaves lying prone (`stance=PRONE`, hp 0.4 climbing slowly) is refused a seat (`seat_refused`) for minutes; start the haul run on a fresh session after a failure rather than rerunning in the same one. (2) For about an hour on 2026-11-27 (`6cb591371`, `#5175` to `#5180`) a server started with `-scrDefine LF_HARNESS` crashed while compiling scripts, so `session up` refused to boot (access violation, 1 s into "Compiling Game scripts"); the lead's fix (#5180) is on main, and this session ran on it.

## Fourth pass: the driven M923A1 in the woodlot (`ws/drive-big-truck`)

The M923A1 (8.2 x 2.9 m, wheelbase read as 4.9 m from its bounds) stuck on the way to the first tree (`why=stalled_blocked_by_HouseAddon_ShedMetal_01_Long_at_0.5m_middle`); the Ural got through. Reproduced in one lowspec session (`truck-check-proof --truck M923A1`, then `drive-haul-proof --cycles 1`), and it was not the sheds themselves but how the truck leaves the saw pad:

| What happened | Evidence | Fix |
|---|---|---|
| **The route leaves the pad behind the truck.** The truck reaches the pad from the lane heading west; the route to the tree starts south-east, 155 degrees off the nose. Pure pursuit answered with full lock (a turn radius of about 8 m) and drove 30 m west into the alley between the sheds and the wood piles; the 15 m off-route test then burnt all six re-plans in 0.7 s (each refused, `no_road_path`), and the creep (`creep_stalled_..._ShedMetal_01_Long_at_1.8m_left`, `creep_budget`, 8 back-ups) wandered in the alley | client log of the first run: `drive road` 168 m, `drive re-plans ... off_route_15m` x6, `drive end stuck ... replans=6 approach=89m creepbackups=8` | a **turn about**: a route that starts more than 75 degrees off the nose at under 6 km/h is swept first (the body's corners and side middles traced along the smallest-radius circle, from the car's bounds and wheelbase); with no room the drive does a three-point turn, forward and reverse legs sized by the room the same sweep finds. The creep fan sweeps its six cheapest headings the same way. A normal drive stuck twice far from the end creeps a local stretch (5 vehicle lengths) and goes back to the normal drive. A refused re-plan waits 4 s |
| **`--truck M923A1` found no Buy button.** The proof banked $12,000 and paid $2,000 pocket money, the licence and the check fees, leaving $11,900 in the bank against the $12,000 M923A1; the lot's sheet offers a row only when the bank covers it | `Car0_Action is not on the sheet`, only `Car1_Action` | `truck-check-proof` funds a pinned truck with `dearest + 3000` (`startCashFor`, selftest) |

Proof after the fix, one lowspec session per truck, `session down` after each: `truck-check-proof --truck M923A1` then `drive-haul-proof --cycles 1` **PASS 13 of 13** (`STUCK 0 of 7`: `yard_to_woodlot` close 9.2 m, the placed Ural on the pad; `to_tree_1` arrived 441.5 m; `to_tree_2` arrived; `to_tree_3` close; `tree_to_saw` close; `saw_to_merchant` arrived; +$195 in 7.5 min); the Ural run (the regression check) **PASS 13 of 13** too, `STUCK 0 of 7` (`to_tree_1` and `to_tree_3` close, the rest arrived).

## Still not run

| Proof | State | Why |
|---|---|---|
| `dialogue-keys-proof` | N/A | a measurement tool: it needs a client already inside a talk (`flow ... ends on the clerk talk`), prints `the talk closed before every press was made` otherwise |
| `vault-offer-proof` | N/A on the island | targets the annex vault (`Annex_Bank_Vault`), a grid-world proof |
| `tracker-rejoin-proof`, `drill-scene-proof` and the shot scripts | not run | not on an X-6 hour |

## Order for Saturday's run (what the table implies)

1. The first-two-hours flow, then `starter-gates-proof` on the same character, in any order: the proof sets up its own precondition (`_preconditions.mjs`, rc-proof-order). It puts every trade back to level 1 and skips the clerk when `arrival_register` is already turned in (the gate is the quest, not the card; `resetplayer` empties the pack but the quest stays). The one thing it cannot reset is a criminal record (no harness verb clears one), so it still goes before the police and jail proofs.
2. Hour 6 `casualty-call` resets the pack; do the truck proofs on a character that has not been reset since `truck-check-proof` bought the truck: `truck-check-proof --truck Ural` (the flag keeps the truck the same on a checkpoint-10 character, which would otherwise buy the M923A1) then `truck-haul-proof` back to back (about 2 min plus 6 min), before the restart or after it (the haul proof collects an impounded truck). `drive-haul-proof` follows in the same session (about 17 min, `session.mjs lease 30` before it): it puts a truck that `truck-haul-proof` left at the saw back on the yard pad itself.
3. Hour 7 `export-crane-proof` once per session, with a bought truck parked first if the order drawn is the 120 planks (`--kind planks` forces it, straight after `truck-check-proof`).
4. Hour 8 `outpost-north-proof` on a character at standing 0 (`repgrant UNDERWORLD 0`), once per session; `booking-desk-proof` with two clients; it holds the officer's handcuffs before every cuffing and grants a pair through `itemgive` when the pack has none (the duty kit is issued once per account; step 1b once flaked `NO_CUFFS` with the officer holding no cuffs yet), so a fresh or a reused officer both pass.
5. Police and crew proofs (`island-cop-proof`, `island-vault-proof`, `island-bank-door-clear-proof`) each want a fresh two-client session, or at least a character that has not used the cop kit; `island-vault-proof` and `island-bank-door-clear-proof` both take the vault's single bag.
6. `mining-iron-chain` and `iron-chain-proof` 5 minutes apart at least.
7. Two-client island proofs each want a fresh session where they rob, cuff or take a run: `postal-van-proof` (about 2 min), `patrol-island-proof` (2 min, needs the roster seed for client 2), `island-jail-breach-proof` (7 min, one run: the jail doors stay blown), then `island-bank-heist-proof` on 4 clients (`session up --clients 4 --roster 3,4`, 4 min). `square-harbor-life-proof` can run any time; a second run in one session is fine now.

### Run `ui-click-smoke` first

8. **`ui-click-smoke` goes first, right after the first-two-hours flow and before the gated proofs** (one lowspec island session, `up --mission island --profile lowspec --hold 20`, then `session.mjs ui-click-smoke`, about 4 min). It presses the first actionable row or button of the F2 Next, errand and skill lists, a shop, the licence desk, the car lot, the ATM and the charge sheet through the real widgets, and fails on any `widget_not_found` (the #5099 class: a row with no button, found by no proof for five days). It buys one cheap row at a till and the lot and runs `checkpoint 10`, so give it its own session: later proofs that want a fresh character (`starter-gates-proof`, `island-cop-proof`) must not follow it in the same session (`ui-click-smoke fresh` keeps the new character). A red smoke is a dead button: file the issue before the rest of the run.

### Proofs added the evening of 2026-11-27 (placed in the run order)

Docs only: this branch ran none of them; each was run by the PR that added it (arguments and what each reads are in
`agent-box/README.md`, "Proofs added 2026-11-27"). Where they go:

9. **`map-search-proof`** and **`lot-sheet-proof`** are read-only (neither buys or changes the character), so they go
   right after `ui-click-smoke`, in any order: `map-search-proof` in a lowspec session, `lot-sheet-proof` (hour 10, the
   car lot and truck yard sheets) in a full one because it takes `ui` pictures.
10. **`downed-call`, `downed-flow-proof` and `clinic-bed-proof`** (hour 8, the medic side) in one two-client full session
    before the police proofs of step 5: each clocks one client in at the clinic desk and downs the other, and each
    (the default for a proof) runs the harness reset first, so the order inside the session does not matter. After
    `down`, grep the three console logs for `GetCharacterControlType` (`downed-flow-proof` prints their directory).
11. **`crime-playtest-proof`** (hour 8, the police and crime loop played through) in its own fresh two-client full
    session next to step 5: the cop signs up at the duty desk and the civilian robs the fuel till, as in
    `island-cop-proof`, so it follows the same rule, a fresh session or characters that have not used the cop kit. It
    books the suspect and the civilian pays bail at the cell phone, so it ends with the suspect free.
12. **`jip-sweep-proof`** goes last of the two-client proofs, in a fresh lowspec session with `up --clients 2`: client 1
    buys a car and a house and is cuffed, and the proof does not reset the character (`noReset`), so no later proof
    in that session wants it. It joins client 3 itself. `--selftest` runs its token parsers with no game.
13. Before every `session down` in this run: `session.mjs pull <dir>`. `down` deletes the client screenshot folders.

## Files changed

Harness UI paths (2026-11-27): `activities pick|go` and `playermenu pick` press the row's widget (`direct` keeps the handler call), the new `ui-click-smoke` proof (one lowspec island session, 14 checks, all pressed) and its selftest. `agent-box/proofs/`: `clothing-till-proof`, `export-crane-proof`, `island-cop-proof`, `island-vault-proof`, `island-bank-door-clear-proof`, `island-bank-heist-proof`, `patrol-island-proof`, `resident-island-proof` (with selftest cases), `truck-yard-proof`, and the new `_grace.mjs`. Second pass (#5047): `square-harbor-life-proof`, `dirty-money-island`, `patrol-island-proof`, `island-jail-breach-proof`, `postal-van-proof` (each with a selftest case for its new pure helper).
