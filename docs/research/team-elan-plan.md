# Team ELAN wiki: integration plan

A page-by-page read of the Team ELAN Arma Life player wiki
(wiki.ph-elan.com, 67 pages, read 2026-11-11), turned into a plan for the
framework (core) and the flagship (`LF_Everon1989`). The per-page notes are in
[team-elan-pages.md](team-elan-pages.md). Read it with the
[Life/RP benchmark](life-rp-benchmark.md) and the
[Altis Life note](altis-life-framework.md).

**What ELAN is.** A hobby-built Arma Reforger life mod on Everon for up to 128
players, live today, with a deep civilian economy, gangs that fight over two
hideouts, and an applied-for police force. It is the closest live competitor:
same engine, same island. Its code is not public. We take ideas only, in our own
words; no wiki text or numbers are reused as ours.

**Status.** A plan, not filed. Nothing here is an issue until the maintainer
picks it. Milestones follow the roadmap: M3 for anything that shapes saved data
or the owner schema, M4 for live ops, M5 for systems and content (Framework and
Flagship tracks), M6 for homes and land. M2 is locked.

## What makes ELAN work

- **Fixed rules, player decisions.** Fines come from a law table, prices from a
  formula, ranks from a table. The server never improvises, so the choice of
  charge, crop or route is the game.
- **Every crime is an announcement.** Smoke from a cook, an alarm and a marker
  for a robbery, a dispatch call. Crime becomes a story for someone else, which
  is our player-drama hook.
- **Scarce, public, decaying rewards.** Held ground pays rent per hour and
  decays, a dealer spot pays less the more it is used, market prices fall as a
  good is farmed. Rewards that must be maintained keep the loop alive.
- **Hauling is the progression.** A house, a crafted item, an event: each is a
  pile of goods carried and worked at stations, not a timer.
- **Declared free-fire contexts instead of PvP bans.** Gang wars, contested
  spots and events say when killing is free; everywhere else it costs.
- **Gates name what is missing.** Every refusal says which level, licence or
  profession you lack.
- **What we do not copy.** Almost all of their information layer is a
  smartphone (dispatch, market, wanted list, car finder). In 1989 each becomes a
  board, a paper form, a radio or a person. Also out: helicopters, tasers, speed
  cameras, cross-server accounts, event-point currencies, reputation-priced shops.

## 1. Decide in M3 (saved data and owner schema)

