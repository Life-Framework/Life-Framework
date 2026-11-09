# M3 Framework Release (outline)

M3 turns the mod into a product other communities can run. A server owner
installs from the Workshop, configures it from documented files or a web
generator, adds a job or a shop without writing script, and hosts on their own
box. Both addons are published on the Workshop. The maintainer set the shape
on 2026-11-08 over four interview rounds.
This file is an outline. Workstreams 2 to 8 are broken into sub-issues on
GitHub, listed under each section (workstream 1, #223, is planned separately);
they get full scope cards after M2 closes.
Read it with [direction](../direction.md) and [decisions](../decisions.md).

**Done when.** A stranger with a dedicated server and no Workbench installs the
two Workshop items, writes one `config.json` with the web generator, changes
prices, jobs, crimes, licences, vehicles, zones and save settings without
forking, and boots a server that players can join. The automated config harness
proves it on every release.

**Rules that apply to every workstream.**

- Config only. An owner never edits script, a pak, or a `.conf` inside our
  addons to change a value on the seven surfaces below.
- One source for the schema. The game, `tools\cli config-check`, the web
  generator, and the docs all read one checked-in JSON Schema. A validator
  fails CI when it drifts from the script config classes.
- Fail one field, not the server. An invalid value logs `LF_Debug.Error`, falls
  back to its default, and the server stays up.
- The flagship eats its own food. The 1989 Everon server runs on the released
  framework and pack, configured the same way an owner would.
- No new gameplay. Bus stops (#203) and trash cans (#205) moved to M5. Config
  demos use systems already in the tree.

**Not in M3** (maintainer, 2026-11-08): an in-game admin config editor, a
modern-setting content pack, an automatic updater, managed hosting for other
communities, extra localizations, cosmetics or donations, and an external
database.

---

## 1. Two addons in one repo

The core and the 1989 content become separate addons that both live in this
monorepo and ship as two Workshop items.

| Addon | Folder | Holds | Never holds |
| --- | --- | --- | --- |
| `LifeFramework` (core) | `addons/LifeFramework/` | Every system and its script, base component prefabs, generic UI kit and layouts, the persistence layer, the test suite, the `DebugWorld` test annex, era-neutral default `.conf` values | 1989 copy, Everon place names, flavour signs, the village, MainWorld placement |
| `LF_Everon1989` (flagship pack) | `addons/LF_Everon1989/` | 1989 Everon content: the village and MainWorld town, site prefabs dressed for the period, flagship `.conf` values (prices, jobs, crimes, catalogue), the flagship mission header, period strings | Script beyond thin flagship glue; anything another pack would need |

**The direction of reference is one way.** The pack depends on the core
(`.gproj` dependency); the core never references a pack GUID, class, or path.
An owner can replace `LF_Everon1989` with their own pack and keep the core.

**Candidate scopes.**

- Inventory: tag every resource and script in `addons/LifeFramework/` as core
  or pack, with the reason, in one table. Move nothing yet.
- Create `addons/LF_Everon1989/` with its `.gproj`, dependency on the core, and
  its own mission header. Wire `tools\cli build|test|serve` to load both.
- Move pack resources in small batches. A `.meta` moves with its resource so
  the GUID is unchanged; references are fixed by `{GUID}path`.
- `validate-addon-boundary`: fails when the core references anything under
  `addons/LF_Everon1989/`, or when the pack adds a system class.
- Split the string table: core keys stay in
  `Language/everonlife_localization.st`; period copy moves to the pack's own
  table.
- The core alone boots: a CI-style check loads the core without the pack and the
  suite passes on DebugWorld.

**Risks.** Saved worlds and persistence data that store a prefab path: a move
keeps the GUID but changes the path, so prove it with `wt e2e` on a save made
before the move. `DebugWorld` uses village content today; decide which parts
stay in core as neutral test fixtures. Every open PR touches files that will
move, so schedule the move as one short freeze, not a long branch.

## 2. Config layer

**Model** (maintainer, 2026-11-08). Shipped `.conf` files are the defaults, in
the core and then the pack. The owner writes one sectioned JSON file at
`$profile:LifeFramework/config.json`. At boot the server merges them. Precedence
is owner JSON, then the pack `.conf`, then the core `.conf`. Overthrow's
`OVT_OverthrowConfigComponent` proves the loading path on a dedicated server:
`$profile:` JSON read through `JsonLoadContext`, with defaults set before
reading. Our `whitelist.txt` already lives under `$profile:LifeFramework/`. We
go further than Overthrow: its JSON only holds multipliers, while ours holds
the real tables.

**The seven surfaces.**

| Section | Covers |
| --- | --- |
| `jobs` | Jobs, pay per task, gather and process drops, levels |
| `shops` | Shops, items, buy and sell prices, stock |
| `crime` | Crimes, wanted values, fines, sentences, police-online rules |
| `licences` | Licences, skill points, progression rules |
| `vehicles` | The vehicle catalogue, prices, garage rules |
| `zones` | Safe, lawless, and normal zones and their rules |
| `persistence` | Save interval, retention, what persists across restart |

**Behaviour.**

- The file carries `schemaVersion`. A missing file boots on defaults and writes
  a commented example beside it, never over an existing file.
- Unknown keys are kept and logged as a warning, so a newer file does not break
  an older server.
- A per-field reject: wrong type, out of range, unknown prefab GUID. The field
  logs, takes its default, and the rest of the section still applies.
- Clients get what they need through a versioned server-to-client stream
  (`RplSave`/`RplLoad`, as Overthrow does); server-only values never leave the
  server.

**Sub-issues** (epic #224, in order):

1. #277 Inventory every config attribute and hard-coded constant
2. #278 Config service merges config.json over shipped defaults, starting with shops
3. #279 A bad config field falls back to its default; the server stays up
4. #280 Generate config.schema.json from the config classes
5. #281 tools\cli config-check validates an owner file offline
6. #282 Jobs, crime, and licences sections read through the config service
7. #283 Vehicles, zones, and persistence sections read through the config service
8. #284 Move hard-coded tuning constants onto config classes
9. #285 Clients receive config values through a versioned stream

**Risks.** Merging arrays of records (a shop's item list) needs stable keys per
record, not array positions. Prefab references in JSON must be `{GUID}path` and
checked on the server.

## 3. Web config generator

A page on life-framework.org (`website/`), built from the schema.

- Import an existing `config.json`; start from a preset (flagship 1989, small
  server, hardcore economy).
- Field help inline, taken from the schema descriptions.
- Validate before export with the same rules as `config-check`; export only a
  valid file.
- No change diff (maintainer, 2026-11-08).

**Risks.** The site build must read the schema from the repo at build time, so
a schema change ships the generator change in the same PR.

**Sub-issues** (epic #225, in order):

1. #286 Generator page renders a form from config.schema.json
2. #287 Generator validates with config-check's rules and exports only a valid file
3. #288 Import an existing config.json into the generator
4. #289 Generator presets: flagship 1989, small server, hardcore economy


## 4. Owner docs

- An install guide from zero: Workshop items, `server.json`, profile folder,
  first boot, joining.
- One page per config section, generated from the schema, with an example.
- "Add a job" and "add a shop" walkthroughs that use only prefabs and config.
- An upgrade page per release that lists schema changes and migrations.

**Sub-issues** (epic #226, in order):

1. #290 Install guide from zero to a joinable server
2. #291 Config reference pages generated from the schema
3. #292 Walkthrough: add a job with prefabs and config only
4. #293 Walkthrough: add a shop with prefabs and config only
5. #294 Upgrade page per release


## 5. The example server

The example server is the flagship in its most basic form: the core plus
`LF_Everon1989` running, with a starter `server.json` and `config.json`. An
owner swaps the pack for their own. It lives under `server/` as a template and
ships as a release download beside the Workshop items.

**Sub-issues** (epic #227, in order):

1. #295 Example server template under server/ boots core and pack (flagship)
2. #296 Release download built from the template
3. #297 The template boots with the pack swapped out


## 6. Config harness

The automated proof for M3. For each preset and a set of broken fixtures it:

1. Parses the file.
2. Validates it against the schema.
3. Boots a dedicated server with it and reads the effective values from the log.
4. Joins a client and checks that a client-visible value (a shop price)
   arrived.

Broken fixtures prove the per-field reject: the server boots, logs the field,
and uses the default. It runs in `wt gate` and before every release.

**Sub-issues** (epic #228, in order):

1. #298 Harness fixtures and the parse and validate stages
2. #299 Harness boots a dedicated server per fixture
3. #300 Harness joins a client and checks a client-visible value
4. #301 Config harness runs in wt gate and before every release


## 7. Upgrade path

- `schemaVersion` in the owner file and a migration per version bump,
  applied at boot. The original file is backed up before a rewrite.
- `config-check` reports which migrations would run.
- The persistence save keeps its own version rules (`docs/persistence.md`);
  a config migration never rewrites save data.

**Sub-issues** (epic #229, in order):

1. #302 schemaVersion in the owner file, with too-new and too-old handling
2. #303 Config migrations run at boot after a backup
3. #304 config-check reports which migrations would run


## 8. Workshop release

The core and the pack ship as two Workshop items; the pack depends on the core
item. Releases are repeatable, not hand-built.

- Fix the item names and version scheme before anything is uploaded; Workshop
  IDs are hard to change once public.
- `validate-release-ready` checks both addons (metadata, preview image,
  dependency, no generated files, APL notice) in CI.
- `toolsi release <version>` gates, stamps versions in both `.gproj`
  files, writes notes, and tags.
- The maintainer does the first upload (core, then pack); an agent prepares the
  checklist.
- Proof: a clean dedicated server with no Workbench boots from the two Workshop
  items alone.
- `docs/release-process.md` covers updates, compatibility, and rollback.

**Sub-issues** (epic #366, in order):

1. #367 Fix Workshop item names and version scheme
2. #368 validate-release-ready checks both addons for publishing
3. #369 toolsi release stamps versions, gates, and tags
4. #370 First Workshop upload of core and pack (maintainer)
5. #371 A clean dedicated server boots from the Workshop items alone
6. #372 Release process doc for updates and rollback


## Order

1 (two addons) and 2 (config layer) go first and in that order, because the
schema has to know which defaults live in which addon. Workstreams 3 to 7 follow
in parallel once the schema exists. Workstream 8 (Workshop release) starts with
the naming decision, and the upload waits for the pack move, the harness, and the
install guide.

## Open questions

- Which DebugWorld content stays in the core as neutral test fixtures.
- Whether a pack can add its own config sections (a modern pack's phone shop)
  or only fill the seven.
- The Workshop item names and the core's version numbering (#367).
