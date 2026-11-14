# Pack move spike (SPIKE-A)

The answers to the five questions in
[m3-plan.md section 7](../roadmap/m3-plan.md#7-the-riskiest-unknown-and-spike-a),
with the log lines behind them. Run on 2026-11-11/12 on Gaming-Box (server
1.9.0.37) from `origin/main` at `2043ac55`, on the throwaway branch
`ws/m3-spike-a`. That branch is never merged; this file is the only output.

## Short answers

| # | Question | Answer |
|---|---|---|
| 1 | Can a pack world own the village layers? | (i) Layer overlay at the same path: **no**. (ii) Pack `SubScene` over the core DebugWorld: **yes**, from repo folders, from packed addons, and on a client |
| 2 | Do saved `{GUID}path` strings resolve after the path changes? | **Yes.** The GUID wins and the path text is ignored. A pre-move save loaded after the move with every e2e assertion green |
| 3 | Which code compares prefab strings by path? | About 20 call sites (list below). One broke a test in this spike: `LF_ChopShop.c:166` |
| 4 | Do both projects build and pack? | **Yes**, after two tooling fixes: repeated `-addonsDir` flags and `cli pack` dropping its flags |
| 5 | What does a host listing only the core see? | A clean refusal: `ENGINE (E): Unable to initialize the game`, then `Game destroyed.`. No crash dump and no VME, but no line names the missing scenario |

**For D7:** saved strings resolve by GUID, so a pre-move save needs no loader
rewrite. What breaks is code that compares a saved or configured string with
`GetPrefabName()`; batch 0 can fix that with a GUID compare. Recommend D7 (a).

## What was moved

Each file moved with its `.meta` (GUID unchanged, `Name` line rewritten to the
new path, as a Workbench move does). The car and the sign went to a
**different relative path**, the worst case for path-keyed code:

| Resource | From (core) | To (pack) |
|---|---|---|
| Locked S105 `{E1A105BE1A000001}` | `Prefabs/Vehicles/Wheeled/S105/EL_S105_beige.et` | `Prefabs/Everon1989/Vehicles/EL_S105_beige.et` |
| Mining sign `{D8D55097BA6DB899}` | `Prefabs/Props/Signs/EL_MiningSign.et` | `Prefabs/Everon1989/Signs/EL_MiningSign.et` |
| `Village_Dressing.layer` | `Worlds/DebugWorld/DebugWorld_Layers/` | first `Worlds/DebugWorld/DebugWorld_Layers/` (overlay), then `Worlds/Everon1989Town/Everon1989Town_Layers/` (subscene) |
| Playtest header `{1054CC8646357708}` and preview | `Missions/` | `Missions/`, `World` pointed at `Worlds/Everon1989Town/Everon1989Town.ent` |

Nothing that references them was edited: `LF_VehicleCatalog.S105_LOCKED`,
`LF_ChopShop.conf`, and the core layers (`Street_Vehicles`, `Street_Lockpick`,
`Crimes`, `MiningChain`) still carry the old paths, the way a save does.

## Method

All boots are `ArmaReforgerServerDiag -server` dedicated servers through the
repo's test harness (`tools/test/test-e2e.ps1`) on the test lane, plus one
`session up --clients 1 --mission playtest` session. The spike branch added
environment switches to the harness (addon folder, addon list, world, and
"pass 1 only" / "pass 2 only" for `cli e2e`) and a probe test,
`LF_Test_SpikePackMove`. The probe logs `Resource.Load` and `GetPath()` for
each string form, spawns the car from the old string, finds the placed
entities by name, and leaves one loose S105 for the engine save.

1. On the unchanged tree, `e2e` pass 1 only (core only, DebugWorld): save made,
   profile copied aside.
2. Control: pass 2 on the same tree loads it, 29/29.
3. Moved the files, rewrote the core rdb, wrote the pack rdb.
4. Restored the pre-move save; pass 2 with `-addons LifeFramework,LF_Everon1989`
   from repo folders.
5. Packed both addons; restored the save again; pass 2 from the packed folders
   with `-addons 79636E668EA37AC9,B1E84D5687E11724`.
6. Subscene world: `test --only` booted on the pack world.
7. Header save: on the pre-move tree, pass 1 booted on the playtest header
   (core only). Back on the moved tree, pass 2 on the header, which now lives in
   the pack and points at the subscene world.
8. Hid the pack and ran the tests that name the moved entities.
9. Booted the header with only the core loaded.
10. One client session on the packed pair, header mission.

## 1. World layering

**(i) Overlay: no.** With `Village_Dressing.layer` at the same relative path in
the pack, the server never reads it. Repo folders and packed give the same
result:

```
[LFDebug:SpikePack] named Village_Payphone found=0
[LFDebug:SpikePack] named RoadmapStreet found=1 prefab='' at <250,0,250>
```

`DebugWorld.ent` is 0 bytes and its layers come from its own folder in the
addon that owns the world. A file at the same path in another addon is not
merged in.

**(ii) SubScene: yes.** `Worlds/Everon1989Town/Everon1989Town.ent` in the pack
is `SubScene { Parent "Worlds/DebugWorld/DebugWorld.ent" }` (the MainWorld/Eden
pattern) with an empty `default.layer` and `Village_Dressing.layer` in its own
`_Layers` folder. The core layers of the parent and the pack layer both load:

```
WORLD : Entities load '$LF_Everon1989:Worlds/Everon1989Town/Everon1989Town.ent'
[LFDebug:SpikePack] named Village_Payphone found=1 prefab='{8F2C6E1D4B7A5093}Prefabs/Comms/LF_Payphone.et' at <129.8,0,240.5>
[LFDebug:SpikePack] named Roadmap_Vehicle_S105 found=1 prefab='{E1A105BE1A000001}Prefabs/Everon1989/Vehicles/EL_S105_beige.et' at <216.5,0,261>
[LFDebug:SpikePack] named LF_MiningSign_Debug found=1 prefab='{D8D55097BA6DB899}Prefabs/Everon1989/Signs/EL_MiningSign.et' at <172.4,0,170.2>
```

On a client joined to the packed pair (lowspec session, header mission), after
registering, `stand` and `look` reach all three:

```
stand Roadmap_Vehicle_S105 -> ok;  look -> SCR_OpenVehicleDoorUserAction, SCR_GetInUserAction, ...
stand LF_MiningSign_Debug  -> ok;  look -> LF_SignReadAction@LF_MiningSign_Debug, ...
stand Village_Payphone     -> ok;  look -> LF_PayphoneAction@Village_Payphone, LF_WitnessPayphoneAction@Village_Payphone, ...
```

The client log has no `Wrong GUID` line for a moved resource. Its only VMEs are
the `SCR_BleedingScreenEffect` / `Slot_ScreenEffects` lines already open in
`debug-log-triage.md`.

**Batch 1 method: SubScene.** The core keeps DebugWorld and its annex; the pack
world is a SubScene of it and holds the village layers. The pack world needs a
`.meta` from Workbench: a headless `-wbModule=ResourceManager -exitAfterInit`
run registers files that have one but does not create one (the spike wrote it
by hand on the throwaway branch).

## 2. Saved paths

**Resolution is by GUID.** After the move, every string with the right GUID
resolves to the new path, whatever path text it carries. Before and after the
move, from the same probe:

```
before: load car-old-path '{E1A105BE1A000001}Prefabs/Vehicles/Wheeled/S105/EL_S105_beige.et' valid=1 GetPath='Prefabs/Vehicles/Wheeled/S105/EL_S105_beige.et'
after:  load car-old-path '{E1A105BE1A000001}Prefabs/Vehicles/Wheeled/S105/EL_S105_beige.et' valid=1 GetPath='Prefabs/Everon1989/Vehicles/EL_S105_beige.et'
after:  load car-guid-only '{E1A105BE1A000001}' valid=1 GetPath='Prefabs/Everon1989/Vehicles/EL_S105_beige.et'
after:  load car-old-path-no-guid 'Prefabs/Vehicles/Wheeled/S105/EL_S105_beige.et' valid=0
after:  load car-wrong-guid-old-path '{0000000000000001}Prefabs/Vehicles/Wheeled/S105/EL_S105_beige.et' valid=0
```

Two edges: a bare path with no GUID stops resolving, and so does a wrong GUID
with the right path, which resolved by path before the move. A hand-typed GUID
that does not match its `.meta` would therefore break silently at the move.

**The pre-move save loads.** The save stores records as full strings (the
`WorldState` blob holds `m_sSpawnPrefab {E1A105BE1A000001}Prefabs/Vehicles/Wheeled/S105/EL_S105_beige.et`
for three `LF_VehicleRecord`s). Pass 2 after the move, repo folders and packed alike:

```
[LFDebug:Persistence] session continued from a save point (-loadSessionSave)
[LFDebug:Vehicle] impounded e2e-street-car owner=e2e-garage-owner plate=EE ST 1602: left on the street at restart
ENTITY : SpawnEntityPrefab @"{E1A105BE1A000001}Prefabs/Everon1989/Vehicles/EL_S105_beige.et"
[LFDebug:SpikePack] loose cars near <40,0.5,40>: 1
[LFTEST] PASS e2e/pass2-verify-load (120 assertions)
[LFTEST] SUMMARY tier=persistence passed=29 failed=0 total=29
```

- Vanilla self-spawned entity: the loose S105 the probe left in pass 1 came
  back through the engine save, now with the new path.
- Our records: the three vehicle records applied, the garaged car stayed
  garaged, the street car went to the impound and was collected, which spawns
  it from the saved old string. The carried juice got its unpaid tag back.
  The juice did not move; see section 3 for a moved item.
- The car's lock: the records came back with `m_bLocked` set and the moved
  car spawned, but the e2e asserts no lock state on the respawned car. Not proven.

**The playtest header save survives too.** The engine keys a header save by the
header's GUID and name, not by addon: pass 1 on the pre-move header wrote
`.save/game/1054CC8646357708-LF-PlaytestTown/` with
`"m_sMissionResource": "{1054CC8646357708}Missions/LF_PlaytestTown.conf"`.
After the move the header lives in the pack and its world changed from
DebugWorld to the SubScene, and pass 2 still continued it:

```
WORLD : Entities load '$LF_Everon1989:Worlds/Everon1989Town/Everon1989Town.ent'
[LFDebug:Persistence] session continued from a save point (-loadSessionSave)
[LFTEST] PASS e2e/pass2-verify-load (120 assertions)
```

So an M1 host keeps its save across the move, provided it lists both addons.
A world-path boot keys its save by world (`.save/game/DebugWorld/`), so
test-harness saves on DebugWorld do not carry into the pack world.

## 3. Path-keyed code

**The mechanism.** `LF_Utils.GetPrefabName(entity)` returns the canonical
`{GUID}<current path>`, not the string the entity was spawned from. After the
move, a car spawned from the old string reports the new path:

```
spawn from old path ok=1 GetPrefabName='{E1A105BE1A000001}Prefabs/Everon1989/Vehicles/EL_S105_beige.et' equalsOld=0 equalsNew=1 equalsCatalog=0
```

Any `==` between a live `GetPrefabName()` and a string written before the move
(a save, a `.conf`, a script constant) is false. The spike hit one with the pack
loaded:

```
[LFTEST] FAIL chopshop/payout
[LFTEST]   - the S105 has its listed value (expected 1000, got 800)
```

`LF_ChopShop.c:166` (`entry.m_Prefab == prefabName`) misses the moved S105
in `LF_ChopShop.conf`, and the payout falls back to the default value.

**Saved string compared with a live entity** (a pre-move save breaks these, so
batch 0 must fix them before any item, vehicle, or tool moves):

| Site | Saved string | Effect after a move |
|---|---|---|
| `Shop/LF_Stores.c` `ReattachUnpaid` via `LF_InventoryUtils.FindItemsByPrefab` (`LF_InventoryUtils.c:209`, `:272`) | unpaid tag `m_sPrefab` | the tag is dropped (`no longer carries unpaid ...`) |
| `Character/Persistence/LF_StorageSnapshot.c:86`, `:524` | stored inventory `m_sPrefab` | restore cannot match occupants to stored rows |
| `Shop/LF_StoreComponent.c:192` `FindLine` | store line stock `m_Prefab` | saved stock does not find its configured line |
| `Character/Spawning/LF_SpawnLogic.c:321` | account character prefab | Workbench-only path; the character prefab is core anyway |

**Config or constant compared with a live entity** (breaks when the resource
moves and the config is not edited in the same change):

- `Crime/LF_ChopShop.c:166` chop values (seen failing above)
- `Shop/LF_ShopPriceResolver.c:26`: `ShopPrices.conf` `m_sFind` path substrings
- `Shop/LF_ShopManagerComponent.c:232`: `LooksLikeItemPath` admits only `Prefabs/Items/`, `Prefabs/Weapons/`, `Prefabs/Characters/`, so an item moved under `Prefabs/Everon1989/` leaves the shop
- `Gathering/LF_GatherAction.c:173`, `:176`: tool requirement
- `Crime/LF_Breach.c:474`: charge prefabs
- `Quests/LF_QuestManager.c:278`: quest item counts
- `Shop/LF_ShopComponent.c:148`, `:458`, `Shop/LF_ShopManagerComponent.c:74`: curated shop lists
- `Components/InventorySystem/LF_TraderManager.c:37`, `LF_RestrictedInventoryStorageComponent.c:12`: tradable lists
- `Chains/LF_ChainSearch.c:100`, `Vehicles/LF_VehicleLotComponent.c:113`, `Components/BuildingSystem/LF_BuildingManagerComponent.c:54`
- `Quantity/ScriptedInventoryStorageManagerComponent.c:61`, `:76`: count invokers keyed by prefab string

**Already GUID-safe:** `LF_OreNodeComponent.SameResource` / `GuidOf`
(also used by `LF_DestructibleResourceHitZone.c:58`) and
`LF_BedDelivery.SameResource` (`Substring(0, 18)`). Batch 0 can promote
`GuidOf` to `LF_Utils` and route the sites above through it. Comparisons
between two live entities (`LF_QuantityComponent.c:290`, the inventory UI
key) are safe.

**Batch 0 (2026-11-12, #323):** done. Proof 2026-11-14: `--tier all` is 480/480 green normally and again with the `Village_Buildings`, `Village_Dressing` and `Village_Roads` layers removed from DebugWorld, so no test depends on them. `LF_Utils.SamePrefab` / `PrefabGuid` replace
both helpers above, and every site in the two lists compares by GUID, plus
`LF_PoliceRanks.ContainsPrefab`, `LF_Contraband.IsContraband` and the store's
stray-copy check. Single-prefab `ShopPrices.conf` rules moved to `m_Prefab` (GUID).
Still path-keyed on purpose, so a batch that moves such a resource must edit
them: the folder and name-family `m_sFind` rules (`Prefabs/Items/Food/`,
`Prefabs/Tools/`, `Handgun_M9`, ...) and `LooksLikeItemPath`; a pack item outside
those folders reaches the shop through `m_aExtraItemPrefabs` and a `m_Prefab` rule.
WORLD test `prefab/moved-path` replays the section 3 failures with a shared
GUID at a new path.

**The boundary check already sees the references.** On the spike branch
`validate-addon-boundary` fails with 26 lines, among them the core layers
(`Street_Vehicles.layer:1`, `Street_Lockpick.layer:21`, `Crimes.layer:187`,
`MiningChain.layer:136`), `LF_ChopShop.conf:10`, `LF_VehicleCatalog.c:9` and
the playtest tests. These are inventory blocker 1: each needs a neutral core
twin before its resource moves.

## 4. Two projects in the pipeline

Both build and pack once the tooling is fixed. Timings on Gaming-Box:

| Step | Core | Pack |
|---|---|---|
| rdb rewrite (headless Workbench) | 7 s, 453 resources, moved entries gone | 7 s, 5 resources |
| `build` (`-buildData PC`) | 8 s, OK | 8 s, OK |
| `pack` | 8 s, 49.4 MB `data.pak` | 8 s, 2.6 MB `data.pak` |
| boot from packed (`-addons 79636E668EA37AC9,B1E84D5687E11724`) | pass 2 29/29, client joined | |

What the tooling needs (none of it is in `main` yet):

1. **Repeated `-addonsDir` flags keep only the last one.** `cmdBuild`,
   `cmdPack` and `ensureResourceDatabase` pass the repo `addons` folder and the
   game junction as two flags. Workbench keeps the gproj's own folder and the
   last flag, which works for the core and fails for the pack:
   `ENGINE (E): Addon 'LF_Everon1989' dependency '79636E668EA37AC9' can't be added`,
   and no pack rdb is written. One comma-separated `-addonsDir` fixes it.
2. **`tools\cli pack` gets only its first argument.** `pack` is missing from the
   dispatch list in `tools/cli.mjs` that passes all flags, so `--out` is
   ignored. A second pack lands in `tmp/workshop-pack/LifeFramework` over the
   first (the pack's `data.pak` replaced the core's).
3. **Everything rdb-related is core-only.** `ensureResourceDatabase`,
   `rdbFreshness`, `build`, `pack` and `wt.copyResourceDatabase` name
   `addons/LifeFramework`. The pack rdb needs its own refresh. After a move the
   core rdb only drops the moved entries when Workbench rewrites it; the
   freshness check sees nothing missing and skips the rewrite. And
   `copyResourceDatabase` copies main's rdb, which still lists the moved files,
   over a smaller worktree rdb unless the worktree's is newer.
4. **`validate --full` compiles the core project only.** The spike did not run
   it. Every boot compiled the core scripts with both addons loaded, with no
   `Can't compile`. The pack has no scripts, so `validate-scripts.ps1` needs a
   pack pass only once the pack allowlist gets one.

## 5. A host that lists only the core

The header GUID is in no loaded addon, so the server stops before it listens:

```
NETWORK : Starting local multiplayer server using command line args.
ENGINE (E): Unable to initialize the game
ENGINE : Game destroyed.
```

No crash dump, no VME, no client can connect. No line names the scenario or
the missing addon, so a host sees a generic failure. The host doc, the
launcher (`playtest-server.json`), and the Workshop release must change in the
same release as batch 1 (D3 (a)), and the host doc needs this exact line in its
troubleshooting.

Hiding the pack while booting the core DebugWorld gives readable failures, not
VMEs. The tests that name the moved entities, with the pack loaded and without:

```
pack loaded:  SUMMARY tier=only passed=17 failed=1 total=18   (chopshop/payout, section 3)
pack hidden:  SUMMARY tier=only passed=10 failed=8 total=18
  RESOURCES (E): Wrong GUID/name for resource @"{E1A105BE1A000001}Prefabs/Vehicles/Wheeled/S105/EL_S105_beige.et" in property "inherited-name"
  RESOURCES (E): Wrong GUID/name for resource @"{D8D55097BA6DB899}Prefabs/Props/Signs/EL_MiningSign.et" in property "path"
  [LFTEST] FAIL signs/prefabs
  [LFTEST]   - sign prefab loads: {D8D55097BA6DB899}Prefabs/Props/Signs/EL_MiningSign.et
  [LFTEST] FAIL e2e/roadmap-vehicles
  [LFTEST]   - the locked S105 child loads
```

The engine's wording is `Wrong GUID/name`, not `Unable to find`. Layer
entities whose prefab is gone are skipped, and the VME count in that log is 0.

## What this means for D7

D7 asks whether a pre-move save must load after the move. Its recommendation
was (a) if saved `{GUID}path` strings resolve after a path change, and (b) if
it needs loader rewrites. They resolve, and the loader needs no rewrite:

- Spawning from saved strings works (vehicle records, the impound collect, the
  engine's own entities).
- The playtest header's save key is its GUID, which the move keeps.
- What breaks is comparison, not resolution: the saved-string sites in
  section 3 (unpaid tags, stored inventories, store stock). A GUID compare in
  batch 0 fixes them before anything they touch moves.

**Recommend D7 (a)**, with the spike's proof as the per-batch gate: pass 1 on
the batch's base, then pass 2 after the move with both addons (`e2e` split in
two, about 20 s a boot). Batch 0 must add the split to `cli e2e`, the pack to
the rdb and pack tooling, and the GUID compare.

**Guard (2026-11-14):** alidate-prefab-compare flags a new GetPrefabName() or *Prefab* variable compare by path (mark a meant one // lf-path-ok: <why>); the sweep also moved LF_BankRobKit and LF_AssetStatesHarness to SamePrefab. WORLD test prefab/guid-compare pins a right GUID under a wrong path.

