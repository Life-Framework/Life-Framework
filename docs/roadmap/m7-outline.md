# M7 Power and turf (outline)

M7 is the update for players who have a living and want power: a crew of their own, ground to hold, bigger
jobs and a law that answers back. Organisations form into gangs, crime climbs a ladder from the till to the
island's reserve, and the police get a law book to charge by. This is an outline for the maintainer to review.
No slice cards are cut from it until it is approved (the M5 rule). Read it with the [roadmap](README.md), the
[M6 outline](m6-outline.md) and the [world bible](../everon/world-bible.md).

**Theme.** Everon in 1989 sits close to the Iron Curtain. Smugglers land goods on the coves at night, rebels keep
camps in the hills, and the gendarmerie is stretched thin. M7 opens the lawless ground (M6 decision 6) and gives
the underworld its own places, people and money.

**Status (2026-11-22).** Seven chapters are filed (#1022 to #1036) and some slices are merged, in-dev (the prison
break rules, the jail work stations, heist stages). The maintainer's wishlist of 2026-11-22 adds the sections
under "Additions" below. **Nothing here starts before the 0.2.1 cut and the first-two-hours review**
([README](README.md) "Now").

**Rules for every M7 item** (maintainer, 2026-11-22):

- **In the world, not in pop-ups.** World interactions are action lists on the object (one action per option,
  per part where it helps) and information sits on physical things: a board, a sign, a screen. A full window only
  where nothing in the world can do the job. The charge sheet (paperwork) is the one exception in this milestone.
- **US English everywhere** a player reads: UI, signs, textures, liveries, screens. Place and people names stay.
- **Fines and payouts are scaled to our economy** (`tools\cli economy`): a new civilian earns about $400 to $600
  an hour, police duty pays $900 to $1,260 and medic duty $792 to $1,224 (2026-11-22 run).
- Framework (core) then Flagship (pack), as every milestone since M5.

## Chapters

Filed chapters keep their issues. **New** chapters have no chapter issue yet; their items are filed as epics so
the plan is visible, and a chapter issue is cut when the maintainer approves this outline.

| Order | Chapter | Holds |
| --- | --- | --- |
| 1 | Running a crew (**new**) | Gang flags, scroll to recruit, the gang radio, gang members on the map |
| 2 | Gang war #1022 | Declared wars #766/#767, contested ground #768/#769, the conflict record #858, **gang hideouts across Everon** |
| 3 | The outposts (**new**) | Two rebel outposts with traders and an outpost reputation |
| 4 | Dirty money #1025 | Laundering #644/#645, cook failures and the rotating buyer #799, **the 1989 drug chains** |
| 5 | The convoy job #1023 | The evidence-locker break-in and convoy #770/#771/#773 |
| 6 | The big heist #1024 | Crew roles #780/#781, hands-up #782/#783, multi-stage heists #662/#663, **the robbery ladder, the hospital robbery, the reserve heist** |
| 7 | By the book (**new**) | **The law book and the charge sheet**, licence seizure #861 |
| 8 | Behind bars #1027 | Prison work #656/#657, **the prison break as a gang event** #798 |
| 9 | Making detective #1035 | As filed |
| 10 | Story: The shipment #1036 | As filed |

**Why this order.** A crew needs a name, a flag and a way to talk before it can fight over ground, so Running a
crew comes first. The outposts give the underworld a safe place to buy before the chains and heists ask for
gear. The law book lands before Behind bars because the prison break and the yard reminders read its charges.

## Additions (maintainer wishlist, 2026-11-22)

