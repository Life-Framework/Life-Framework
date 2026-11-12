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
  or `type:<flags>,mode:<mode>` for a rule that matches by arsenal type
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

## D4 seam: where pack defaults come from

Maintainer decision D4 (`docs/roadmap/m3-plan.md`) is open. The merge is built;
only the source of pack files waits:

- `LF_ConfigPackSource.GetOverlays(coreConf, overlays)` returns the pack
  `.conf` files that overlay a core `.conf`, lowest precedence first. The
  default returns none.
- (a) Same-path override: nothing to do; the engine loads the pack file in place
  of the core one and the default source stays.
- (b) Header-listed overlays (recommended): a subclass reads an overlay array
  from the pack-owned `LF_MissionHeader` and is installed with
  `LF_ConfigService.SetPackSource`.
- (c) Manifest: a subclass reads the manifest; same install.

The field-by-field merge (`LF_ShopPricesConfig.ApplyContainer`) does not change
with D4. `LF_Test_ConfigPackOverlay` proves it with a stub source and a fixture
overlay (`Configs/Tests/LF_Test_ShopPricesOverlay.conf`).

## Clients: the config stream (#285)

The owner layer is server-only; the server prices every buy and sell with it.
`LF_ConfigStream` carries the server's effective values to the clients:

- **What.** `BuildPayload` writes one JSON object: a header (`stream` format,
  `schemaVersion`, `owner` = whether a `config.json` is behind the values) and
  one object per client-relevant section, written from the typed fields
  (shops: `LF_ShopPricesConfig.WriteStream`: the defaults, every rule in walk
  order keyed `"0".."n-1"` with prefab, find, type, mode, prices and hidden,
  and the extra prefabs). The owner document itself, other sections and
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
- **A new section.** Write it in `BuildPayload`, read it with
  `OpenReceivedSection` in the feature's apply step, and re-apply the feature in
  `ApplyToFeatures`.

On a listen host the host is the server and prices with the owner values directly.

## Files

- `LF_ConfigService.c`: `LF_ConfigService`, `LF_ConfigPackSource`,
  `LF_EConfigJsonKind`, `LF_ConfigReader`.
- `LF_ConfigStream.c`: `LF_ConfigStream`, `LF_EConfigStreamStatus` (#285).
  `LF_ConfigReloadCommand.c`: `/configreload` and its server gate.
- Shops: `Feature/Shop/LF_ShopPricesConfig.c` (`ApplyLayers`,
  `ApplyContainer`, `ApplyOwner`), `LF_ShopPriceResolver.c` (`RuleKey`,
  `ParseRuleKey`, `FindRule`), `LF_ShopManagerComponent.LoadConfig` calls
  `ApplyLayers`.
- Tests: `LF_Test_ConfigJsonProbe`, `LF_Test_ConfigMerge` (fields, keyed,
  reject, no-owner), `LF_Test_ConfigOwnerFile`, `LF_Test_ConfigPackOverlay`,
  `LF_Test_ConfigReject` (range), `LF_Test_ConfigUnknownKeys`,
  `LF_Test_ConfigSchemaVersion`, `LF_Test_ConfigMigrationChain`, `LF_Test_ConfigMigrationFile`,
  `LF_Test_ConfigStreamRoundTrip`, `LF_Test_ConfigStreamClientOnly`, `LF_Test_ConfigStreamReject`.
