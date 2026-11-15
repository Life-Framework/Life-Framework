# Car colours at the lot (spike)

Maintainer request: "I'd like to purchase cars in different colors". Spike, 2026-11-16. This
note is the design; nothing is built yet. Slice cards are at the end.

## What exists (looked up, game build 26266485)

**Vanilla does colour as one thin prefab per colour.** Each colour prefab overrides the body
materials on the `MeshObject` component (`{51DAA09FEFBFC0E7}`) and nothing else. Example,
`Prefabs/Vehicles/Wheeled/S105/S105_darkred.et` (`game_read`):

```
MeshObject "{51DAA09FEFBFC0E7}" {
 Materials {
  MaterialAssignClass "{64D877CFFC7BC934}" { SourceMaterial "S105Body_mat"
   AssignedMaterial "{F4A476FC46B986BC}Assets/Vehicles/Wheeled/S105/Data/S105Body_mat_darkred.emat" }
  MaterialAssignClass "{64D877CFFC7BC957}" { SourceMaterial "S105Body_mat_int_FC800CBC7671EDFD"
   AssignedMaterial "{361A2EE169A3ED3C}Assets/Vehicles/Wheeled/S105/Data/S105Body_mat_darkred_int.emat" }
```

The colours are siblings: `S105_beige` and `S105_darkred` both inherit `S105_lightblue`, which is
the mesh's own paint. `S105_randomized.et` lists them as `SCR_EditableEntityVariant`s (an
editor feature; we don't use it).

A runtime remap also exists: `IEntity.SetObject(VObject object, string options)`
(`scripts/Core/generated/Entities/IEntity.c:404`). It is per machine, so it needs replication
and a save.

