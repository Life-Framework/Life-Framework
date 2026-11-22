# Next release: 0.3.0 (the week plan)

The plan for the next major release on `main`. It folds the five experience scopes
([work](scopes/work/experience-scope.md), [crime](scopes/crime/experience-scope.md),
[gangs](scopes/gangs/experience-scope.md), [police](scopes/police/experience-scope.md),
[world](scopes/world/experience-scope.md)) into one release with more than 10 hours of play after the
onboarding. It does that in seven days, with about eight agents at once on the laptop pool. 0.2.1 is finished
separately on `release-0.2.1`. Its first-two-hours fixes always take a lane before any card here
([roadmap](README.md) "Now").

This is a plan for the maintainer. No issue is cut and nothing is dispatched until the maintainer approves
it and answers (or accepts the defaults of) the decisions in section 5. Then one planner turns each release
chapter below into a plan file and runs `tools\cli slice new` ([pipeline](../process/roadmap-pipeline.md),
[card format](../process/slice-card.md)).

**In numbers.** Seven release chapters. 162 must-have cards: 133 of the 238 scope cards, 4 mining slices
already filed under #3206, 8 suggested additions, 9 playtest-town parity cards and 8 integration cards. There is
also an ordered stretch queue of 47 cards and a backlog for 0.4. An agent playtest round at the end of every
day, with full runs of the 10-hour arc mid-week and on Saturday, one release candidate on Saturday, the release on
Sunday 2026-11-29. 49 decisions, each with a default. Pay follows the maintainer's rule of 2026-11-22: police
are the lowest paid, medics a little more (mid-tier civilian), and high-tier civilian trades out-earn both
(section 2).

**Changes after the plan merged (maintainer, 2026-11-22 late).**
- **Liveries are full repaints** (N30 changed): whole vehicle bodies are retextured for Police, Ambulance,
  Rebel and the company colours, and vanilla work clothes may be tinted. Each derived texture gets a credit
  row (`docs/credits.md`). Lettering overlays stay as a second layer where the spike shows they hold. WO-L7
  (rebel) moves from stretch to must-have.
- **Medics earn a bit more than police**: medic shift pay sits on the mid-tier civilian rung, about $480 to
  $600 an hour with the bonus, still below the high-tier trades. Police stay at $350 to $450 (X-8).
- **Chapter 7, Playtest town parity**: the playtest town (the DebugWorld village and `Everon1989Town`) gets
  everything built for the island. The maintainer asked for it, so these cards edit his town layers with his
  explicit approval.
- **A daily agent playtest round** (X-9) replaces the two one-off playthroughs; X-5 and X-6 stay as the
  mid-week and Saturday full runs.

**Milestones for the cut issues.** One release issue holds seven chapter epics. Chapters 1, 2, 6 and 7 and the
integration cards file under M6 Making a living; chapters 3, 4 and 5 under M7 Power and turf. Existing issues
keep their milestones.

Card keys carry their scope: `WK-` work, `CR-` crime, `GA-` gangs, `PO-` police, `WO-` world, and `X-` for
the integration cards this plan adds, `TP-` for the playtest-town parity cards. `WK-W4` is card W4 of the work scope.
CR-A4 (the cash van route) is already merged as #3398 (`needs-playtest`): the X-9 rounds check it, and it gets no new card.

## The seven release chapters

| # | Chapter | What a player gets | Scope chapters it takes |
|---|---|---|---|
| 1 | A trade of your own | Every legal job levels with a sound and a card, tells you what it opens next, wears and repairs tools, picks a perk at level 10, and has a weekly shift board. The iron chain runs end to end across the island | Work A, B (part), D (part), E (export orders); mining #3206 (iron) |
| 2 | Bigger wheels | A capacity ladder from pockets to truck, "Feed from the truck" at a station, the truck check at the licence desk and the truck yard | Work C |
| 3 | By the book | The law book, the booking desk and the charge sheet; dispatch calls with outcomes and capped bonuses; rank cars, the radar gun, the stolen-car hotlist; the police and medic skills | Police 1, 2, 3 (part), 6 (part) |
| 4 | Smoke and bottles | The robbery ladder works on the island (till, store safe, cash van, bank, all under one big-job lock); the cigarette and vodka run to village bars; stop searches; the pharmacy robbery and the clinic shortage | Crime K0, K1, K3 (part), K5 (goods class) |
| 5 | Running a crew | Found a gang, pick one of 20 flags, recruit by scroll, talk on a gang radio, see each other on the map; the north rebel outpost with its quartermaster, chalk board and contraband buyer | Gangs: Running a crew, the outposts (north) |
| 6 | The island talks | The square comes alive; the weekly *Everon Courier*; rumors that fill the map; foremen and shopkeepers at work; bells and the ship's horn; police, ambulance, company and rebel repaints and the respray bay | World W1, W2, W3 (liveries), W4 (part), W5, W6 (part) |
| 7 | Playtest town parity | The playtest town has what the island has: ATM kiosks, bus stops and boards, the onboarding route (clerk pack, starter car, orchard, press, market), mining (desk, rocks, furnace, buyer), the postal run, clothing mannequins and counters, map POIs and run chains, the welcome guide and HUD, the jail doors and the bail phone | New (maintainer ask, 2026-11-22) |

What waits for 0.4 (Power and turf): the prison and the prison break, hideouts and turf, the drug chains,
night landings, the reserve heist, and the helicopter and pilot licence. Section 3 gives the reasons.

---

## 1. The 10-hour arc

What a new player does, hour by hour. Hours 0 to 2 are the onboarding that 0.2.1 polishes. After that the
player climbs a ladder with three branches (the worker, the officer, the criminal). Most players try all
three, and by hour 7 the branches meet in a crew. The arc uses the shipped pay numbers (section 2): about
700 XP an hour in a trade (level 5 at 1.5 h, level 10 at 7 h), a start of $2,000 in the bank, and the
`tools\cli economy` ladder ($5,460 banked by hour 6 on runs alone).

Every hour below names the must-have cards that make it reachable. A beat that needs players online (police
for a robbery, a second gang member) has a solo fallback, so a quiet server still has a full hour.

