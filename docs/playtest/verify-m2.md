# Verify M1, M2 and M2.5: the human pass

Checked on `main` at b8227a1f (2026-11-12). 63 open issues in the milestones "M1 Playtest Town", "M2 Living Town"
and "M2.5 Showcase and Outreach" are labelled `in-dev` or `needs-playtest`. An agent proved 24 of them, one proof
failed and has an issue, and 38 are left for you. This page is the 38, in walking order. About 43 minutes:
solo walk 27, two-player block 6, restart block 5, desk 5. Nothing here closes an issue; you do, with
`tools\cli promote --to <sha> --confirm [--fail N]` after the pass.

Setup: start the playtest town on your own (`Play on my own` in the launcher, or `tools\cli playtest`). You are the
first player, so you are on the police roster. Spawn, then use the travel globe (table at 119, 243.5, *Travel to a
playtest area*) for every "go to" below; walk the last few metres. Tick a line when the thing is seen; a failure
is `--fail N`. Items marked [look] are visuals an agent cannot judge (the dedicated server has no renderer).

## Solo walk, in order

**Spawn square (119, 243)** (2 min)
- [ ] Table globe: travel to bank, jail, farm, fence, quarry, chop shop, fuel station. Each lands in the open, not in a wall or under a floor; Set Night then Set Day work. #607 (proven: list and landing at 3 areas)
- [ ] Look up in Clear weather: blue sky, no streaky clouds. #602 [look]
- [ ] Press Esc on a sheet the server opened (wanted card at the police desk, ledger after founding): it closes. #199

**Bank (177, 258)** (3 min)
- [ ] *Talk to the teller* shows from in front of the counter; Edith stands centred on the footing; withdraw 100, deposit it back. #590, #679 [look]
- [ ] Chest east of the bank: *Take bank robbery kit*, drill the vault at dusk: the alarm flash lights the site without whiting out your screen; the beacon stands on the grass at the door. #605, #571 [look]

**General store (179, 244) and car lot (216 to 230, 261)** (3 min)
- [ ] Take an apple, use *Pay for goods* on the register on the counter: toast and receipt; from across the shop it is refused. #582
- [ ] Chalkboard price boards at the car lot read the lot's prices, board face visible from the street. #582 [look]
- [ ] Garage pad: read the ledger card, retrieve the S105, collect the UAZ at the impound desk. #101

**Police station (233 to 244, 238 to 246)** (5 min)
- [ ] Police car (243.5, 246): the blue bar touches the roof; the rear plate is clear of the spare tyre; clocked in, the siren key flashes the bar. #595, #679 [look]
- [ ] Porch (233, 238): armory cabinet and evidence locker stand against the wall, doors facing the porch; clock in at the desk, both actions show. #679
- [ ] Duty desk: *Clock in* is the first row and sits on the clock. #625 [look]
- [ ] Dispatch area (249 to 257, 229 to 233): boards, console and payphone stand on the ground, nothing inside another prop; *Test medic call* gives the toast. #625 [look]
- [ ] Red board outside the jail: *Send me to jail (playtest)*: you are in the cell, slip toast, scrub the floor and see minutes drop, released near 60 s. #604

**Fuel station (258 to 285, 240)** (3 min)
- [ ] South face of the kiosk (service window, near 285.5, 240.2): with the till and clerk in view, *Rob the till* shows, pays 150 and raises wanted. #729, #627
- [ ] Alarm beacon sits on the till counter, not above it. #571 [look]

**Bank street and cash van (196, 252 north lane)** (2 min)
- [ ] Rob the van's cash box with the drill: the bench drill stands on your right, not under your feet; the beacon sits on the box at the back of the van. #571 [look]

**Lock-up, chop shop (322, 300) and moonshine (384, 300)** (3 min)
- [ ] Chop shop yard: a stolen car (lockpick the beige one east of the lock-up) fits on the pad beside the crane; a non-stolen car gives the "Only stolen cars" toast, readable. #601 [look]
- [ ] Moonshine camp from the south: the still reads as a boiler tank, not a plain white boiler; barrels still load and work. #497 [look]

**Hideout, barn convoy and registry desk (302, 288; registry 289, 256)** (4 min)
- [ ] Registry desk: found an org, open the ledger: the lease line is not cut off and no interaction prompt draws over the sheet. #494 [look]
- [ ] Barn pallet: *Start convoy*, drive the van to the cage 27 m south, *Deliver contraband*: +200 cash; the pub cage offers nothing. #616

**Crime annex (30 to 48, 50)** (2 min)
- [ ] Read each board; the safe doors face the boards, rifle, money and lamp rest on the table, nothing clips; the safe action reads "Rob the safe". #608, #609, #679 [look]

**Quarry and mining (178, 168)** (5 min)
- [ ] Gate (184, 189): the info board reads, each station reaches without clipping, no tree hangs over the sawmill or dune, bags sit on their tables. #618, #679 [look]
- [ ] Walk in from the dirt lane: red LAWLESS AREA tag and entry toast, gone past 32 m. #591 [look] (proven: zone changes)
- [ ] Inventory: gold bar, nugget and pickaxe previews centred and sized alike. #592, #612 [look]
- [ ] Miner stand (176, 167): swing at the iron rock, it shrinks each piece; nuggets lie on the ground; smelt two at the boiler (169.8, 171.5), which looks like a furnace. #592, #593 [look]
- [ ] Inventory with an apple, plum and apple juice: previews not pixelated, not far, not too close. #612 [look]