These shape persisted records or the config schema. Adding them after owners
build on M3, or after the persistent launch (#556), means a migration. Most need
only the record shape and config fields now; behaviour can follow in M5.

| Item | ELAN source | What to settle in M3 | Status |
| --- | --- | --- | --- |
| Dynamic market | Dynamic market, Goods | Per-server price state per good: floor, ceiling, current, recovery rate; owner-tunable curve | Decided 2026-11-07, **no issue found** |
| Crime catalog depth | Laws, Police | #685 also covers escalation, no-bail crimes, repeat offences, licence seizure | Extend #685 |
| Skills by doing | Skills, Professions | A per-character XP map per skill, and job-plus-level unlock gates in config | Gap |
| Organisation ranks, rights and wars | Gangs, Gang conflicts | Rank table with rights bits; a persisted conflict record (declared, accepted, ends) | Partly built (`LF_OrganizationManager`) |
| Held ground | Gang spots, Supremacy | Hold time and points per site on the hideout record | Gap |
| Vehicle record fields | Garage, Vehicle tuning, Vehicle guide | Slot cap, tow state, inspection date, hidden-compartment contents, cargo-capacity tuning | Extends #688 |
| Personal lockers | Lockers | Storage keyed by character and site, surviving death and restart | Gap; storage exists |
| Item condition | Weapons, Outpost | Whether items carry wear and a repair count; decide yes or no before the schema ships | Decision needed |
| Hidden reputation | Reputation | A per-character record of how they treat people, never shown as a number and not a price modifier | Gap |
| Police career schema | Police, Police application | Ranks with pay and qualifications (driving, firearms, detective) as config | Extends `LF_PoliceRanks` |
| Zone rule sets | Rules, Combat zone | Rule overrides scoped to a zone (lawless, event, safe) | Partly built (#242) |
| Planted crops and race records | Growing, Racing | Placed-plant persistence and a record-time table, if wanted | Decide, build later |
| Property record | Housing | Plot, owner, build stage, contents, so M6 does not reshape saves | M6 shape, reserve now |

## 2. Framework systems (M5 Framework track)

Each is era-neutral and config-driven; the flagship row in section 3 is the
content that uses it.

| System | ELAN source | Notes |
| --- | --- | --- |
| Market price curve service | Dynamic market | Reads section 1 state; a public price board at each trader, no app |
| Skill XP and unlock gates | Skills, Professions | One unlock per job level; no perk tree, no XP popups |
| Practical licence exams | Licenses, Shooting range | A course runner: checkpoints, timer, examiner NPC, pass issues the licence; a restart refunds |
| Event shell | Prison break, Weapon transport, Airdrop | One reusable shell: world-state preconditions, prep, a hot phase that lifts zone rules, cooldown, hunter-only reward |
| Declared organisation wars | Gang conflicts | Declare at an NPC, answer window, both sides announced; incident log tags the kills |
| Contested sites with rent | Gang spots, Supremacy | Capture by holding ground; points accrue per hour and decay |
| Staged break-in with defenders | Evidence locker | Two workers at different points, a defender reset, a fill-level hint for scouts; builds on `LF_Breach` and #662 |
| Garage slots and towing | Garage | Slot cap with rising slot price, tow-to-impound after abandonment; pairs with #690 cleanup |
| Personal locker | Lockers | Rent once at a site; private and persistent |
| Vehicle inspection and hidden compartments | Vehicle tuning | Police check a sticker; compartments hide goods from a quick search |
| Crew robbery roles | Gas station robbery | Upgrade our single-till robbery: lookout, holder, a police response window |
| Hands-up as a state | Gestures | Search and robbery read it; new animations only through the animation pipeline |
| Recycler | Recycler | Break an item into part of its inputs, always at a loss; needs the M5 crafting bench |
| Stall board market | Auction house | Player listings at a fixed price, paid in cash at a clerk; never bank-to-bank |
| Fuel station stock | Vehicle refueling | Stations hold real stock that the fuel-delivery job refills; buy only while there is room |
| Race runner | Racing | Entry fee into a cash pot, checkpoints, a record board at the track |
| Gazette from the event bus | Press | A printed town paper built from incidents and events; no volunteer staff needed |
| Onboarding chain | Onboarding, Beginner's guide | Extends the M5 arrival: a short task chain that walks the loop, with a non-cash welcome gift and a grace period |

## 3. Flagship content (M5 Flagship track, 1989 Everon)

- **Market goods** and their curves for the village economy; a chalk price
  board at the trader.
- **Driving test at the station** and a **firearms test at a range**, with
  period forms and an examiner.
- **Militia-era police career**: ranks, pay, qualifications, checkpoint kit.
- **The evidence-locker convoy**: when the locker fills, police truck the stock
  out and anyone can hit the trucks.
- **A prison break** as a rare, player-started gang job on the M2 jail and
  jailbreak.
- **The hideout as contested ground** for the one lawless area.
- **Cook failures and a rotating buyer** in the illicit chains (#417).
- **Fishing, beehives and honey** as quiet civilian income, if base-game props
  exist (check), otherwise through the 3D harness.
- **A collective-farm fuel depot** feeding the fuel stations.
- **Village texture**: chess boards at the café, a dirt race loop, a shooting
  range, a parish or community hall as a social hub.

## 4. Live ops and community (M4)

- Police application gates: playtime (#692), a cooldown after a rejection, a
  trial period that expires (#128, #314).
- Moderation process, appeals, and rules copy (#312, #313, #315), borrowing
  their tickets-not-chat rule.
- A clip-recording guide for reports, with an in-game corner stamp (time,
  server) so a clip can be matched to the incident log.
- Discord account verification for support and whitelists. Not their bonus
  paycheck, which pays from outside the world.
- A one-page FAQ and an "external mods" explanation for players.

## 5. Homes and land (M6)

- **Housing built by hauling**: buy a plot, carry sand and planks, work build
  stages together. The strongest social engine in their wiki.
- **Growing**: hidden, unprotected plants that need watering; other players are
  the threat.
- A house spawn point once housing exists (rejoin-in-place #640 covers today).

## Not a fit or parked

| ELAN feature | Why |
| --- | --- |
| Smartphone and its apps | Wrong era, and it breaks "the place is the interface" |
| Helicopters, military rifles, Humvees, the Outpost shop | No aircraft or weapon packs |
| Tasers, speed cameras | Not 1989 |
| Meth | Wrong era; our hard chains fill the slot |
| Combat Zone, scheduled community events, event points | Events are parked; prize currencies sit outside the economy |
| Cross-server accounts, streaks, achievements, Elo | Per-server persistence, no external database |
| Reputation as a price modifier | Keep reputation hidden and social |
| Machines that run unattended, public machine boxes | Against hands-on stations, and griefable |
| Blueprint rarity rolls | An MMO loot loop, not 1989 drama |
| Server-wide "new miner" announcements, XP popups | Hidden pacing |
| Light of Elan lore | Their story; ours is the 1989 relocation |

## Suggested next steps

1. File the M3 decisions first (section 1): the dynamic market issue, the #685
   extension, and a decision issue for item wear and for reputation.
2. File the section 2 and 3 items as paired M5 Framework and Flagship issues,
   in the order listed.
3. Add the section 4 items to the M4 epics (#230, #231).
4. Fold section 5 into the M6 outline when it is written.
