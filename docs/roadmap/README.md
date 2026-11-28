# Roadmap

> **Now: first 2 hours polish, then 0.2.1.** The maintainer's focus since 2026-11-22 is the first two hours on
> Everon (hostel bed, clerk and welcome pack, starter car, apples, juice, market, the F2 starter quests):
> polished and easy to learn, every step marked, every screen simple. After the 0.2.1 wave lands, a full
> first-two-hours review with two fresh characters; its fixes come before any new system. The wishlist below
> waits for that. The next major release on `main` is planned in [next-release-0.3.0.md](next-release-0.3.0.md)
> (the 10-hour arc, the cut and the week plan), waiting for the maintainer's approval. What the 0.3.0 playtest
> week left open, ranked by player impact, is in [after-030-backlog.md](after-030-backlog.md).

The gameplay and release plan. Milestones are the one plan: each ends with a
public build, and they run in the order below ([direction](../direction.md)). The next
milestone starts once the one before it is code-complete and only playtest and
sign-off are left (M5 started on 2026-11-15 while M4 finished).

A **scope** is one bounded plan: the player loop, the assets it may use, the
first slice, and how to prove it. Scope cards live in [scopes/](scopes/README.md)
and each names its milestone. The site's roadmap page (`website/`) renders these
docs with the milestones, their issues, and each epic's sub-issues from GitHub.
The November 2025 community checklist is in git history
(`website/content/archive/roadmap-2025.json`, removed in `4c96b4e`).

## Milestones, chapters and foundation

A **milestone** is a release: a public build, then an update to the live server.
Inside a milestone, work is grouped into **chapters** (GitHub label `chapter`):
a story a player can live through end to end, in one 60 to 120 second video.
Every chapter issue has the same parts: a pitch from the player's view, a beat
sheet (the video, and the acceptance test), a **Framework (core)** section and a
**Flagship (1989 pack)** section listing its sub-issues, and a release note. A
chapter is done when its video can be filmed on `main` and the maintainer has
verified it; the clip and a post go out then, and the clips make the
milestone's trailer. Plumbing with no video is a **foundation** epic (label
`foundation`) and gets a line in the release notes.

