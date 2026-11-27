# Proof health, 2026-11-27: the island proofs the X-6 run (#4836) leans on

**Commit played:** `origin/main` `6dbb681b` (compiled-ok) plus this branch's proof fixes, worktree `ws/proof-health-1127`. 0.3.0 feature freeze: tooling and test fixes only, no game code changed.
**Setup:** island sessions on this PC, `session up --mission island` (lowspec for logs-only proofs, full for the two-player police proofs), `--hold 20`, nobody queued, `session down` after each. Six sessions: A (2 clients, hours 0 to 10), B and C (1 client, fresh characters), D to F (2 clients, police and heist), one failed `--clients 4` boot (no lane free for 4 clients, see "Not run").
**Before / after (43 island proofs run at least once; `dialogue-keys-proof` and `vault-offer-proof` are not standalone proofs and `island-bank-heist-proof` could not boot, see below):** first run **25 pass / 18 fail**; after this branch **38 pass / 5 fail**. Of the 13 that turned green, 7 were stale checks fixed in the proof (clothing till, export crane, truck yard, resident walk, island cop, island vault, bank door clear), and 6 were run order, a fresh-account precondition, a missing argument or one client stall (starter gates, truck haul, run plate, no-free-cars, dock stand, drill quick slot). No failure was a game bug, so there is no "RC blocker" issue; the five still red are [#5047](https://github.com/Life-Framework/Life-Framework/issues/5047) (tooling).

## Result per proof

Hours are the X-6 checkpoint each proof belongs to (`docs/playtest/rounds/2026-11-25-x6-dryrun2.md`); "extra" is island coverage outside the ten hours.

| Hour | Proof | First run | Now | What it was / the fix |
|---|---|---|---|---|
| 0 to 2 | `harness.mjs flow playtest-030-first-two-hours` | pass | **pass** (9 of 9, three sessions) | none |
| 0 to 2 | `starter-gates-proof` | FAIL | **pass** (fresh account) | not stale: it reads the clerk's ID card and `resetplayer` from earlier proofs wiped it, and the clerk issues one per account. **Run it first on a fresh character** (it says so in its header) |
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
| 7 | `export-crane-proof` (`--board Fit_saint_philippe_dock_warehouse_export_board --spot Fit_saint_philippe_dock_dock_crane_spot`) | FAIL | **pass** (order of 40 iron bars, $1,040) | **stale**: it assumed a level-1 hauler (a character that already had DELIVERY took the order at the refusal step) and put the whole order in the pockets (a 120-plank order does not fit). Now `levelset DELIVERY 1` first, keeps the pack when the character owns a truck (the key), and when the pockets cannot hold the order it parks the owned truck at the crane spot, unlocks it and loads it with `exportorder give truck`. **The truck branch has not run live** (every run drew the 40-bar order); it needs a bought truck (run `truck-check-proof` first). A second run in one session fails at the take step because the board's order is already held: one run per session |
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

## Still red or not run

| Proof | State | Why | Issue |
|---|---|---|---|
| `patrol-island-proof` | FAIL (6 of 10) | client 2 `clocks in at the island police desk: action_not_found`, so no officer is on duty and the kit, strip and dispatch steps cascade; same family as the cop proof's stale desk rows. My quick-slot edit to it is untried | [#5047](https://github.com/Life-Framework/Life-Framework/issues/5047) |
| `island-jail-breach-proof` | FAIL | arrests with `LF_JailArrestAction` (not offered on the island), so B is never in the cell; needs the booking desk path of `booking-desk-proof` | #5047 |
| `postal-van-proof` | FAIL (1 of 5) | the seat step takes the nearest vehicle within 6 m, which is now the parked police UAZ, not the van 11 m away; needs `engine veh_PostalVan` and a spot beside the van | #5047 |
| `dirty-money-island` | FAIL (3b) | the informant's greeting offers 3 replies, the proof wants 4 (zone, move, work, bye) at UNDERWORLD 100; the dialogue or the gate moved | #5047 |
| `square-harbor-life-proof` | FAIL (2 night-bed lines) | at 23:00 the harbor night and village-night bed windows are open but no start line appears; the day beds and the outpost beds pass | #5047 |
| `island-bank-heist-proof` | **not run** | needs `--clients 4`; the pool had no lane free for four clients (`no session lane freed on any machine within 90 min`). Patched like the vault proof (`graceOff`, quick-slot press) but unverified: run it at the start of Saturday's session on 4 clients | |
| `dialogue-keys-proof` | N/A | a measurement tool: it needs a client already inside a talk (`flow ... ends on the clerk talk`), prints `the talk closed before every press was made` otherwise | |
| `vault-offer-proof` | N/A on the island | targets the annex vault (`Annex_Bank_Vault`), a grid-world proof | |
| `tracker-rejoin-proof`, `drill-scene-proof` and the shot scripts | not run | not on an X-6 hour | |

## Order for Saturday's run (what the table implies)

1. Fresh character: the first-two-hours flow, then `starter-gates-proof` before anything that calls `resetplayer`.
2. Hour 6 `casualty-call` resets the pack; do the truck proofs on a character that has not been reset since `truck-check-proof` bought the truck: `truck-check-proof` then `truck-haul-proof` back to back (about 2 min plus 6 min), before the restart or after it (the haul proof collects an impounded truck).
3. Hour 7 `export-crane-proof` once per session, with a bought truck parked first if the order drawn is the 120 planks.
4. Hour 8 `outpost-north-proof` on a character at standing 0 (`repgrant UNDERWORLD 0`), once per session; `booking-desk-proof` with two clients.
5. Police and crew proofs (`island-cop-proof`, `island-vault-proof`, `island-bank-door-clear-proof`) each want a fresh two-client session, or at least a character that has not used the cop kit; `island-vault-proof` and `island-bank-door-clear-proof` both take the vault's single bag.
6. `mining-iron-chain` and `iron-chain-proof` 5 minutes apart at least.

## Files changed

`agent-box/proofs/`: `clothing-till-proof`, `export-crane-proof`, `island-cop-proof`, `island-vault-proof`, `island-bank-door-clear-proof`, `island-bank-heist-proof` (unverified), `patrol-island-proof` (unverified), `resident-island-proof` (with selftest cases), `truck-yard-proof`, and the new `_grace.mjs`.
