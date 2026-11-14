# Config service

> The owner's `$profile:LifeFramework/config.json` merged over the shipped
> `.conf` defaults. Issue #278 (M3 Configuration, epic #224); outline:
> `docs/roadmap/m3-outline.md` section 2.

## Model

Three layers, highest first:

1. **Owner**: one sectioned JSON file, `$profile:LifeFramework/config.json`,
   read on the server at game start (`LF_ConfigService.Boot`, called from
   `LF_GameModeRoleplay.OnGameStart`).
2. **Pack**: `.conf` overlays of the same class as the core file, supplied by
   `LF_ConfigPackSource` (the D4 seam, below).
3. **Core**: the `.conf` the feature already loads (shops:
   `Configs/Shop/ShopPrices.conf`).

Each layer changes only the fields it sets. A pack `.conf` sets a field when the
file writes it (`BaseContainer.IsVariableSetDirectly`); the owner sets a field
when the key is in the JSON. Records merge by a stable key, never by position:
an existing key keeps its place in the list and changes only the fields given;
a new key is appended, so it wins the rule walk.

The service is generic. A feature loads its core instance, then calls its own
apply step (shops: `LF_ShopPricesConfig.ApplyLayers`), which asks the service
for pack overlays and an `LF_ConfigReader` on its owner section.

## Owner file shape

```json
{
  "schemaVersion": 1,
  "shops": {
    "defaultBuyPrice": 50,
    "defaultSellPrice": 25,
    "priceRules": {
      "Prefabs/Items/Food/": { "buyPrice": 10, "sellPrice": 5 },
      "Prefabs/Weapons/": { "hidden": true },
      "type:64,mode:2": { "buyPrice": 900 },
      "prefab:C9D661E5B0714711": { "buyPrice": 12 },
      "Apple": { "prefab": "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et", "buyPrice": 12 }
    },
    "extraItemPrefabs": ["{C9D661E5B0714711}Prefabs/Items/Food/Apple.et"]
  }
}
```

- JSON keys are the script member name without its Hungarian prefix, lower
  camel case: `m_iDefaultBuyPrice` is `defaultBuyPrice`. #280 can derive the
  schema from the attributes with that one rule.
- Records are an **object keyed by the record's stable key**, not a list (see
  "Why records are keyed objects"). A shop rule's key is its `find` substring,
  `prefab:<GUID>` for a rule that matches one prefab (`m_Prefab`; the GUID without braces, upper case), or `type:<flags>,mode:<mode>` for a rule that matches by arsenal type
  (`LF_ShopPriceResolver.RuleKey` / `ParseRuleKey`; mode defaults to 2, any).
- A new rule that leaves out `buyPrice` or `sellPrice` gets 100 and 50, the
  same defaults as a rule written in a `.conf`.
- `extraItemPrefabs` adds to the shipped list; it never removes.
- `schemaVersion` is read (a file without it is version 1) and compared with the
  build's `LF_ConfigService.SCHEMA_VERSION`; see "Schema version" below.

A missing `config.json` boots on the shipped defaults and writes
`config.example.json` beside it, only when no file is at that path. The
service never writes `config.json`.

## Bad input

Never a VME, never a crash, never more than the one value:

| Input | Result |
|---|---|
| File missing | Info line, shipped defaults, example written |
| File is not valid JSON | `LF_Debug.Error`, every section on shipped defaults |
| Section is not an object | `LF_Debug.Error`, that section on shipped defaults |
| Field of the wrong type (`"buyPrice": "x"`, `12.0` for an int) | `LF_Debug.Error` naming `section.record.field`, the lower layer's value stays, the rest applies |
| Record that is not an object, or whose key does not parse | `LF_Debug.Error`, record skipped |
| Number outside the range its attribute declares (`"buyPrice": -3`, `5000000`) | `LF_Debug.Error` naming `section.record.field`, the value, the limit it broke and the kept value; the lower layer's value stays, the rest applies |
| Unknown key in the file, a section or a record (`defaultBuyPrise`) | `LF_Debug.Warn` naming the path, in file order; the key is ignored, the rest applies |
| Unknown top-level key (`"shop"`) | `LF_Debug.Warn`; ignored |
| `null` | Reads as an empty string; anywhere else it is the wrong type |