After launch (M4), each update deepens the stage of a player's life most of the
community has reached, and carries one **story chapter** that moves the 1989
relocation arc on (ideas: #214).

Listed in the order they run (restructured 2026-11-14; status as of 2026-11-16).

| # | Milestone | Status | Chapters |
| --- | --- | --- | --- |
| M1 | Playtest Town | **Closed** 2026-11-14. The Workshop publish (#109) moved to M3 | [m1-playtest-town.md](m1-playtest-town.md) |
| M2 | Living Town | **Built, in playtest.** Most open issues wait on a playtest verdict ([m2-outline.md](m2-outline.md)) | Cops and robbers #1003 · The investigation #148 · Downed, not dead #146 · Crime pays #410. Foundation: moderation #145 |
| M3 | Framework Release | **Published as 0.1.0, in sign-off.** Both Workshop items are up (core `4550A2B656E1B569`, pack `B1E84D5687E11724`) and a clean server boots from them ([m3-outline.md](m3-outline.md), [release notes](../releases/m3-framework-release.md)) | Your town in five minutes #1004 · Make it yours #1005. Foundation: two addons #223, upgrade path #229, Workshop release #366, record shapes #746, patterns #684 |
| M2.5 | Showcase and Outreach | **Running alongside M4 and M5** now that there is a build to point at: weekly playtests, recruiting posts, contributor docs ([m2-5-outline.md](m2-5-outline.md)) | The showcase #566 |
| M4 | Flagship Live | **Finishing.** The alpha server is up behind a password; the first alpha session is Saturday 2026-11-21, 19:00 UK, then every Saturday ([m4-outline.md](m4-outline.md)) | Opening day #1006 |
| M5 | Settling in | **In development** since 2026-11-15, released chapter by chapter. First weeks after launch: every player is new ([m5-outline.md](m5-outline.md)) | Armed and dressed · Evenings in the village · On patrol · Arrival · story: The relocated |
| M6 | Making a living | **Outline in review** ([m6-outline.md](m6-outline.md)): map state 2, the working island. Players with jobs and money want more from them | The working island · Getting better at the job (with standing and professions) · The market moves (with the supermarket) · Gone fishing · Working the routes · Bigger wheels (new) · Scrap and salvage · The bank · story: The state factory |
| M7 | Power and turf | **Outline in review** ([m7-outline.md](m7-outline.md)): organisations form into gangs and crime escalates | Running a crew (new) · Gang war · The outposts (new) · Dirty money · The convoy job · The big heist · By the book (new) · Behind bars · Making detective · story: The shipment |
| M8 | Roots | Long-term players want something of their own | A place of your own · Working the land · Garages and lockers · Neighbours (pull forward if launch is quiet) · story: The anniversary |
| Backlog | Unscheduled | Filed, not next. An item moves into a chapter when it is picked | Catch the bus #1793 · GitHub milestone "Backlog" |

Beside them runs **Development** (tooling and pipelines), which is not a release.
The asset pipelines (#339 to #343) and the ArchBuilder harness (#995) are
foundation epics in M5.

## After 0.2.1: the maintainer's wishlist (2026-11-22)

Seventeen items the maintainer listed (item 16 is two heists) for after the 0.2.1 cut and the first-two-hours review. Each has an
outline section (goal, what a player sees, what it builds on, open questions, size) and one epic issue, or a
link to the issue that already covers it. No slices are cut until the maintainer approves the outlines.

| # | Item | Milestone | Section | Issue |
| --- | --- | --- | --- | --- |
| 1 | Truck dealer | M6 | [m6-outline.md "Truck dealer"](m6-outline.md#truck-dealer) | #4141 |
| 2 | Civilian helicopter dealer | M6 | [m6-outline.md "Civilian helicopter dealer"](m6-outline.md#civilian-helicopter-dealer) | #4142 |
| 3 | Rebel outposts | M7 | [m7-outline.md "Rebel outposts"](m7-outline.md#rebel-outposts) | #4145 |
| 4 | Bug: the vault drill and the playtest robbery kit | Bug (post-review fix list) | [below](#bug-the-vault-drill-and-the-playtest-robbery-kit) | #4156 |
| 5 | Vehicle and inventory progression | M6 | [m6-outline.md "Vehicle and inventory progression"](m6-outline.md#vehicle-and-inventory-progression) | #4144 |
| 6 | Gang hideouts across Everon | M7 | [m7-outline.md "Gang hideouts across Everon"](m7-outline.md#gang-hideouts-across-everon) | #4146 |
| 7 | Gang flags | M7 | [m7-outline.md "Gang flags"](m7-outline.md#gang-flags) | #4147 |
| 8 | Gang radio | M7 | [m7-outline.md "Gang radio"](m7-outline.md#gang-radio) | #4148 |
| 9 | Scroll to recruit | M7 | [m7-outline.md "Scroll to recruit"](m7-outline.md#scroll-to-recruit) | #4149 |
| 10 | Gang members on the map | M7 | [m7-outline.md "Gang members on the map"](m7-outline.md#gang-members-on-the-map) | #4150 |
| 11 | Truck and pilot licences | M6 | [m6-outline.md "Truck and pilot licences"](m6-outline.md#truck-and-pilot-licences) | #4143 |
| 12 | Prison break as a gang event | M7 | [m7-outline.md "The prison break as a gang event"](m7-outline.md#the-prison-break-as-a-gang-event) | #798 (filed; chapter #1027) |
| 13 | The law book and the charge sheet | M7 | [m7-outline.md "The law book and the charge sheet"](m7-outline.md#the-law-book-and-the-charge-sheet) | #4151 |
| 14 | Vehicle livery pipeline (spike) | Development | [below](#development-the-vehicle-livery-pipeline-spike) | #4155 |
| 15 | The 1989 drug chains | M7 | [m7-outline.md "The 1989 drug chains"](m7-outline.md#the-1989-drug-chains) | #4152 |
| 16a | The reserve heist (top of the robbery ladder) | M7 | [m7-outline.md "The reserve heist"](m7-outline.md#the-reserve-heist), [ladder](m7-outline.md#the-robbery-ladder) | #4153 (building #2283) |
| 16b | The hospital robbery | M7 | [m7-outline.md "The hospital robbery"](m7-outline.md#the-hospital-robbery) | #4154 |
| 17 | Professions and skills | M6 | [m6-outline.md "Professions and skills that feel worth it"](m6-outline.md#professions-and-skills-that-feel-worth-it) | #4140 |

### Development: the vehicle livery pipeline (spike)

Epic: #4155.

- **Goal.** Make it easy to repaint existing vanilla cars, trucks and helicopters as Police, Ambulance, Civilian
  and Rebel. One-off liveries exist (the postal van's "POSTAL SERVICE" from #4088, the police cars); the spike
  finds a repeatable pipeline.
- **What it delivers.** A livery is a material override per variant (or a decal or texture layer, if Enfusion
  supports one on that vehicle), a template per vehicle UV, and a CLI step that builds the variant prefab with US
  English lettering. Then it is proven on two vehicles, a car and a helicopter, before any rollout.
- **Builds on.** The vehicle pipeline (#342), car colours (#1826, a colour prefab per variant), the postal van and
  police car liveries, the texture tools in `tools/` and `wb_preview` / `look item` for the pictures.
- **Open questions.** S1. Which mechanism: material swap, decal slots or texture layering? **Recommend research
  first** (`api class`, the vanilla variants' prefabs) and pick the one that survives game updates. S2. Mastery
  liveries (M6 professions) and civilian helicopter paint (M6) use it: **recommend the spike lands before either**.
- **Size.** S to M.
- **Acceptance.** `tools\cli` builds a Police and a Civilian variant of one car and one helicopter from a template;
  `look item` pictures of all four; the text reads in US English; no GUID reuse.

### Bug: the vault drill and the playtest robbery kit

Epic: #4156.

The maintainer reports (2026-11-22) that the vault drill cannot be picked up or does not work, and the robbery
kit in the playtest town does not work. Related open review findings are in #2418 (vault offers dead on a
dedicated client, the fuse path, `IsDrillingBy` read on the client). Reproduce on a dedicated server first. It
can go into the post-review fix list if the first-two-hours review touches crime.

Foundation designs that span milestones: [building-foundation.md](building-foundation.md), placing
items and props with a preview (epic #927).

Each milestone is planned in full when the one before it is code-complete.

## Rules for every scope

- **Framework, then flagship.** Ship the hook and one 1989 Everon setup. Server
  owners retune items and places in config. Core is era-agnostic; 1989 lives in
  the `LF_Everon1989` pack from M3.
- **1989 Everon.** Flagship jobs, places, and copy fit the period. A taxi rank, a
  fuel delivery, a parcel run. No phone apps.
- **Two tracks from M5.** Every M5 item is split into a Framework issue (the
  system, in the core) and a Flagship issue (the content, in `LF_Everon1989`).
- **Base-game assets.** Doors, vehicles, weapons, medical supplies, fuel cans,
  and buildings come from Arma Reforger. A scope configures those prefabs. It
  does not add a weapon pack, a vehicle pack, or new aircraft assets; the base game's
  helicopters are base-game vehicles (the M6 helicopter dealer).
- **Money stays physical.** Cash is the MoneyStack. The ATM is the only
  cash-to-account boundary. Stolen money leaves as cash.
- **In the world.** The place is the interface: a bank vault, a jail door, a
  till, a hideout machine. See [design](../design.md).
- **Reuse before writing.** [sources.md](sources.md) names the script or prefab
  to extend: this repo first, then Arma Reforger, then Overthrow and
  [LNGCLifeRP_Public](https://github.com/SouLGenetic/LNGCLifeRP_Public) (MIT).
  [references.md](references.md) lists outside projects to check for ideas.
- **Three gates.** [testing/README.md](testing/README.md) is the headless case,
  the DebugWorld entity, and the screenshot for each setup.
- **Proof classes.** In-session checks do not prove a restart. A host-only run
  does not prove a dedicated server.

## Scope cards

| Scope | Player loop | Milestone |
| --- | --- | --- |
| [Forced entry](scopes/crime/forced-entry.md) | A charge opens a door or container until someone repairs it | In the tree (M1) |
| [Bank robbery](scopes/crime/bank-robbery.md) | Breach the vault, carry the cash out in bags | In the tree (M1) |
| [Hospital robbery](scopes/crime/hospital-robbery.md) | Take the till, breach the medicine cabinet | In the tree (M1) |
| [Contraband](scopes/crime/contraband.md) | Gather, process at the hideout, run a convoy | In the tree (M1) |
| [Jailing](scopes/police/jailing.md) | A sentence at a placed jail | In the tree (M1) |
| [Police ranks](scopes/police/ranks.md) | Rank gates duty kit drawn from vanilla gear | In the tree (M1) |
| [Organizations](scopes/ownership/organizations.md) | Persistent groups, shared funds, bought upgrades | In the tree (M1) |
| [Hideouts](scopes/ownership/hideouts.md) | The building an organization owns | In the tree (M1) |
| [Vehicles](scopes/ownership/vehicles.md) | Own and buy base-game vehicles | In the tree (M1) |
| [Resources](scopes/work/resources.md) | The legal gather, process, sell chains | In the tree (M1) |
| [Civilian jobs](scopes/work/civilian-jobs.md) | Taxi, fuel delivery, parcel delivery, with levels | In the tree (M1) |
| [UI kit](scopes/later/ui-kit.md) | Shared 1989 menu parts | In the tree; menu migration not planned |
| [Jailbreak](scopes/crime/jailbreak.md) | The same breach opens the jail door | M2 |
| [Gas station robbery](scopes/crime/gas-station-robbery.md) | Rob the till | M2 |
| [Lockpicking](scopes/crime/lockpicking.md) | Pick doors and cars by hand; hotwire | M2 |
| [Chop shops](scopes/crime/chop-shop.md) | Steal a car, dismantle it for cash; stolen-plate reports | M2 |
| [Illicit chains](scopes/crime/drug-chains.md) | Tiered production chains on hands-on stations | M2 (system, easy chain), M5 (other chains) |
| [Police missions](scopes/later/police-missions.md) | A mission board for on-duty police | M4 |
| [Private housing](scopes/later/housing.md) | Buy, rent, and furnish a home; the dacha | M5 |
| [Crafting](scopes/later/crafting.md) | A general bench; cooking and food | M5 |
| [Tasks](scopes/later/tasks.md) | Workplace tasks; the state factory | M5 |
| [The supermarket](scopes/later/supermarket.md) | Full shelves of 1989 groceries, paid at the till | M6 |
| [The clothing store](scopes/later/clothing-store.md) | 1989 clothes on mannequins and folded stacks, paid at the till | M6 |
| [Events](scopes/later/events.md) | World events | Parked |

"In the tree" means the system exists; [features.md](../features.md) is the
ground truth for its behaviour, and the card is kept as design intent. Revive
folded into the M2 medic theme (#146). Other placed issues: police application
#128 and jail work #127 (M2), day and night #129 (M4), trash cans #205 (M6), bus stops #203
(Backlog).

## How to add a scope

Copy the headings in [scopes/README.md](scopes/README.md). Put the file under
`scopes/<area>/`. Name the milestone. If it needs a new mesh, a new weapon, or a
new vehicle, it does not belong in the core or the flagship pack.