**Colours per car we sell** (`game_browse` of each vanilla folder; the lot's current prefab first):

| Lot car (catalog prefab) | Civilian colour prefabs in vanilla |
|---|---|
| Skoda S105 (`S105_beige`) | beige, darkblue, darkbrown, darkred, green, lightblue, lightgreen, red, white, yellow |
| Dana V2101 (`V2101_beige`) | beige, darkBlue, darkRed, green, lightBlue, lightGreen, red, white, yellow |
| Sputnik T601 (`T601_beige`) | beige, blue, green, grey, orange, white; twoTone beige, blue, orange |
| S1203 (`S1203_transport_beige`) | beige, blue, brown, red, white, yellow (`_red` is 578 B, the others about 1 KB: look before use) |
| UAZ469 (`UAZ469_covered_CIV_blue`) | covered CIV blue, brown, forest, red, teal (JZD, PTS, VAK are liveries) |
| UAZ452 (`UAZ452_base`) | the lot sells the base. CIV colours exist only on `_cargo_CIV_` and `_transport_CIV_`: blue, grey, orange |
| M923A1 (`M923A1_transport`) | none civilian (MERDC is camouflage). No colour choice |

The rally kits are 1990s liveries; leave them out.

**How our lot works.** `LF_VehicleLotComponent` holds `LF_VehicleOffer` rows (`m_sPrefab` the
vanilla catalog prefab, `m_sSpawnPrefab` our locked child such as `EL_S105_beige.et`, price,
name key). The rows are set on the lot entity in `DebugWorld_Layers/Street_Vehicles.layer` and
`Annex.layer`. The sheet (`LF_VehicleLotSheet`) gets name keys and prices from the server
(`LF_ShowVehicleLot`), and Buy sends back a row index (`LF_AskBuyVehicle`).
`LF_VehicleOwnership.Buy` registers a record with both prefabs, a plate and a key, and spawns
the locked child. The locked child carries the lock, lockpick, storage access control and the
car actions on top of the vanilla prefab.

**What the save already stores.** The `vehicles` record stores `m_sPrefab` and `m_sSpawnPrefab` as
`{GUID}path`, and both go through `LF_SaveIds.ResolvePrefab`. A garage retrieve or restart
respawns `record.m_sSpawnPrefab`, or `m_sPrefab` when it is empty (`LF_VehicleOwnership.c:1368`).
Prices, impound fees and the catalog lookup key on `m_sPrefab`. The impound fee tries
`m_sSpawnPrefab` first and then falls back.

## Options

1. **Colour = spawn prefab (recommended).** One thin child of our locked prefab per colour. It
   overrides only the `MeshObject` body materials with the vanilla colour's `.emat`. The offer
   keeps its catalog prefab, and the colour picks the spawn prefab.
   - Persistence: free. The colour is `m_sSpawnPrefab`, which is already in the frozen record.
     No sidecar, no version bump (stays 34).
   - Replication and late join: free. The server spawns a replicated prefab; every client and
     every late joiner loads the same prefab, so it has the same paint. No RplProp, no RPC.
   - Prices, chop shop, impound and garage: unchanged, because they key on the catalog prefab.
   - Cost: about 35 small prefabs, each with a `.meta`.
2. **Colour as a runtime material remap** (`IEntity.SetObject` with a remap string) on the one
   locked prefab. Fewer files, but the colour must be saved (a sidecar `vehiclecolour` payload, a
   version bump to 35), replicated (an RplProp colour index, applied on every client and in
   `RplLoad` for late joiners), and re-applied after any `SetObject` the engine does (destruction
   swaps the mesh). Every risk class in AGENTS.md at once, for a cosmetic.
3. **Spawn the vanilla colour prefab directly.** Not possible: the car would have no lock, key or
   plate actions. Copying the locked components onto each vanilla colour duplicates a 90-line block
   per colour (a fix to one would miss the rest).

## Recommendation

Option 1. The save shape, the network and the restart path stay exactly as they are; the work is
config, a sheet page and data.

**Where the colours are configured.** A `LF_VehicleColoursConfig` (`Configs/Vehicles/LF_VehicleColours.conf`),
on the `LF_VehiclePricesConfig` pattern: rows keyed by catalog prefab, each with an ordered list of
`LF_VehicleColour { m_sNameKey, m_iSwatch (RGB), m_sSpawnPrefab, m_iPriceDelta }`. Keying by model
(not by lot) means no layer edit, every lot sells the same paints, and the pack overlays the list
through `m_aLF_ConfigOverlays`. A model with no row sells as today (one colour, the offer's spawn
prefab).

## UX

The sheet still lists models. A model with colours shows its row action as "Choose" instead
of "Buy". That opens a second page on the same sheet:

```
 SKODA 105                                  from $  900
 ───────────────────────────────────────────────────────
 [■ Beige] [■ Dark red] [■ Dark blue] [■ Light green] [■ White]   <- KEYS row, MenuLeft/Right
 Dark red                                        + $ 50
 Total                                            $ 950
 [ Buy ]                                       [ Back ]
```

- Built from kit parts: TITLE, KEYS (the row of buttons that already exists), LINE, BUTTON,
  CLOSE. The one new kit piece is a colour chip on a KEYS button: a small filled square in the
  button, coloured from `m_iSwatch`. It is built once in `LF_UIScreen`, so the paint shop and
  clothing can use it later.
- Default is the first colour, which is the colour sold today, so buying with no choice gives
  the same car as now.
- Price difference: `m_iPriceDelta` per colour, shown as "+ $50" and added before the
  cash-ceiling rule, so a delta that crosses the ceiling moves payment to the bank.
- 1989 palette for the pack: the period's plain factory colours, no metallics or rally
  liveries. S105 and V2101: beige, white, dark red, dark blue, light blue, green, light green,
  yellow (the base colour is free; the rare reds and yellows cost about +5%). T601: beige, blue,
  green, grey, orange, white, with two-tone at a premium. S1203: beige, blue, brown, white. UAZ469:
  blue, brown, forest, teal. UAZ452 and M923A1: no choice.
- Colour names (`#LF-Colour_DarkRed`, ...) are neutral, so they go in the core table and a pack
  can override them.

## Changes

| Layer | Change |
|---|---|
| Script (core) | `LF_VehicleColour`, `LF_VehicleColoursConfig` (load, overlay, `ColoursFor(catalogPrefab)`); `LF_VehicleOffer.SpawnPrefabFor(colour)` / `PriceFor(colour)`; `LF_VehicleLotComponent.Buy(..., int colour)`; `LF_ShowVehicleLot` also sends each model's colour count, names, swatches and deltas (flat parallel arrays); `LF_AskBuyVehicle(lot, index, colour)`. The server rejects a colour out of range and logs it. The restart respawn falls back to the catalog's base spawn prefab when the saved colour prefab no longer loads (see risks) |
| UI (core) | KEYS button colour chip in the kit; lot sheet colour page |
| Config (core) | `LF_VehicleColours.conf` empty in core (prices pattern) |
| Data (pack) | colour children of the locked prefabs under `LF_Everon1989/Prefabs/Vehicles/Wheeled/<car>/`, the overlay conf with the palette and deltas, rows in `core-pack-inventory.md` if they land in core first |
| Strings | colour names (core), "Choose", "+ %1", "Total" |
| Persistence | none. Record shapes and save version 34 unchanged. `docs/persistence.md` gets one line: the colour is the spawn prefab |
| Docs | `docs/features.md` row, `Feature/Vehicles/Feature.md` section |

## Risks

- **Material array override semantics.** Our colour child inherits `EL_S105_beige` → `S105_beige`,
  which already assigns the beige `.emat` in elements `{64D877CFC0DC5560}` and `{64D877CFC0DC55BA}`.
  The child must override those same element GUIDs; a new element GUID may append a second entry
  for the same `SourceMaterial` and the first may win. Prove each car with `tools\cli look item`
  before it ships (the dedicated server cannot render).
- **A pack drops a colour.** The saved spawn prefab then does not load. `LF_SaveIds` keeps the id
  and logs `save content missing`, but the car would not spawn. Fix: the respawn falls back to the
  catalog row's base spawn prefab and logs a warning. A pack can also ship a `prefab` alias.
- **Odd vanilla files.** `S1203_transport_red.et` (578 B) and `S1203_cargo_red.et` (107 B) are smaller
  than their siblings, and we don't know the paint of `UAZ452_base`. Look before listing them.
- **RPC signature change** on `RpcAsk_LF_BuyVehicle` and `RpcDo_LF_ShowVehicleLot`. Client and
  server always run the same build, so this is safe, but run interrogate (money path).
- **Things that ignore colour today:** price boards (#540) show the base price ("from"), the test-car
  grant uses colour 0, and police plate checks and witness descriptions don't name the colour (a
  follow-up: "a dark red Skoda").
- **GUID hygiene** for about 35 new prefabs: generate them, never hand-copy `.meta` files;
  `tools\cli validate` catches duplicates.

## Proof plan

- LOGIC: colour index maps to spawn prefab and price; out of range is refused; no colour row means
  today's offer; a missing colour prefab falls back.
- `look item` for every colour prefab (PR pictures).
- Session (full profile): buy two colours, shots of both side by side.
- `wt e2e`: buy dark red, restart, retrieve: still dark red (the record's spawn prefab).
- Manual: a second client joins after the purchase and sees the same colour.

## Slice cards (proposed)

Each fits within 45 min and 60 calls; they run in order, except 2 can run beside 1.

1. **cc-1 core colour mechanism** (logic, T2, `lf-builder`). `LF_VehicleColour`, `LF_VehicleColoursConfig`
   (empty core conf), offer `SpawnPrefabFor`/`PriceFor`, the lot `Buy` colour argument, the RPC colour
   int with a server range check, the respawn fallback. LOGIC tests with red proofs. Interrogate:
   RPC and money. No UI change: today's Buy sends colour 0.
2. **cc-2 S105 and V2101 colour prefabs** (data, item, `lf-builder`). Children of the two locked
   prefabs overriding the beige element GUIDs, generated. Pack overlay rows with the 1989 palette
   and deltas. `look item` pictures for each.
3. **cc-3 lot sheet colour page** (ui, `lf-builder`, after cc-1). KEYS colour chip in the UI kit
   (`validate-ui-kit`), the Choose page, strings. Session proof, full profile: two colours bought,
   shots.
4. **cc-4 remaining models and restart proof** (data plus proof, after cc-2 and cc-3). T601, S1203
   and UAZ469 children after looking at each. `wt e2e` restart proof, and a manual late-joiner
   step in the PR.
5. **cc-5 colour in descriptions** (optional). Plate check and witness text name the colour.
