# Roadmap

The gameplay and release plan. Milestones are the one plan: each ends with a
public build and a filmed showcase, and a milestone's features do not start
before the previous milestone is done ([direction](../direction.md)).

A **scope** is one bounded plan: the player loop, the assets it may use, the
first slice, and how to prove it. Scope cards live in [scopes/](scopes/README.md)
and each names its milestone. The site's roadmap page (`website/`) renders these
docs with the milestones, their issues, and each epic's sub-issues from GitHub.
The November 2025 community checklist is in git history
(`website/content/archive/roadmap-2025.json`, removed in `4c96b4e`).

## Milestones

Listed in the order they run (restructured 2026-11-14).

| # | Milestone | Status | Plan |
| --- | --- | --- | --- |
| M1 | Playtest Town | **Closed** 2026-11-14. The Workshop publish (#109) moved to M3 | [m1-playtest-town.md](m1-playtest-town.md) |
| M2 | Living Town | **Current.** Built; finishing four small fixes, then a playtest pass: moderation, medic, comms, investigation (epics #145-#148) | [m2-outline.md](m2-outline.md) |
| M3 | Framework Release | Next: two addons, config layer, the content-pack pieces needed before the first publish (#960-#965, #957, #958), record shapes (#746), generator, owner docs, example server. Nothing is published until the whole addon move is done | [m3-outline.md](m3-outline.md), execution plan [m3-plan.md](m3-plan.md) |
| M2.5 | Showcase and Outreach | Runs **after M3**, before M4, so outreach points at a build people can get: hero video, landing page, Discord onboarding, outreach kit, playtester roster (epic #566) | [m2-5-outline.md](m2-5-outline.md) |
| M4 | Flagship Live | Hosting and ops, staff and community, the live 1989 server (epics #230-#232) | [m4-outline.md](m4-outline.md) |
| M5 | Flagship Story | Trimmed to what follows launch: the arrival arc, gazette, voiced NPCs, the item catalogue and the realistic weapon and clothing split (#988-#990), the event shell and shared pieces, the asset pipelines (#339-#343). Two tracks: Framework #632, Flagship #633 | [m5-outline.md](m5-outline.md) |
| M6 | Homes and Land | Property, renting, the dacha, farming (#747), and **Residents**: named NPCs on timetables that make a small server feel inhabited (#991 core, #992 cast) | epic #747 |
| Backlog | Unscheduled | Filed ideas and framework/flagship pairs that are not next. Each stays in its epic; an item moves to a milestone when it is picked | GitHub milestone "Backlog" |

Beside them runs **Development** (tooling and pipelines), which is not a release.

Foundation designs that span milestones: [building-foundation.md](building-foundation.md), placing
items and props with a preview (epic #927).

Each outline is planned in full only after the milestone before it closes.

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
  does not add a weapon pack, a vehicle pack, or aircraft.
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
| [Events](scopes/later/events.md) | World events | Parked |

"In the tree" means the system exists; [features.md](../features.md) is the
ground truth for its behaviour, and the card is kept as design intent. Revive
folded into the M2 medic theme (#146). Other placed issues: police application
#128 and jail work #127 (M2), day and night #129 (M4), bus stops #203 and trash
cans #205 (M5).

## How to add a scope

Copy the headings in [scopes/README.md](scopes/README.md). Put the file under
`scopes/<area>/`. Name the milestone. If it needs a new mesh, a new weapon, or a
new vehicle, it does not belong in the core or the flagship pack.