| Hour | What the player does | Built by (must-have) | The hook into the next hour |
|---|---|---|---|
| 0 to 1 | Wakes in the hostel, gets the welcome pack and the starter car from the clerk, picks apples, presses juice, sells at the market (built, polished in 0.2.1). **New:** pigeons lift off the square, two gossips on the bench, the church bell on the hour, a printed till slip, and the level-up card at Farming 2 | 0.2.1; WO-W1.1 to W1.6, WO-W2.6, WK-W1, WK-W3, WK-W4 | F2 "What to do next" offers the mine, the post office and police duty, each with a person to ask |
| 1 to 2 | Buys a worn pickaxe ($150) at the hardware store and takes the job at Bruno's clock on Coalman's Brow. A level every 10 to 15 minutes, with the shift whistle at 06:00 and the first shift of the day at double XP. Or the postal run in the "POSTAL SERVICE" van | WK-W5, WK-W7, WK-W12, WK-W23, WO-W4.4, WO-W4.7 | Bruno: "The coal seam at East Rise will take you on at Mining 4." The place is marked "heard of" |
| 2 to 3 | **Worker:** coal at East Rise, sold at the power plant on the edge of town. **Officer:** asks the desk sergeant to join, clocks in as a Cadet, gets the beige S105 with "POLICE" on the doors, takes a payphone call on the main street, closes it as resolved (+$15 and police XP; duty pays like an entry job, the draw is the uniform and the car). Buys the *Courier* ($1) and reads about Tuesday's robbery | WK-#3243, WK-A1, PO-R1, PO-D1, PO-D3, PO-D4, PO-R8, WO-L4, WO-W2.1 to W2.4, WO-W4.8 | The gossips: "the old still barn isn't empty at night". The barn is marked "heard of" for players the town knows |
| 3 to 4 | Asks a shopkeeper "Heard anything?" and the map fills in. Repairs the worn pickaxe at the hardware bench. **Criminal taste:** moonshine at the still barn (built), sold behind the dock warehouse for marked notes, washed at Vince's | WO-W5.1 to W5.4, WK-W9, WK-W10, WK-W11 | Vince's chalk board: "This week I want radios and tires". The store safes are "worth a look" |
| 4 to 5 | **Worker:** the iron chain by pickup, Highstone hill to the centre-east smelter (Smelting is its own trade). **Officer:** the radar gun on the Meaux road and three speeding classes on US English signs; a stop for speeding becomes a booking with a charge sheet. **Criminal:** a till, then a store safe; caught, the charge sheet offers "Pay $450 now" or ten minutes in the holding cell with its work stations | WK-#3242, #3244, #3245, WK-W2, PO-R4, PO-R5, PO-L1 to L10, PO-X1, CR-A1 to A4, CR-A6 | F2 "My file" lists your tickets and the licence that was seized. The car lot has a van for about $3,500 |
| 5 to 6 | **Worker:** buys a van ($3,000 to $4,000; it carries 8 sacks to a car's 4). **Criminal:** the cigarette and vodka run. Marcel at the container yard leaves container nine short, and Josette buys two cartons under the counter at the Chotain bar. **Officer:** stop searches find cartons in the boot; the stolen-car hotlist on the pager flags a plate | WK-W16, WK-W20, CR-B1 to B9, PO-hotlist, PO-P10 | The shift board shows you seventh this week. Level 10 is close |
| 6 to 7 | **Worker** (one who mostly hauled): Hauling 10 opens the dock's export order board ("40 iron bars to the steamer, $1,000, by 18:00"). The van backs up to the smelter and "Feed the furnace from the van" empties it. **Officer:** a promotion to Patrol brings the UAZ, a line on the rank board and a higher bonus cap. **Medic:** outcome bonuses and the ambulance, if that is the player's duty | WK-W17, WK-W24, WK-W22, PO-R9, PO-S1, PO-S5, WO-L5 | Two friends ask at the café whether you want to start a crew |
| 7 to 8 | Three friends found a gang at the registry ($250), choose a flag from the cloth wall, invite each other by scroll (a new member starts as a prospect), buy gang radios ($60) and see each other as red marks on the map. A player alone carries on in a trade, or takes a friend's invite later | GA-C1 to C15, GA-A2 | The radio: "Sarge at the north camp sells to people he's heard of." |
| 8 to 9 | The crew drives to the north outpost: guards at the gate, a safe zone, Sarge's chalk board. Tier 1 underworld standing (about 2 h of crime) buys cigarettes and a pistol; contraband crates sell 10 to 20% above the fence. **Or** a two-person pharmacy robbery: one holds the counter, one fills a bag from the cabinet, the clinic door shows the shortage notice, and the morphine counts as narcotics class B on the charge sheet | GA-O1 to O13, CR-D1 to D3, CR-D8, CR-F1 | The chalk board's last line: "Rifles for people I trust." Next week's *Courier* prints the robbery with no names |
| 9 to 10 | **Crew:** the cash van or the bank vault against two or three officers. The radio carries the plan, and the big-job lock keeps it the only heist running. **Worker** (one who mostly mined): the first perk pick at the labour office at Mining 10, a chance of double yield or faster swings. The truck check at the licence desk in a lent Ural ($300), and the truck yard on the north-east compound with the Ural at $9,500 | CR-A5, CR-A6, WK-W8, WK-W8b, WK-W13, WK-W18 to W21 | Past hour 10: your own truck (about 20 h of work), the rifle rack (tier 2, about 8 h of crime), Sergeant rank, the next *Courier*, and in 0.4 the prison break and hideouts |

**What keeps hours 2 to 10 varied.** Each hour adds a new place, a new person and a new verb. A full hour is
never one loop: the worker branch moves from the brow to East Rise, Highstone, the smelter and then the
dock; the criminal branch from the still barn to the safes, the container yard, a village bar and the
outpost. Rewards come every 10 to 15 minutes early on (levels, tools, rumors, the *Courier*) and every 30 to
60 minutes later (the van, the perk, a rank, a flag on the map).

**Solo and quiet-server fallbacks.** Robberies above the till need police online. On a quiet night the run
to the bars, the moonshine and the export orders need nobody else. The crew hour works for one player as a
hauler or through the advertised crew. The officer branch always has calls, because payphone reports also
come from resident NPCs who see a crime (built in M5).

---

## 2. Cross-scope coherence

The five scopes were written in parallel. They share systems and disagree in a few places. This section
gives one owner per shared system and one answer per disagreement. A slice brief follows this section over
its scope file.

### Shared systems: one owner each

| System | Owner (cards) | Rule for everyone else |
|---|---|---|
| Professions and XP (`LF_JobLevels`) | Work (WK-W1 to W6, W8, W11) | Police and medic skills (PO-R8, PO-S5) use the same track type, curve and level-up card. **XP comes from outcomes, never from time on duty** (the police rule wins over the work table's "patrol minutes"), because shift pay already pays for time. Crime has no skill line: it progresses by record, standing and equipment |
| Level-up moment and F2 skill card | Work (WK-W3, W6) | One sting, one card layout, for every trade including police and medic |
| Standing | Existing `LF_Reputation` | One underworld track for crime, gangs and the outposts. Tier 1 is about 2 h of crime, tier 2 about 8 h, tier 3 about 25 h (gangs numbers). A per-outpost "known here" flag (GA-O4) sits on top, so no player grinds twice. Standing is never shown as a number |
| Law book and charge sheet | Police (PO-L1 to L10) | Crime only tags goods (CR-F1, classes A ≤ 2, B ≤ 10, C > 10 units) and counts them. Gangs add no charges. The work scope's "overloading" and "flying without a licence" charges go in as code rows (L2), not new systems |
| Robbery police minimums | Pack config, one table (below) | All scopes read the same rows |
| Big-job lock | Crime (CR-A6) | Rungs 4 to 6 and, in 0.4, the prison break share one server-wide lock |
| Liveries | World (WO-L1 to L6, L9, L10) | **Police R2 folds into WO-L4** (police S105, UAZ469 and the S1203 transport van). **Police S2 folds into WO-L5** (ambulance on the vanilla UAZ452 ambulance and the S1203; the M997 is dropped as a US military body). Full repaints of the body in 0.3.0, lettering overlays on top where they hold, a credit row per derived texture (decision N30, changed 2026-11-22) |
| Newspaper and rumors | World (WO-W2, W5) | The crime scope's "newspaper crime column" is the *Courier*. Crime places never go on the public map: one rumor pool with a crime flag, given only by bar staff and the existing informant to players the town knows (`friendly` or better). It prints only public facts: no open case, no hidden record |
| NPC cast | World (WO-W4) for behaviour, outfits and voices | One NPC per role. Bruno and the other foremen are WO-W4.4 (the work scope names them). The driving examiner is WK-W21, and the police licence retake (PO-S3) reuses him. The desk sergeant's stand-in behaviour is WO-W4.8; his booking lines are PO-L9. The fishmonger and the harbour master are WO-W4.2 |
| Places | The town pipeline, one card per place | New places in 0.3.0: the truck yard (north-east compound, 0.6 km), the labour office counter (town hall), the container yard on the dock, the Chotain and Levie bars, the power plant coal buyer, the north outpost. Everything else uses places already fitted in `Place_*.layer` |
| Radios | Comms | Gangs use a pack copy of the R148 (`LF_GangRadio`). Police keep the ANPRC68. Two items, no shared frequency code beyond `Feature/Comms` |
| Vehicles and capacity | Work (WK-W16) | The capacity table is the one source for the car, van and truck numbers in the crime scope (a car carries one crate, a van four, a truck a pallet) |
| F2 player menu tabs | Three cards: WK-W6 (skills), GA-C13 (gang), PO-P10 (my file) | Each tab in its own file. If tabs register in one shared file, the three run one after another (Day 4, 4 and 5), never at once |
| `tools\cli economy` | Work W11 (perks), police D6 (duty with bonuses), X-2 (crime and gang lines) | Three section files, one command |

### Disagreements resolved

| # | Where they differ | Resolution |
|---|---|---|
| 1 | Visible levels: M6 decision 7 says none; work K1 shows numbers; crime wants none | Legal trades show their level in F2, on the level-up card and on the shift boards. Gates still refuse in words. Crime and standing never show a number (decision N1) |
| 2 | All five scopes, the M6 and M7 outlines and today's configs keep duty at 1.5 to 2.5x civilian pay, and the work scope caps a master at 40% over a novice to stay under it | **Superseded by the maintainer (2026-11-22): "Civilians on the high tier jobs should get more than cops, cops should be the lowest paid."** The band is gone. Police and medic duty pay the least steady money, about entry civilian level. Civilian trades climb above duty with skill and tier, and the 40% master cap is dropped. Crime stays high-swing on top. The ladder is in "The economy table" below; card X-8 retunes the live configs and the `economy` check |
| 3 | Police outcome bonus caps ($150 to $350) were sized against the old band | Caps are sized so shift plus bonus never passes a mid-tier civilian loop ($520) |
| 4 | Medic pay sat beside police in the old band | **Changed by the maintainer (2026-11-22 late):** medics earn a bit more than police, on the mid-tier civilian rung ($480 to $600 with the bonus), still below the high-tier trades |
| 5 | Police per robbery rung: police scope 0/1/2/3/4; M7 ladder 1/1/2/2/3/5 | One table below. The till needs nobody (its alarm still calls), so the first crime is never locked |
| 6 | Jail cap: the law book allows 60 min, police wants 30 min in a prison, today's cap is 5 min | 0.3.0 has no prison. Sentences top out at **15 online minutes in the Saint-Philippe holding cell**, with work stations scaled to match (PO-X1). The prison's 30 min with a 10 min split comes with 0.4 |
| 7 | Recapture bonus $300 (gangs) or $150 (police); recaptured escapees get 20 min or 1 h | Prison break is 0.4. Recorded for then: $50 per escapee per officer, inside the hourly bonus cap of the new duty pay, 20 min and a 24 h bar (decision N41) |
| 8 | Break needs a jailed member with 10+ min (gangs) or 25+ min (police, wishlist) | 0.4. 15 min fits the 15 min cell cap in 0.3.0 and the 30 min prison in 0.4 (decision N41) |
| 9 | Smelting XP: #3239 gives MINER XP per batch, WK-W2 adds a SMELTING track | SMELTING track (decision N3); #3239 is retargeted to it |
| 10 | Truck yard: Montignac or the dock (M6 T2), north-east compound (work Q6) | North-east compound: reachable before Montignac is live |
| 11 | Fines to the officer or the town | The town treasury, everywhere (police P4). An officer earns only shift pay and capped bonuses |
| 12 | Crime pay was set as "1.5 to 2.5x civilian", the same band as duty | Crime sits on top of the civilian trade at the same stage, as expected clean value after the wash, seizures and failed runs, with much more swing. Tier 1 (moonshine) is the on-ramp at about entry civilian pay |
| 13 | Tip line, front business, prison contraband, protection money: proposed by two or three scopes each | One design each, all 0.4 |
| 14 | Hire for the shift (work A3) and scroll to recruit (GA-C5) | Both are actions on another character. C5 builds the pattern in 0.3.0; hire for the shift reuses it later |

### Robbery police minimums (pack config)

| Rung | Target | Police on duty | Crew | Cooldown | Ships |
|---|---|---|---|---|---|
| 1 | Shop or fuel till | 0 (the alarm still calls) | 1 | 20 min per till | 0.3.0 (exists, checked by CR-A4) |
| 2 | Store safe | 1 | 1 to 2 | 45 min | 0.3.0 (CR-A3, #2282) |
| 3 | Cash van | 1 | 2 to 3 | 90 min | 0.3.0 (CR-A4, #3398) |
| 4 | Hospital pharmacy | 1 medic or 2 police | 2 | 2 h, big-job lock | 0.3.0 (CR-D1 to D3) |
| 5 | Bank vault | 2 | 3 to 4 | 3 h, big-job lock | 0.3.0 (CR-A1, A5) |
| 6 | Reserve depot | 4 | 4 to 6 | 6 h, big-job lock | 0.4 |

### The economy table

**The ladder (maintainer rule, 2026-11-22).** Police duty is the lowest steady pay, about entry civilian level.
Medic duty sits a little higher, on the mid-tier civilian rung. Outcome bonuses never lift police above the
mid-tier floor or medics above the high-tier floor. Civilian trades climb above
duty with skill, tools and vehicle tier: a high-tier or master loop (haulage by truck, export orders, gold)
clearly out-earns an officer. Crime sits on top with the most swing. Duty is still worth doing for the uniform,
the car, the powers, the kit, the police and medic skills and the play itself, not for the money. Medics earn a
little more than police (maintainer, 2026-11-22 late) because their shift is mostly waiting for a call.

Reference run: `tools\cli economy` on 2026-11-22 (mission `LF_Everon1989`, 135 overlays), re-run after the
rule change. The run still prints the old band check: duty at $900 to $1,260 and every loop flagged "in band
(1.5 to 2.5)". **Card X-8** retunes the live pay and replaces that check with this ladder. WK-W11 adds the
full-perks line, PO-D6 duty with bonuses, X-2 the crime and gang lines, and WK-W36 tunes the civilian rows.
After them `economy` flags any row that leaves its rung.

**The rungs ($/h, steady):**

| Rung | Who | Target | Rule `economy` enforces |
|---|---|---|---|
| Police duty | Police, every rank, bonus included | $350 to $520 | Shift pay at or under the entry civilian rung ($350 to $450); shift plus the bonus cap at or under the mid-tier floor ($520) |
| Medic duty | Medic, every rank, bonus included | $480 to $600 | Shift pay on the mid-tier rung ($480 to $560); shift plus the bonus cap at or under the high-tier floor ($600) |
| Entry civilian | Levels 1 to 5, on foot or a starter car | $400 to $480 | At or above the Cadet shift |
| Mid-tier civilian | Levels 5 to 12, car or van | $480 to $600 | Above every police rank's shift |
| High-tier civilian | Levels 12 to 20, van or truck, export orders | $600 to $800 | At or above every duty rank with its full bonus |
| Master civilian | Levels 20 to 30 with perks: gold, the truck haulage runs, the big export orders | $800 to $1,000 | About twice the novice loop in the same trade |
| Crime | Above the civilian rung at the same stage, high swing | see below | Expected clean value at or above the matching civilian rung |

**Civilian, measured now (steady $/h):**

| Loop | $/h now | Rung | Change in 0.3.0 |
|---|---|---|---|
| Parcel | $400 | Entry | None |
| Postal | $480 | Entry | None |
| Iron on foot, Coalman's Brow | $471 | Entry | None |
| Coal feeder, East Rise | $385 | Entry | Power plant buyer (WK-A1) lifts it to about $450 |
| Wheat to the mill and bakery | $476 | Entry | None |
| Softwood by hand | $398 to $410 | Entry | None |
| Fuel | $563 | Mid | None |
| Iron by pickup, Highstone to the smelter | $515 (first hour $560) | Mid | None |
| Taxi | $600 | Mid | None |
| Harbour contracts (standing) | $520 to $560 | Mid | None |
| Copper by car | $514 (first hour $701) | Mid, stretch | Rises with Smelting perks toward $650 |
| Stone by truck | $460 | Should be high | WK-W36 lifts truck loops to $600 to $700: the truck is the climb |
| Hardwood by truck | $365 | Should be high | WK-W36 as above; Hornbeam Valley (stretch) |
| Export orders (WK-W24) | new | High | Target $650 to $800, +15% over wholesale plus the order bonus |
| Stone on foot | $8 | Never | Intended: "you'll want a truck" |

With the 40% master cap gone, the perks rise too (decision N2): double yield up to 20%, buyer price up to
+15% at premium buyers, speed 20%, wear -40%, one carry step. A maxed perk loop lands on the master rung.

**Duty (retuned by X-8; "typical" is half the bonus cap):**

| Rank | Shift now | Shift in 0.3.0 | `m_fPayPerSecond` | Bonus cap | Most per hour | Typical |
|---|---|---|---|---|---|---|
| Police Cadet (patrol) | $900 | $350 | 0.0972 | $100 | $450 | $400 |
| Police Patrol (senior) | $1,008 | $375 | 0.1042 | $100 | $475 | $425 |
| Police Senior Patrol | $1,080 | $400 | 0.1111 | $100 | $500 | $450 |
| Police Sergeant | $1,188 | $425 | 0.1181 | $95 | $520 | $473 |
| Police Lieutenant | $1,260 | $450 | 0.1250 | $70 | $520 | $485 |
| Medic Orderly | $792 | $480 | 0.1333 | $40 | $520 | $500 |
| Medic EMT | $936 | $500 | 0.1389 | $50 | $550 | $525 |
| Medic Paramedic | $1,080 | $530 | 0.1472 | $50 | $580 | $555 |
| Medic Doctor | $1,224 | $560 | 0.1556 | $40 | $600 | $580 |

Bonuses: a call resolved $15, a booking $10, a stolen car recovered $20; a revive $10 and a delivered patient
$15. All are paid with the shift at clock-out and count against the cap. Fines and bail go to the town
treasury, never to the officer. The rank fine caps ($500 patrol to $5,000 lieutenant) don't change: a fine is
a sink, not duty pay.

**Crime (expected clean $/h per head, after the 35% wash, inputs, wear, seizures and failed runs):**

| Activity | Tier | Expected clean | Above | Good night | Ships |
|---|---|---|---|---|---|
| Moonshine | 1 | $500 to $650 | Entry civilian | $1,100 | Built |
| Chop shop (pays clean) | 1 | $600 to $900 | Mid civilian | $900 | Built |
| Cigarette and vodka run | 2 | $750 to $950 | Mid to high civilian | $1,500 | 0.3.0 |
| Till and store safe | rungs 1, 2 | $700 to $1,000 averaged over cooldowns | Mid to high civilian | $1,200 | 0.3.0 |
| Cash van, pharmacy, bank, as a crew evening | rungs 3 to 5 | $1,000 to $1,500 | Master civilian | $2,400 | 0.3.0 |
| Gang member across a night (chains, the outpost buyer, a heist share) | all | $900 to $1,500 | High to master civilian | $2,400; a bad night costs 1 to 3 hours | 0.3.0 |

**Fines as minutes of work** (at an entry wage of about $8 a minute): minor road $50 to $75 (6 to 9 min),
road $150 to $250 (20 to 30 min), serious road $300 to $400 (40 to 50 min), property $200 to $600 or 5 to
10 min in the cell; violent charges and robbery are jail only (10 to 15 min in 0.3.0). Bail stays $150 a
minute left, with a $500 minimum.

**Sinks that ship:** tool tiers ($150, $600, $1,800) and repairs ($12 and a scrap), the van, the truck licence
($300), the respray ($150) and liveries ($250 to $400), the *Courier* ($1), gang founding ($250), radios
($60), outpost weapons ($400 to $600 for a pistol at tier 1), the wash, fines and bail, seizures.

---

## 3. The cut

**The rule.** A card is must-have when the 10-hour arc needs it, or when the arc has a visible hole without
it (no sound, no sign, a menu where the world should answer). Everything a player meets after hour 10, or that
needs a new walled site, a new save-heavy system or a policy check, waits for 0.4. Quality first: about a
fifth of the week's capacity is kept for fix cards from the daily playtest rounds.

### Must-have (162)

**1. A trade of your own and 2. Bigger wheels: 32**

| Cards | What | Day |
|---|---|---|
| WK-W1, W2, W3, W4, W5, W6 | 30-level tables, SMELTING and SALVAGE tracks, the level-up moment, an unlock per level, first-shift double XP, the F2 detail card | 1 to 4 |
| WK-W7, W9, W10, W11, W12 | Tool tiers, condition and wear; the hardware bench; the three finishes; passive perks and the full-perks economy line; tiers on sale | 1 to 4 |
| WK-W8, W8b, W13 | Perk store as a save section, the pick at levels 10 and 20, the labour office counter | 2 to 5 |
| WK-W22, W23 | Weekly shift boards; the shift whistle and site ambience | 1, 5 |
| WK-W16, W17, W18, W19, W20, W21 | Capacity table and validator, Feed from the truck, the truck check, the truck yard, the truck catalogue and lot move, the salesman and examiner | 1 to 5 |
| WK-W24 | Export order board at the dock | 3 |
| WK-W35, W36 | The manual and the F3 Trades page; the economy tuning pass | 5 |
| WK-#3242, #3243, #3244, #3245 (filed) | Highstone iron, East Rise coal, the centre-east smelter, the iron chain end to end | 1 to 3 |
| WK-A1 (2 cards, suggested) | The power plant coal buyer: rows, then the place | 3, 5 |
| WK-A10 (suggested) | Apprentice bonus: a master working beside someone under level 10 gets a little XP, and so does the apprentice | 4 |

**3. By the book: 29**

| Cards | What | Day |
|---|---|---|
| PO-L1 to L10 | Law codes, the pack law book, charge sheet rules, the paper screen, the booking desk and offer, pay now, licence seizure, the lectern, booking sounds and lines, the station placement | 1 to 5 |
| PO-D1 to D6 | Call outcomes, the 911 abuse rule, unit status, capped bonuses, the dispatcher voice, duty with bonuses in `economy` | 1 to 3 |
| PO-R1, R4, R5, R8, R9, R10 | Rank cars, the radar gun, US English speed signs and lay-bys, the police skill, the rank board and plaque, the placement | 1 to 5 |
| PO-S1, S3, S5 | Medic bonuses and the ambulance in the fleet, the licence retake, the medic skill | 4, 5 |
| PO-P10 | F2 "My file" | 5 |
| PO-hotlist (2 cards, suggested) | Stolen-car reports carry the plate to the pager; "Run plate" on the car in front | 2, 3 |
| PO-X1 (new) | Sentences up to 15 online minutes in the holding cell; work stations scale to match | 1 |

**4. Smoke and bottles: 20**

| Cards | What | Day |
|---|---|---|
| CR-A1 to A6 | The vault drill (#4156, dropped if 0.2.1 fixes it), the robbery kit, store safes, the cash van route, the bank job proof, the big-job lock | 1 to 4 |
| CR-B1 to B9 | Buyer demand caps, carton and bottle items, the short container and Marcel, the barkeeper outlet and Josette, the stop search for goods, voices, the container yard and two bars | 1 to 4 |
| CR-D1, D2, D3, D8 | Clinic shortage, the presence rule, cabinet loot as goods, the notice on the clinic door | 3, 4 |
| CR-F1 | Narcotics amount class on goods, so the charge sheet can class pharmacy stock | 2 |

**5. Running a crew: 27**

| Cards | What | Day |
|---|---|---|
| GA-C1 to C15 | 20 preset flags, the flag saved and locked, the flag wall, recruiting from the action list (and retiring the old menu entry), the gang frequency and radio, live map marks and their rules, flags on poles, the F2 Gang tab, voices, the registry placement | 1 to 5 |
| GA-O1 to O7, O9, O11, O12, O13 | The north outpost: safe zone, the quartermaster's crate by tier, the chalk board, "known here", the contraband buyer, stock tables, scout, dressing, NPCs, ambience, placement | 1 to 5 |
| GA-A2 (suggested) | Prospect rank: no fund or stash rights for 48 h | 3 |

**6. The island talks: 38**

| Cards | What | Day |
|---|---|---|
| WO-W1.1 to W1.6 | Ambience beds, birds, the square cast and voices, the US English sign check, the placement | 1 to 4 |
| WO-W2.1 to W2.7 | The *Courier* item, the weekly edition builder and its save section, the newsagent and the road map, opening-hours plates, slips, the placement | 1 to 5 |
| WO-L1 to L7, L9, L10 | Livery spike (full repaint, tinted clothing and overlays), the `livery build` step that writes repainted variants with their credit rows, livery sign styles, police (with the old police R2), ambulance (with the old police S2), company and rebel repaints, the respray bay, liveries kept across a restart | 1 to 5 |
| WO-W4.1, W4.2, W4.4, W4.5, W4.7, W4.8, W4.9 | The work loop, the dock cast, foremen with "what's next", the shop cast, their voices, front-desk stand-ins, the placement | 1 to 5 |
| WO-W5.1 to W5.4 | The rumor pool, "Heard anything?", "Sit and listen", the 1989 rumor rows | 1 to 4 |
| WO-W6.1, W6.2, W6.5 | The island calendar, bells and the ship's horn, the placement (W6.5 now depends on W6.2 only) | 2 to 5 |
| WO-roadsigns (2 cards, suggested) | US English road and town-entry signs with distances: rows, then placement | 4, 5 |

**7. Playtest town parity: 9 (new, maintainer ask 2026-11-22)**

The playtest town is the DebugWorld village layers plus the pack world `Everon1989Town`. The maintainer built it
and normally edits it by hand ([world zones](../architecture/world-zones.md)); for this chapter he asked agents
to bring it up to date, so each card edits his layers **with his explicit approval** (each card says so). Each card
runs in a `town-<N>` worktree, places with `tools\cli place` and the town kit tools, and passes
`audit placement --strict` with `look` pictures before and after. Systems come over as they are on the island: no
new features here.

| Card | What | Day |
|---|---|---|
| TP-0 | The gap list: every island system the playtest town lacks, by site group, with the island source layer or kit and the town spot (`docs/roadmap/playtest-town-parity.md`). It may split or merge the groups below | 1 |
| TP-1 | ATM kiosks, bus stops and their boards | 2 |
| TP-2 | The onboarding route: the clerk's welcome pack, the starter car, orchard, press and market | 2 |
| TP-3 | Mining: the desk, the rocks, the furnace and the buyer | 2 |
| TP-4 | The postal run | 2 |
| TP-5 | Clothing mannequins and the shop counters | 2 |
| TP-6 | Map POIs, run chains, the welcome guide and HUD rows for the town | 2 |
| TP-7 | Jail doors and the bail phone | 2 |
| TP-8 | Parity check: the X-9 round walks hours 0 to 2 in the playtest town and the gap list is empty or filed | 3 |

**Integration: 8 more (X-1 is counted under police)**

| Card | What | Day |
|---|---|---|
| X-2 | `tools\cli economy` prints the crime and gang lines of the table above | 5 |
| X-3 | F3 guide pages: "Law and order", "The underworld", "Crews" | 5 |
| X-4 | F2 "What to do next" starters: police and medic duty, the first rumor of crime (only after a record), founding a crew | 5 |
| X-9 | **Daily agent playtest round** (recurring, one card a day, Days 1 to 5): at the end of the day, two fresh clients on a dedicated server on `main` play the 10-hour arc as far as it is built, timed against section 1, and write a rough-edge report with pictures in the shape of the first-two-hours review (graded rows, top ten, counts). Each failed or rough row becomes a fix card for the next morning | 1 to 5 |
| X-5 | Mid-week full run, hours 0 to 6 (Day 4; that day's X-9 round) | 4 |
| X-6 | End-of-week full run, hours 0 to 10+ (Day 6) | 6 |
| X-7 | Release notes, manual, version bump, release checklist | 6, 7 |
| X-8 | **Retune live pay to the new ladder** (data, T1, opus, risk money, interrogate). Police and medic `m_fPayPerSecond` in `LF_Police_Everon1989.conf` and `LF_Medic_Everon1989.conf` to the "Shift in 0.3.0" column (medics on the mid-tier rung, 2026-11-22 late); the bonus caps handed to PO-D4 as config rows; `tools/lib/economy-{mining,lumber,wheat}.mjs` and `tools/commands/economy.mjs` replace the 1.5 to 2.5 band with the rung checks; `server/flagship/README.md` and any doc that states the old band. Acceptance: `tools\cli economy` prints police duty at $350 to $450 and medic duty at $480 to $560, and flags a police rank above the entry rung, a medic rank off the mid-tier rung or over $600 with its bonus, or a civilian loop below its rung; the economy selftests pass | 1 |

Each chapter's town card is in the lists above (WK-W13, WK-W19, PO-L10, PO-R10, CR-A3, CR-B9, GA-C15, GA-O13,
WO-W1.6, WO-W2.7, WO-W4.9, WO-W6.5, WK-A1 place). Chapter 7's TP cards are town cards for the playtest town.

### Stretch (47, pulled in this order when ready falls below twice the running count)

1. GA-O8, O10 and a south placement card: the south outpost (3)
2. WK-W14: active abilities with the HUD chip (1)
3. WK-W25, W26: Salvage at the landfill (2)
4. WK-W28, W30: Hornbeam Valley; the north-east farm and Étoupe orchard (2)
5. PO-R6, R7: tow and the impound lot (2)
6. PO-R3: rank patches (1)
7. CR-F2: the buyer as informant (1)
8. WO-W6.3, W6.4: market day and the Friday dance, tied to the real Saturday session (2)
9. WO-W4.3, W4.6: the Montignac cast; tinted aprons and work jackets, by the repaint method the spike proves (2)
10. The outpost mechanic's paint job (WO-L7 itself is must-have since 2026-11-22) (0)
11. WO-W7.1, W7.2 (two places): the style bible and look passes (3)
12. WK-W15: titles (1)
13. The copper and stone chains from #3206: #3247, #3248, #3253, #3250, #3251, #3254, #3235 (7)
14. Crime suggestions: car to order, smuggled jeans and radios, the heist mask (Meshy) (3)
15. Police suggestions: the breathalyzer and drunk driving (2), community service (2), wanted posters in
    other towns (1), PO-S4 road check (1), PO-S6 the Meaux post and the ambulance bay (1) (7)
16. Gang suggestions: the most wanted gangs board (1), pirate radio (2) (3)
17. World suggestions: player notices on the town board (2), the local radio station (3), lost and found (2) (7)

### Later: 0.4 Power and turf and beyond

| What | Cards | Why not now |
|---|---|---|
| The Correctional Center and prison life | PO-P1 to P9, P11 | A new walled site through the town pipeline plus four persistence and replication cards. A first-week player sees at most a 15 min cell |
| The prison break as a gang event (wishlist 12) | PO-B1 to B6, GA-P1 to P12 | Needs the prison, three officers online and a gang with a jailed member: a server event for week three, not hour ten. It is the headline of 0.4 |
| Hideouts with perks, turf poles | GA-H1 to H10, T1 to T6 | Hideouts are where the drug chains process, so they go with them. Turf needs several gangs to matter |
| Drug chains, landings, informants, the reserve | CR-C1 to C7, D4 to D7, E1 to E7, F2 to F4, G1 to G9 | They need the hideouts, the policy check on named drugs (N37, which starts this week) and the prison for the stakes |
| Pilot licence, helicopter dealer, airport | WK-W31 to W34, WO-L8 | A $15,000 helicopter is about 30 hours of work, well past the arc |
| Gold, Pinewood Lake, mastery outfits | WK-W27, W29, W15b | Level 15 to 30 content |
| The other suggestions | Work A2 to A9, crime, gangs and police suggestions not in stretch | Reviewed after 0.3.0 ships, against the ledger and the playtests |

---

## 4. The week plan

### Capacity

- **Agents.** About eight at once, on the Gaming account, started from the laptop thread. Opus takes the 30
  risk cards (marked R in the day table) and the livery spike; Sonnet takes the rest; Haiku takes the voice-manifest and
  doc cards. The slice ledger's median is 25 minutes a card with 28% rework (`tools\cli slice stats`,
  685 slices), so eight agents can merge about 30 cards a day and keep room for fixes.
- **Pool (lf-testbox).** 8 test lanes, 4 session lanes, 3 Workbench lanes, 47 GB RAM and a 71 GB commit
  limit, with the CPU capped at 50% for the fans. Tests never limit the week. These do:
  - **Two-player sessions:** a client is about 6.4 GB of commit, so at most two two-player sessions run at
    once. They are spread over the days below.
  - **Workbench (3):** every `look` picture and every town card. There are 14 town cards, at most three at
    once, and two town cards never edit the same place layer at once (PO-L10 before PO-R10 in the police
    villa).
  - **Render targets:** Workbench draws no sign face. Liveries, the *Courier* and the charge sheet need a
    full-profile session picture, which is session time, not a `look`.
- **Main guard.** It compiles each new head first and reverts a red compile. With about 30 merges a day, a
  revert costs one card's rework, not a day.

### Before Day 1 (Sunday evening, 2026-11-22)

- The maintainer reads this plan and answers the "before Day 1" decisions (N1 to N9), or lets the defaults
  stand.
- The lead keeps 0.2.1 moving on `release-0.2.1`. Its fix cards always go first in the pool.

### Day by day

| Day | Date | Headline | Starts (must-have) | Gates and proofs | Merged by night |
|---|---|---|---|---|---|
| 1 | Mon 11-23 | **Foundations**: the planner cuts all seven chapters by 09:00; every card with no dependency starts | 33: WK-W1, W7, W16, W18, W23, #3242, #3243; PO-L1, D1, D3, R1, R9, X1; CR-A1 (R), A4, A6 (R), B1 (R), B3 (R), B7 (R); GA-C2 (R), C5 (R), C7 (R), C10 (R), O1, O7; WO-L1 (spike), W1.1, W1.3, W2.1, W4.1, W5.1; X-8 (R); TP-0 | Livery spike verdict by night (go or the fallback, below). Two-player session proofs: GA-C5, C10. **X-9 round 1** (hours 0 to 2 on the island) | about 25 |
| 2 | Tue 11-24 | **Second layer**: the systems the content hangs on | 39: WK-W2, W4, W8 (R), W9 (R), W10, W17 (R), #3244; PO-L2, L3 (R), D4 (R), R4, hotlist-1; CR-A2, B2, B4, B5 (R), F1; GA-C1, C3, C8, C11, O2 (R), O4 (R), O5 (R); WO-W1.2, W1.5, W2.2 (R), W2.5, W4.4, W5.2, W6.1, L2; TP-1 to TP-7 (town, at most three at once on the Workbench lanes) | Interrogate on W8, W17, L3, B1, B3, B5, C2, C7, O2, O4, O5. Two-player: GA-C8. **X-9 round 2**, round 1's fix cards first | about 30 |
| 3 | Wed 11-25 | **Content on the systems**: levels feel right, the law book binds, the first places fill | 33: WK-W3, W5, W11, W20, W24 (R), #3245, A1 rows; PO-L5 (R), D2, D5, D6, R8, hotlist-2; CR-A3 (town), B6, D1 (R), D2, D3; GA-C4, C6, C9, C12, O3, O6, O11, A2; WO-W1.4, W2.3 (R), W2.6, W4.2, W5.3, L3; TP-8 | Interrogate on L5 (money and replication), D4, W24, D1, W2.3. `wt e2e` on W8, C2, O4, W2.3. Level-up card session picture. **X-9 round 3** (hours 0 to 4, and 0 to 2 in the playtest town) | about 30 |
| 4 | Thu 11-26 | **Into the world, and the mid-week full run** | 32: WK-W6, W12, W19 (town), W8b (R), A10; PO-L4, L6 (R), L7, L8, L9, S1 (R), S5, R5; CR-A5 (two players), B8, B9 (town), D8; GA-C13, C14, O9, O12; WO-W1.6 (town), W2.4, W4.5, W5.4, W6.2, L4, L5, L6, L7, L9 (R), roadsigns rows | **X-5 mid-week full run** (Day 4's X-9 round) from 14:00: two clients, hours 0 to 6 of the arc on `main` from checkpoint saves; each failed beat becomes a fix card at once. Charge sheet session picture (PO-L4), livery pictures (full profile) | about 30, plus the first fix cards |
| 5 | Fri 11-27 | **Close the chapters**: town passes, guides, the economy pass, fixes | 22 plus fix cards: WK-W13 (town), W21, W22, W35, W36, A1 place; PO-L10 (town), R10 (town, after L10), S3 (R), P10; GA-C15 (town), O13 (town); WO-W2.7 (town), W4.7, W4.8, W4.9 (town), W6.5 (town), L10 (R), roadsigns place; X-2, X-3, X-4 | **X-9 round 5** (hours 0 to 10 as built) at 18:00, its fix cards before the cutoff. `tools\cli economy` shows no flag except stone on foot (the X-8 ladder: duty lowest, every civilian loop on its rung). `audit placement --strict` clean for every place touched. **Must-have merge cutoff 22:00**; stretch runs only while fix cards are clear | all must-have |
| 6 | Sat 11-28 | **Release candidate** | Fix cards only | **X-6 end-of-week full run** 08:00 to 13:00 (all ten hours from checkpoint saves, two clients, a dedicated server, one restart in the middle). Cut `release-0.3.0` from the newest `green` main-guard head at 15:00. The lead may put the RC on the 0grind alpha (wipe allowed) for the **19:00 UK community session**, which is the real playtest of hours 0 to 4 | RC on the alpha |
| 7 | Sun 11-29 | **Release** | Fixes from the session, built with `--base release-0.3.0`, then `wt sync-base main --source release-0.3.0` | The release checklist below. The maintainer publishes from `release-0.3.0` (or the lead, under its sign-off authority) | 0.3.0 public |

### The critical paths

Four chains each run five days. Their first cards start at 06:00 on Day 1 and go first in the queue
(`slice list --ready` already sorts by critical path).

| Chain | Cards | Risk and fallback |
|---|---|---|
| Liveries | WO-L1 → L2 → L3 → L4, L5, L6, L7 → PO-R10, WO-L10 | The spike must show a full body repaint that renders (not white) on a moving vehicle in a full-profile session, and a tinted vanilla garment. Fallback the same night: vanilla colour variants plus the light bars for police and ambulance, the repaints in 0.3.1. R10 and the rank cars do not wait for it |
| Law book | PO-L1 → L3 → L5 → L6, L7 → S3, P10, L10 | L5 is Opus with an independent reviewer. If interrogate finds a money hole, L6 waits and the booking ships "to the cell" only |
| The *Courier* | WO-W2.1 → W2.2 → W2.3 → W2.4 → W2.7 | If the save section slips, the *Courier* prints the current week from live records, and its back issues are lost on a restart |
| Crew | GA-C2 → C3 → C4 → C14 → C15 | If the flag wall slips, founding takes the first free flag, and choosing one waits |

### Quality gates

| Gate | Applies to | When |
|---|---|---|
| Acceptance first, seen red | Every card | Before code (`--red` batched for new tests) |
| Targeted `--only`, then `--tier all` once | T2 cards | Before ship |
| Interrogate (`lf-hard` reviews the diff, findings in `tmp/review-<slug>.md`) | The 30 risk cards (money, RPC, persistence, inventory): X-8; WK-W8, W8b, W9, W17, W24; PO-L3, L5, L6, D4, S1, S3; CR-A1, A6, B1, B3, B5, B7, D1; GA-C2, C5, C7, C10, O2, O4, O5; WO-W2.2, W2.3, L9, L10 | Before merge |
| `wt e2e` two-boot proof | WK-W8, CR-A6, GA-C2, GA-O4, WO-W2.3, WO-L10 | Before merge |
| `look` before and after, the polish checklist, the pair in the PR | Every visible card; town cards at the full polish bar | Ship |
| Session proof (full profile) | HUD and UI (WK-W3, W6, PO-L4, P10, GA-C13), repaints and render-target lettering (WO-L4 to L7, W2.1), two players (GA-C5, C8, C10, CR-A5), audio (PO-D5, WO-W1.1, W6.2) | Ship |
| `audit placement --strict` | Every town card; the island gate on `wt ship` | Ship |
| US English | Every sign, livery and voice card; WO-W1.5 makes it a check | Ship |
| No pop-ups | Every world interaction is an action on the object; the only new full screens are the charge sheet and the *Courier* (paper) and the F2 tabs | Review of each brief |
| Pay ladder | X-8 (Day 1), WK-W11, W36, PO-D6, X-2 | Day 5 |
| Playtest rounds | X-9 every evening, Days 1 to 5 (the arc as far as it is built, timed, a graded rough-edge report with pictures); X-5 (hours 0 to 6, Thursday) and X-6 (hours 0 to 10+, Saturday) as the full runs; the community session (Saturday 19:00) | Each failed or rough row is a fix card for the next morning; fix cards take a lane before new cards |

**How a playtest round runs.** Two fresh clients on a dedicated server on `main`, following section 1's table.
The daily X-9 round covers the hours built so far and writes `docs/playtest/rounds/<date>.md` in the shape of the
first-two-hours review: setup, method limits, a summary table with session and estimated real-player times, the
top ten issues, graded rows (blocker, major, minor, polish) with pictures, and counts. Each
hour starts from a checkpoint save that grants that hour's money, levels and standing, so ten hours of play
take about five hours of agent time. Each beat is pass or fail, with a picture or a log line. A beat fails if
the player is lost (no marker, no person to ask), sees a French word, meets a pop-up where an object should
answer, waits more than 15 minutes for a reward, or hits a bug. One integration agent runs each round
and files the failures as cards. It fixes nothing itself.

### Release checklist (Day 7)

- [ ] Every must-have card merged and `in-dev`; stretch cards merged or left on `main` behind the release
- [ ] Main guard `green` on the `release-0.3.0` head: compile, build, full suite, placement audit, e2e
- [ ] `tools\cli economy`: no flag except stone on foot; duty, civilian and crime lines on their rungs (duty lowest, master trades and crime on top)
- [ ] `audit placement --world MainWorld --baseline` clean
- [ ] X-6 full run: every hour passes; open failures are fixed or listed as known issues
- [ ] Playtest town parity: TP-8 passed, hours 0 to 2 play in the playtest town as on the island
- [ ] Community session findings triaged: blockers fixed on `release-0.3.0`, the rest filed
- [ ] A save from 0.2.1 loads on 0.3.0 (new sections take their `OnAbsent` path), or the wipe is announced
- [ ] US English check green; no `EL_` identifiers; no other server's text or name anywhere
- [ ] Release notes in `docs/releases/`, the player manual and the F3 pages updated, the website rebuilt
- [ ] `tools\cli playtest checklist` written for the maintainer's sign-off of the six chapters
- [ ] Workshop publish of the core and the pack from `release-0.3.0`; the 0grind server updated with 0
      players, smoke-tested, the Discord post out
- [ ] `wt sync-base main --source release-0.3.0` so `main` keeps every release fix

---

## 5. Decisions for the maintainer

Every open question from the five scopes, the M6 and M7 outlines that the scopes repeat, and this plan, with
duplicates merged. "Accept all" takes every recommendation. With no answer, the default holds, and in every
row but N41 the default is the recommendation.

**Decided, not asked.** The pay ladder (maintainer, 2026-11-22): police are the lowest paid, medics a little more
(mid-tier civilian); high-tier civilian trades out-earn both; crime is high-swing on top. The old band questions (the reference wage, the
Orderly raise, the crime band) are dropped.

### Before Day 1 (they change what gets built)

| # | Decision | Recommendation | Default | Was |
|---|---|---|---|---|
| N1 | Show levels? | Legal trades show their level in F2, on the level-up card and on the shift boards. Gates refuse in words. Crime and standing never show a number | As recommended | work Q1, #4140 K1, crime Q3, M6 decision 7 |
| N2 | Curve and perks | 30 levels per trade, the first five in the first two hours. With the 40% master cap gone, perks may lift a master loop to $800 to $1,000: double yield up to 20%, buyer price up to +15% at premium buyers, speed 20%, wear -40%, one carry step | As recommended | #4140 K2, K3, work balance, the 2026-11-22 pay rule |
| N3 | Separate SMELTING and SALVAGE tracks | Yes; #3239 moves to SMELTING | As recommended | work Q3 |
| N4 | Police and medic progression | Kit, titles and liveries, never pay. XP from outcomes only. Rank stays the whitelist ladder | As recommended | #4140 K4, police systems |
| N5 | Jail in 0.3.0 | Up to 15 online minutes in the Saint-Philippe holding cell, work stations scaled to it. Bail unchanged | As recommended | police P2, P3, law book cap |
| N6 | Fines | To the town treasury. Retune today's tickets (speeding $200, reckless $500, crash $1,500) to the law book table | As recommended | police P4, #4151 B1 |
| N7 | Who signs the charge sheet | Any on-duty officer; "jail only" override from Sergeant, logged | As recommended | #4151 B2, police P6 |
| N8 | Police per robbery rung | Till 0, store safe 1, cash van 1, pharmacy 1 medic or 2 police, bank 2, reserve 4, all in pack config | As recommended | M7 ladder, police systems, #4154 P1 |
| N9 | The cut | Ship the six chapters; prison, break, hideouts, turf, drugs, landings, reserve and helicopters go to 0.4 | As recommended | this plan |

### During the week (needed by the day shown)

| # | Decision | Recommendation | Default | Was | By |
|---|---|---|---|---|---|
| N10 | Perk change | $500, once an in-game week, at the labour office | As recommended | work Q4 | Day 3 |
| N11 | First shift of the day | Double XP for the first 30 minutes per trade, reset by the morning whistle | As recommended | work W5 | Day 2 |
| N12 | Truck yard | The north-east industrial compound; trucks leave the car lot | As recommended | work Q6, #4141 T1, T2 | Day 3 |
| N13 | Truck licence | A short practical check in a lent truck, $300, 10 minutes before a retake | As recommended | #4143 L1, L2 | Day 2 |
| N14 | Capacity ladder | One config table (body, packs, car, pickup, van, truck) with a validator; native vehicle storage first, the cargo bed for bulky loads; each chain's best loop needs the next vehicle up | As recommended | #4144 V1 to V3 | Day 1 |
| N15 | Shift boards | Weekly top five per site, reset by the server clock and saved | As recommended | work Q9 | Day 4 |
| N16 | 911 abuse | Three unfounded calls a day file the charge; "unfounded" needs the officer at the scene; staff flag on officers above 50% | As recommended | police P8 | Day 2 |
| N17 | Big-job lock | Server-wide over rungs 4 to 6, and the prison break in 0.4 | As recommended | M7 L1 | Day 1 |
| N18 | Hospital robbery cost | The clinic shortage for an hour (medkits dearer, revives slower at the clinic, a notice on the door) and a town standing loss for every robber identified; never a block on revives | As recommended | #4154 P2 | Day 3 |
| N19 | Crime chapter order | K0 (the ladder on the island) before K1 | As recommended | crime Q8 | Day 1 |
| N20 | Gang flags | The 20 preset flags now; the designer after the livery spike | As recommended | #4147 G1, gangs Q2 | Day 1 |
| N21 | Flag lock | The first gang keeps its flag. A second gang may not pick it while a member of the first is online. When both are online, the later gang's poles fly a plain cloth in its colour | As recommended | #4147 G2, gangs Q1 | Day 2 |
| N22 | Gang radio | The native radio on a hidden per-gang frequency, after a research step; no text fallback | As recommended | #4148 R1, R2 | Day 1 |
| N23 | Recruiting | One path: the action list. Retire the interaction-menu entry | As recommended | #4149 P1 | Day 2 |
| N24 | Prospect rank | 48 hours without fund or stash rights | As recommended | gangs A2 | Day 3 |
| N25 | Map marks | Every 5 s, members only; grey when downed or jailed; none while the viewer is in custody | As recommended | #4150 M1, M2 | Day 1 |
| N26 | Gang size | 8 members | As recommended | gangs Q7 | Day 2 |
| N27 | Outpost standing | One underworld track plus a "known here" flag per outpost | As recommended | #4145 O1 | Day 2 |
| N28 | Police at outposts | They may enter; the traders don't serve them; the safe zone holds | As recommended | #4145 O2, gangs Q5 | Day 1 |
| N29 | Outpost site and stock | The scout picks a site at least 4 km from Saint-Philippe, off the main roads. Stock: every vanilla small arm the gun store doesn't sell, no launchers; pistol $400 to $600 (tier 1), rifle $1,500 to $2,500 (tier 2). North in 0.3.0, south as stretch | As recommended | #4145 O3, O4 | Day 1 |
| N30 | Livery method | **Changed by the maintainer (2026-11-22 late):** full repaints of whole vehicle bodies (Police, Ambulance, Rebel, company colours) and tinted vanilla work clothes, each with a credit row; lettering overlays on top where they hold | Full repaints | world Q1, #4155 S1 | Day 1 |
| N31 | Newspaper names | Character names for public facts only (arrests, rewards, openings); never an open case or a hidden record | As recommended | world Q3 | Day 2 |
| N32 | Rumors about crime | From bar staff and the informant only, to players the town counts as `friendly` or better | As recommended | world Q5 | Day 2 |
| N33 | Dogs | Sound only, at kennels | As recommended | world Q6 | Day 3 |
| N34 | Power plant coal buyer | Yes, as a suggested addition in 0.3.0 | As recommended | work A1 | Day 3 |
| N35 | Apprentice bonus | Yes, as a suggested addition in 0.3.0 | As recommended | work A10 | Day 4 |
| N36 | Release candidate on the Saturday session | Put the RC on the 0grind alpha for the 19:00 UK session, with a wipe | As recommended (the lead decides on the day) | this plan | Day 6 |
| N37 | Named drugs | Start the Workshop policy check this week, so the 0.4 chains don't wait; keep "press, cut, bag" and never a recipe; a period euphemism if a name fails | As recommended | crime Q1, #4152 D1, D2 | Day 3 |

### For 0.4 (answer any time; nothing this week waits on them)

| # | Decision | Recommendation | Default | Was |
|---|---|---|---|---|
| N38 | Prison site | The inland compound at 7447, 6705 after a scout; fallback 5355, 10543 | As recommended | police P1 |
| N39 | Prison custody | 30 online minutes cap, the cell up to 10 min and the prison above, work off up to 40% | As recommended | police P2, P3 |
| N40 | Prison clothes | A grey pack colour of vanilla work clothes, not an orange jumpsuit | As recommended | police P9 |
| N41 | Prison break numbers | Jailed member with 15+ min; 3 police on duty, Sergeant and up count double; 90 min server-wide cooldown; recapture $50 per escapee to each officer, inside the bonus cap; recaptured: 20 min and a 24 h bar; no airdrop | Wait for the maintainer: the wishlist asked for 25 min and 1 h | M7 break, gangs Q3, police balance, J1 |
| N42 | Hideouts | Lease the quiet ones, capture the lawless ones; five hideouts with one perk each; two per gang; contested sites don't count | As recommended | #4146 H1 to H3, gangs Q6 |
| N43 | Turf poles | Police take-down bonus $100; inactive gangs lose poles and leases after 7 days | As recommended | gangs Q4, A3 |
| N44 | Drug-chain extras | Pharmacist bribe yes; tip line paid only on a seizure; one landing a day in a real-time window; informant buyer at a hidden chance | As recommended | crime Q4 to Q6, #4152 D3 |
| N45 | Heist mask | Spend the Meshy mask budget on the heist mask Vince sells (pulled forward as stretch if time allows) | As recommended | crime Q7 |
| N46 | Helicopters | The UH-1H only, $12,000 to $20,000, no landing in a town's safe zone except marked pads; read the README's "no aircraft" rule as "no new aircraft assets" | As recommended | work Q7, #4142 A1 to A3 |
| N47 | Police helicopter | Lieutenant with a pilot licence, after the dealer and the livery | As recommended | police P7 |
| N48 | Mastery and gold | Mastery outfits at level 30 with one item never sold; Ore Ridge gold at Mining 25 with no lawless zone | As recommended | work Q5, Q8 |
| N49 | Market day | Tie it to the real Saturday session as well as the game calendar | As recommended (stretch this week) | world Q4 |