**Farm (148, 299)** (2 min)
- [ ] Plant a cabbage plot: sprouts, half grown after a minute, full heads after two; harvest gives 3. #614 [look] (proven: take, plant, grow, harvest on the server clock)

**Inventory keys** (1 min)
- [ ] House key (brass) and vehicle key (black fob) show their own icons, large and distinct; drop one and hold one: the lie and grip look sane. #613, #679 [look]

## Two players (6 min, second client on this machine or LAN)

- [ ] Hospital (262, 258): player A clocks in as medic, player B is downed and A holds *Revive*: A kneels in CPR on both screens, B wakes at 4 s; release at 2 s stands A up at once. Clip looks right on a standing medic. #512 (proven: start, play and stop logs) [look]
- [ ] B gives up while downed (hold X after 30 s): the death title is not clipped ("You are dead"). #493 [look]
- [ ] Player A downs B and drags then carries them, loads the body into a car: the body sits in the seat. #697
- [ ] Player A shoots a civilian near the fuel station with a real gun: it dies, A is wanted for murder, the clerk takes no damage (real bullets were never exercised). #610
- [ ] Player B hosts and A joins while a rock is half mined and a plot half grown: both show the same stage. #592, #614

## Restart (5 min, dedicated server: `tools\cli host start`, then stop and start with `-loadSessionSave`)

- [ ] Take an apple in the general store, restart, rejoin: the apple is back with a 5 minute pay toast; pay at the register, or leave unpaid and get the shoplifting alarm. #479
- [ ] `m_iMaxPerAccount 2`: register Ada, rejoin, register Bob; Ada is not wanted when Bob is; both survive a restart with their own bank. #331
- [ ] Plant a plot, restart: it is still growing. #614

## Desk (5 min)

- [ ] Open `/join` and `/join/everon-life` on a deploy preview, press both buttons; skim `docs/community/outreach/faq.md` and one community file. #568, #569, #570, #572, #573, #574, #575, #576
- [ ] Watch `trailer-v3` and approve or reject; confirm "v1 = M3 Framework Release" and "Built in the open with AI agents". #571
- [ ] Dev tooling, nothing visible in play, tick on the PR evidence unless you want to re-run it: film actors #393, camera rig #392, storyboard #395, scene reset and audio capture #394, #396 (WIP, audio sync unproven), post-production #397, Discord relay #696 (needs the webhook URL and a test account).

## Not on this pass

- #109 Workshop release: needs a real Publish first (`docs/playtest/workshop-publish.md`); the steps are in PR #711.
- #335 128-player budget: needs many clients; steps in `docs/architecture/performance.md`.

## What the agent ran

Lowspec playtest sessions on this PC, 2 or 3 clients, `session down` after each. Suite: `tier=all passed=357 failed=0 total=357`;
`tools\cli validate` 52 checks 0 failed (covers #708 item volumes); `audit placement` 0 of 728 entities flagged
(covers overlaps for #608, #618, #679). PASS lines from `session.mjs`:

| Issue | Proof | Result |
|---|---|---|
| #127 | `jail-work-proof` | pass (arrest, laundry takes about 20 s off, repeat refused) |
| #128 | `police-application-proof` | `police application proof: PASS` (session without a roster) |
| #242 | `quarry-proof` | pass (normal, lawless, safe) |
| #415, #601 | `chop-proof` | pass |
| #512 | `revive-anim-proof` | pass (logs only; the kneel is in the two-player block) |
| #513 | `work-anim-proof` | pass (vault drill, door pick, car pick, van drill) |
| #564 | `scanner-proof` | pass |
| #590 | `bank-teller-proof` | pass after a fix in this PR (the proof passed its arguments in the wrong order and died with `unknown verb: undefined`) |
| #591 | `quarry-proof` | pass for the zone; the tag is above |
| #592, #593 | `mining-proof` | pass |
| #600 | `backroom-police-proof` | pass |
| #605 | `bank-kit-proof` | pass |
| #607 | `spawn-hub-proof` | pass |
| #610, #615 | `npc-witness-proof` | `npc-witness proof: PASS` (session without a roster) |
| #613 | `keys-proof` | pass (house=1 vehicle=1) |
| #614 | `farm-proof` | pass |
| #625, #626 | `dispatch-proof` | pass in a fresh session |
| #495, #540, #708 | suite and validate | pass |

Also passed on the way: `cash-van-proof`, `stolen-proof`, `armory-proof`, `jailbreak-proof`, `medic-proof`,
`lockpick-proof`, `village-proof`, `bank-kit-proof`.

Failed: #496. The per-proof reset still prints `still dirty: wanted`, so proofs cannot run back to back; filed as #867.
Not proven, environment: `downed-proof` and `carry-proof` wait for a late third client that the pool refused for RAM
(`join refused, free RAM 1.6 GB`), so #697 and the late-joiner rows are in the two-player block. `store-proof` (#582) was
not run: the output was lost, so the register is on the walk. `search-proof` (#419, not in this set) failed twice in a
used session with leftover state; not retried.
