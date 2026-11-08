# M1: Playtest Town

M1 is done when a stranger joins a dedicated server hosted from the maintainer's
PC and gets through the whole loop with nobody helping. They take a short
guided arrival, read 1989 signs, and finish spawn → job → cash → ATM
→ shop → vehicle → crime → police response → arrest, fine, or jail →
organization, hideout, garage. After a server restart, what they earned is
still there. Every step runs on a dedicated server with several clients. Every
step gives visible feedback in the world. Every step is proven from the
player's view by `agent-box` and has a filmed clip
([direction](../direction.md), [decisions](../decisions.md)).

Baseline: `main` at `2bb73b2` (2026-11-07). `release/next` (#56), the dev gate (#71), and dedicated client state (#83) are merged. Status below
is from reading the code and the audit. Nothing has been played on a dedicated
server yet, so "works" means "works as Workbench host".

GitHub: milestone [M1 Playtest Town](https://github.com/Life-Framework/Life-Framework/milestone/1),
label `m1`; one issue per scope (§e).

## Starting line: M0 in flight (not re-planned)

M1 scopes depend on these PRs by number and do not repeat their work. #71, #77,
#79, and #83 merged on 2026-11-07 with full-suite runs and observed red-proofs.
The rest are being finished the same way. The maintainer's answers to the
plan's open questions are at the end ([Decided 2026-11-08](#decided-2026-11-08)).

| PR | What M1 needs from it |
| --- | --- |
| #71 (merged) | `EL_DevGate`: debug menu and harness RPCs gated server-side (the multi-client harness depends on it) |
| #73 | Arrest/fine/armory/ATM/org/garage range checks; jail teleport through vanilla |
| #76 | Tests that can fail; shop partial-funds and ATM boundary tests |
| #77 (merged) | Bank not wiped on first connect; jail sentence in save v5; no ground key on vehicle restore; plate registry; whitelist safe load; `-loadSessionSave` |
| #79 (merged) | Dupes: quest turn-in, cash split, breach needs a charge, locked storage, job-switch guards |
| #80 | Lifetime: job equipment GUIDs, rob cooldown units, breach stuck-armed, contraband watch, disconnect cleanup |
| #83 (merged) | Dedicated clients: replicated faction/duty/wanted/rank, creation flow over RPC, wanted list over RPC, private notifications, license bitmask |
| #97 | UX foundation: the missing toast layout (every toast fails today), UI kit gaps, menu input context, menus opening on the wrong machine |
| #111 | Filming harness spike (ffmpeg `ddagrab` + NVENC). M1-06 is its acceptance |

#97 edits the same police menu, duty, confiscation, and faction-screen files
that #83 changed. Rebase #97 onto main before it merges. A wave-1 scope starts when its
listed dependencies are merged. It does not wait for all of M0.

## a. The player journey

Status key: **works** = works as Workbench host, never proven on dedicated;
**works (code)** = fixed for dedicated in merged code, not yet seen live;
**broken**; **missing**; **UI** = works but lives behind a menu that should be
in the world (§b).

| # | Step | Where | Status | Evidence |
| --- | --- | --- | --- | --- |
| 1 | Join the hosted server | maintainer PC | **missing** | No playtest mission (only `Missions/EL_DebugTest.conf`). No host command. No auto-load of the last save (#77 only documents `-loadSessionSave`). No dedicated + multi-client session has ever been recorded. → M1-03, M1-05 |
| 2 | Choose a faction (police whitelisted) | faction screen | **UI**, to delete | #83 (merged) moved the picks to server RPCs (`EL_CharacterCreationController.c`). Decided 2026-11-08: no faction screen; everyone spawns civilian and police clock in at the duty desk. → M1-11 deletes the screen |
| 3 | Name and age | creation screen | **broken** | `EL_CharacterCreationManager.CreateAndSpawnDefaultCharacter` skips the screen, so every character is "Default Citizen". → M1-08 |
| 4 | Spawn and orientation | civilian spawns 100-110, 130-137 (`Gamemode.layer`) | **missing** | No signs. The only labels are debug text (`EL_TextLabelComponent` → `DebugTextWorldSpace`). `QuestGiver_Start` (110,137) offers test quests: talk, kill, timed. → M1-01, M1-09, M1-18 |
| 5 | ID card | inventory | **missing** | `Prefabs/Items/Roleplay/IDCard.et` exists, but nobody is given one. `EL_ShowIDAction` shows a toast. → M1-08 |
| 6 | Take a job | `Roadmap_Job_Depot` 228,120 (post counter) | **works** | `LF_CivilianJob` signup and runs. Job strings are Spanish literals (`EL_PlayerJobComponent`). Feedback is a toast, which fails until #97. |
| 7 | Earn cash | depot → `Roadmap_Job_Drop`, traders | **works** + **UI/broken** | A run pays cash once. Gather → trader pays cash. The passive paycheck pays $700 every 300 s to every job, including UNEMPLOYED. That invisible mint makes every other payout irrelevant. → M1-07 |
| 8 | Bank it | `ATM_Debug` 139,130 | **works**, **UI** | `EL_ATMMenu` moves real cash with rollback. It is a MenuManager menu. → M1-13 |
| 9 | Shop | general store 144,130 | **works**, **UI** | `EL_ShopMenu` opens for the local player whoever interacted (features.md). Stock is infinite. → M1-13 |
| 10 | Buy a vehicle | `Roadmap_Vehicle_Lot` 243,120 | **works** (in-world) | Each vehicle has its own buy action. The key is a real item and the lock and plate are real. → proven by M1-23 |
| 11 | Rob a till | `Roadmap_Fuel_Till` 201,120, `Roadmap_Hospital_Till` 195,120 | **works** (code) | Offers read a server-only account on clients. #83 (merged) moved them to `EL_AccountReplication`. Not yet seen live. DebugWorld sets min police 0. |
| 12 | Rob the bank | `Roadmap_Bank_Vault` 189,120 + bag | **works**, ⚠ dedicated | `LF_Bank` still reads accounts directly in its offer (#83 ⚠). Bags are real carried cash. Decided 2026-11-08: no police minimum on the flagship (0 for every crime); a louder alarm when no officer is on duty. → M1-20 |
| 13 | Hospital cabinet | `Roadmap_Hospital_Cabinet` 198,120 | **works** | Breach → medicine items. |
| 14 | Contraband run | `Roadmap_Convoy_Start/Drop` 222/225,120 | **works**, **broken look** | The cargo is `RowanBerries.et`. The markers are `EL_DebugAreaBoundary` debug meshes. → M1-14 |
| 15 | Police clock in | `Roadmap_Police_Desk` 204,120 | **broken** | The desk has **no components** (`Roadmap.layer`). Duty is at `PoliceStation_Debug` (170,120) on `Police.layer`. `docs/playtest.md` sends players to the wrong place. The desk becomes the only way to become police: whitelist check and loadout there. → M1-11 |
| 16 | Police learn of a crime | toast | **broken** | Alerts are toasts, and toasts fail until #97. Nothing happens at the site. → M1-20 |
| 17 | Find the suspect | police menu | **broken** | The wanted list now comes over RPC (#83), but the layout does not render `GetWantedPlayers()`. The menu buttons are unlabeled. → M1-11 |
| 18 | Arrest and escort | on the suspect, `Roadmap_Escort_Car` 204,108 | **works** | `LF_JailArrestAction`, `LF_JailPutInVehicleAction`. Ranges in #73. |
| 19 | Fine | police menu | **UI**, **broken** | A remote action takes cash at any distance (`EL_PoliceMenu.RpcAsk_EL_PoliceFine`). The menu is a stub. → M1-19 |
| 20 | Jail | `Roadmap_Jail_Cell` 192,122 | **works** | Timed sentence. #77 persists it. The prisoner gets no slip and release is silent. → M1-21 |
| 21 | Found an organization | `Roadmap_Org_Desk` 210,120 | **works**, **UI** | The desk screen has create, deposit, withdraw, and upgrade. Invite and kick are unreachable from the world (`EL_OrganizationManager` only). → M1-17 |
| 22 | Hideout | door 213, machine 216, stash 219 (z 120) | **works** / **missing** | The door and machine work. The stash has no storage component. → M1-15 |
| 23 | Garage | `Roadmap_Garage` 186,102.1 | **works**, partly unreachable | Store and retrieve work. Retrieve spawns at the lot pad. The `LF_Garage` catalog screen is unreachable. There is no impound rule. → M1-16 |
| 24 | Restart keeps it | server | **broken** | Carried items and cash reset to the starter kit. Careers, levels, police rank, licenses, and quests are not saved. Bank and org fund, hideout, and vehicle records are saved. → M1-03, M1-04, M1-10 |
| 25 | Die and respawn | death screen | **works**, unproven on dedicated | The body keeps what it carried (matches decision "Death drops what you carry"). The respawn RPC bridge has never run on a multi-client server. Decided 2026-11-08: you wake in a hospital bed and pay a small bill from the bank (debt if broke); today respawn is free at a civilian spawn. → M1-26, M1-23 |

## b. In-world redesign

The test is "how would this exist in the world, and does doing it in the world
make it more fun or just slower?". **Adopt** = in M1. **Later** = right idea,
not needed for the loop. **Keep UI** = the in-world version adds friction
without adding play, so the UI stays and is made to look like an object.

| System | Today | In the world | Verdict |
| --- | --- | --- | --- |
| Orientation and help | none; debug floating text | Notice boards, pub signs, A4 notices, cardboard signs; Read opens a paper card | **Adopt** (M1-01, M1-09) |
| Arrival | test quest giver | Town clerk at the spawn plaza hands you your ID card and sends you on four errands | **Adopt** (M1-18) |
| Faction choice | screen at first join | Everyone spawns civilian; police clock in at the station desk, where the whitelist is checked and duty sets the police loadout | **Adopt** (M1-11). Delete the faction screen (decided 2026-11-08) |
| Name and age | never shown | Registration form, typed; it does not pick a faction | **Keep UI** (typing a name), styled as a 1989 form (M1-08) |
| ID | toast | `IDCard.et` carried, shown face to face, and police can ask for it | **Adopt** (M1-08, M1-12) |
| Meeting people | scattered actions | One person-to-person menu: introduce, show ID, give cash or item, role actions. After an introduction the name shows only up close, and is remembered across sessions | **Adopt** (M1-12) |
| Job signup and runs | actions at the post counter | Already in the world: parcel, jerrycan, taxi | Keep; add signs |
| Paycheck | $700 timer for everyone | Delete it. Civilians are paid in cash at the drop. Police get duty pay in cash at the desk when they clock out. Broke players collect an unemployment benefit at the counter | **Adopt** (M1-07) |
| Pay envelope / pay slip item | – | An envelope holding the cash | **Later**: it adds a step and no play |
| Selling goods | drop into trader crate → cash | Already in the world | Keep |
| ATM | menu | Terminal-screen UI at the machine; typing an amount is a correct UI | **Keep UI**, restyled (M1-13) |
| ATM card and receipt | – | A card needed to use the ATM, and a paper receipt | **Later**. The card adds stakes only once robbery or loss of it matters. The receipt is clutter |
| Shop | list menu | Goods on shelves (`ShelfShop_01`), pick up, pay at the till | **Later**: needs stock, theft rules, and a checkout. M1 keeps a paper price-list UI (M1-13) |
| Vehicle buy | per-vehicle action at the lot | Already in the world | Keep |
| Keys and locks | real key item | Already in the world | Keep |
| Garage | actions + unreachable catalog | Park at the garage, retrieve on its pad, read a ledger card; ungaraged cars go to impound for a fee | **Adopt** (M1-16). Delete the catalog purchase path |
| Robbery, breach, bank bags, cabinet | actions, charge item, carried bags | Already in the world | Keep |
| Police alert | toast | Alarm at the site, a radio chirp to on-duty officers, a dispatch line on the station board. With no officer on duty the alarm is louder and more visible | **Adopt** (M1-20). Payphones and pagers are **later** |
| Wanted list | menu stub | Wanted board at the station; posters with names for civilians, details for on-duty police | **Adopt** (M1-11) |
| Duty | action at a debug station off the street | Clock in and out at the station desk (`ClockPunch_01`). Clocking in stores the civilian inventory and restores the police one; clocking out swaps back | **Adopt** (M1-11). Replaces the once-per-shift armory for M1 |
| Death and respawn | death screen, free respawn at a civilian spawn | Wake in a hospital bed; a small bill from the bank, debt if broke | **Adopt** (M1-26) |
| Fine | remote menu, any distance | Officer writes a paper ticket face to face. The suspect carries it and pays in cash at the police desk within N minutes. Unpaid, it doubles once; unpaid again, +1 wanted. Never from the bank. A paid ticket stays as a receipt | **Adopt** (M1-19). The amount entry is a UI field |
| Arrest and escort | actions, vehicle seat | Already in the world | Keep |
| Jail | silent timer | Sentence slip with the release time; door sound on release | **Adopt** (M1-21) |
| Confiscation | deletes weapons | Seized items go to an evidence locker at the station | **Later** |
| Organization | desk menu | Ledger-page UI at the registry desk. Invite and remove are person-to-person actions | **Adopt** (M1-17). The ledger stays a UI: it needs typing and a list |
| Org fund | number in a menu | Cash in a safe at the hideout that can be robbed | **Later**: it changes the economy and needs persistence |
| Hideout stash | crate mesh | A real container for members only | **Adopt** (M1-15) |
| Licenses | skill points, no player path (`PurchaseLicense` has no caller) | Card items issued at a town-hall desk | **Later**. Not on the M1 loop. The gun shop comes with it |
| Notifications | toasts (broken) | World cue first (sound, item, door); toast second | **Adopt** as a rule in every scope. #97 fixes toasts |
| Quest log | menu | Paper notebook | **Keep UI** |
| Death screen, debug menu, survival HUD | UI | – | **Keep UI** |

## c. Signs

**Prefabs.** Each one inherits a base-game prop, so the mod ships no new meshes.

| Prefab | Parent (vanilla) | Use |
| --- | --- | --- |
| `LF_Sign_Board` | `Prefabs/Props/Civilian/NoticeBoardCork_01.et` (or a `Filled/` variant) | Town map at spawn, police board, job board |
| `LF_Sign_Pub` | `Prefabs/Props/Commercial/SignPub_01_base.et` | Shopfronts: store, bank, garage, post office |
| `LF_Sign_Notice` | `Prefabs/Props/Commercial/Posters/Notice_A4_Horizontal_V1.et` | Rules on doors: ATM, jail, hospital cabinet |
| `LF_Sign_Cardboard` | `Prefabs/Props/Signs/CardboardSign_01/CardboardSign_01_base.et` | Hand-made signs: black market, convoy pickup |
| industrial | `SignIndustrial_*` (already used by `EL_MiningSign`) | Mining and work-site warnings |

**Behaviour.**
- `LF_SignComponent` holds one `m_sSignId`.
- `LF_SignReadAction` is named `Read: <title>`. The title shows in the interaction prompt, so a player sees what a board is without opening it.
- The Read action opens `LF_SignCard`, a paper card on `LF_UIScreen`. It has four short sections: **What**, **How** (the exact action names), **Needs** (items, faction, police online), and **Rules** (cooldowns, wanted). MenuBack or walking 4 m away closes it.
- The card is client-only and has no RPC.

**Content is config.** `Configs/Signs/LF_Signs.conf` maps id → title, what,
how, needs, rules.
- A value that starts with `#` is a localization key (`#LF-Sign_<Id>_<Field>`, appended to `everonlife_localization.st` with translator comments).
- Any other value is shown as written. A server owner can retitle or rewrite a sign in their own config without touching the string table.
- A missing id logs `EL_Debug.Error("Signs", …)` and that sign's action hides.

**Placement.** A new `Signs.layer` (M1-09).
- **Spawn plaza** (civilian spawn ~105,134 today; M1-25 moves it to one end of the street): a `LF_Sign_Board` "Welcome to Everon". Its card lists the loop in order and where each site is ("Post office: east on the main street, by the red counter").
- **One sign per site**, within 8 m of the site: post office, traders, ATM, general store, black market, vehicle lot, garage, bank, fuel station, hospital, police station, jail, organization registry, hideout, convoy pickup and drop, and the mining and farm chains.
- **Retire debug text.** `EL_MiningSign` and `EL_WhitelistSign` move to `LF_SignComponent`. `EL_TextLabelComponent` is deleted.

**Proof.** `report town` (M1-02) fails a site with no sign within 8 m, or with
a blank card.

**Board face.** The title is printed on the board itself (decided 2026-11-08).
#119 prints it on `LF_Sign_Board` and `LF_Sign_Notice` as a render-target paper
sheet. `LF_Sign_Pub` and `LF_Sign_Chalkboard` follow in #116.

## d. Guided arrival chain

The chain runs on the existing quest system (`Scripts/Game/Feature/Quests/`):
`LF_QuestGiverComponent`, `LF_QuestDefinition`, and prerequisite chaining with
`m_sRequiredPreviousQuestId`.
- **One addition:** a generic `SIGNAL` event type with a string key, reported through `LF_QuestManager.ReportSignal(user, key)`.
- **Hooks are modded classes** in `LF_QuestHooks.c`, the same pattern it already uses for gather and process. The job, ATM, and sign files are not edited.

| Step | Quest id | Objective | Event | Reward |
| --- | --- | --- | --- | --- |
| 1 | `arrival_register` | Talk to the town clerk at the spawn plaza | `TALK` (giver `clerk`) | Your ID card, handed over |
| 2 | `arrival_board` | Read the town board | `SIGNAL sign.read:town_map` | – |
| 3 | `arrival_work` | Sign up at the post office and deliver one parcel | `SIGNAL job.run_complete:courier` | Paid at the drop (the job's own cash) |
| 4 | `arrival_bank` | Deposit any amount at the ATM | `SIGNAL atm.deposit` | – |
| 5 | `arrival_done` | Return to the clerk | `TALK` | Small cash welcome; "the town is yours" |

**Rules for the chain.**
- **Length.** Five to ten minutes. After step 5 nothing is pushed.
- **Signs only point.** Each step's card points at the sign for the next site. The chain never adds waypoint markers.
- **Returning players.** Turn-ins are persisted (M1-10), so a returning player never sees the chain again.
- **The giver moves.** The clerk is a new `QuestGiver_Clerk.et`. `QuestGiver_Start` keeps the test quests but moves off the spawn plaza.
- **The ID card.** It is granted at spawn behind `m_bGrantIdAtSpawn` until this chain lands. Then step 1 hands it over.

## e. Scopes and waves

Each scope is sized for one agent in one worktree. The issue is the full card:
context, files, dependencies, acceptance proof, and out of scope. Within a
wave, scopes do not touch the same files apart from the append-only ones
(localization, `docs/features.md`, `agent-box/README.md`). Waves are not time
boxes. A scope starts as soon as its dependencies are merged.

- **Wave 1 (foundations):** M1-00 street split, M1-01 sign system, M1-02 town manifest, M1-03 playtest host, M1-04 carried inventory persists, M1-05 multi-client harness, M1-06 filming, M1-07 pay at the counter, M1-08 registration + ID card.
- **Wave 2 (sites):** M1-25 one-street town, M1-09 signs placed, M1-10 progression persists, M1-11 police station, M1-12 player interaction menu, M1-13 counter screens, M1-14 contraband cargo, M1-15 hideout stash, M1-26 wake up in hospital.
- **Wave 3 (consequence and polish):** M1-16 garage + impound, M1-17 org registry, M1-18 arrival chain, M1-19 fine ticket, M1-20 alarm + dispatch, M1-21 jail slip, M1-22 town tidy.
- **Wave 4 (acceptance):** M1-23 journey proof on dedicated, M1-24 playtest release.

### Wave 1

| ID | Issue | Scope | Depends on | Player-view proof | Game |
| --- | --- | --- | --- | --- | --- |
| M1-00 | [#85](https://github.com/Life-Framework/Life-Framework/issues/85) | **Split the street into site layers**. `Roadmap.layer` holds 24 sites and AGENTS.md forbids editing it unless a task names it. | M0 | `node agent-box/harness.mjs report roadmap` shows the same rows and no new failures. | no |
| M1-01 | [#86](https://github.com/Life-Framework/Life-Framework/issues/86) | **Diegetic sign system with a Read card**. Decision "Signs are diegetic, with a Read for detail". | M0 | Place a temp sign in a scratch layer (not committed) or use the mining sign: `layer EL_MiningSign_Debug` shows `Read: Mining area`, `shot ui` of the card is not blank, `shot world` of the board is not white. | yes |
| M1-02 | [#87](https://github.com/Life-Framework/Life-Framework/issues/87) | **Town manifest and `report town`**. `report roadmap` only walks `Roadmap_*` entities. | M0 | `node agent-box/harness.mjs selftest` passes with the new cases. One `report town` run in play mode produces `agent-box/reports/town.html`. | yes |
| M1-03 | [#88](https://github.com/Life-Framework/Life-Framework/issues/88) | **Playtest host: mission, server config, `cli host`, auto-load**. Playtests are hosted from the maintainer's PC on a dedicated server. | #77, #71 | Manual: `host start`, join from the game, earn cash, `host stop`, `host start`, state is back. Log lines `[ELDebug:Persistence]` load + backup path recorded in the PR. | no |
| M1-04 | [#89](https://github.com/Life-Framework/Life-Framework/issues/89) | **Carried inventory and cash survive a restart**. Every restart hands a fresh starter kit: carried cash and items are not persisted (audit). | #77, #79/#80 | Real restart on the playtest host (M1-03 if landed, else `-loadSessionSave`): carry 237 cash + 3 apples, restart, rejoin, `state` shows the same. | no |
| M1-05 | [#90](https://github.com/Life-Framework/Life-Framework/issues/90) | **Multi-client harness on a dedicated server**. The harness runs only in Workbench play as host. | #71, #83 | `node agent-box/session.mjs up --clients 2` then client 1 robs `Roadmap_Fuel_Till` while client 2 (police) sees the alert; `state` on both before/after. | yes |
| M1-06 | [#91](https://github.com/Life-Framework/Life-Framework/issues/91) | **Filming harness**. Direction: "Filming harness records the same runs as video: short clips attached to PRs". | #111, multi-window capture later with M1-05 | `film.mjs world-atm` produces a playable clip that shows the ATM interaction. | yes |
| M1-07 | [#92](https://github.com/Life-Framework/Life-Framework/issues/92) | **Pay at the counter: remove the passive paycheck**. `EL_PlayerJobComponent` pays $700 cash every 300 s to every job including UNEMPLOYED (`EL_PlayerJobComponent.c` ~line 63). | M0 | Police client goes on and off duty at the station; `state` cash rises by the expected amount. | no |
| M1-08 | [#93](https://github.com/Life-Framework/Life-Framework/issues/93) | **Registration form and a real ID card**. The name/age screen is never opened: `EL_CharacterCreationManager.CreateAndSpawnDefaultCharacter` makes every character "Default Citizen". | #83, #97 | Fresh client on dedicated: form appears, type a name, spawn, `look` at self inventory shows the card; second client sees the card view after Show ID. | yes |

### Wave 2

| ID | Issue | Scope | Depends on | Player-view proof | Game |
| --- | --- | --- | --- | --- | --- |
| M1-25 | [#120](https://github.com/Life-Framework/Life-Framework/issues/120) | **One-street town layout**. Decision "Every M1 site moves onto one street now". Move every M1 site from the `Street_*` layers onto one street: spawn plaza at one end, then clerk → post office and jobs → ATM and bank → store → vehicle lot and garage → police station and jail → hospital and fuel → org registry and hideout off a side alley → contraband out of town. A small 1989 Everon village: vanilla Everon buildings, a real road, a square at the spawn, and a fenced test annex off to the side for dev fixtures. Names and GUIDs stay; positions change. | M1-00 (#115), M1-02 | `report roadmap` and `report town` pass at the new positions; a filmed fly-through of the street (`tools/film`). | yes |
| M1-09 | [#94](https://github.com/Life-Framework/Life-Framework/issues/94) | **Place the signs: one per site, town map at spawn**. M1 "done when": a sign at spawn explains the loop, every site has a sign saying what it is and how to use it. | M1-00, M1-01, M1-02, M1-25 | `report town` with `requireSigns` passes: every site has a sign within 8 m and a non-blank card. | yes |
| M1-10 | [#95](https://github.com/Life-Framework/Life-Framework/issues/95) | **Progression survives a restart**. Job careers and levels, police rank and on-duty seconds, licenses, and quest turn-ins are not in `EL_PlayerAccountRecord`; a restart returns officers to patrol and couriers to level 1 (features.md: LF_PoliceRanks ⚠, Jobs ⚠). | M1-04 | Real restart: senior officer stays senior; courier level 2 still opens fuel delivery. | no |
| M1-11 | [#96](https://github.com/Life-Framework/Life-Framework/issues/96) | **Police station on the street: duty desk and wanted board**. `docs/playtest.md` says go on duty at `Roadmap_Police_Desk`, but that desk has no components at all (`Roadmap.layer`, 204 0 120); duty lives at `PoliceStation_Debug` (170,120) on another layer. Clocking in replaces the faction screen, which this scope deletes. Clocking in and out swaps a separate police inventory with the civilian one; both persist. | M1-00, M1-25, M1-04, #83 | 2 clients: civilian robs the fuel till; whitelisted client clocks in, gets the police inventory, reads the board, sees the robber at level 1. Clock out, restart, clock in: both inventories come back. | yes |
| M1-12 | [#98](https://github.com/Life-Framework/Life-Framework/issues/98) | **One player interaction menu**. Decision "One player interaction menu": introduce yourself, show ID, give cash or an item, and role actions (search, cuff, fine) on the person in front of you. After an introduction the name shows only up close and is remembered across sessions. | M1-08, #73 | 2 clients: introduce, show ID, give 50 cash; `state` cash changes on both. The name shows within a few metres, not beyond, and again after a restart. | yes |
| M1-13 | [#110](https://github.com/Life-Framework/Life-Framework/issues/110) | **Counter screens: ATM terminal and shop price list on the UI kit**. ATM and shop are the two menus every player uses. | #97 | `flow world-atm`, `flow ui-shop` on a dedicated client: shots not blank, cash and balance change in `state`. | yes |
| M1-14 | [#99](https://github.com/Life-Framework/Life-Framework/issues/99) | **Contraband: real cargo and real drop points**. The convoy hauls `RowanBerries.et` between two `EL_DebugAreaBoundary` markers (`Roadmap_Convoy_Start`/`_Drop`). | M1-00 | `report town` rows for start/drop pass (not white, action present). | yes |
| M1-15 | [#100](https://github.com/Life-Framework/Life-Framework/issues/100) | **Hideout stash that holds items**. `Roadmap_Hideout_Stash` is a crate mesh with no storage component (features.md, playtest.md). | M1-00, M1-04 | Org member stores 3 items, restart, items present. | no |
| M1-26 | [#121](https://github.com/Life-Framework/Life-Framework/issues/121) | **Wake up in hospital**. Decision "You wake up in hospital". Respawn is free at a civilian spawn today (`EL_SpawnLogic.RespawnPlayer_S`). Respawn in a hospital bed; a small bill from the bank, debt if broke. Respawn point and bill are config. | M1-25 | Dedicated client dies, wakes at a hospital bed; `state` bank drops by the bill; a broke player goes negative. | yes |

### Wave 3

| ID | Issue | Scope | Depends on | Player-view proof | Game |
| --- | --- | --- | --- | --- | --- |
| M1-16 | [#101](https://github.com/Life-Framework/Life-Framework/issues/101) | **Garage it to keep it: garage pad, ledger card, impound**. Decision "Garage it to keep it". | M1-00, M1-10 | Buy, drive, park, restart, retrieve at the garage; leave a second car on the street, restart, pay impound. | yes |
| M1-17 | [#102](https://github.com/Life-Framework/Life-Framework/issues/102) | **Organization registry: ledger desk and invite by facing**. The org desk screen lists name, fund, and the processing upgrade, but invite/kick exist only in `EL_OrganizationManager` and are unreachable from the world. | M1-12, M1-00 | 2 clients: found org, invite, member opens the hideout door. | yes |
| M1-18 | [#103](https://github.com/Life-Framework/Life-Framework/issues/103) | **Guided arrival chain**. Decision "Short guided arrival, then free". | M1-07, M1-08, M1-09, M1-10, #97 | Fresh dedicated client completes the chain with flows; `state` shows ID card, cash, bank > 0. | yes |
| M1-19 | [#104](https://github.com/Life-Framework/Life-Framework/issues/104) | **Fine as a paper ticket**. Fines are a remote menu action that takes cash from a wanted player at any distance (`EL_PoliceMenu.RpcAsk_EL_PoliceFine`; #73 adds a range). The ticket is paid in cash at the police desk within N minutes, doubles once at the deadline, then +1 wanted. | M1-11, M1-12 | 2 clients on dedicated: ticket written, paid at the desk; a second ticket left past both deadlines doubles, then `state` shows wanted +1. | yes |
| M1-20 | [#105](https://github.com/Life-Framework/Life-Framework/issues/105) | **Alarm and dispatch**. Police learn about a robbery only from a toast (and toasts are broken until #97). No crime needs police online; with no officer on duty the alarm is louder. Flagship minimum 0. | M1-11, M1-03 | 2 clients: till robbery → police client `state` shows dispatch received; alarm audible in clip. Robbery with nobody on duty plays the loud alarm. | yes |
| M1-21 | [#106](https://github.com/Life-Framework/Life-Framework/issues/106) | **Jail you can see**. Arrest, escort, and the timed sentence work (`LF_Jail.c`), and #77 persists the sentence, but the prisoner gets no in-world statement of the sentence and release is silent. | #73, #77, M1-00 | 2 clients: arrest → slip → release; restart mid-sentence resumes. | yes |
| M1-22 | [#107](https://github.com/Life-Framework/Life-Framework/issues/107) | **Town tidy**. A stranger walks past debug clutter: visible `EL_DebugAreaBoundary` markers on ATM, Shops, Jobs, NPCs, Survival, chain layers, test fixtures (`PRFeatureCrate` 176,130, `Tests.layer`), and duplicate stations. Dev fixtures move into the fenced test annex (M1-25) rather than being deleted. | M1-25, M1-09, M1-11, M1-14, M1-15 | `report town` passes with no outliers; a wide shot from spawn shows no debug meshes. | yes |

### Wave 4

| ID | Issue | Scope | Depends on | Player-view proof | Game |
| --- | --- | --- | --- | --- | --- |
| M1-23 | [#108](https://github.com/Life-Framework/Life-Framework/issues/108) | **Journey proof on a dedicated server**. M1 is done only when every loop is proven from the player's view on a dedicated server with several clients, including a restart. | all M1 scopes, M1-05, M1-06 | `report journey` all green; report committed as an artifact link in the PR. | yes |
| M1-24 | [#109](https://github.com/Life-Framework/Life-Framework/issues/109) | **Playtest release: guide, Workshop, showcase**. M1 is published on the Workshop, hosted from the maintainer's PC, and collects feedback through GitHub issues. | M1-23 | A clean machine (or clean profile) subscribes, joins the hosted server, and completes the arrival chain. | yes |

## f. Not in M1

Do not build these in M1. File them for later.

- **Buy and furnish property** (decision recorded; after M1, on the house lock and key code).
- **Licenses as cards, a gun shop, the weapon license desk.** The weapon decision stands; it ships with the gun shop.
- **Goods on shelves and shop stock. NPC prices that follow supply and demand** (decision: after M1).
- **ATM cards and receipts. A pay-slip item.**
- **The org fund as a physical safe. An evidence locker for confiscated items.**
- **Payphones and pagers as new items.** M1 uses the vanilla radio for dispatch.
- **AI police when officers are offline. Tiered crime thresholds.**
- **Configurable death drop.** M1 keeps the flagship rule: the body keeps everything it carried.
- **Heist state across a restart.** An open breach and bank bags on the ground reset on restart, by design. Sentences persist through #77.
- **Everon placement, reusable site prefabs, MainWorld** (M2).
- **Name tags. A phone UI. Custom meshes or textures for signs.**
- **New jobs, crafting, revive, police missions, world events.**
- **Rented hosting. Monetization.**

## Hot files (read before taking a scope)

| File | Rule |
| --- | --- |
| `Roadmap.layer` | M1-00 splits it into `Street_*` layers. After that, each scope owns its site layer. |
| `EL_PersistenceComponentSerializer` | One version bump per wave: M1-04 (w1), M1-10 (w2), M1-16 (w3). The hideout stash uses M1-04's keyed snapshot and does not bump. |
| `Character_Roleplay.et` | Only M1-12 edits it. It leaves hidden `fine` and `org_invite` slots that M1-19 and M1-17 fill from script. |
| `everonlife_localization.st`, `docs/features.md`, `agent-box/README.md` | Append only. On a rebase conflict keep both sides, then `tools\cli regen-localization`. |
| `EL_TestRegistrations.generated.c` | Never merge it by hand. Run `tools\cli regen-tests`. |

## Top risks

1. **One rendering machine.** 19 of 25 scopes need the game, and only the
   maintainer's PC has it. Workbench builds hold a machine-wide lock. The
   real limit on parallelism is game time, not agents.
   - **Mitigation:** write code and LOGIC tests headless, then batch the
     `needs-game` proofs into scheduled play sessions. Treat a second game
     machine as an M1 enabler.
2. **Several clients on one PC.** M1-05 assumes a dedicated server plus two
   or three game clients run side by side with their own profiles. If Steam
   or memory refuses, every multi-client proof (M1-05, M1-11, M1-12, M1-19,
   M1-20, M1-23) needs a second machine.
   - **Mitigation:** spike this first in M1-05.
3. **M0 is unverified.** Six open PRs are "code only, suite not run", and
   #97 overlaps files #83 just changed. M1 builds on them.
   - **Mitigation:** M0 must land green on the full tier before wave 2 starts.
4. **Saving carried inventory.** Nested storage, quantity stacks, hand-carry
   items, and death all have to save at the right moment. A mistake makes
   dupes: die, restart, get it back.
   - **Mitigation:** M1-04 needs a real restart proof, not an in-session
     round trip.
5. **The harness on a dedicated server re-opens the hole #71 closed.**
   Teleport and remote `use` must stay behind `EL_DevGate`. The playtest
   mission and any Workshop build must keep that gate closed.
   - **Mitigation:** a test pins it (M1-05).

## Decided 2026-11-08

The maintainer answered the plan's six questions and made four more calls.
Each answer is an entry in [decisions](../decisions.md).

- **Q1. Sign titles:** printed on the board, not only in the prompt. Done in #119 for the board and notice; pub sign and chalkboard in #116. ([decision](../decisions.md#gameplay))
- **Q2. Faction choice:** no faction screen. Everyone spawns civilian; whitelisted players become police by clocking in at the duty desk, which sets the loadout (M1-11, M1-08). ([decision](../decisions.md#gameplay))
- **Q3. Passive paycheck:** deleted; unemployment benefit for broke players at the job counter (M1-07). ([decision](../decisions.md#gameplay))
- **Q4. Fines:** a paper ticket paid in cash at the police desk within N minutes; it doubles once at the deadline, then +1 wanted. Never from the bank (M1-19). ([decision](../decisions.md#gameplay))
- **Q5. Police minimums:** major crimes always allowed; louder alarm when no officer is on duty. Flagship minimum 0, configurable per crime (M1-20). ([decision](../decisions.md#gameplay))
- **Q6. Town layout:** every M1 site moves onto one street now (M1-25, #120). ([decision](../decisions.md#delivery))
- **Respawn:** you wake in a hospital bed and pay a small bill from the bank; debt if broke (M1-26, #121). ([decision](../decisions.md#gameplay))
- **Names:** after an introduction, a name shows only up close, never over a crowd, and is remembered across sessions (M1-12). ([decision](../decisions.md#gameplay))
- **Police gear:** clocking in swaps in a separate police inventory and clocking out swaps it back; both persist across a restart (M1-11, M1-04). ([decision](../decisions.md#gameplay))
- **Town style:** a small 1989 Everon village with a real road and a square at the spawn, plus a fenced test annex for dev fixtures (M1-25, M1-22). ([decision](../decisions.md#delivery))
