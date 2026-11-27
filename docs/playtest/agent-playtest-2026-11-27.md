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
