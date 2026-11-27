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

---

# Round 3: hours 3 to 6 (mining, smelter, truck yard, lumber, a house)

**Build played:** `ws/playtest-1127-r3` on `origin/main` `eb1857d3` (`compiled-ok`), the 0.3.0 feature freeze. Game client 1.9.0.42.
**Setup:** one `--profile full` island session on lf-testbox (`up --mission island --clients 1 --hold 20`), one fresh account (rules quiz, form R-89), about 20 minutes up, `session down` straight after. The first step was `send checkpoint 5` (Miner 7, Smelting 1, the `MINER_COAL` and `MINER_IRON` licences, $150 cash, $4,750 bank); the lumber leg is `session.mjs lumber-arc` (its own `resetplayer`: $700, Lumberjack 3). The server log holds no `SCRIPT (E)`, `VME`, `Unable to find` or `Can't find` line from the route.
**Route:**
- **Mining.** The Miner's supply till at the Saint-Philippe hardware store (price list), a pickaxe in hand (`itemgive`; the till sells it), real melee swings at the Highstone iron rock (3 swings a piece, 2 nuggets picked up) and the East Rise coal rock (1 lump).
- **Smelter.** The centre-east furnace: prompt, run, 60 s wait, the bar picked up (Smelting XP 28 arrived on the run's end).
- **Selling.** One iron bar at the dock wholesale iron counter.
- **Truck yard.** The hut counter's price list and the examiner's "Take the truck check ($300)" prompt.
- **Lumber.** `lumber-arc` (13 of 13 steps): Standard axe at the Miner's till ($600), a real felling hold on a softwood, a log to the woodlot saw, 8 planks sold at the timber buyer.
- **A house.** The "for sale" sheet and the Buy house row on two signs (`sp-b83`, `di-b05`) with $170 in the pocket.

## Findings, ranked by what a player hits

| # | Rank | Finding | Where | Picture | What I did |
|---|---|---|---|---|---|
| 1 | Medium | A player with $4,750 in the bank who tries to buy a house is told only "You do not carry enough cash for this house." The for-sale sheet says "the price in carried cash", the toast does not say where the cash comes from, and the player has the money, just not in the pocket. The next step (the ATM) is never named. The price is on the F prompt ("Buy house (tier, price)"); the server log read `costs 4000, carried 170` | House for-sale sign (`LF-Property_HouseNoCash`) | [no-cash toast](../../website/public/docs-images/playtest-1127-r3/house_no_cash_toast.jpg), [sheet](../../website/public/docs-images/playtest-1127-r3/house_for_sale_sheet.jpg) (old text) | **Fixed in this PR**: the toast now ends "Take the price out of the bank at an ATM, then try the sign again." (en_us only; the key is unchanged) |
| 2 | Low | Every processor's start toast says "Running. **The parts come out here** in 60 seconds ..." The smelter makes iron bars, the juicer apple juice, the saw planks: "parts" names none of them, and it is the one sentence a new smelter user reads | Smelter, juicer, saw, any `LF_ProcessAction` (`LF-Process_Started`) | [smelter toast](../../website/public/docs-images/playtest-1127-r3/smelter_running_toast_parts.jpg) (old text) | **Fixed in this PR**: "The finished goods come out here in %1 seconds" (en_us only; no test pinned the old words) |
| 3 | Low | The truck yard's price list shows "Cargo truck needs $12,000" and "Ural 4320 cargo truck needs $9,500" and says nothing about the truck license. Both rows are `needs $X`, so a player who saves $12,000 first meets "Trucks need the truck license first" only at the Buy press; the examiner's "Take the truck check ($300)" prompt is a separate stop at the yard. The rows are also in price order descending and the two names read alike | Truck yard hut counter (`LF-Lot_Rule` is shared by every lot) | [price list](../../website/public/docs-images/playtest-1127-r3/truck_yard_sheet.jpg), [examiner](../../website/public/docs-images/playtest-1127-r3/truck_check_prompt.jpg) | Noticed: the sheet text is shared with the car lots, so a truck-only line needs a pack override and a layout check; not a freeze fix |

## What read well

- **The Miner's supply till.** Worn, Standard and Fine pickaxes, axes and rods at $150, $600 and $1,800 in a clean list, "Order slip: pick a line from the list", and "Leave" ([till](../../website/public/docs-images/playtest-1127-r3/miners_till_price_list.jpg)). The $150 Worn pickaxe is exactly the starter purse; nothing sells a tool a new player cannot afford.
- **Mining feels like work.** Three swings a piece, five to six pieces a rock, the nugget lands 1 to 2 m from the rock and is picked up from where the miner stands ([iron rock](../../website/public/docs-images/playtest-1127-r3/iron_rock_swung.jpg)). No floating or clipping piece in the pictures; the ore rocks, the cart and the timber stack at Highstone sit on the ground.
- **The smelter.** One use takes 2 nuggets and 1 lump, the toast names the wait, the run ends on the 60 s mark with "1 output dropped" and Smelting XP 28 ([bar](../../website/public/docs-images/playtest-1127-r3/iron_bar_carried.jpg)). The first-shift "double xp for 1800 s" log line fired.
- **Selling.** The dock iron counter paid $20 for the bar ("Sold for $20!", round 1's wording fix) and the cash readout showed "+$20 $170".
- **Lumber, end to end.** Standard axe $600 at the till, a "Fell tree (softwood, 25 s)" prompt on the tree ([prompt](../../website/public/docs-images/playtest-1127-r3/fell_prompt_stump.jpg)), 2 logs, 8 planks from the saw ([saw](../../website/public/docs-images/playtest-1127-r3/woodlot_saw.jpg)), sold for $64 in two bundles; 6.5 minutes with the walk, **$587/h**.
- **Pay against the ladder.** `tools\cli economy`: iron at Coalman's Brow $447/h net and iron by pickup $515/h, hand-haul lumber $446 to $473/h steady, truck hardwood $742/h; police patrol $350/h up to lieutenant $450/h. Every mining and lumber loop sits over the police entry rung; the measured lumber run ($587/h) is above its model. A truck pays $742/h against $446 by hand, so the $9,500 Ural pays itself back in about 32 hours of hauling.
- **House prices.** The two signs asked $4,000 (`sp-b83`) and $13,000 (`di-b05`), 9 and 29 hours of lumber by hand: a house is a long goal, a cottage within a day's saving.

## Not reached (say so, do not read it as a pass)

- **A purchase.** No truck (needs $12,000 and the truck license), no house (needs cash in hand) and no car upgrade was bought: the character never held the money, and the harness has no cash verb for a character that has not earned it. Only the refusals and the sheets were read.
- **Hauling a load in a truck.** The truck-haul proof needs a bought truck; round 3 covered the lumber chain on foot only (`lumber-arc`).
- **The house sign face and the door.** `stand` put the camera at the door, not on the sign; the "for sale" sheet was read from its action.
- **Selling coal** (the power plant buys it at $8 a lump) and **copper**. Only iron bars were sold.
- **Audio** (`-noSound`), and a **walk** (every position was a harness `stand`, so the walk to the dock, the smelter and the truck yard was not timed).
- The F2 "What to do next" and the HUD still read "First get your papers from the clerk" and "Next: New in town" with a waypoint 2,240 m away: the checkpoint jumped the character past the clerk, so this is the setup, not a game finding.

## Noticed (no action)

- The buyer's prompt reads "Sell goods / Sell" with a greyed icon and no reason when the pockets are empty ([picture](../../website/public/docs-images/playtest-1127-r3/woodlot_buyer_empty_hands.jpg), [dock](../../website/public/docs-images/playtest-1127-r3/iron_buyer_after_sale.jpg)); the next step is obvious on the first sale and not on the second.
- The Miner's supply sheet shows "Pick a line from the list first." under the buttons before a row is chosen; pressing "Sell these" with no row chosen kept the same sheet and the same line ([picture](../../website/public/docs-images/playtest-1127-r3/miners_till_price_list.jpg) shows the sheet). I did not test it with a row chosen, so I do not know whether this till buys anything.
- The Highstone mine has no desk and no pickaxe seller; the tools come from the hardware store in town, so a player at the mine sign cannot buy a pick there.
- `itemgive` and `pickup` are verbs, not flow steps (`pickup` and `send itemgive` fail in a `.txt` flow); a node driver calling `sendCommand` is the way for a mining run.

---

# Round 4: hours 6 to 10 (bank, house, crew, outpost, fence, export crane)

**Build played:** `ws/playtest-1127-r4` on `origin/main` `8df4e1d6` (`compiled-ok`), the 0.3.0 feature freeze. Game client 1.9.0.42.
**Setup:** session 1, `up --mission island --clients 1 --profile full --hold 20`, one fresh account (rules quiz, form R-89), about 14 minutes up, `session down` straight after. Session 2 (`--hold 12`, same flow shape, 4 minutes) re-checked the three fixes below on the changed text. Both server and both client logs hold no `SCRIPT (E)`, `VME`, `Unable to find` or `Can't find` line.
**Grants (what skipped grind, never the step under test):** `send checkpoint 6` (cash $200, bank $5,460, Miner 8, Delivery 6, `UNDERWORLD` 70, the driving licence) at the start; `send levelset DELIVERY 10` before the export board (the board asks Delivery level 10); `send repgrant UNDERWORLD 0` before the outpost, so the visit is at standing 0 (a checkpoint character holds 70); `send exportorder give short|rest` for the goods at the crane (40 iron bars do not fit the pockets, the harness fills them); two `itemgive` (car parts, moonshine). The harness has no walk, so every position is a `stand`; no distance was walked.
**Route:**
- **Bank.** The island ATM's in-world rows (withdraw $500, withdraw all), the teller's terminal sheet, a locker rental.
- **House.** The sign's "For sale" sheet, the price in the prompt, the purchase with $5,620 cash (withdrawn at the ATM first).
- **Crew.** The registry desk: found "Lantern" ($250), take the yard lease, the flag wall (12 "Fly the ... flag" rows offered to a founder), fly one.
- **Underworld.** The north rebel outpost at standing 0: the gate guard's talk, the chalk board, the quartermaster's talk and crate (radio $60 sold, pistol refused in words).
- **Fence.** Vince's talk ("what do you do"), the price list, car parts sold ($100).
- **Export.** The dock board's take row, the crane spot hand-in: short count refused, full count paid $1,040.

## Findings, ranked by what a player hits

| # | Rank | Finding | Where | Picture | What I did |
|---|---|---|---|---|---|
| 1 | High | Every outpost sale and refusal toast is **cut to a single `"`**. The quartermaster's reply reads `Quartermaster: "` and nothing else after paying $60, so the player gets no word on what they bought or why a buy was refused. Ten en_us rows (`LF-OutpostBuyer_Full|FullNever|NoCrate|NoRun|Sold|Unavailable`, `LF-OutpostStock_Bought|NoCash|NotKnown|Unavailable`) were written with escaped quotes (`"\"Take it. ...\" You paid %1."`) and the string table cuts the text there. They are the only rows of 5,600 that held an escaped quote | Outpost quartermaster and buyer | [before](../../website/public/docs-images/playtest-1127-r4/outpost-toast-bare-quote-before.png) | **Fixed in this PR**: the quote marks are gone (the toast title names the speaker). After: [sale](../../website/public/docs-images/playtest-1127-r4/outpost-toast-fixed-after.png) reads "Take it. Don't tell anyone where you got it. You paid $60.", [refusal](../../website/public/docs-images/playtest-1127-r4/outpost-refusal-fixed-after.png) reads "I don't know you well enough for that one. Come back when people talk about you.". New check `validate-no-escaped-quote-targets` (red on the ten old rows) |
| 2 | Medium | The house purchase toast shows the **internal house id**: "You bought house saint-philippe.home.sp-b83. The key is in your inventory." (the id is the `m_sDebugIdentifier`) | House for-sale sign (`LF-Property_HouseBought`) | [before](../../website/public/docs-images/playtest-1127-r4/house-bought-raw-id-before.png) | **Fixed in this PR**: "You bought the house. The key is in your inventory." ([after](../../website/public/docs-images/playtest-1127-r4/house-bought-fixed-after.png)) |
| 3 | Medium | The org ledger names the lease "**Alley yard lease**" and its toasts say "The alley yard is yours", the old playtest village's name, while the same page's "Held ground" line lists "Farm Compound, Base lease". Two names for one place on one page; core text must keep no place names | Registry ledger (`LF-Org_*` rows) | [before](../../website/public/docs-images/playtest-1127-r4/ledger-alley-yard-before.png) | **Fixed in this PR**: "Yard lease", "The yard is yours. Members open its gate.", "You gave the yard back. Its gate is shut." ([after](../../website/public/docs-images/playtest-1127-r4/ledger-yard-fixed-after.png)) |
| 4 | Low | The crane's payout toast reads "The shipper paid you **$1040**." while the HUD beside it reads "+$1,040 $7,620" | Crane spot (`LF-ExportCrane_Paid`) | [paid](../../website/public/docs-images/playtest-1127-r4/crane-paid.png) | Noticed (the cap on fixes this round was three; one row) |
| 5 | Low | The short-count refusal says "That is not the full order. Bring every unit, then hand it in." but not how many are still missing | Crane spot (`LF-ExportOrder_RefuseShort`) | [short](../../website/public/docs-images/playtest-1127-r4/crane-short-refusal.png) | Noticed |

Findings 1 to 3 are core strings in `everonlife_localization.st` (en_us rows; `regen-localization` rewrote the other six language tables, which carried the same English). They have no logic test: a string has no cheap game test, and the new validation guards the one structural cause (finding 1). Session 2 re-read all three on screen; the pictures above are from it. No money logic changed, so no `interrogate` file.

## Money checks

| What | Read | Verdict |
|---|---|---|
| House `sp-b83` | $4,000 for sign `sp-b83` (the only one bought); hours-6 pocket $200 plus $5,460 in the bank; about 9 hours of lumber by hand (round 3) | A long goal, as intended; the ATM step is spelled out by round 3's refusal text |
| Org founding | $250 fee in cash, then the yard lease free; "Processing upgrade" $250 | Fine for hour 7 to 8 pay |
| Locker | $40 at the bank | Cheap and clear ("You rented a locker here.") but the toast does not say the price or that it is a one-off |
| Outpost crate | radio $60 at standing 0; pistol refused until the standing is up | Fine; the refusal is now readable (finding 1) |
| Fence | car parts "We pay $100"; lockpick $250, scanner $900, keycard $1,500, drill $2,500, breaching charge $4,000 | Sensible for crime's high-swing band: the dearest tool (a $4,000 breaching charge) costs the same as the house |
| Export order | 40 iron bars, $1,040 (wholesale + 15%) at about an hour of mining and smelting | About $1,000/h, over the police rung ($350 to $450/h) and in the high-tier rung the order intends |
| Pay ladder | police $350 to $450/h stays lowest: the order, a lumber run ($587/h, round 3) and the fence's parts all pay above it | OK |

## What read well

- **The ATM.** The in-world rows (withdraw $20 to $500, all, deposit all) move exact amounts and the cash HUD shows "-$500"-style flashes; the teller opens the same green terminal, "FIRST BANK OF EVERON", keypad, Deposit, Withdraw, **Transfer**, Exit ([sheet](../../website/public/docs-images/playtest-1127-r4/teller-sheet.png), [rows](../../website/public/docs-images/playtest-1127-r4/atm-withdraw500.png)).
- **The locker.** "You rented a locker here." and the prompt turns into "Open your locker" with a greyed "Rent a locker" ([picture](../../website/public/docs-images/playtest-1127-r4/locker-rented.png)).
- **Founding a crew.** The register's one page: name, "$250 fee in cash", "Sign and pay $250" ([picture](../../website/public/docs-images/playtest-1127-r4/org-register-open.png)); after signing the ledger shows rank, fund, members, "Held ground (1): Farm Compound, Base lease". The flag wall offers 12 flags to a founder only and "Your gang now flies the new flag." appears ([picture](../../website/public/docs-images/playtest-1127-r4/flag-flown.png)). The name rule's notice ("Use letters, digits, spaces, - ' & and . only.") appeared when the harness typed an underscore, which is the rule working.
- **The outpost at standing 0.** The gate guard stops you in character ("Hold it there. This is not a public road. State your business or turn around.", replies "I'm here to trade" and "Who are you keeping out?") ([picture](../../website/public/docs-images/playtest-1127-r4/outpost-guard-talk.png)); the quartermaster ("I keep the crate and I keep the count. Nothing leaves this camp without a price on it.") ([picture](../../website/public/docs-images/playtest-1127-r4/quartermaster-talk.png)). The tent and the camp props sit on the ground.
- **The fence.** A three-reply greeting, "what do you do", the list "The Fence" with five tools and a "Car parts, We pay $100" line, a receipt "Sold for $100." ([list](../../website/public/docs-images/playtest-1127-r4/fence-price-list.png), [sold](../../website/public/docs-images/playtest-1127-r4/fence-sold-parts.png)).
- **The export order.** The board's take row, a refused short count and a full count paid $1,040, a plain "Order handed in." ([picture](../../website/public/docs-images/playtest-1127-r4/crane-paid.png)).

## Not reached (say so, do not read it as a pass)

- **The truck leg.** The brief asked for the crane order *with a truck*. The truck check, the $12,000 truck and the truck-backed hand-in were not completed: my driver stood away from the lent truck and the examiner failed the course ("#LF-TruckCheck_FailedLeft", then `no_lent_truck` at gate 3). That is my driver (the proof stands beside the start first), not a game finding. Round 2026-11-27 flows covered the truck hand-in ($1,040 paid from a truck's storage); here the order was filled from the pockets by the harness.
- **A house's door, key and storage.** The purchase and its toast were read; the key in the pocket, the door lock, and the house's storage were not tried (the session's hold ran out first, and the sold sign's rows `look` listed show "Sold" by design). Not a pass.
- **A dirty-money run.** The informant and buyer loop (`dirty-money-island`) was not run; the fence was the dirty-goods leg (car parts only; no moonshine sold). Crew robbery, hideout stash and the gang radio were not driven; the ledger and flag wall were.
- **A loan or the bank vault.** The teller offers withdraw, deposit and transfer; no loan. The vault door's only action is the heist (`LF_BankRobAction`), so a civilian is not offered a vault. Neither was used.
- **Walking, sound and a second player.** Every position is a `stand`; the session runs without sound; one client.

## Noticed (no action)

- The cash HUD readout and the HUD "Next: New in town" with a "The town clerk 3,940 m" waypoint stay on screen at the outpost: the checkpoint jumped the character past the clerk, as in round 3.
- In session 2 the quartermaster and the tent drew as chrome for the first frames after the teleport (streaming at 29 fps), gone in the next picture; it did not show in session 1.
- The ATM's "deposit all" moved $5,620 of a harness-granted $12,000: the quick row reads the replicated cash, which `resetplayer` does not refresh. A real player's pockets replicate, so I read it as a harness artifact, not a bug.
- The chalk board at the outpost is large white-on-grey text read from the camp; from the gate the lower lines are cut off by the frame. The board's reading action returns "none" in `ui text` (it is a sheet-less prompt).
- The Saint-Philippe house sign's `look` still lists the "Sold" row after a purchase, as designed (`LF_HouseSaleBuyAction` shows "Sold" once owned).

## Tooling notes

1. `stand <entity> <side>` takes `e`, `w`, `n`, `s`, `n2` or `e7`; `itemgive` and `resetplayer cash=N` need a node driver (`sendCommand({verb, prefab|cash})`), `send itemgive` fails in a flow.
2. `use LF_ATMQuickAction__<n>`: 0 to 3 withdraw $20, $50, $100, $500, 4 withdraw all, 5 deposit all.
3. The org name field takes only one plain token from `type` (letters, digits): no space, `_` or `-`.
4. `harness.mjs flow <name> --verbose` prints each `look`, `state` and `ui text` answer; the compact run hides them.