Each section: goal, what a player sees, what it builds on, open questions (each with a recommendation) and a rough
size (S under a week of slices, M one chapter's worth, L more than one). The epic issue carries the same text and
the acceptance criteria.

### Rebel outposts

Epic: #4145.

- **Goal.** Two rebel outposts, one in the north of Everon and one in the south: detailed camps with their own
  traders for illegal goods. A reputation with the outpost unlocks better stock. Both are safe zones, and between
  them they sell nearly every weapon the gun store does not.
- **What a player sees.** A dirt track into the hills ends at a camp: tents, a radio mast, a fire, guards who
  wave you through if you are unarmed. The quartermaster's crate lists what he will sell you today, on a chalk
  board by his table; a "Buy" action per item on the crate. The first visit he sells you cigarettes and a
  pistol; after a few deliveries of contraband he opens the rifle rack. Nobody can shoot inside the wire.
- **Builds on.** `LF_Zones` (a SAFE rule like the spawn square), the fence shop and its config stock (#1854),
  standing from M6 ([reputation.md](../architecture/reputation.md): a per-outpost or underworld track, tier
  words only), shop stock tables and the gun store, the town pipeline for the two places (survey, map, kits,
  check), the chains' buyers (an outpost buys contraband crates).
- **Open questions.**
  - O1. One shared underworld standing, or a reputation per outpost? **Recommend one underworld standing** (M6
    R1) with a per-outpost "known here" flag, so a player does not grind twice.
  - O2. Can on-duty police enter? **Recommend yes, but the traders do not serve them** and the safe zone still
    holds; a raid is a later event, not M7.
  - O3. Where: north near the old military ground, south in the hills above the coast? **Recommend the scout
    pass picks two sites at least 4 km from Saint-Philippe**, off the main roads, through the town pipeline.
  - O4. Which weapons. **Recommend every vanilla civilian-legal and military small arm the gun store does not
    sell**, no launchers or explosives beyond the breach charge, priced so a rifle costs about 3 to 5 hours of
    civilian work ($1,500 to $2,500).
- **Size.** M (two places through the town pipeline, a trader kit, a stock table with standing gates).
- **Acceptance.** Two outposts pass `town ready`; each has a safe zone, a trader whose stock changes by tier
  word, and a contraband buyer; a two-player run buys a pistol at the first tier and a rifle after the tier rises;
  firing inside the zone does no damage.

### Gang hideouts across Everon

Epic: #4146.

- **Goal.** Several hideouts across the island (a barn, a farmhouse, a bunker, a boathouse), each owned by one
  gang at a time. Owning one gives perks, for example a drug processor station, a bigger stash or a hidden garage.
- **What a player sees.** A gang leader leases or takes a hideout; the gang's flag goes up on its pole; inside,
  the perks are physical: a press in the back room, a stash crate, a parked van in the shed. A rival gang can take
  it in a declared war.
- **Builds on.** `LF_Hideout` (owner, lock, stash, `CanProcess`), `LF_OrgHideoutLease`,
  `LF_HideoutMachineComponent`, the contested sites with rent #768 and the lawless quarry #769 / #2689, declared
  wars #766, the org upgrades on the ledger.
- **Open questions.**
  - H1. Lease, capture, or both? **Recommend both:** quiet hideouts are leased at the ledger desk, the two or
    three hideouts in lawless ground are taken by force (#768).
  - H2. How many and which perks? **Recommend five hideouts, one headline perk each** (a drug processor, a big
    stash, a hidden garage, a radio relay for the gang radio, a counterfeit press), so a gang chooses.
  - H3. Cap per gang? **Recommend two hideouts per gang** so one gang cannot own every perk.
- **Size.** M.
- **Acceptance.** Five hideouts registered by component through `LF_Places`; a gang owns at most two; each perk
  works only for the owner; the owner and perks survive a restart (`wt e2e`).

### Gang flags

Epic: #4147.

- **Goal.** A gang chooses its flag when it is founded, from a set of about 20 period flags or a simple flag
  designer. A flag is locked while another gang with an online member uses it.
- **What a player sees.** At the registry ledger, after the name, a page of flags (and a "Design your own" page:
  a field pattern, two or three colours, an emblem). The flag flies on the gang's hideout pole, shows on the gang
  map and on the flag board of contested ground (#2685).
- **Builds on.** The org registry ledger (`LF_OrgLedger`), vanilla flag prefabs (`Flag_1_2_*`, `Flag_Base`),
  the contested-site flag board, the livery spike (texture layers) for a designer.
- **Open questions.**
  - G1. Designer in M7, or the set only? **Recommend the set of 20 first**, the designer behind a spike: Enfusion
    needs a runtime-composed texture or a layered material, which the livery spike will answer.
  - G2. "Locked while online" or locked for good? **Recommend as the maintainer said:** free when no member of
    the holding gang is online; the first gang keeps it on the save, a second gang may not fly it while the first
    is on.
- **Size.** S for the set, M with the designer.
- **Acceptance.** Founding offers the set; a flag held by a gang with an online member is refused for another;
  the flag shows on the pole, the map and the flag board; it survives a restart.

### Gang radio

Epic: #4148.

- **Goal.** Gang members talk to each other across the island by radio or something as intuitive.
- **What a player sees.** A handheld radio from the outpost trader (or the hardware store). Switched on, it sits
  on the gang's channel; press the radio key to talk to every member carrying one. Police with a scanner may hear
  a burst of static nearby, not the words.
- **Builds on.** The base game's radio voice (VON) channels and radio items, the carried radio set (#3155,
  #3187, #3674), org membership.
- **Open questions.**
  - R1. Native VON radio or our own channel? **Recommend the native radio:** give each gang a frequency the
    server sets and hides, so the talk path is the game's own. Research first (`api class` on the radio
    components).
  - R2. Text chat as a fallback? **Recommend no;** voice by radio is the period and the native path.
- **Size.** S to M (depends on R1).
- **Acceptance.** Two members with radios hear each other at 3 km; a non-member with the same radio does not;
  leaving the gang removes the channel at once.

### Scroll to recruit

Epic: #4149.

- **Goal.** Add a player to your gang by facing them and using an action from the scroll list.
- **What a player sees.** A leader looks at a player with no gang: "Invite to <gang>" in the action list. The
  player gets "Join <gang>" / "Decline" on the leader. "Remove from <gang>" for a member.
- **Builds on.** Face-to-face invite already exists in the player interaction menu (`LF_OrgInviteRole`, M1-17,
  `docs/features/ownership.md`); this moves it onto the vanilla action list on the character and keeps the server
  rules (`LF_OrgRights`).
- **Open questions.** P1. Keep the interaction-menu entry too? **Recommend no:** one path.
- **Size.** S.
- **Acceptance.** Invite, join, decline and remove work from the action list on a dedicated server with two
  clients; a player in another gang sees no invite.

### Gang members on the map

Epic: #4150.

- **Goal.** Members of a gang see each other live on the map.
- **What a player sees.** Open the map: every online member of your gang is a small mark in the gang's colour
  with a name on hover, updated every few seconds. Nobody outside the gang sees them.
- **Builds on.** `LF_MapShares` org rows and org marks (#2240), the Everon map (#2223), org membership refresh.
- **Open questions.**
  - M1. Update rate? **Recommend every 5 s**, server-sent to members only, throttled.
  - M2. Hide a member? **Recommend yes:** a member inside the prison or downed shows grey, and none show while
    the map holder is in custody.
- **Size.** S to M.
- **Acceptance.** Two members see each other move on the map; a third player does not; marks clear on leave and
  disconnect; a late joiner sees the members within one update.

### The 1989 drug chains

Epic: #4152.

- **Decision: M7, not M6.** The chains are a living, but they need the gang hideouts (the processing perks),
  the law book's narcotics classes and the outposts' buyers, all M7. M6 decision 6 also keeps M6 on policed,
  legal work. Smuggled cigarettes and vodka (already a tier in [illicit chains](scopes/crime/drug-chains.md))
  ship with them.
- **Goal.** Chains a local or smuggler of 1989 Everon would plausibly run, never a modern lab:
  - **Cannabis** grown in hidden field plots in the hills, dried in a loft, pressed and bagged.
  - **Hashish or heroin landed by boat at night** on a cove (a dropped crate on the shore, since the base game
    has no boats), cut and bagged in a barn.
  - **Amphetamine "speed" pills** pressed from diverted pharmacy stock (the hospital robbery feeds it).
  - **Smuggled cigarettes and vodka** alongside them.
- **What a player sees.** Each chain is grow or source, then process, then pack, then sell, on hands-on stations
  at real places (a press, a cutting table, a drying rack). The buyer moves (#799). A plot or a barn gives off
  smell and smoke an officer can notice; a buyer can be an informant.
- **Builds on.** `LF_ChainCatalog` and hands-on stations (#416, #417), cook mistakes (#2854, #2855), heat and
  the smell trace (#418), hidden crops (#809), the moonshine barn (#3402) and the contraband convoy (#3490), the
  hideout processing perk, the law book's narcotics classes A, B and C.
- **Open questions.**
  - D1. This changes the 2026-11-09 decision "period black market, not named modern drugs" and adds informants,
    which that decision left out. **Recommend accept** the 2026-11-22 list, and keep the rule of no real-world
    chemistry steps: a station is "press", "cut", "pack", never a recipe.
  - D2. The Bohemia Workshop content policy. **Recommend the policy check the scope card already asks for**
    (the poppy-straw row) before any build, for every named drug.
  - D3. Informants. **Recommend an NPC buyer who, at a hidden chance per sale, tips the police** with a rough
    location, through the investigation system.
- **Size.** L (four chains, each sliced: config rows, stations, places, buyer, heat).
- **Acceptance.** Each chain runs end to end on GridWorld with two workers; heat shows at its site; a search at
  a stop finds the goods; the charge sheet classes the amount (A, B or C); the buyer moves after a sale.

### The robbery ladder

- **Goal.** Robberies are a ladder with distinct stakes, timelines, requirements and cooldowns, so a crew grows
  from the till to the reserve.

  | Rung | Target | Police needed (pack, to tune) | Crew | Timeline | Payout (to tune) | Cooldown | State |
  | --- | --- | --- | --- | --- | --- | --- | --- |
  | 1 | Shop or fuel till | 1 | 1 | 1 to 2 min hold | $150 to $300 | 20 min per till | In the tree |
  | 2 | Store safe | 1 | 1 to 2 | 3 min | $400 to $800 | 45 min | #2282 |
  | 3 | Cash van on its route | 2 | 2 to 3 | the route, about 10 min | $1,500 to $3,000 | 90 min | In the tree |
  | 4 | Hospital pharmacy | 2 (a medic or officer on duty) | 2 | 5 min | morphine and pills, about $1,500 on the street | 2 h | #4154 |
  | 5 | Bank vault | 3 | 3 to 4 | about 10 min (door, drill, getaway) | $4,000 to $6,000 | 3 h | #1010, #3748 |
  | 6 | The reserve depot | 5 | 4 to 6 | 20 to 25 min, multi-stage | $15,000 to $25,000 split | 6 h server-wide | #4153 |

- **Builds on.** Safe types (#2288) carry most numbers per type; heist stages (#3409, #3411); crew roles #780.
- **Open question.** L1. One server-wide "big job" lock so rungs 4 to 6 never overlap? **Recommend yes**, and the
  prison break shares it.

### The hospital robbery

Epic: #4154.

- **Goal.** A different stake: steal pharmacy stock and morphine from the clinic. It feeds the drug chains and
  has a moral cost, because it disrupts medic service.
- **What a player sees.** Two robbers in the clinic: one holds the counter, one opens the drug cabinet (charge or
  lockpick) and fills a bag with morphine ampoules and pill boxes. The alarm calls police and the on-duty medic.
  For the next hour the clinic is short: medkits cost more at the counter and a revive there takes longer, and a
  notice on the clinic door says why.
- **Builds on.** `LF_Hospital` (till and medicine cabinet, `docs/features/crime-police.md`), its alarm (#1938),
  safe types, the pharmacy shops (#3221), town standing (a loss for the robbers), the drug chains (an input).
- **Open questions.**
  - P1. Presence rule. **Recommend at least one medic or two police on duty**, so there is someone to rob from
    and someone to answer.
  - P2. The moral cost. **Recommend the clinic shortage above plus a town-standing loss** for every robber
    identified, never a block on revives.
- **Size.** S to M.
- **Acceptance.** A two-robber run on GridWorld takes the stock in under 5 minutes; the shortage starts and ends;
  the stolen goods are a chain input; the presence rule refuses with words.

### The reserve heist

Epic: #4153.

- **Goal.** The top rung: a robbery at the island's central reserve, the state cash depot. More police online,
  more crew, a multi-stage plan, a long timer, a big payout, and a heavy police and armory response.
- **What a player sees.** Days of planning: buy the guard rota from a contact at an outpost, cut the power at the
  substation (the depot's lights and alarm line go down for a few minutes), blow the outer door, drill the inner
  vault, carry heavy armoured cash boxes to a truck, and run. Police get the armory (heavier kit) while the alarm
  runs.
- **Builds on.** `reserve_vault` safe type and the depot building (#2283, filed in M6; the M6 outline moves it to
  M7, the maintainer decides the milestone), heist stages and the heist run (#3409, #3411, #662/#663), crew roles
  #780, the two-layer breach from the jailbreak (#4058), the carry system for heavy boxes, the police armory
  (#275).
- **Open questions.**
  - V1. Where. **Recommend a walled depot outside a town, through the town pipeline**, not inside
    Saint-Philippe.
  - V2. Inside info. **Recommend an item** (the rota papers) bought at an outpost at a high underworld tier; the
    depot cannot be started without it.
  - V3. Payout. **Recommend $15,000 to $25,000 split**, as marked cash that must be laundered (#644).
- **Size.** L.
- **Acceptance.** A four-robber run against five police on a dedicated server through every stage; each stage
  can fail; the armory opens and closes with the alarm; the six-hour cooldown holds across a restart.

### The law book and the charge sheet

Epic: #4151.

- **Goal.** A structured law book in codes and a police charge sheet that picks charges from it, totals them,
  enforces stacking rules and gives "pay the fine now" or jail.
- **What a player sees.** An officer with a cuffed suspect opens the charge sheet (paperwork, on the UI kit):
  sections Criminal, Road traffic, Narcotics, Weapons; ticks charges; the sheet totals the fine and the minutes
  and greys out what may not stack. The suspect is offered "Pay now" (where every charge allows it) or goes to the
  cell. In the cell a note lists the charges and the HUD clock counts down (#4097); the yard warden says how long
  is left.
- **Codes (pack values, scaled to our economy; the core ships the code shape and neutral examples).**
  - **Criminal:** insult $50; threat $150; abuse of the emergency line $200; disobeying police $150; resisting
    $300 or 5 min; theft $200 or 5 min; vehicle theft $500 or 10 min; robbery 15 min, no fine option; murder 30
    min, no fine and no bail; mayhem (repeated violence in one incident) 20 min, massive mayhem 40 min; hostage
    taking 25 min; deprivation of liberty 15 min (not stackable with hostage taking); illegal items $300 per
    class; trespassing $100; VIN tampering $600 or 10 min; aiding and abetting carries the main offense.
  - **Road traffic:** reckless driving $250; failing an inspection $75; dangerous intervention $400; hit-and-run
    $600 and 10 min; speed limits 100 open road, 50 in town, 30 near markets, with three classes (up to 15 over
    $75, 16 to 30 over $150, more than 30 over $300); illegal parking $50 and a tow; driving without a licence
    $300; flying without a licence $800.
  - **Narcotics** by amount: class A (personal) $150; class B (dealing) $600 or 10 min; class C (trafficking)
    25 min.
  - **Weapons:** a legal-weapon list; illegal possession $800 and seizure; carrying in a no-carry zone $300;
    carrying without a licence $400; unlawful discharge $250; an illegal conversion 15 min. On-duty police are
    exempt.
  - Against our economy: a speeding ticket is 10 to 20 minutes of civilian work, a vehicle theft about an hour,
    and the rank fine caps (pack: $500 patrol to $5,000 lieutenant) cap a sheet's total. Jail minutes add up to a
    cap of 60; bail stays as the pack has it ($150 a minute, $500 minimum).
- **Powers.** Murder, a repeat offense in the same day, or a rank override mean jail only. An officer can seize
  a driving or weapon licence (permanent until the exam is retaken, #861). Ranks exist (#1769: patrol to
  lieutenant, with fine caps); the override is sergeant and up.
- **Builds on.** `LF_CrimeCatalog.conf` (typed crime codes, #685), wanted level, fine tickets, booking at the
  jail and the prisoner HUD clock (#4097), bail (#4058), police ranks (#1769), licence seizure (#861), the UI kit.
- **Open questions.**
  - B1. Retune the current ticket fines (speeding $200, reckless $500, crash $1,500) to the table? **Recommend
    yes**, in the pack, after `tools\cli economy` prints the fine as minutes of work.
  - B2. Who may use the sheet? **Recommend any on-duty officer**, the override from sergeant.
  - B3. Money and authority are risky: **interrogate before ship** (golden rule, money and RPC).
- **Size.** L.
- **Acceptance.** An officer charges a suspect with three codes; the totals and the stacking refusals match the
  config; "Pay now" takes the cash once; a jail-only charge removes "Pay now"; the cell note and the clock show
  the charges and time; a seized licence stays seized across a restart.

### The prison break as a gang event

Filed: Flagship #798 in Behind bars #1027, with merged rule and door slices (#3420, #3421, #3422) and the pack
values card #3423. This section is the fuller design to fold into #798; no new epic.

- **Goal.** Rework the escape and the two-C4 jailbreak (#4058, #4106) into one coherent, rare gang event, and
  keep bail.
- **Starting it.** A free gang member talks to a contact in the prison yard. It needs a jailed member of the same
  gang with enough time left (our sentences are short: **recommend 10 minutes or more**), enough experienced
  police on duty (**recommend 3, sergeant or up counting double**), and no other break running anywhere. The
  contact says in words what is missing. A server-wide cooldown follows (#3423 recommends once a real day;
  **recommend 90 minutes on the alpha**, tune from play).
- **Prep (about 2.5 min).** Police get a dispatch naming the prisoners; the prisoners hear help is coming. Nobody
  else is told.
- **Escape (about 15 min).** The prison's safe zone drops for everyone on the grounds. Escapees may enter ground
  vehicles, never a helicopter. The gang gets a waypoint to a random extraction point 1 to 5 km away; everyone
  involved sees a HUD timer. The extraction contact appears five minutes in, and only then do police see the
  point. The prisoner holds "Escape" next to the contact.
- **Police side.** Escapees show on the police map every 30 s. "Take back to cell" on a cuffed escapee. A
  recapture pays every on-duty officer a bonus (**recommend $300**, about 20 minutes of duty); killing an escapee
  pays nothing.
- **Outcomes per prisoner.** Escaped: sentence wiped. Recaptured: an hour in the cell, inventory wiped, barred
  from later breaks. Killed: respawns in prison and serves the rest. Timed out: serves the rest. A summary at the
  end lists every outcome.
- **Restarts.** The event holds the automatic restart off and resumes after a crash.
- **Kit.** Escapees leave with nothing, so the gang brings a car, a weapon and clothes; an airdrop crate with a
  break-out loadout is an option (open question J1, **recommend no airdrop in M7**).
- **Size.** M on top of what is merged.
- **Acceptance.** As #798, plus each outcome above in a two-gang, three-police dedicated-server run and a restart
  during the escape phase.

## Risks

- **Money.** Heists, drugs and outposts add cash and guns. Mitigation: marked cash through laundering, the big
  job lock, the ledger reasons per source and a one-week ledger review after each chapter.
- **Police numbers.** Rungs 4 to 6 and the prison break need several officers online. On a quiet alpha they never
  start. Mitigation: the presence numbers are pack config; tune them to the real duty roster.
- **Workshop policy** for the named drugs (D2).
- **Engine unknowns:** native radio channels per gang, runtime flag textures. Each gets a research step first.