**Ranges.** `LF_ConfigReader.ReadInt(key, value, min, max)`. A feature declares the
limits twice: as `params: "min max"` on the attribute (the Workbench editor
enforces it) and as constants the apply step passes (shops:
`LF_ShopPricesConfig.PRICE_MIN`/`PRICE_MAX`, 0 to 1000000). The engine exposes no
attribute reader at run time, so the two are kept equal by hand. Float fields and the
`{GUID}path` prefab-exists check are not done yet.

**Unknown keys.** The reader lists a section's or record's keys with `StartMap` +
`ReadMapKey` before entering it (only after `KindOf` says OBJECT) and the feature calls
`WarnUnknownKeys(known)`. The root's keys cannot be listed (an anonymous map fails), so
`LF_ConfigService.WarnUnknownSections` wraps the document as `{"doc": ...}` and lists
that. A new section adds its key to `IsKnownTopLevelKey`.

## Schema version

`LF_ConfigService.ClassifySchema` and `GetSchemaStatus` (#302). Migrations: next section (#303).

| `schemaVersion` | Status | Log |
|---|---|---|
| missing or equal to the build's | `CURRENT` | Info |
| below the build's | `OLDER` | Migrated at boot first (below); `OLDER` only when `LoadOwnerString` is called without a migration |
| above the build's | `NEWER` | `LF_Debug.Error`: update the server; the fields this build knows still apply, the rest warns as unknown |
| not a whole number, or below 1 | `INVALID` | `LF_Debug.Error`; read as version 1 |

Today the build reads version 1, so `OLDER` is reached in tests only
(`LoadOwnerString(json, source, buildSchema)`).

## Migrations (#303)

`LF_ConfigMigration.c`. At boot `LoadOwnerFile` calls `LF_ConfigMigrator.MigrateFile` before
parsing, only when the file's `schemaVersion` is below `SCHEMA_VERSION`:

1. `PeekVersion` reads the version with `KindOf` first (no crashing call on a bad file).
2. `RunChain` applies the steps in version order in memory. A step is an
   `LF_ConfigMigration` subclass: `FromVersion()`, `GetName()`, `Apply(json)` returning the
   new JSON text or an empty string on failure. It produces version `FromVersion() + 1`; the
   migrator stamps `schemaVersion` after each step and refuses a result that does not parse
   or carry the new version. Each step logs an Info line.
3. The file is copied to `config.json.bak-v<N>-<yyyymmdd-hhmmss UTC>`; a name that exists
   gets `-2`, `-3`, ... and is never overwritten. No backup, no migration.
4. The migrated text is written to `config.json`, then loaded.

A failure at any point (no step for a version, a step returns empty, a bad result, backup or
write fails) logs `LF_Debug.Error`, leaves `config.json` exactly as it was, and gives no owner
layer, so every section runs on shipped values. The server always boots. The unit is the whole
file, not a section: a step works on text, so a half-migrated document is never loaded.

A migrated file is at the build's version, so the next boot finds nothing to do and writes no
backup. A file with no `schemaVersion` reads as 1 and is not migrated.

**Adding a migration.** Bump `SCHEMA_VERSION`, write the step class, add it to
`LF_ConfigMigrator.BuildShipped` (in version order), update `docs/config/config.schema.json`
and the example, and add a test. None ship at schema 1. `LF_Test_MigrationV0ToV1` (a v0 file
calls the section `shop`, v1 `shops`) is the test-only fixture; `V1ToV2` and `V2ToV3` prove
ordering. Steps that read the document must keep the `KindOf`-first rule above.

## Why the engine parses and the reader is hand-written (the #278 probe)

`LF_Test_ConfigJsonProbe` pins what `SCR_JsonLoadContext` does:

- **Wrong type:** `ReadValue` returns false and leaves the target untouched;
  the context stays usable. No coercion (`"12"` and `12.0` do not read as int).
- **Unknown key:** ignored by `ReadValue`; `DoesKeyExist` sees it; `StartMap`
  lists every key of a named object in file order.
- **Missing key:** `ReadValue` false, target untouched, `DoesKeyExist` false;
  `StartObject` on a missing name returns false and leaves the cursor.
- **List of keyed records:** a well-formed list reads by hand (`StartArray`,
  then `StartObject()` per element). But the engine's class reader
  (`ReadValue("rules", array<ref T>)`) reports success while giving a missing
  field the class initializer (no "not set" for the merge) and silently
  dropping a wrong-type field (no error to log).

So: the engine parses, and `LF_ConfigReader` reads field by field with
`ReadValue` / `DoesKeyExist`.

### Why records are keyed objects

Two calls crash the process (`ENGINE (F): Crashed`, not a VME), so they cannot
be asserted in a test and were found with step-logged scratch runs:

- `StartObject(name)` on a **number**. On a string, bool, `null` or list it
  returns **true**, so it is no type check.
- `StartObject()` on a number element of a list, and the class reader on a
  list holding a string.

A scalar list element never reads (`ReadValue("", x)` is always false), so a
list element cannot be classified before it is opened. A named value can be:
`LF_ConfigReader.KindOf` uses only calls that never crash (int, float, bool
and string reads, then `StartArray`) and the reader opens an object only after
`KindOf` says `OBJECT`. Without that check, `"Prefabs/Weapons/": 5` in
`priceRules` crashes the server (red-proof in `LF_Test_ConfigMerge.c`).

String lists (`extraItemPrefabs`) are read whole by the class reader, which is
safe for them: it refuses a list with a non-string, and the reader checks the
target for `null` (a JSON `null` replaces it).

## D4 seam: where pack defaults come from (option b, decided 2026-11-12)

The mission header lists the pack's config overlays:

- `LF_MissionHeader.m_aLF_ConfigOverlays` is an array of `.conf` resources, lowest
  precedence first (a later file wins over an earlier one).
- `LF_MissionConfigPackSource` (`LF_MissionConfigPackSource.c`) is the default
  `LF_ConfigPackSource`. `GetOverlays` returns the running mission's list
  (`LF_MissionHeader.GetCurrent()`). A bare world or a header with no list gives none.
  The core names no pack: a world picks its pack by picking a mission whose header names
  the overlay files.
- Each feature's apply step keeps the overlays whose class matches its own core file and
  logs the rest as skipped (`LF_ShopPricesConfig.ApplyLayers`), so one list can carry
  overlays for several sections. A header that lists the core file itself as an overlay is
  skipped with an error.
- Precedence is owner over pack over core, per field and per key. Several overlays apply in
  list order.
- `LF_ConfigService.SetPackSource` still replaces the source (tests).

Proven in the core test annex, with no pack resource: `LF_Test_ConfigPackHeader` loads
`Configs/Tests/LF_Test_PackHeader.conf` (a header listing two shops overlays) and checks
the merge order; `LF_Test_ConfigPackOverlay` proves the field-by-field merge with a stub.

## Clients: the config stream (#285)

The owner layer is server-only; the server prices every buy and sell with it.
`LF_ConfigStream` carries the server's effective values to the clients:

- **What.** `BuildPayload` writes one JSON object: a header (`stream` format,
  `schemaVersion`, `owner` = whether a `config.json` is behind the values) and
  one object per client-relevant section, written from the typed fields
  (shops: `LF_ShopPricesConfig.WriteStream`: the defaults, every rule in walk
  order keyed `"0".."n-1"` with prefab, find, type, mode, prices and hidden,
  and the extra prefabs; vehicles: `LF_VehiclePricesConfig.WriteStream`: the percents, the chop
  cap and every vehicle keyed `"0".."n-1"` with prefab, spawn prefab, base price and the five
  overrides). The owner document itself, other sections and
  unknown keys never leave the server (`config/stream-client-only`).
- **When.** On join: `LF_GameModeRoleplay.RplSave`/`RplLoad` append the format,
  the payload's hash and the payload after the base game mode's state. On
  change: `LF_PublishConfigStream` rebuilds it and, when the hash moved, sends a
  reliable broadcast RPC. No `RplProp`; the payload is a string in a bit stream
  or an RPC argument.
- **Client.** `LF_ConfigStream.Receive` refuses another format, another
  schema, a payload whose hash differs, or text that is not JSON
  (`LF_Debug.Error`, current values kept). An accepted stream is the client's
  last layer in `LF_ShopPricesConfig.ApplyLayers` (`ApplyStream` replaces the
  values; one bad field and the whole section keeps the client's values), and
  `LF_ShopManagerComponent.ReloadConfig` re-prices a built catalog. With no
  `config.json` the server streams the shipped values and nothing changes.
- **Change.** `/configreload` (chat; harness verb `configreload`) asks the
  server to read `config.json` again. Administrators only on a live server
  (`LF_DevGate`; dev worlds and harness builds open it). The server re-prices,
  then streams. The client sends no config data, only the request.
- **Size.** The shipped shops section is about 3.8 KB. The engine documents no
  limit for one replicated string; above 32 KB the server logs a warning.
- **Vehicles.** `LF_VehiclePricesConfig.ApplyLayers` ends with the stream layer like shops; the
  class caches one instance, so `ApplyToFeatures` calls `LF_VehiclePricesConfig.Reload()` and the
  next price read (chalkboard price boards, lot, chop shop, impound) layers the new values. The
  format stays 1: adding a section is additive and an older reader ignores it.
- **A new section.** Write it in `BuildPayload`, read it with
  `OpenReceivedSection` in the feature's apply step, and re-apply the feature in
  `ApplyToFeatures`.

On a listen host the host is the server and prices with the owner values directly.

## Live reload (#970)

`LF_ContentReload.c`. `reload <what>` (harness verb; `LF_ContentReload.ServerRun`) runs one reloader per name, then
publishes the config stream once. A reloader is the class `LF_ContentReloader_<Name>`, found by name (no list):
`Config` (`config.json` read again, features re-applied) and `Dialogue` (`LF_DialogueOverrides`, the dev caption
file, dev tools only). `NeedsDevTools` keeps a developer's file from being read on a live server; the pure
`LF_ContentReload.Permits` is the decision. The `devDialogue` stream section is written only while overrides exist,
so a payload without them is unchanged. Terms (#958) add `LF_ContentReloader_Terms` in their own file.
Test `reload/dialogue-file`; proof `agent-box/proofs/live-reload-proof.mjs`; usage `docs/worktrees.md` "Live reload".

## Catalogue section (#988)

`LF_ItemCatalogueConfig` (`Configs/Catalogue/LF_ItemCatalogue.conf`) is the `catalogue` section: the
item catalogue and the outlets' offers (`Feature/Catalogue/Feature.md`).

- `items`, keyed by item id; `offers`, keyed by `outlet/item`. An existing key changes only the fields
  given; a new item must set a `prefab` that loads; `removed: true` takes a record out and a later layer
  can set it back. Until the shared overlay model (#962) lands, the merge is the class's own
  (`ApplyContainer` for packs, `ApplyOwner` for the owner and the stream).
- `category` and `legality` are words; one the catalogue does not know is refused and the lower
  layer's word stays.
- Streamed: `WriteStream` writes every record in the owner's shape (removed ones too), and the client
  applies it with `ApplyOwner`, so its price list shows what the server charges. `ApplyToFeatures`
  drops the cached instance (`LF_ItemCatalogueConfig.Reload`).

## Jobs section (#749, D8)

`LF_JobsConfig` (`Configs/Jobs/LF_Jobs.conf`) is the `jobs` section: duty pay, the
unemployment benefit, and the job XP tables of `LF_JobLevels`.

- `tracks`, keyed by job id: `levelThresholds` (total XP to reach level 2, 3, ...; a list
  of whole numbers, each 1 or more) and `dailyXpCap` (0 = none). A new job must set
  `levelThresholds`.
- `xpAwards`, keyed by the action id the game awards (`delivery_run`, `fruit_pick`,
  `gather`): `job`, `xp` per unit, `cooldownSeconds`. A new action must set `job`.
- `licenceGates`, keyed by licence name (`LF_ELicenseType`): `job` and `minLevel`. A
  licence with no row keeps its shipped gate; a new row must set both.
- Server-side only, so no stream: `ApplyToFeatures` drops the cached instance
  (`LF_JobsConfig.Reload`) and the next read layers the owner file again. The DELIVERY
  level the sign-up offer needs reaches clients through `LF_PlayerJobComponent`.
- `ReadFloat` takes `2` as well as `0.5`. `ReadInts` reads an int list with the class
  reader, which refuses a list holding a string (no crash, proven in
  `config/jobs-owner`), then checks each number against the range; one bad element
  refuses the whole list.

## Zones, police, organizations, persistence and licences sections (#282, #283, D8)

Each is a config class with a `SECTION`, an `ApplyOwner` with a handled-key list and ranges
(`params` on the attribute, the same numbers as constants), and `ApplyLayers`, so the generated
schema, `/config` form and reference page list them. A `config.json` that names none of them changes nothing.

- `zones` (`LF_ZoneConfig`): `ruleSets` keyed by rule name (`NORMAL`, `SAFE`, `LAWLESS`), plus
  `safeBlocksPlayerDamage` and `lawlessSuppressesWanted` (read for a rule with no table entry; the
  shipped table has all three). Server only: the damage hook and the crime features read it on the
  server and the HUD reads the rule, not its rule set.
- `police` (`LF_PoliceConfig`): every tunable, and `ranks` keyed by rank id. An id the ladder has
  changes only the fields given; an id it lacks appends a rung (a saved rank is its index, so a
  rung never moves). Prefab and sound names go through `ReadResource`: a name that does not load is
  refused and the shipped one stays. Streamed to clients (arrest reach, desk range, ladder, ...);
  the alarm prefabs and each rank's kit stay on the server.
- `organizations` (`LF_OrgConfig`): the tunables, and `rankDefaults` keyed by `founder`, `officer`
  or `member`: a bool per permission, starting from the shipped rights
  (`LF_OrganizationConfig.ShippedRights`). `LF_OrganizationConfig.DefaultRights` returns the
  configured mask, so a new organization gets it; a saved organization keeps its ranks. A
  `nameMin` above `nameMax` is refused (no name would be valid). The tunables are streamed; the
  rank defaults are not (the server builds an organization's ranks and replicates them).
- `persistence` (`LF_PersistenceConfig`, no `.conf`): `enableAutosave` and `autosaveInterval`.
  The shipped values are the `LF_PersistenceManagerComponent` attributes; `Resolve` starts from them,
  so a prefab edit still works. Read once when the timer is scheduled, so it needs a restart
  (`/configreload` does not move the timer). There is no retention setting: an autosave overwrites
  the one AUTO slot of the playthrough.
- `licences` (`LF_LicencesConfig`, no `.conf`): `rules` keyed by licence name with
  `isInitialLicense` and `requiresWhitelist`, layered over the catalog the licence manager builds.
  `GiveInitialLicenses` reads the initial flags, in `LF_LicenseBits.Order`. The level a licence
  needs stays `jobs.licenceGates`. Server only; a character spawned after the change picks it up.
- Tests: `LF_Test_ConfigSectionsA`, `LF_Test_ConfigSectionsB`, `LF_Test_ConfigSectionsBackcompat`.

## Identity card section (#902)

- `idCard` (`LF_PhotoIdCardConfig`, `Configs/Identity/LF_PhotoIdCard.conf`): `replacementFee`
  (0 to 100000, the town clerk's cash price for a lost card) and `showSeconds` (2 to 60, how long a
  card held up to someone stays on their screen; the server sends it with the card). Server only;
  `/configreload` drops the cached instance (`LF_ConfigStream.ApplyToFeatures`).

## Files

- `LF_ConfigService.c`: `LF_ConfigService`, `LF_ConfigPackSource`,
  `LF_MissionConfigPackSource.c` (the header-backed source),
  `LF_EConfigJsonKind`, `LF_ConfigReader`.
- `LF_ConfigStream.c`: `LF_ConfigStream`, `LF_EConfigStreamStatus` (#285).
  `LF_ConfigReloadCommand.c`: `/configreload` and its server gate.
- Shops: `Feature/Shop/LF_ShopPricesConfig.c` (`ApplyLayers`,
  `ApplyContainer`, `ApplyOwner`), `LF_ShopPriceResolver.c` (`RuleKey`,
  `ParseRuleKey`, `FindRule`), `LF_ShopManagerComponent.LoadConfig` calls
  `ApplyLayers`.
- Tests: `LF_Test_ConfigJsonProbe`, `LF_Test_ConfigMerge` (fields, keyed,
  reject, no-owner), `LF_Test_ConfigOwnerFile`, `LF_Test_ConfigPackOverlay`, `LF_Test_ConfigPackHeader`,
  `LF_Test_ConfigReject` (range), `LF_Test_ConfigUnknownKeys`,
  `LF_Test_ConfigSchemaVersion`, `LF_Test_ConfigMigrationChain`, `LF_Test_ConfigMigrationFile`,
  `LF_Test_ConfigStreamRoundTrip`, `LF_Test_ConfigStreamClientOnly`, `LF_Test_ConfigStreamReject`,
  `LF_Test_ConfigStreamVehicles`, `LF_Test_JobLevelsConfig` (jobs).
