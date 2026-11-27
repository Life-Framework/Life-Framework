# Agent playtest round 2026-11-27: the first hour as a new player

**Build played:** `ws/playtest-1127` on `origin/main` `03731faa` (`compiled-ok`), the 0.3.0 feature freeze (#4836, cut Sat 2026-11-28): fixes only.
**Setup:** one `--profile full` island session on lf-testbox (`up --mission island --clients 1 --hold 20`), one fresh account (rules quiz, form R-89, $100 cash and $2,000 bank), 12 minutes, `session down` straight after. The client and server logs hold no `SCRIPT (E)`, `VME`, `Unable to find` or `Can't find` line.
**Route:** hostel spawn, clerk and ID card, starter car, orchard, juicer, market sale, F2 (Progress, What to do next, Person), F3 welcome guide, rules sign, courier sign-up at the post counter, general store counter. The first part is `agent-box/flows/playtest-030-first-two-hours.txt` (9 of 9 steps pass); the rest was a scratch flow, not committed. Checkpoint 2 (the flow's step 2c) sets the hour-2 purse, so the cash, bank, Farming 3, Mining 3 and the "Field hand" and "Coal face permit" lines on the Person tab after step 2c are the flow's doing, not a bug.

Findings are ranked by what a new player hits. Text was read for US English: no British spelling in any string the route showed (a scan of every `Target_en_us` row for colour, grey, centre, licence, metre, harbour and the like also came back empty).

## Findings

| # | Rank | Finding | Where | Picture | What I did |
|---|---|---|---|---|---|
| 1 | Medium | The sale toast reads "Trade Successful / Sold item for $18!" for every sale, including "Sell all goods" of several items. "item" is wrong for a bundle and tells the player nothing | Market trader (`LF_TraderSale`), key `EL-Item_Sold` | [market sale](../../website/public/docs-images/playtest-1127/market_sold_toast_item.jpg) | **Fixed in this PR**: en_us now reads "Sold for $18!". The key is unchanged, so no code or other language moves |
| 2 | Medium | F2 "What to do next" lists **Found a crew, $400/h** under "Ways to earn a living". The registry pays nothing; the figure sits beside Mining $450/h and Postal van run $480/h and reads as a wage. The first row selected on a minute-2 character is also "Mining run" (339 m away), while the errands say apples | F2 Next tab; pack row `crew` in `LF_Activities_Everon1989.conf` | [F2 Next](../../website/public/docs-images/playtest-1127/f2_next_found_a_crew_400h.jpg) | **Issue #5119** (two pack tests pin `m_iPayPerHour > 0`, so it is not a one-line fix) |
| 3 | Low | The F3 welcome guide, opened after all five errands are done, still opens on "Walk south from the hostel ... talk to the town clerk ... carry no papers" with a "Set waypoint to the clerk" button | F3 guide page 1 | [F3 guide](../../website/public/docs-images/playtest-1127/f3_guide_finished_player.jpg) | Noticed (player can close it; content changes are out of the freeze) |
| 4 | Low | Taking the starter car out shows three cards at once on the right: "Retrieve vehicle", "Apple picking", "Your car". Two of them say the car is waiting; together they cover a third of the right edge while the F prompt is on screen | Garage pad | [garage](../../website/public/docs-images/playtest-1127/garage_three_toasts.jpg) | Noticed |
| 5 | Low | The market step card keeps the title "Sell your juice at the market stall" for its "You are on your way. For steady pay, take a job ..." finish text, and the old "Trade Successful" toast is still fading behind it | Market after the sale | [market sale](../../website/public/docs-images/playtest-1127/market_sold_toast_item.jpg) | Noticed |

## What read well

- **Hostel spawn:** the card "New in town: Walk south from the hostel to the town hall and talk to the clerk at the desk for your papers" and a "The town clerk, 119 m" waypoint are on screen from the first frame ([picture](../../website/public/docs-images/playtest-1127/hostel_spawn_objective.jpg)).
- **Clerk:** the welcome pack line names the contents (ID card, $100, bread, a sausage, a bottle of water) and the car lot.
- **Juicer:** "Make 1 apple juice with 3 apples" and a grey "Needs Apple" are self-explaining; the next-step card names the market and the F press ([picture](../../website/public/docs-images/playtest-1127/juicer_market_step_card.jpg)).
- **Pay against the ladder:** apples $250/h, fishing $300, lumber $400, mining $450, postal van $480 per hour on the F2 list; the juice sold for $18 and the general store sells water $3, apple $6, canned meat $8, so the first sale does not buy back a meal's worth of apples at a loss ([store](../../website/public/docs-images/playtest-1127/general_store_picked_water.jpg)). Nothing on the first-hour list felt out of line.
- **Courier sign-up:** "You are a courier now. Pick up a parcel at this counter." ([picture](../../website/public/docs-images/playtest-1127/courier_signup_toast.jpg)); F2 Next afterwards ([picture](../../website/public/docs-images/playtest-1127/f2_next_after_courier_job.jpg)).
- **Rules sign:** short, plain, US English ([picture](../../website/public/docs-images/playtest-1127/rules_sign_open.jpg)). F2 Progress "First errands 5 / 5" with all five rows DONE ([picture](../../website/public/docs-images/playtest-1127/f2_progress_errands.jpg)).

## Not reached (say so, do not read it as a pass)

- **The map (M).** The harness has no map verb; `playermenu open map` just re-opened the Person tab. Not seen.
- **The town board on the square.** `use LF_BoardAction` answered `action_not_found` on `Fit_saint_philippe_board_Place_MapBoard`, and the `stand` spot put the camera at a stairwell door whose prompt is "Open". The board's cards were not read this round.
- **Signs read in the world.** `stand` points the camera wherever the character ends up, so the bank, car lot, farm, clinic and barber fascia shots show walls and a van, not a sign face. Only the rules sign (a sheet) was read. `island-sign-faces` and `look` are the tools for faces.
- **The courier run itself** (pick up, carry, deliver, pay): only the sign-up was done. Hostel clerk audio was not heard.

## Noticed (no action)

- Sale toasts in the other six languages still say "item"; only en_us is in scope of US English.
- The HUD's right-edge client stats (`0 ms`, fps) are the session client's, not the game's.

## Tooling notes

1. `stand <Entity>` is a position, not a view: a flow that wants a sign or board face needs `shot world <name> x y z yaw pitch roll` with a camera, as `island-sign-faces.txt` does.
2. A flow's `shot world` line needs a name `layers` knows or full coordinates; an unknown name fails the whole flow before it starts.
3. `tab=0` is what `playermenu open map|skills|status|file` all answer; only `progress` and `next` select their own tab.

---

# Round 2: the map, the town board, sign faces, a courier run, fishing and a sale

**Build played:** `ws/playtest-1127-r2` on `origin/main` `dd22b1ec` (`compiled-ok`), the 0.3.0 feature freeze. Game client 1.9.0.42 (build 26639613), dedicated server build 26634883 (see "The session setup" below).
**Setup:** one `--profile full` island session on lf-testbox (`up --mission island --clients 1 --hold 20`), one fresh account (rules quiz, form R-89), about 11 minutes up, `session down` straight after. The character stayed in the arrival state ($100 cash, $2,000 bank, no ID card): round 1 covered the clerk, car, orchard and market, so this round starts where it stopped. The harness teleports (`stand`), so no walk was timed. The server log holds no `SCRIPT (E)`, `VME`, `Unable to find` or `Can't find` line; the client log holds 60 `GUI (E)` lines on the map open (finding 3).
**Route:**
- **Sign faces** (`agent-box/flows/playtest-030-sign-faces.txt`, 8 of 8 steps): bank, barber, car lot, clinic, farm, hostel, post office and supermarket signs, a front and a back shot each.
- **Town board:** `stand` and `use LF_TownBoardCardAction__0..8` on `Fit_saint_philippe_board_sign_town_map`; all nine cards opened and their text read.
- **Map:** `send map open`, `pan`, `hover`, `descriptors`; shots at 565 m across and at island zoom.
- **Courier:** sign-up, pickup, delivery, pay.
- **Fishing and a sale:** `island-fishing-proof --spot pond` (7 of 7): rod and bait can at Seed & Feed, a cast at the Spring Pond, one trout, sold at the dock wholesale buyer.

## Findings, ranked by what a player hits

| # | Rank | Finding | Where | Picture | What I did |
|---|---|---|---|---|---|
| 1 | Medium | The courier pickup toast sends the player the wrong way: "Carry the parcel **east along the street to the porch past the brick-walled lock-up**, then Complete the run." That is the old playtest village's drop. On Everon the waypoint ("Delivery stop, 324 m") and the drop are a dock crate to the **north**, so the written step and the marker disagree on the first run. The parcel's own description line ("Deliver to: the porch past the brick-walled lock-up, east end of the street") says the same | Courier counter at the post office (`LF-Job_RunStarted_Courier`, since renamed `LF-Job_RunStarted_CourierDeposit`, and `LF-Job_ParcelDest`: core text carrying one map's geography) | [pickup toast](../../website/public/docs-images/playtest-1127-r2/r2-courier-pickup-after.png) (the old text) | **Fixed in this PR**: both strings are neutral ("the delivery stop marked on your map"); test `jobs/parcel-description` |
| 2 | Medium | The same toast never says the pickup **costs $25**: cash goes $100 to $75 at "Pick up parcel", with only a red "-$25" on the cash readout. The deposit is explained when a player cannot pay it (`LF-Job_NeedDeposit`) and when they abandon (`LF-Job_AbandonedDeposit`), not on the happy path. It comes back on delivery ($75 to $140: +$25 deposit, +$40 pay, the "+$65" on the readout) | Courier counter | [pickup toast](../../website/public/docs-images/playtest-1127-r2/r2-courier-pickup-after.png), [delivery](../../website/public/docs-images/playtest-1127-r2/r2-courier-done-after.png) | **Fixed in this PR**: the toast now ends "Your $25 deposit comes back on delivery." (`LF_JobStartRunAction` passes the amount) |
| 3 | Low | Opening the map prints **60 `GUI (E): Can't find image 'poi_...'`** lines on the client (`poi_StoreCounter` ten times in each of two imagesets, `poi_WholesaleCounter`, `poi_WheatField`, `poi_contested-site`). The icons that show are the right ones (the candidate probe falls through to the row's own icon), so a player sees nothing wrong, but every map proof's log scan reads these as defects | Map open | none (log) | **Issue #5127** (not a one-line change: the probe loads each name into the widget) |

Findings 1 and 2 are core strings in `everonlife_localization.st`; the other six languages keep their existing text for these two keys. The fix has no picture of its own: it is wording in a toast, and its test reads the translated text (red run: `the run-start toast names the deposit: Carry the parcel to the delivery stop marked on your map, then Complete the run.`; green after the sentence and the argument were added).

## What read well

- **The town board.** All nine cards open and read in plain US English, each with a place and a distance ("Post office, 30 m south-west", "Organization office, 170 m south", "Plot for sale, 120 m south-east"); the sheet closes with "Put it back" ([board](../../website/public/docs-images/playtest-1127-r2/r2-board-stand-after.png), [card](../../website/public/docs-images/playtest-1127-r2/r2-board-card0-after.png)). The courier card says a run pays $40, and the run paid $40. `Set waypoint` was not pressed this round (the board proof covers it).
- **The map.** It opens centred on the board with the waypoint ("The town clerk, 126 m"), a how-to panel ("Shift + Left click: set a waypoint", "Hover an icon: see its name", the right-edge key), a category key, distinct shop, bank, post, hospital, farm and mine icons and area circles; at island zoom the vanilla names (Saint-Philippe, Meaux, Ile-aux-Pins) and clustered markers draw ([town](../../website/public/docs-images/playtest-1127-r2/r2-map-town-after.png), [island](../../website/public/docs-images/playtest-1127-r2/r2-map-zoomed-after.png)). `hover` at the post counter answered `57 places`.
- **The courier run end to end.** Sign-up toast, pickup toast with a "Delivery stop" marker at 324 m, the parcel in the hands ("Unequip: Parcel"), delivery paid $40 and a "Delivery, Level 2, Apprentice, a steady start" card. Cash $100, $75, $140; the ledger lines are `job-deposit -25`, `job-deposit-back +25`, `job-pay +40`.
- **Fishing and its pay.** Seed & Feed lists Fishing rod $60, Bait can $30, Cabbage seeds $4 and Water bottle $3 in a clean price list ([till](../../website/public/docs-images/playtest-1127-r2/island-fishing-till-after.png)). One cast bit in 39 s, a small trout was caught and the dock buyer paid **$4** ("Sold for $4!", round 1's wording fix) ([sale](../../website/public/docs-images/playtest-1127-r2/island-fishing-sold-after.png)). At about 39 s of bite plus the strike that is near the $300/h the F2 list shows, after $90 of tackle to earn back.
- **Sign faces read.** "VEHICLE LOT" (white on blue) from the yard side ([car lot](../../website/public/docs-images/playtest-1127-r2/r2-carlot-a-after.png)); "VEGETABLE PLOTS" on the chalkboard ([farm](../../website/public/docs-images/playtest-1127-r2/r2-farm-a-after.png)); the barber's "OPENING HOURS" plate and the red BARBER fascia ([barber](../../website/public/docs-images/playtest-1127-r2/r2-barber-a-after.png)); and the bus-stop board "SAINT-PHILIPPE BUS LINE: Clinic, You are here; Coalman's Brow mine, $17". No British spelling on anything the route showed.

## Pay against the ladder

Courier $40 per run at about $400/h (`tools\cli economy`), postal van $480/h, fishing $300/h and apples $250/h on the F2 list; police patrol $350/h (up to lieutenant $450/h), medic $480/h and up. Courier and everything above it sit over the police entry rung, as the order of 2026-11-22 says. The first-hour trades (apples $250/h; fishing $300/h plus $90 of tackle) sit **under** the police entry rung of $350/h. I read that as the entry step before courier work and changed nothing; flagging it for whoever owns the ladder.

## Not reached (say so, do not read it as a pass)

- **The map's "Find a place" search.** The panel shows on the left ("Find a place" over a greyed icon). The harness has no search verb, so no query was typed and no result read. `hover`, `pan` and `descriptors` were.
- **The bank, hostel, post-office and supermarket sign faces.** The bank front shows the door with only "TELL..." at the right edge; the hostel shot is cut off at the top ("HOSTEL" half in frame); the post-office front shot shows the square with the BARBER fascia far off and the post plate not in view; the supermarket shot ended on a roof. The camera offsets (yaw read as a direction 3 m out) fit the car lot, farm and barber and missed these. The clinic and bank back shots are rooms (the camera ended indoors); the clinic front shows the bus-stop board.
- **Lumber** (the proof needs a $600 axe; the hold went on the board, map, courier and fishing). The second job is fishing.
- **The walk.** Every position was a harness `stand`; the 324 m to the drop was not timed.
- **Audio** (the session runs `-noSound`).
- Of the 16 sign pictures I opened 12; the backs of the barber, farm, hostel and supermarket signs were not looked at.

## Noticed (no action)

- The back of the car lot's pole plate is blank white for anyone coming from the street ([picture](../../website/public/docs-images/playtest-1127-r2/r2-carlot-b-after.png)); pole plates are single-sided.
- "VEGETABLE PLOTS" stands behind an elder bush; it is readable, but the bush covers the foot of the board.
- On the map, the "Zoom out" button label overlaps the scale bar's "0", and far zoom draws unlabelled grey cluster circles with three dots.
- The post counter's `look` list shows a `SCR_DoorUserAction` beside three `LF_JobSignUpAction` rows (courier, fuel, taxi) before sign-up; the fuel and taxi rows are standing-gated (not checked).
- The harness `seen` line reports `toast=#LF-Quest_ArrivalRegister_Desc`, the raw key; the toast on screen is translated, so this is the harness reading the key, not a player-visible raw key.

## The session setup (not a game finding)

At 15:03 every `session up` on lf-testbox failed with "Client uses unsupported protocol version": Steam had updated the **game client to build 26639613** while the **dedicated server install (app 1874900) stayed on 26257316**. That app is set to update only on launch, and its update stayed suspended while any `ArmaReforgerServer.exe` ran (other agents' failing retries kept starting one). With the maintainer's approval I triggered the update (`steam://install/1874900`) in a quiet window the lead arranged; it reached **26634883** at about 15:19. **Client 26639613 and server 26634883 match protocol**: the session joined in 36 s and every verb above ran. The pool's server build id changed with it, so the test pass cache starts cold.

## Tooling notes

1. The map is driven through `send map open|close|pan <x> <z> <zoom>|hover <x> <z>|descriptors|help open|status`, not `playermenu`; `map open` equips `PaperMap_01_folded` itself.
2. The town board cards are `use LF_TownBoardCardAction__<0..8> Fit_saint_philippe_board_sign_town_map`; the sheet's buttons are `MarkButton` and `Close` (round 1's `LF_BoardAction` on `..._Place_MapBoard` was the wrong class and entity).
3. `session.mjs pull` writes BMPs; `bmpFileToPng` in `agent-box/bmp.mjs` makes PNGs the viewer can read.
