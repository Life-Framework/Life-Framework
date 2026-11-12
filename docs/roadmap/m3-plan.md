# M3 Framework Release: execution plan

How the 79 open M3 issues get built: workstreams, dependencies, issue fixes,
waves, the decisions the maintainer owes, and the first spike. The outline
([m3-outline.md](m3-outline.md)) says what M3 is; this file says in what order
to build it. Read it with [core-vs-pack](../architecture/core-vs-pack.md), the
[inventory](../architecture/core-pack-inventory.md) and the
[integration spec](../research/integration-spec.md).

Taken on 2026-11-11 from `main` at `27ca906f`. M1 has 3 open issues
(#101, #109, #199) and M2 has 48, so the milestone gate in
[direction](../direction.md) applies (decision D1). Nothing here edits an
issue yet: section 4 lists the proposed edits.

## 1. Is the core/pack split the foundation?

**Partly.** The split is the foundation for the release half of M3 (Workshop,
example server, solo play, install guide, string split). It is not the
foundation for the config layer, and the outline's order ("1 then 2") has one
dependency backwards.

- The config service (#278) needs nothing from the move. It can layer core
  `.conf`, then pack `.conf`, then owner JSON while the pack layer is still
  empty.
- The move needs the config service. The inventory's blocker 2 and #323's
  own "Depends on" say the value `.conf` splits (`LF_Jobs`, `LF_Police`,
  `ShopPrices`, `ResourceChains`, `LF_Signs`, `LF_Garage`) wait for #278,
  and the vehicle catalogue and UI skin wait for #284. A pack has no way to
  override a core value until the config service gives it one lookup.
- The record shapes (#746, #684) are a third root. They decide the shape of
  saved records and config sections, so they must settle before the schema
  (#280) is published with the first release. They need neither the move nor
  the config service.

So M3 has **three roots that start in parallel**: A, the addon split (starting
with a spike); B, the config service; C, the record and section shapes. They
meet at the late move batches (`.conf` splits), at #553 (pack config
sections) and at the release.

## 2. Workstreams

| WS | Epic | Issues | Root or depends on |
|---|---|---|---|
| A. Addon split | #223 | #322, #323 (to split), #325, #326, #327 | Root A. Spike first; #322 waits for M1 sign-off |
| B. Config layer | #224 | #277, #278, #279, #280, #281, #282, #283, #284, #285, #553 | Root B. #278 first |
| C. Shapes and patterns | #746, #684 | #685, #686, #688, #689, #690, #691, #692, #693, #748 to #761 | Root C. Needs a scope cut (D5) |
| D. Upgrade path | #229 | #302, #303, #304 | B (#278, #281) |
| E. Config harness | #228 | #298, #299, #300, #301 | B (#279, #281, #285) |
| F. Web generator | #225 | #286, #287, #288, #289, #381 | B (#280, #281) |
| G. Owner docs | #226 | #290, #291, #292, #293, #294 | A and B |
| H. Example server | #227 | #295, #296, #297 | A, F (#289) |
| I. Workshop release | #366 | #367, #368, #369, #370, #371, #372, #374, #375 | Everything; #367 and #375 start early |
| Strays | none | #481, #583, #603, #716 | Not M3-shaped (section 4) |

### Dependency map

Arrows read "must land before". Only cross-workstream edges and the critical
path are drawn; the in-epic order from each epic body still holds.

```
Root A  SPIKE-A ─► #322 ─► #323 batch 0 ─► #326 ─► #323 batch 1 ─► #325
                                                       │
Root B  #277 ─► #278 ─► #279 ─► #280 ─► #281           │
                  │       │       │       │            ▼
                  │       │       │       └─► #298 ─► #299 ─► #300 ─► #301 ─► #369
                  │       │       ├─► #286 ─► #287 ─► #288 / #289 ─► #381
                  │       │       └─► #291 ─► #294
                  │       └─► #282, #283 ─► #284 ─► #323 late batches (.conf splits) ─► #327
                  ├─► #285 (client stream) ─► #300
                  └─► #302 ─► #303 ─► #304
Root C  #754a, #755, #688, #685, #750a, #751, #752a, #749 ─► #282 / #283 (section shapes) ─► #280 first release
        #692 (any time: counts from the day it lands)
        #693a lint ─► #693b fixes
Release #323 + #289 ─► #295 ─► #296, #297, #374 ; #368 ─► #369 ─► #370 ─► #371 ; #290 ─► #370
```

The critical path is root A: SPIKE-A, #322, batch 0, #326, the move batches,
#327, #295, #370, #371. Root B is nearly as long (#278 to #301 to #369) but
runs beside it. The longest single chain of agent work is the config harness
chain, because #300 also needs the client stream (#285).

## 3. What is in the issues today

- **Sizes.** Most B, D, E, F, G and H issues are one-PR sized and already carry
  "Why / Work / Depends on / Acceptance proof". The weak spots are #323 (one
  issue for the whole move), the #746 specs (four are M to L with behaviour
  attached), and the four strays with no template.
- **The village has grown.** The inventory counted 21 tests finding 23 named
  village entities. On `27ca906f` it is 38 test files and 41 names
  (`Roadmap_*`, `Sign_SafeZone*`, `Street_*`). Every M2 site test adds more,
  so batch 0 grows until something stops it (W1-8 below).
- **Config roots have grown.** #277 says about 14 roots; there are 18
  `CONFIG_PATH` constants today (perf, comms, chop shop, medic, evidence,
  witness, incident, ledger, zones and more came in M2).
- **Workshop IDs already exist.** A Reforger Workshop mod ID is the
  `.gproj` GUID. The core is `79636E668EA37AC9`, which the M1 playtest publish
  (#109, `docs/playtest/workshop-publish.md`) uploads first, with the
  flagship mission header `LF_PlaytestTown.conf` inside it. The pack is
  `B1E84D5687E11724`. #370's premise ("the first upload creates the IDs") is
  half stale, and the move strands M1 hosts who list only the core (D3).
- **Release tooling exists.** `tools\cli pack`, `tools\cli playtest release`,
  `tools/release/` and `release-selftest.mjs` already pack the addon, build
  the zip and write the server file. #369 and #296 should extend them.
- **Append-only payloads do not need M3.** `docs/persistence.md` appends a
  payload with a version bump, and older saves load because deserialize stops
  at the stored version. A new payload added after the persistent launch
  breaks nothing. Only a **reshape of an existing record** (a field added to
  a positional record) or a **changed existing config field** gets expensive
  after release. That is the test for what in #746 must be in M3 (D5).

## 4. Proposed issue edits

Section 6 is decided. On 2026-11-12 the edits those decisions settle were applied: #367 (D2), #685 and its split #861 (D9), #750, #752 and #754 with their splits #858, #859 and #860 (D5), #761 (D6, D7, D12), #283 and #284 (D5, D8), #481 (D10). The other rows are still proposed.

### Stale

| Issue | Problem | Proposed edit |
|---|---|---|
| #224, #279, #285 | Say `EL_Debug.Error`; the rename landed (#334) | `LF_Debug.Error` |
| #283 | Depends on #242, which is closed | Drop it. Add #688 and #752a (vehicles shape) and #755 (zones shape) |
| #758, #759, #760, #761 | "Depends on: #692 for playtime where gates read it": a pasted line, none reads playtime | Replace with real deps; #761 depends on the reshape issues |
| #277 | "~14 roots, ~234 attributes, ~40 constants" | "Count from the tree when you start"; 18 roots today |
| #323 | 21 tests and 23 fixtures | 38 tests and 41 names on `27ca906f` |
| #367 | Names, semver, the pack's core range, and "schema or save bump is major" are already in decisions (2026-11-09 "Two Workshop items, two brands") | Rescope to the licence line only (D2); close when that is recorded |
| #370 | "Creates the Workshop IDs" | The IDs are the `.gproj` GUIDs. The core item exists after #109; M3's first upload is the pack, plus retitling the core to "Life Framework (Core)" |
| #369 | Reads as a new tool | Build on `tools/release/`, `tools\cli pack` and the `workshop-*` tags |
| #374 | The M1 zip launcher already plays the town solo | Rescope: after the move, the pack's header in the scenario list with core and pack loaded, plus the first-run briefing. Add a harness solo boot as automated proof beside the manual run |
| #381, #295 | Wait on #370 for Workshop IDs | The IDs are known now; the dependency is soft |
| #684 | Carries `needs-playtest`; no sub-issue has merged | Remove the label (an epic is not playtested) |

### Duplicated or overlapping

| Issues | Overlap | Proposed edit |
|---|---|---|
| #284 with #754, #688 | #284 moves police caps and the vehicle catalogue; #754 and #688 reshape the same values | #284 lists those rows as owned by #754a and #688 |
| #685 body with its own comment | The body stores a list of charges per account and derives the wanted level (a reshape). The spec comment keeps `LF_CrimeRecordRow {uid, offence, count}` and adds a `licencebans` sidecar | Pick one (D9) and rewrite the body |
| #686 with #748 | Both define the no-loop rule | #686 owns the checker; #748 adds its floor and ceiling fields to it |
| #297 with #553 | Both need a stub pack fixture | One fixture under `tools/test/`, built in whichever lands first |
| #761 with each #746 spec | Each spec bumps its own payload version; #761 says "one pass" | Resolve with D6 |

### Too big: split

| Issue | Split into |
|---|---|
| #323 | **0a, 0b, 0c**: annex twins for the 41 fixtures, grouped by test area (crime and police; jobs, chains and vehicles; medic, carry and the rest). **1**: pure pack rows (village layers, MainWorld, title poster, `EL_MiningSign`, the two flagship headers). **2**: prefab splits (NPC base, duty NPC, quest clerk, `LF_` vehicle variants, M151A2). **3**: split world layers (`Street_*`, `Signs`, the chain layers). **4**: `.conf` value splits (after #282, #283, D4). One issue each |
| #750 | **a**: rights bitmask and rank order (a reshape of `organizations`, M3). **b**: the war record and its rules (a new payload, M5) |
| #752 | **a**: record fields on `LF_VehicleRecord` (a reshape, M3). **b**: tow, inspection, the hidden compartment and the `vehiclecache` sidecar (behaviour, M5) |
| #754 | **a**: the rank ladder as config (no save change, M3). **b**: the qualifications payload and the training desk (M5) |
| #693 | **a**: the lint and a baseline of today's 56 `RpcAsk_*` handlers. **b**: the shared check helper and fixes, one PR per area |
| #685 | **a**: the crime catalog in config and the call-site switch (M3). **b**: licence seizure (new behaviour, M5) |

### Missing or weak acceptance proof

| Issue | Gap | Proposed proof |
|---|---|---|
| #760 | "A known DebugWorld position returns its place name", but DebugWorld has no map place names | Add a named-place fixture entity to the annex; WORLD test reads it |
| #761 | "A save from before the pass" | Name the save: the e2e legacy save at version 25, plus one `wt e2e` per reshape |
| #374 | Manual only | Add a harness boot of the pack header with both addons and one client, asserting no `SCRIPT (E)` |
| #690, #691, #689 | Fine, but missing deps | #691 needs #685a; #689 needs #283 (persistence section); #690 needs #278 for its rules |
| #481, #583, #603, #716 | No Why, Work or Acceptance; no `m3` label | See D10: move three, template the fourth |

## 5. Waves

Lanes that serialize work regardless of wave:

- **Persistence lane.** One open PR at a time appends to
  `RegisterPersistencePayloads` in
  `Persistence/Serializers/LF_PersistenceComponentSerializer.c` or reshapes a
  saved record. Two parallel PRs both claim the same version number. M2's
  payload PRs share this lane.
- **Config-service lane.** `Core/Config/LF_ConfigService.c` has one writer
  at a time until #279 and #302 land.
- **`tools/cli.mjs` launch lane.** #322, #326 and #369 all edit the launch
  arguments; one at a time.
- **Freeze.** Move batches 1 to 4 run back to back in one short freeze. During
  it, no other PR touches the paths being moved.

Model column: **S** is Sonnet (`lf-builder`), **O** is Opus (`lf-hard`),
**DS** is DeepSeek in opencode (docs only). Run worktrees for M3 work with
`--priority low` while M2 is open (D1).

### Wave 0: the spike (starts now)

| ID | Issue | Model | Touches | Risk |
|---|---|---|---|---|
| SPIKE-A | Pack-move dry run, for #322, #323 and #327 (section 7) | O | Throwaway branch, never merged. Ships only `docs/architecture/pack-move-spike.md` and a comment on #322, #323 and #327 | Low to the repo; high value. Two pool sessions |

### Wave 1: can start now, in parallel, with no shared files

| ID | Issue | Model | Files it touches | Risk |
|---|---|---|---|---|
| W1-1 | #278 Config service, shops first. First step is a LOGIC probe of `SCR_JsonLoadContext` on a wrong type, an unknown key, a missing key and an array of keyed records; that decides between engine loading and a hand-written per-field reader | O | New `Scripts/Game/Core/Config/LF_ConfigService.c` and its `Feature.md`; `Feature/Shop/LF_ShopPricesConfig.c`, `LF_ShopPriceResolver.c`; one boot hook in `Core/LF_GameModeRoleplay.c`; new `Tests/LF_Test_ConfigMerge.c`, `LF_Test_ConfigJsonProbe.c` | Medium. A new core system on the critical path; the pack layer follows D4. The game-mode hook can collide with M2 edits |
| W1-2 | #277 Config inventory and the constant check | S | New `tools/config/inventory.mjs`, `tools/validation/validate-config-constants.mjs` with a baseline file; new `docs/config-inventory.md` | Low. A noisy constant scan; baseline it |
| W1-3 | #692 Playtime per account and per job | O | New `Feature/Account/LF_Playtime.c`, `Feature/Account/Persistence/LF_PlaytimeRecord.c`; appends `playtime` in `LF_PersistenceComponentSerializer.c`; `docs/persistence.md` row; new `Tests/LF_Test_PlaytimePersist.c`. Reads job and duty state through `LF_EventBus` and accessors only, no edits in `Feature/Police` or `Feature/Jobs` | Medium. Holds the persistence lane for wave 1; cannot be backfilled, so it is worth landing early |
| W1-4 | #693a Request audit lint and baseline | S | New `tools/validation/validate-request-audit.mjs`, `request-audit-baseline.json`, fixtures under `tools/validation/fixtures/request-audit/` | Low to medium. Heuristics for "server-side check present" will need tuning |
| W1-5 | #755 Zone rule sets as data | S | `Feature/Moderation/LF_Zones.c`, `Configs/Moderation/LF_Zones.conf`, `Feature/Crime/LF_Murder.c`, `HUD/LF_LawlessHud.c`, `Tests/LF_Test_ZoneRules.c`, new `Tests/LF_Test_ZoneRuleSetBackcompat.c` | Low. `EVENT` is appended to the enum only if #757 stays (D5) |
| W1-6 | #754a Police rank ladder as config | S | `Feature/Police/LF_PoliceRanks.c`, `LF_PoliceConfig.c`, `LF_PoliceMenu.c`, `LF_DutyPay.c`, `Configs/Police/LF_Police.conf`, `Tests/LF_Test_PoliceRankPermissions.c`, new `Tests/LF_Test_PoliceRankTable.c` | Medium. M2 police work (#127, #128) edits the same folder; `PATROL` and `SENIOR` stay ladder ids 0 and 1 |
| W1-7 | #368 `validate-release-ready` for both addons (metadata, pack-to-core dependency GUID, no generated files, no `EL_`, licence notice); preview images and page copy warn until #375 lands | S | New `tools/validation/validate-release-ready.mjs`, fixtures under `tools/validation/fixtures/release-ready/` | Low. The licence check follows D2 |
| W1-8 | New issue: `validate-annex-fixtures`. Fails when a new test names a world entity outside the annex layers; today's 41 names are the baseline that batch 0 burns down | S | New `tools/validation/validate-annex-fixtures.mjs` and its baseline | Low. Stops #323 batch 0 growing while M2 adds tests |
| W1-9 | #375 Branding and narrative brief (draft for sign-off) | DS | New `docs/branding/core.md`, `docs/branding/everon-1989.md` | Low. Needs the maintainer's review; no code |

No two wave-1 items share a file. The generated test registry and
`docs/features.md` have merge drivers. Under the three-agent cap, start
SPIKE-A, W1-1 and W1-3 first (the Opus items on the critical path), then
the Sonnet items as slots free. W1-9 runs in the background and does not
count.

### Wave 2: after the spike and the first config PR

| Issue | Model | Needs | Note |
|---|---|---|---|
| #279 Per-field reject, with #302 schemaVersion in the same lane | S | #278 | Config-service lane |
| #280 Schema generator and `validate-config-schema` | S | #278 | Parses script attributes offline; tools only |
| #322 Load the pack in build, test, serve, host, playtest | S | SPIKE-A findings, M1 sign-off (D1) | `tools/cli.mjs` lane |
| #323 batch 0a, 0b, 0c | S x3 | W1-8 | Partition by test file; 0a waits for W1-6 because both edit `LF_Test_Roadmap_Ranks.c` |
| #688 Vehicle base price and multipliers | S | none | Owns the `vehicles` price shape before #283 |
| #685a Crime catalog | S, or O if D9 picks the reshape | W1-6 merged (`LF_PoliceConfig.c`) | |
| #750a Org rights bitmask | O | Persistence lane after W1-3 | Reshape of `organizations` |
| #693b Request fixes, per area | S | W1-4 | One PR per feature folder |
| #367 Licence line | DS | D2 | Docs only |

### Wave 3: section routing and the first move batch

| Issue | Model | Needs |
|---|---|---|
| #281 `config-check` | S | #280 |
| #282 Jobs, crime, licences sections | S | #279, #280, #685a, #749 decision |
| #283 Vehicles, zones, persistence sections | S | #279, #280, #688, #752a, W1-5 |
| #285 Client config stream | O | #278 (replication) |
| #286 Generator page | S | #280 |
| #291 Config reference pages | DS | #280 |
| #326 Core-alone suite run | S | #322, batch 0 (launch lane) |
| #323 batch 1, pure pack rows (freeze starts) | S | #326, D3 |
| #751 Held ground fields, #752a vehicle record fields | O | Persistence lane, one at a time |
| #749 Skills on job levels | O | D12, persistence lane |
| #686 No-profit-loop check | S | #688 (one price shape) |
| #303 Config migrations | S | #302 |
| #369 `tools\cli release` | S | #368 (launch lane) |

### Wave 4: the freeze and the harness

| Issue | Model | Needs |
|---|---|---|
| #323 batches 2, 3, 4 (back to back) | S, O for batch 4 | Batch 1; batch 4 needs #282, #283 and D4 |
| #325 String table split | S | #322, batch 1 |
| #327 Pre-move save loads | O | Batch 1 onward; scope per D7 |
| #284 Remaining constants, per area | S | #282, #283 |
| #553 Pack config sections | O | #280, D4, a stub pack |
| #298, #299, #300, #301 Config harness | S, O for #300 | #281, #279, #285 |
| #287, #288, #289 Generator validate, import, presets | S | #281, #286 |
| #304 Pending migrations in `config-check`, #294 upgrade page | S | #303, #291 |
| #689, #690, #691 Persistence switches, cleanup, disconnect rules | O, S, O | #283, #278, #685a |
| #290 Install guide (draft from the spike notes) | S | Batch 1 |

### Wave 5: release

| Issue | Model | Needs |
|---|---|---|
| #295 Example server template | S | Move done, #289 |
| #296 Release download, #297 pack swapped out | S | #295; #297 reuses #553's stub pack |
| #374 Solo play from the scenario list | S | Move done |
| #292, #293 Add-a-job and add-a-shop walkthroughs | S | #282, #290 |
| #761 Record-shape close-out: the rule in `docs/persistence.md` and one `wt e2e` over every reshape | O | All reshapes |
| #372 Release process doc | DS | #369, #294 |
| #370 Pack upload and core retitle (maintainer) | Human | #369, #290 |
| #371 Clean dedicated server from the Workshop | S plus maintainer | #370, #295 |
| #381 Server bundle export | S | #287 |

## 6. Decisions (decided 2026-11-12)

The maintainer answered every question on 2026-11-12; the answer is on the line marked **Decided**, and the full list is in `docs/decisions.md`. Also decided: the two profit loops (gold furnace +30, cement plant +24) stay baselined for now. The text of each question is kept as asked.

**D1. Does M3 work start while M2 is open?** `direction.md` says a
milestone's features wait until the previous one is done; M2 has 48 open
issues and M1 has 3.
(a) Start waves 0 and 1 now at `--priority low`, so M2 keeps the pool.
(b) Start only the spike and the tools-only items (W1-2, W1-4, W1-7, W1-8,
W1-9) now; code waits for M2.
(c) Wait for M2 to close.
*Recommend (a).* Wave 1 adds no gameplay, and #278 and the spike sit on the
critical path. #322 and the freeze still wait for M1 sign-off.
**Decided:** (a). Waves 0 and 1 start now at low priority.

**D2. Which licence does the Workshop build carry?** `decisions.md` holds two
2026-11-08 entries that disagree: "The Workshop build is APL; the GitHub repo
stays MIT" and "Both addons are MIT ... Published releases stay MIT".
(a) Workshop build APL, source MIT. This matches the Workshop terms and
`docs/playtest/workshop-publish.md`; amend the MIT entry to cover the source
only.
(b) MIT everywhere (the Workshop terms appear to forbid it).
(c) A custom non-commercial licence on the Workshop.
*Recommend (a).* It unblocks #367 and the licence check in #368.
**Decided:** (a). Workshop build APL, source MIT. The MIT entry is amended to cover the GitHub source only.

**D3. What happens to M1 hosts when the playtest header moves to the pack?**
#109 publishes the core item with `LF_PlaytestTown.conf` inside. After batch
1 that header lives in the pack, and a host listing only `79636E668EA37AC9`
loses the scenario.
(a) Publish the pack item in the same release as batch 1; the host doc and
`playtest-server.json` list both IDs; announce it on Discord one session ahead.
(b) Keep the header in the core until the M3 release, with a boundary
exception (this breaks the 2026-11-09 "no temporary exception rows" decision).
(c) Freeze Workshop updates from batch 1 until the M3 release.
*Recommend (a).*
**Decided:** (a). The pack item publishes in the same release as batch 1; the host doc and `playtest-server.json` list both IDs; Discord is told one session ahead.

**D4. How does a pack supply its `.conf` defaults?** The outline's precedence
(owner, then pack, then core) needs a mechanism, and the core may not name the
pack.
(a) Enfusion's "override in addon": the pack ships a file at the same path
with the same GUID. No code, but it replaces the whole file, there is no
per-field merge, and only one pack wins.
(b) The mission header lists config overlays: `LF_MissionHeader` gets an
array of `.conf` resources. The pack-owned header names its files, and the
config service merges them per field and per key.
(c) A manifest at a fixed core path that each pack overrides.
*Recommend (b).* The header is already pack-owned, and a world picks its pack
explicitly. W1-1 builds the layer stack so either fits.
**Decided:** (b). `LF_MissionHeader` gets an array of `.conf` overlays; the config service merges them field by field.

**D5. Trim #746 to what is expensive after release?** Only reshapes of
existing records and changes to existing config fields cost a migration
later. New payloads and new sections can be appended in a minor release.
(a) Keep in M3: #685a, #688, #749, #750a, #751, #752a, #754a, #755, #761.
Move to M5 Framework (#632): #748, #753, #757, #758, #759, #760, #750b,
#752b, #754b. Move #756 to M6.
(b) Keep everything in M3, as decided on 2026-11-11.
(c) Keep (a) plus the shared pieces #758, #759 and #760, and move only #756
and #757.
*Recommend (a).* It removes about ten agent-issues from M3. #757 also
contradicts "Events: Parked" in `direction.md`.
**Decided:** (a). Kept: #685a, #688, #749, #750a, #751, #752a, #754a, #755, #761. Moved to M5 Flagship Story (the milestone #632 sits in): #748, #753, #757, #758, #759, #760, #750b (#858), #752b (#859), #754b (#860). #756 moved to M6 Homes and Land.

**D6. Record reshapes: one save reset, or one per PR?**
(a) Each reshape is its own PR on the persistence lane, bumping the version.
The playtest Workshop item only takes them at a release boundary, with one
announced reset. #761 becomes the close-out: the rule in `docs/persistence.md`
and one `wt e2e`.
(b) One combined PR for every reshape (#761 as written), which is large and
blocks the lane for days.
*Recommend (a).*
**Decided:** (a). One PR per reshape with its own bump; one announced reset at a release boundary; #761 is the close-out.

**D7. Must a pre-move save load after the move (#327)?** Before #556 a save
reset is allowed; the EL_ to LF_ rename set the precedent.
(a) Yes, prove it after every batch. This sets the discipline owners will
need for 1.x.
(b) No: declare a reset at batch 1, and keep only the spike's evidence.
*Recommend (a) if SPIKE-A shows saved `{GUID}path` strings resolve after a
path change; (b) if it needs loader rewrites.*
**Decided:** (a). Pre-move saves must load after every move batch.

**D8. How many config sections?** The specs add `organizations`, `police`,
`property`, `lockers` and `events` beside the outline's seven.
(a) Allow new core sections and fix the list in `decisions.md`: jobs, shops,
crime, police, organizations, licences, vehicles, zones, persistence.
(b) Keep seven and fold the rest in (police and organizations under `crime`,
lockers under `shops`).
(c) One section per config root, generated.
*Recommend (a).* Folding police ranks into `crime` hides them from owners.
**Decided:** (a). Nine core sections: jobs, shops, crime, police, organizations, licences, vehicles, zones, persistence.

**D9. Crime catalog shape (#685).**
(a) The spec comment: the catalog lives in config, the saved
`{uid, offence, count}` keeps its shape with the offence key as the crime
id, and the licence seizure sidecar moves to M5 (#685b).
(b) The issue body: a list of charges per account, with the wanted level
derived (a reshape of the wanted and crime records).
*Recommend (a).* Same owner value, no reshape.
**Decided:** (a). The catalog lives in config; the saved `{uid, offence, count}` is unchanged; licence seizure is #685b (#861, M5).

**D10. Strays in the milestone.**
#583 (store 3D previews) to M5 with #339. #716 (account publish cost) to M2
with #335. #603 (town grid) to after the move, as pack-world work in M4 or
M2.5. #481 (credits screen) stays in M3 as release polish, once it has the
issue template and a link to the credits-source issue.
*Agree or move individually.*
**Decided:** As proposed. #583 to M5, #716 to M2 Living Town, #603 to M4 Flagship Live, #481 stays in M3 with the issue template and a link to #480.

**D11. M1 sign-off for the freeze.** #322 needs it, and #101, #109 and #199
are open. Is M1 signed off once #109's publish lands, or does the freeze wait
for all three?
**Decided:** Not signed off. The freeze (#322) waits for all three open M1 issues: #101, #109 and #199.

**D12. Skills on job levels (#749): replace or sit under?**
(a) The per-job XP map replaces `LF_PlayerLevelComponent`'s single level (the
spec's preference), and licence gates read job levels.
(b) It sits under the single level, which stays as the licence gate.
*Recommend (a),* because it is the shape owners will configure.
**Decided:** (a). Per-job XP replaces the single level; licence gates read job levels.

## 7. The riskiest unknown, and SPIKE-A

**The unknown:** whether a second addon can own the world placement and the
mission while the core owns the systems, on a dedicated server, from packed
(Workshop-shaped) addons, with existing saves and layers still resolving the
moved prefabs.

Why this one and not config loading: Overthrow already proves `$profile:`
JSON loading on a dedicated server, and the per-field behaviour of
`JsonLoadContext` is a ten-second LOGIC probe inside W1-1. A config failure
stays in one feature. A failure in the move blocks about 30 issues and the
freeze, touches every open PR, and can strand the M1 Workshop hosts.

Specific questions, each with a yes or no answer:

1. **World layering across addons.** Can a pack world own the village layers
   while the core keeps DebugWorld? Try (i) a layer file at the same relative
   path in the pack (`Worlds/DebugWorld/DebugWorld_Layers/`), and (ii) a pack
   `SubScene` world whose parent is the core DebugWorld (MainWorld already
   wraps Eden this way, `docs/features.md`).
2. **Saved paths.** Do saved `{GUID}path` strings resolve after the path
   changes? This covers vanilla self-spawned items and our own records
   (`LF_VehicleRecord` prefab and spawn prefab, store unpaid tags by prefab).
3. **Path-keyed code.** Which code compares prefab strings by path?
   `ShopPrices.conf` substring rules are known; list the rest.
4. **Two projects in the pipeline.** Does `build` produce a fresh rdb for both
   projects, and does `tools\cli pack` produce a pack `data.pak` that loads
   with `-addons <coreGUID>,<packGUID>`?
5. **M1 hosts.** What does a host listing only the core ID see after the move:
   a clean "scenario not found", or a crash?

**Steps** (worktree `wt new spike-pack-move --priority low`, never merged):

1. On the branch's base, make a save with `tools\cli e2e` pass 1: a player
   with cash, a parked and locked `LF_` S105 variant, and an unpaid store tag.
   Keep the profile.
2. Move, each with its `.meta`: one S105 `LF_` variant, `EL_MiningSign.et`,
   the `Village_Dressing` layer, and `LF_PlaytestTown.conf` with its preview.
   Point the header at the pack world from question 1.
3. Load both addons in a local patch of `tools/dedicated/session.mjs`
   (`-addons LifeFramework,LF_Everon1989`); run `validate --full` (both
   projects compile) and `build`.
4. Boot a dedicated server on the saved profile with one client
   (`session up --clients 1`, lowspec). Grep for `Unable to find`,
   `SCRIPT (E)` and `[LFDebug:Persistence]`. Check that the car and its lock
   came back, the sign stands, and the tag is reattached.
5. Pack both addons with `tools\cli pack` and boot from the packed output.
6. Hide the pack and run `test --only` on the tests that name the moved
   entities. They should fail with a readable `Unable to find`, not a VME.
7. Boot the M1 header with only the core loaded.

**Budget:** one Opus agent, at most two pool sessions, about a day. Stop and
report after two environment failures.

**Output:** `docs/architecture/pack-move-spike.md` with the five answers and
the log lines behind them, and a comment on #322, #323 and #327. The answers
decide the batch 1 method (overlay or subscene), D7, and the list of
path-keyed code that batch 0 fixes first.

**Smaller second unknown:** the `JsonLoadContext` probe at the start of W1-1
(wrong type, unknown key, missing key, keyed arrays). It decides whether
per-field reject (#279) can use the engine loader or needs a hand-written
reader.
