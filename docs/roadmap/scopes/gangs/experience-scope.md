# Gangs and turf: experience scope

Milestone: M7 Power and turf
Extends: organizations and the registry ledger, hideouts and the stash, contested sites with rent (#768),
declared wars (`LF_OrgWars`, #2666 to #2677), org map marks (#2240), the carried radio set (#3155), the
two-charge jailbreak and `LF_PrisonBreak` (#3421, #3422), standing (M6, [reputation.md](../../../architecture/reputation.md)),
zones (`LF_Zones` SAFE and LAWLESS rules), the fence and the illicit chains.
Epics: #4147 flags, #4148 radio, #4149 recruiting, #4150 map, #4146 hideouts, #4145 rebel outposts,
#1022 Gang war, #1027 Behind bars, #798 prison break.

Design only. Nothing here is dispatched. The M7 outline (`docs/roadmap/m7-outline.md`, PR
roadmap-after-021) holds the epics' short form; this file is the playable detail behind them.

## 1. Player fantasy and core loop

**Hour 1 in a gang.** Two friends who have done the apple and mining runs want more. They walk into the
registry office in Saint-Philippe, sign the ledger for $250, pick a flag from the cloth samples pinned on the
wall, and walk out with a crew. One faces the other and scrolls "Invite to Red Hand". Each buys a radio set
at the hardware store, and the channel crackles: "You there?" "Yeah, at the brow." On the map they see each
other as two small red marks. They hang their flag on a free pole in Tyrone and do a contraband run to the
north outpost, where the quartermaster sells them cigarettes and a pistol and nothing more. The hook is
that it is theirs: a name, a flag on a pole in the world, a voice in the ear.

**Hour 10.** The crew is five people with ranks. They lease a barn hideout with a press in the back room and
run a cannabis chain from a hidden plot to the barn to the outpost buyer. The quartermaster has opened the
rifle rack. They hold two village poles that pay a cut of every illicit sale in those villages into the fund,
and another gang has started raising its flag in the next village over. A war is declared on the board in
Saint-Philippe. One member gets caught with product and jailed for 30 minutes, and the others plan their
first prison break: a contact in the yard, two charges on the doors, a car waiting at a cove. The hooks are
the fund growing toward the next hideout, the next outpost tier, and the rivalry.

**Hour 50.** The gang is part of how the island works. It holds a lawless contested site (the hilltop mast or
the quarry) that banks rent, owns two hideouts with different perks, and is known at both outposts. Its flag
flies over four villages, and police know it from the wanted posters. Its members have records, and they
recruit prospects off the street. Weekends are wars, convoy jobs and breaks; weekdays are the chains that
pay for them. Losing a hideout in a war hurts, and taking it back is the best night of the month. What keeps
it going is social: a gang is a reason to log on at 19:00 because the others will be there.

**Why it is addictive.**

- Visible progress in the world: the flag on more poles, the outpost unlocking racks, a second hideout.
- A shared fund that pays for upgrades everyone sees.
- Variety from mixing loops: chains on weekdays, wars, breaks and convoys on weekends.
- Rivals: another gang's flag in your village is an itch.
- Loss you can recover from: a lost hideout or pole can be taken back, and nothing is deleted.
- Roles inside the crew: driver, grower, talker at the outpost, shooter, lookout on the radio.

## 2. Systems

| Shared system | What gangs reuse | What gangs add |
|---|---|---|
| Organizations (`Feature/Organizations/`) | Registry, ranks and rights (`LF_OrgRights`), fund, ledger page, invites | Flag choice on the ledger; a `prospect` rung; an upkeep debit for hideouts |
| Hideouts (`Feature/Hideouts/`) | Owner on the lock, stash, `CanProcess`, lease | Several hideouts by component through `LF_Places`, one perk per hideout, a cap of two per gang |
| Contested sites (`LF_Sites`, #768) | Rent banked per hour, capture time, the flag board | Turf poles in villages as a light version of the same rule |
| Wars (`LF_OrgWars`) | Declare, accept, lapse, pair cooldown, war kills | A lapsed declaration forfeits one turf pole (proposal T2 below) |
| Standing (M6) | The underworld track, tier words, never shown | Per-outpost "known here" flag; stock gates by tier |
| Wanted level and the crime record | Escaped custody, the jailbreak offense, war kills tagged | Gang membership on the record line a detective can read |
| Legal system (charge sheet, scope by the crime agent) | Narcotics by amount, illegal weapons, aiding and abetting | "Member of a gang at war" is not a crime; a gang flag on a pole is not evidence of anything |
| Jailing and bail (#4058, #4106, #4097) | Cell, sentence clock, slip, bail | The prison-break event wraps them |
| Comms (`Feature/Comms/`) | Radio set item, frequency tuning (as the service radios do), the scanner | A hidden per-gang frequency; a police direction-finder (proposal) |
| Map (`LF_MapShares`, org marks) | ORG audience, marks saved per org | Live member marks every 5 s |
| Vehicles and cargo | Hidden compartments (#3776), trunk storage, liveries (#4155) | Rebel livery at the outposts; a hideout garage perk |
| Economy and market | Buyers with demand caps, the wholesale curve, `tools\cli economy` | Outpost buyers for contraband; turf street tax |
| Illicit chains (`Feature/Chains/`) | Stations, recipes, police risk | The hideout press as a perk station |
| HUD | Notifications, the jail clock line | One small event timer line during a break (not a window) |
| F2 player menu | Character file, skills column | A "Gang" tab: name, flag, rank, fund balance, held hideouts and poles. Read only; everything is done in the world |

## 3. Places

Coordinates are x, z on `CoreOnEveron`. Every new site goes through the town pipeline (scout, map, kits,
check) and gets its own card; candidates marked "scout" need `tools\cli scout` before a card is written.

| Place | Where | What it looks like | What is there |
|---|---|---|---|
| Registry office | Saint-Philippe town hall (exists, #3410) | A clerk's desk, the ledger, a wall of pinned cloth flag samples, a wooden war board | Found a gang, choose a flag, the war desk, the war board |
| North rebel outpost | Scout: the wooded ridge east of the airbase (about 7000 to 8000, 11000), at least 4 km from Saint-Philippe, off a dirt track | A clearing with canvas covers, camo nets over pallets, a radio mast, a campfire, a generator, sandbags at the gate, a flag pole with a plain green flag | Quartermaster's table and crate, chalk stock board, a contraband buyer, a mechanic with the rebel livery, guards at the gate. SAFE zone |
| South rebel outpost | Scout: the hills between Morton (5135, 4012) and Pick Mountain, away from the hilltop mast contested site | A farm in ruins taken over: a barn with a tarp roof, a pickup with a mounted spotlight, crates, a washing line, a dog kennel | A smuggler trader (different stock from the north), a boat contact at the nearest cove, guards. SAFE zone |
| Hideout: the still barn | Étoupe, 5361, 9900 (scouted) | A farm barn with a back room | Perk: drug press (a chain station) |
| Hideout: the farm compound | Near Chotain, 6649, 5900 (scouted, a contested site) | Walled farm, 14 buildings, sheds | Perk: hidden garage (one parked gang vehicle that survives restarts, out of police search) |
| Hideout: the boathouse | Scout: a dock at Lancre, Le Bosc or West Cape | A wooden boathouse on a quiet dock | Perk: night landings (a smuggling pickup point for the boat chain) |
| Hideout: the bunker | Scout: an old military site, from the lawless list | Concrete, a steel door, an iron bed | Perk: big stash (three times the base stash) |
| Hideout: the radio shack | Scout: a hilltop hut with a mast | A hut, a table radio, a car battery | Perk: radio relay (gang radio range doubled; members hear the police scanner within 500 m) |
| Turf poles | One per village square: Meaux, Tyrone, Entre-Deux, Provins, La Valette, Chotain to start. Never Saint-Philippe (spawn) or Montignac (work town) | A wooden pole by the café or the war memorial, an empty halyard until taken | Raise a flag, take one down, read the pole's plate |
| Contested sites (exist) | The quarry, the farm compound, the hilltop mast (9946, 2283, Pick Mountain) | As built in #768, #769, #3413 | Rent, capture, lawless rules from M7 |
| Prison | Now: the Saint-Philippe jail (`Place_saint-philippe_jail.layer`). Later: the separate prison of Behind bars #1027 | Cells, an outer door, a yard with a bench and a fence | The yard contact, the doors the charges open |
| Extraction points | Six in the pack, 1 to 5 km from the jail: two coves, a barn, a forest track, a lay-by, a ruined chapel | Each a small dressed spot: a parked car with the engine ticking, a boat on the sand | The extraction contact appears 5 minutes into the escape |

## 4. People

All voices through the ElevenLabs pipeline, US English (a French accent is fine), every line voiced, and
dialogue on number keys with "0. Goodbye". Clothing from vanilla character prefabs and loadouts.

| NPC | Where | Role | Clothing (vanilla) | Lines (examples) | Behaviour |
|---|---|---|---|---|---|
| The registrar | Registry office | Founds gangs, takes the flag choice, reads the war rules | `Character_CIV_Businessman_*` (exists as the desk clerk) | "Name, flag, and two hundred fifty dollars." | Behind the desk, works the ledger |
| North quartermaster, "Sarge" | North outpost | Weapons and gear, gated by standing | FIA officer loadout (`Prefabs/Characters/Factions/INDFOR/FIA/Character_FIA_*`) minus helmet | "I don't know you. Cigarettes, a pistol. Come back when people talk about you." / "Rack's open. Pick one." | Sits at the table, stands when you approach, refuses on-duty police |
| South trader, "Marguerite" | South outpost | Contraband buyer, boat contact, smuggled goods | Civilian farm clothes (`Character_CIV_*` farmer family) with an FIA jacket | "Boat comes Tuesdays. You didn't hear that from me." | Smokes by the barn, counts money at a crate |
| Outpost guards (2 per outpost) | Gates | Mood and the safe-zone rule in person | `Character_FIA_*` riflemen | "Weapons down inside the wire." | Stand at the gate, wave unarmed players in, turn to face anyone who draws |
| Outpost mechanic | North outpost | Rebel livery and a hidden compartment | FIA crew loadout (`Character_FIA_Crew`) | "Paint's dry in a minute. Don't park it at the station." | Under a canvas cover with a car |
| Yard contact, "Old Pascal" | Prison yard | Starts a break, says what is missing | Civilian work clothes, no hat (`Character_CIV_ConstructionWorker_*` family, no vest) | "Not today. Too few cops to make it worth it." / "Tell your people: two minutes." | Sits on the yard bench, only talks to a free member of a gang with a member inside |
| Extraction driver | The chosen point | Ends the escape | Civilian | "Get in, get down." | Appears at 5 minutes, waits until the timer ends |
| Prospect recruiter (proposal) | Bars in turf villages | Points new players to gangs that are recruiting | Civilian | "Red Hand's looking for drivers. Ask at the Tyrone pole." | Reads the recruiting flag on the ledger |

## 5. Assets

| Asset | Source | Prefab family or plan | Why |
|---|---|---|---|
| Flag cloth and pole | Vanilla | `Prefabs/Props/Fabric/Flags/Flag_1_2.et`, `Flag_2_3.et` and their `Flag_1_2_*_01.et` variants | The cloth animates already; a gang flag is a material swap |
| 20 preset gang flags | Own build script | A script that writes 20 `.edds` + `.emat` pairs on the `Flag_1_2` UV: field patterns (bicolour, tricolour, cross, saltire, chevron, quartered) in period colours, with one emblem each from a small set (star, skull, anchor, wolf head, crown, bolt) drawn as vector shapes | Simple shapes generate cleanly; no outside licence; no real national or political flag |
| Flag designer textures | Spike (with the livery spike #4155) | Composed at runtime from layers, or a fixed palette baked into N variants | Unknown until the spike says whether a layered material works |
| Gang radio | Vanilla | `Prefabs/Items/Equipment/Radios/Radio_R148.et` (a pack copy, as `LF_HandsetRadio.et` already is) | The game's own radio voice path; players know it |
| Outpost camp | Vanilla | `Prefabs/Structures/Military/Camps/Canvas_Covers/CanvasCover_*`, `Prefabs/Props/Military/Camps/CamoNet_Pallet*`, `BoxMRE_01.et`, `Prefabs/Particles/Campfire_burning.et`, sandbags, generators and antenna props from the props catalog (`docs/everon/catalog-props.md`) | A dressed military camp needs no new mesh |
| Chalk stock board | Vanilla board + our sign system | The board prefabs `Configs/Signs` already uses | Stock shown on a physical thing, not a window |
| Weapons at the outposts | Vanilla | Every small arm the gun store does not sell (the list is a config, not a scope decision) | No weapon pack |
| Rebel livery | Livery pipeline (#4155) | Olive and rust paint with hand-painted US English lettering ("FARM SUPPLY") on a vanilla pickup and the UAZ | Disguise, not a faction badge |
| Turf pole | Vanilla | A flag prefab on a wooden pole; a brass plate from the sign kit | One pole per village |
| Wanted gang poster | Our sign kit | A board kind like the wanted board | Police side of turf |
| Yard bench, extraction car | Vanilla | Bench props, any civilian car | |
| SFX: flag raise | ElevenLabs SFX | Rope through a pulley, cloth snap | Feedback for the raise action |
| SFX: radio | Vanilla first | The radio's own squelch; an ElevenLabs "channel open" click only if vanilla has none | |
| Ambience: outposts | ElevenLabs SFX | Generator hum, a distant shortwave radio with period music, dogs, a kettle | Each outpost sounds different |
| SFX: prison break | Vanilla alarm first, else ElevenLabs | Alarm bell, then a siren that fades | |
| Voice lines | ElevenLabs | Every NPC above, US English | House rule |
| HUD event timer | UI kit | One line on the existing notifications strip: "Escape: 12:40" | The only HUD addition |
| F2 Gang tab | UI kit | A read-only page in the player menu | F2 is an allowed screen |
| Flag order form | UI kit, paperwork | Only for the designer: a printed order form with the pattern and colours | Paperwork is an allowed screen |

No Meshy. The masks Meshy is allowed for (Purge-style, 1989) belong to the crime or clothing scope; gangs
can sell them at the south outpost once they exist.

## 6. Interactions

Every one is an action on the object or person in the vanilla action list. Refusals give the reason as the
action's greyed text.

| Object | Actions |
|---|---|
| Registry ledger | "Found a gang ($250)" (opens the ledger page, which needs a typed name), "Gang ledger" (members, fund), "Wars" (the war sheet, exists) |
| Flag sample wall | One action per free flag: "Choose: Red Hand saltire". A held flag: "Taken by Black Dogs (online)". "Order a custom flag ($500)" opens the order form |
| A player with no gang | "Invite to <gang>" (rank with invite right) |
| The leader, as seen by an invitee | "Join <gang>", "Decline" |
| A member | "Remove from <gang>", "Promote", "Demote" (exist) |
| Radio set in hand | "Tune to gang channel", "Tune to open band" |
| Turf pole | "Raise our flag" (hold 60 s), "Take down the flag" (hold 30 s, also police), "Read plate" ("Tyrone. Held by Red Hand since Tuesday.") |
| Hideout door, stash, press | Exist: "Open", "Stash", "Process" |
| Hideout lease board (beside the door) | "Lease for Red Hand ($300 a day)", "Give up the lease", "Read" (owner, perk, upkeep) |
| Garage perk spot | "Park gang vehicle", "Take gang vehicle" |
| Quartermaster's crate | One "Buy <item>: $<price>" per stocked item; locked items do not show |
| Chalk board | "Read" (today's stock and one line hinting at the next tier) |
| Outpost buyer | "Sell <item> x<n>: $<price>" per carried contraband |
| Outpost mechanic | "Paint rebel livery ($400)", "Fit hidden compartment" (the chop-shop action) |
| Yard contact | Dialogue: "1. We need to get someone out." "2. What do you need?" "0. Goodbye" |
| Outer and cell doors | Exist: "Place breaching charge" |
| Extraction contact | "Escape" (hold 5 s, prisoner only) |
| A cuffed escapee (police) | "Take back to cell" |

## 7. Balance

Reference numbers from `tools\cli economy` (2026-11-22): civilian runs and chains pay $400 to $600 an hour,
police $900 to $1,260, medic $792 to $1,224. Gang income has to beat a legal job for the risk, but not by so
much that every player joins a gang.

**Targets.** An active gang member earns $600 to $900 an hour on average across chains, turf and rent,
before losses. A bad night (a seizure, a jail term, a lost war) costs one to three hours of that.

| Item | Number | Note |
|---|---|---|
| Founding fee | $250 (exists) | |
| Preset flag | Free with founding; change later $150 | |
| Custom flag | $500 | A status item |
| Gang radio | $60 at the hardware store, $40 at the outposts | The handset set is $25; the gang radio is the same R148 on a hidden frequency |
| Hideout lease | $300 a day from the fund (quiet hideouts) | Debited at the daily reset; unpaid for 2 days releases it |
| Hideout cap | 2 per gang | A gang chooses perks |
| Contested site rent | $500 to $700 an hour banked, cap $2,500 to $3,500 (exists) | Per gang, so for five members it is about $120 each; keep |
| Turf street tax | 10% of every illicit sale in the village to the fund, capped $150 an hour per pole | Paid out of the buyer's margin, not created |
| Turf cap | 4 poles per gang | Leaves room for others |
| War fee | $2,500 (exists) | |
| Outpost pistol | $400 to $600 | Tier 1 underworld |
| Outpost rifle | $1,500 to $2,500 (3 to 5 hours of civilian work) | Tier 2 |
| Outpost machine gun or marksman rifle | $3,500 to $5,000 | Tier 3 |
| Outpost contraband buyer | 10 to 20% above the fence, with a demand cap that refills | Gives the outposts a reason beyond weapons |
| Rebel livery | $400 | |
| Prison-break recapture bonus | $300 to every on-duty officer | About 20 minutes of police pay; only for a cuffed recapture, nothing for a kill |
| Prison-break cooldown | 90 minutes server-wide | |
| Recaptured escapee | 20 minutes in the cell, barred from breaks for 24 hours | Scaled down from the wishlist's 1 hour, to fit our sentence lengths; open question |

**Standing.** Underworld standing comes from outpost sales, chain deliveries, wars won and breaks completed
(once per character per day each). Tier 1 needs about 2 hours of crime, tier 2 about 8, tier 3 about 25.
Arrests and convictions take some away (M6 R4).

**Sinks.** Hideout lease, war fees, flag changes, weapons, liveries, bail, seized contraband, the radio. An
idle fund buys nothing on its own; it is spent or it sits.

**Cooldowns.** Turf pole: 30 minutes after a flag change before it can change again. Contested site
capture: as configured. War: the pair cooldown (exists). Prison break: 90 minutes server-wide.

## 8. Slices

Five chapters in an order that is playable at each step. Each card is 30 to 45 minutes, file-disjoint,
acceptance written first.

1. **Running a crew** (new chapter; epics #4147, #4148, #4149, #4150). A gang has a flag, talks by radio,
   recruits by scroll and sees itself on the map. Playable alone: two friends can be a gang on day one.
2. **The outposts** (new chapter; epic #4145). Two safe camps with traders and standing gates. Gives gangs
   a destination, weapons and a contraband buyer.
3. **Hideouts with perks** (epic #4146, lands in Gang war #1022). Five hideouts, two per gang, one perk each.
4. **Turf** (Gang war #1022). Village poles, street tax, wars that forfeit a pole.
5. **The prison break** (Behind bars #1027, #798). One event from the yard contact to extraction.

| Key | Title | Type | Tier | Model | Owns | Depends | Acceptance | Risk |
|---|---|---|---|---|---|---|---|---|
| C1 | Flag set: 20 preset gang flag materials from a build script | visual | T1 | sonnet | `tools/assets/gang-flags/`, `addons/LF_Everon1989/Assets/Props/Flags/Gang/` | - | proof `look item` of all 20 on `Flag_1_2` | none |
| C2 | Flag choice stored on the organization and saved | system | T2 | opus | `Feature/Organizations/LF_OrgFlag*.c` | - | test LF_Test_OrgFlagSave | persistence |
| C3 | Flag lock: refused while a holder with an online member uses it | capability | T2 | sonnet | `Feature/Organizations/LF_OrgFlagRules.c` | C2 | test LF_Test_OrgFlagLock | none |
| C4 | Flag sample wall at the registry, one action per flag | capability | T2 | sonnet | `Feature/Organizations/LF_OrgFlagWall*.c`, `Prefabs/Organizations/LF_OrgFlagWall.et` | C1, C3 | test LF_Test_OrgFlagWallActions | none |
| C5 | Recruit from the vanilla action list (invite, join, decline, remove) | capability | T2 | sonnet | `Feature/Organizations/LF_OrgInviteActions.c` | - | test LF_Test_OrgInviteActions; proof two-client | replication |
| C6 | Retire the interaction-menu invite entry | system | T2 | sonnet | `Feature/Organizations/LF_OrgInviteRole.c` | C5 | test LF_Test_OrgInviteOnePath | none |
| C7 | Gang frequency: a hidden per-gang band, assigned and released | system | T2 | opus | `Feature/Comms/LF_GangRadio*.c`, `Configs/Comms/LF_GangRadio.conf` | - | test LF_Test_GangRadioBand | replication |
| C8 | Gang radio item and "Tune to gang channel" | capability | T2 | sonnet | `LF_Everon1989/Prefabs/Items/Comms/LF_GangRadio.et`, `Feature/Comms/LF_GangRadioActions.c` | C7 | proof gang-radio (two members hear, a third does not) | replication |
| C9 | Leaving the gang drops the channel at once | capability | T2 | sonnet | `Feature/Comms/LF_GangRadioMembership.c` | C7 | test LF_Test_GangRadioLeave | none |
| C10 | Live member marks on the map every 5 s, members only | capability | T2 | opus | `Feature/Organizations/LF_OrgLiveMarks.c` | - | test LF_Test_OrgLiveMarksAudience; proof two-client | replication |
| C11 | Marks: grey when downed or jailed, none while in custody, cleared on leave | capability | T2 | sonnet | `Feature/Organizations/LF_OrgLiveMarkRules.c` | C10 | test LF_Test_OrgLiveMarkRules | none |
| C12 | Flag on the hideout pole and the contested flag board | capability | T2 | sonnet | `Feature/Hideouts/LF_HideoutFlagPole.c` | C2 | test LF_Test_HideoutFlagShown | none |
| C13 | F2 Gang tab (read only) | visual | T2 | sonnet | `Feature/PlayerMenu/LF_PlayerMenuGangTab*.c`, its layout | C2 | test LF_Test_PlayerMenuGangTab; proof look | none |
| C14 | Registrar and flag wall voice lines | data | T1 | haiku | `LF_Everon1989/Sounds/Voice/registrar/`, dialogue conf rows | C4 | fixture the voice manifest | none |
| C15 | Town placement for Running a crew | town | T1 | sonnet | town layers (registry office) | C1 to C14 | proof `audit placement --strict` | none |
| O1 | Outpost zone: SAFE rule, no service to on-duty police | system | T2 | sonnet | `Feature/Outposts/LF_Outpost.c`, `Configs/Outposts/LF_Outposts.conf` | - | test LF_Test_OutpostRules | none |
| O2 | Quartermaster crate: one Buy action per stocked item, gated by underworld tier | capability | T2 | sonnet | `Feature/Outposts/LF_OutpostStock*.c` | O1 | test LF_Test_OutpostStockGate | money |
| O3 | Chalk board shows today's stock and the next-tier hint | capability | T2 | sonnet | `Feature/Outposts/LF_OutpostBoard.c` | O2 | test LF_Test_OutpostBoardLines | none |
| O4 | "Known here" flag per outpost; standing awards for outpost sales | system | T2 | opus | `Feature/Outposts/LF_OutpostStanding.c` | O1 | test LF_Test_OutpostStandingAward | persistence |
| O5 | Outpost contraband buyer with a demand cap | capability | T2 | sonnet | `Feature/Outposts/LF_OutpostBuyer.c` | O1 | test LF_Test_OutpostBuyerCap | money |
| O6 | Outpost stock tables (north and south differ) | data | T1 | sonnet | `LF_Everon1989/Configs/Outposts/` | O2 | test LF_Test_EveronOutpostStock (pack) | none |
| O7 | Scout and map the north outpost site | town | T0 | sonnet | `docs/everon/places/outpost-north/` | - | fixture dossier.json | none |
| O8 | Scout and map the south outpost site | town | T0 | sonnet | `docs/everon/places/outpost-south/` | - | fixture dossier.json | none |
| O9 | North outpost dressing kit (camp, gate, mast, fire) | visual | T1 | sonnet | grid cell `slice-O9` | O7 | proof `look grid:slice-O9` | none |
| O10 | South outpost dressing kit (barn camp) | visual | T1 | sonnet | grid cell `slice-O10` | O8 | proof `look grid:slice-O10` | none |
| O11 | Quartermaster, trader, guards, mechanic: NPCs, loadouts, voice | data | T1 | sonnet | `LF_Everon1989/Prefabs/Characters/Outposts/`, voice folders | O1 | proof `look item` per NPC; fixture voice manifest | none |
| O12 | Outpost ambience beds | data | T1 | haiku | `LF_Everon1989/Sounds/Ambience/Outposts/` | - | fixture the sound manifest | none |
| O13 | Town placement for the outposts | town | T1 | sonnet | outpost place layers | O1 to O12 | proof `audit placement --strict` | none |
| H1 | Hideout registry by component; cap of two per gang | system | T2 | opus | `Feature/Hideouts/LF_HideoutRegistry.c` | - | test LF_Test_HideoutCap | persistence |
| H2 | Lease board with daily upkeep from the fund | capability | T2 | opus | `Feature/Hideouts/LF_HideoutLeaseBoard*.c` | H1 | test LF_Test_HideoutUpkeep | money |
| H3 | Perk: hidden garage | capability | T2 | sonnet | `Feature/Hideouts/LF_HideoutGarage.c` | H1 | test LF_Test_HideoutGarage; e2e | persistence |
| H4 | Perk: big stash | capability | T2 | sonnet | `Feature/Hideouts/LF_HideoutStashSize.c` | H1 | test LF_Test_HideoutStashPerk | inventory |
| H5 | Perk: radio relay | capability | T2 | sonnet | `Feature/Comms/LF_GangRadioRelay.c` | H1, C7 | test LF_Test_GangRadioRelay | none |
| H6 | Perk: night landings pickup | capability | T2 | sonnet | `Feature/Hideouts/LF_HideoutLanding.c` | H1 | test LF_Test_HideoutLanding | none |
| H7 | Perk: drug press as the barn's chain station | data | T1 | sonnet | `LF_Everon1989/Configs/Hideouts/` | H1 | test LF_Test_EveronHideoutPress (pack) | none |
| H8 | Scout the boathouse, bunker and radio shack | town | T0 | sonnet | `docs/everon/places/hideout-*/` | - | fixture dossier.json | none |
| H9 | Hideout dressing kits (five, one card each when big) | visual | T1 | sonnet | grid cells `slice-H9*` | H8 | proof look per cell | none |
| H10 | Town placement for hideouts | town | T1 | sonnet | hideout place layers | H1 to H9 | proof `audit placement --strict` | none |
| T1 | Turf pole: raise and take down, 30 min lock, saved holder | capability | T2 | opus | `Feature/Turf/LF_TurfPole*.c` | C2 | test LF_Test_TurfPoleRules; e2e | persistence |
| T2 | A lapsed war declaration forfeits one pole | system | T2 | sonnet | `Feature/Turf/LF_TurfWarForfeit.c` | T1 | test LF_Test_TurfLapseForfeit | none |
| T3 | Street tax: 10% of illicit sales in the village to the holder's fund, capped | capability | T2 | opus | `Feature/Turf/LF_TurfTax.c` | T1 | test LF_Test_TurfTaxCap | money |
| T4 | Pole plate and holder notice on the radio when someone raises at our pole | capability | T2 | sonnet | `Feature/Turf/LF_TurfNotice.c` | T1, C7 | test LF_Test_TurfNotice | none |
| T5 | Police take a flag down for a bonus | capability | T2 | sonnet | `Feature/Turf/LF_TurfPolice.c` | T1 | test LF_Test_TurfPoliceTakeDown | money |
| T6 | Six pack turf poles in village squares | town | T1 | sonnet | village place layers | T1 | proof `audit placement --strict` | none |
| P1 | Break event shell: preconditions, phases, one at a time, cooldown | system | T2 | opus | `Feature/Police/LF_PrisonBreakEvent.c` | - | test LF_Test_PrisonBreakPhases | none |
| P2 | Yard contact dialogue says what is missing | capability | T2 | sonnet | `Feature/Police/LF_PrisonBreakContact.c` | P1 | test LF_Test_PrisonBreakContactLines | none |
| P3 | Prep: dispatch names the prisoners; prisoners told help is coming | capability | T2 | sonnet | `Feature/Police/LF_PrisonBreakNotice.c` | P1 | test LF_Test_PrisonBreakNotice | none |
| P4 | The two charges open the doors only in the escape phase | system | T2 | sonnet | `Feature/Police/LF_JailBreak.c` | P1 | test LF_Test_PrisonBreakDoors | none |
| P5 | Extraction: random point, contact at 5 min, "Escape" hold | capability | T2 | sonnet | `Feature/Police/LF_PrisonBreakExtract.c` | P1 | test LF_Test_PrisonBreakExtract | none |
| P6 | Police track every 30 s and "Take back to cell" | capability | T2 | sonnet | `Feature/Police/LF_PrisonBreakTrack.c` | P1 | test LF_Test_PrisonBreakTrack | replication |
| P7 | Outcomes per prisoner, bonus, the summary notice | capability | T2 | opus | `Feature/Police/LF_PrisonBreakOutcome.c` | P5, P6 | test LF_Test_PrisonBreakOutcomes | money |
| P8 | Hold the auto restart; resume after a crash | system | T2 | opus | `Feature/Police/LF_PrisonBreakSave.c` | P1 | e2e prison-break restart | persistence |
| P9 | HUD event timer line | visual | T2 | sonnet | `Feature/Notifications/LF_EventTimerLine.c` | P1 | proof session look (HUD) | none |
| P10 | Six extraction points dressed | visual | T1 | sonnet | grid cell `slice-P10` | P5 | proof look | none |
| P11 | Yard contact and driver: NPCs and voice | data | T1 | haiku | `LF_Everon1989/Prefabs/Characters/Prison/`, voice | P2 | fixture voice manifest | none |
| P12 | Town placement for the break | town | T1 | sonnet | jail and extraction layers | P1 to P11 | proof `audit placement --strict` | none |

Paths are under `addons/LifeFramework/Scripts/Game/` unless they start with `LF_Everon1989/` or `docs/`.
Cards C1, C5, C7, C10, O1, O7, O8, H8 and P1 can start together.

## 9. Risks and open questions

**Risks.**

- **Gang radio frequency privacy.** A vanilla radio can be tuned by hand. A hidden frequency in a wide band
  can still be found by scanning. Research the radio components first (`tools\cli api class` on the
  transceiver); if a frequency can be locked on an item, use that, otherwise pick a fresh band per session
  and accept that a patient scanner hears it.
- **Live map marks at scale.** 20 gangs with 6 members each is 120 marks every 5 s, sent only to members.
  Throttle on the server; send deltas.
- **Offline raiding.** Turf and hideouts lost while a gang sleeps feel unfair. The design only lets a pole
  change hands when it is unclaimed, during an accepted war, or by forfeit; contested sites keep their
  existing rules.
- **Prison break and restarts.** Holding the automatic restart and resuming after a crash touches the save
  and the restart scheduler. Opus, interrogate, and a real two-boot `wt e2e`.
- **Two-C4 jailbreak overlap.** Today the cell charge frees every prisoner and clears the sentence. The event
  changes that rule; the old path either becomes the event's door step or is switched off by config.
- **Flag duplication.** "Locked while online" means two gangs can own the same flag. When both are online,
  the second one's poles would show the same cloth.

**Questions for the maintainer.**

1. Flag lock: the first gang keeps its flag for good, and the online rule only stops a second gang from
   picking it while the first is on? **Recommend** that, plus: when both are online, the later gang's poles
   fly a plain cloth in its colour.
2. Custom flag designer in M7, or the preset set first? **Recommend** the set first and the designer after the
   livery spike.
3. Recaptured escapee: 1 hour in a cell (the wishlist) or 20 minutes? **Recommend** 20 minutes and a 24 hour
   bar, since our sentences run in minutes.
4. Turf poles: should police taking one down pay a bonus? **Recommend** yes, $100, so police have a reason
   to drive to villages.
5. Outposts serve on-duty police? **Recommend** no service, but they may enter; the safe zone holds for them.
6. Can a gang hold a contested site and two hideouts at once? **Recommend** yes; contested sites do not count
   toward the hideout cap.
7. Gang size cap? **Recommend** 8 members, so a server of 40 has room for several gangs.

## Suggested additions (proposals for the maintainer)

These go beyond the wishlist. None is planned until the maintainer picks it.

| Proposal | What | Why it helps the whole | Size | Touches |
|---|---|---|---|---|
| A1. Gang colours | A gang picks one clothing colour (an armband, bandana or jacket from the clothing store). Members wearing it count for turf and are named in witness descriptions ("wearing a red armband") | Gangs are visible in the street; police investigation gets a real clue; the clothing store gets a reason to exist | M | clothing store, investigation, crime |
| A2. Prospect rank | A trial rung with no fund or stash rights for the first 48 h | Safe recruiting of strangers off the street; stops a new member emptying the stash | S | organizations |
| A3. Inactive gang decay | A gang with no member online for 7 days loses its leases and poles (fund kept) | Frees the map for active players on a small server | S | hideouts, turf |
| A4. The tip line | A payphone number where anyone can report a hideout or a pole for a small reward; the gang learns "someone talked" | Social tension and a civilian way into crime-fighting; ties the payphones in | M | comms, police, standing |
| A5. Dead drops | The outposts post contracts: "leave 10 cartons at the bus shelter in Provins by dawn". A hidden crate in a village takes them | A driving loop between outposts, villages and hideouts; uses the vehicles and cargo work | M | vehicles, chains, outposts |
| A6. Pirate radio | The north outpost runs a shortwave station any radio can tune: period music and coded lines between songs that hint at the dead drops and the next boat | Ambience that teaches the underworld loop without a window | S to M | comms, sound, outposts |
| A7. Protection money | A gang holding a pole can "collect" from one NPC shop in that village once a day; the shopkeeper calls police half the time | A turf payoff that police can answer | M | turf, shops, police |
| A8. Most wanted gangs board | A board at the police station listing gangs by recent crimes, with their flags | Police have a target list; gangs get notoriety to brag about | S | police, crime record |
| A9. Wake for the fallen | A member killed in a war leaves a cap or flag at the hideout's memorial spot | A small emotional anchor; no mechanics | S | hideouts |
| A10. Prison economy | Cigarettes as currency inside the prison; a visitor can pass a pack through the yard fence | Makes jail time part of the gang loop, not dead time | M | jailing, Behind bars |
| A11. Front business | A gang can launder its fund through a leased shop front (a café in a turf village) | Ties laundering, turf and property; a reason to hold a village | L | laundering, property, turf |
| A12. Gang vehicle registry | The hidden garage vehicle carries no plate on police checks | Gives the garage perk a clear edge and police a counter (stop and search) | S | vehicles, license plates, police |
