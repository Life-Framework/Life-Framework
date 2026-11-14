# Placement and building foundation

Epic #927, first child #928. One way to put an item or a set of props into the world with a
preview: a spike strip on the road first, then a roadblock, a market stall, and house furniture.
This page is the research into the vanilla Conflict building system, the design, and the issue
list. The spike strip prototype in section 7 is in the tree behind the dev-tools flag.

## 1. Verdict on Conflict's building system

Conflict has two placement systems, and they are very different:

- **The building mode** is a Game Master editor mode. A provider entity opens a free camera, a
  content browser and the editor's placing and transforming components. That whole stack is
  Conflict's: factions, military bases, ranks, supply budgets and editable entities.
- **Item placement** is a first-person ghost for a carried item (mines, charges). It knows nothing
  about factions, and it has exactly the hooks we need.

What that means for us:

- **Reuse as is:** the build materials, `Assets/Conflict/CanBuild.emat` (blue) and
  `Assets/Conflict/CannotBuild.emat` (red); `SCR_Global.SetMaterial`; the local-ghost pattern (a
  non-replicated `GenericEntity` with the item's mesh); the oriented-box space check and the
  "not on anything dynamic" surface rule from `SCR_ItemPlacementComponent`.
- **Mod or subclass:** `SCR_PlaceableItemComponent`'s override hooks (`OverridePreviewTransform`,
  `OverrideSpaceValidation`, `OverrideIsSurfaceValid`, `OverrideStartPlaceAction`) and the
  `CharacterPlacementContext` / `CharacterPlaceItem` inputs, once aimed placement (#1028) needs
  them.
- **Replace:** the confirm. Vanilla trusts the client's transform (its server checks only the
  distance) and places the item itself. We spawn a different prefab and decide on the server.
- **Strip:** the editor building mode, providers, budgets and supplies, faction and rank checks,
  layouts built with the shovel, and `SCR_PersistenceSystem` saving.

## 2. What the vanilla code does

Read with `tools\cli call game_read` and `api_search` on 2026-11-14.

| Piece | Where | What it does | Conflict-only |
| --- | --- | --- | --- |
| `SCR_CampaignBuildingProviderComponent` | `Scripts/Game/Building/` | The thing you build at: radius (50 m), minimum rank, budgets, faction check, supplies through `SCR_ResourceComponent` | Yes: extends `SCR_MilitaryBaseLogicComponent` |
| `SCR_CampaignBuildingManagerComponent` | `Scripts/Game/Campaign/` | Game-mode side: the composition catalogue, refunds, supply events, `SCR_PersistenceSystem` load guard | Yes |
| `SCR_CampaignBuildingNetworkComponent` | `Scripts/Game/Building/` | Player-controller RPCs: enter build mode, add build value, client lock | Yes |
| `SCR_CampaignBuildingEditorComponent`, `...PlacingEditorComponent`, `...TransformingEditorComponent` | `Scripts/Game/Editor/` | The Game Master mode: provider list, budget per placement, cooldowns, move inside the radius | Yes |
| `SCR_CampaignBuildingPlacingObstructionEditorComponent` | `Scripts/Game/Editor/Components/Editor/` | Ghost validity in build mode: inside the zone, cylinder traces for blockers, a tilt warning over 20 degrees, sea level and water | The rules are generic, the class is editor-bound |
| `SCR_PreviewEntityEditorComponent`, `SCR_EditablePreviewEntity` | `Scripts/Game/Editor/` | The multi-entity ghost of a composition (preview entries, vertical modes) | Needs editable entities |
| `SCR_CampaignBuildingCompositionComponent`, `...LayoutComponent` | `Scripts/Game/Building/` | A placed composition and the outline a shovel builds up into it | Yes |
| `SCR_ItemPlacementComponent` | `Scripts/Game/Components/` | On the player controller. While a placeable item is in the weapon slot: ghost from the item's mesh, a trace along the camera, tilt limit, an oriented-box space check, a dynamic-surface check, the blue or red material, place on `CharacterPlaceItem` | No |
| `SCR_PlaceableItemComponent` | `Scripts/Game/Components/` | Per item: reach, tilt, placement type (XZ fixed or XYZ), the preview mesh, and the override hooks | No |

How vanilla replicates and persists a placement: the client sends the transform in
`RPC_AskSetPlacementPosition`, and the server checks only that it is within reach. The item is
then dropped from the inventory at that transform and is an ordinary replicated item from there
on. Conflict compositions are spawned on the server by the editor and saved by
`SCR_PersistenceSystem`.

## 3. Compositions and the ghost

A composition is a parent prefab with child props. Vanilla previews one through
`SCR_EditablePreviewEntity.SpawnPreview(entries)`, which needs `SCR_EditableEntityComponent` on
the props. #1031 tries those preview entries first and falls back to a light LF ghost: one local
entity per child mesh, all in the build material, moved together.

## 4. The core API

```
LF_PlaceableComponent (on the item or catalogue entry; #1026)
    placed prefab, footprint box, snap mode, rule kind, lifetime (temporary | permanent)
LF_PlacementProbe            what a spot measured: ground, step, blocker      (in the tree)
LF_PlacementRules.Judge      probe -> VALID | NO_GROUND | TOO_STEEP | BLOCKED (in the tree)
LF_PlacementGhost            the local ghost in the build colours             (in the tree)
LF_PlacementService.Confirm  server: re-probe, rules, cost, rate limit, spawn (#1026)
LF_PlacementPolicy           who, where, cost, per kind, from .conf           (#1030)
```

- **Who places.** The server decides from the account: on-duty police for police kit, the owner or
  a key holder inside a property, an organization member at its base (#1030). The client only
  hides an action it would refuse.
- **Where.** Snap modes: ground, across a road, floor, wall face (#1028). Zones: anywhere, roads
  only, inside an owned property, an org base radius. Land ownership joins in M8.
- **Validation.** One probe, one pure verdict, run the same on both sides. Ground (terrain or a
  static entity), the step along the footprint (the placement tool's 0.25 m rule), a clearance box
  lifted off the surface for things standing there, and the surface itself (not a car, not
  something dynamic, not another placement of the same kind). Roads and doors come next (#1029).
  Tilt and water follow `SCR_CampaignBuildingPlacingObstructionEditorComponent` when a consumer
  needs them.
- **Cost.** Money (the player's cash, the physical MoneyStack), an item used up, or free. It is
  taken only after the verdict is `VALID`, and refunded on pick-up when the kind says so.
- **Confirm and replication.** The client proposes a transform: with the action's own data
  (`BaseUserAction.OnSaveActionData` / `OnLoadActionData`), or with one `RpcAsk_ConfirmPlacement`
  on the character when there is no action. The server re-probes at that spot, accepts within a
  tolerance, runs `LF_RateLimit` (SPAWN), takes the cost and spawns a replicated prefab. The ghost
  is never replicated.
- **Removal and pick-up.** A pick-up action on the placed thing, with the same policy; it gives back
  the item or the refund. A temporary placement despawns on its timer and on the world reset.

## 5. Persistence

Temporary placements (the spike strip, a roadblock, a market stall that packs up) never save.
Permanent ones (furniture, an org base) write a record through a new LF payload: a stable
placement id, the prefab, the transform, the owner key (player uid, property id or org id) and the
kind. Never an `EntityID`. Load is idempotent, skips a prefab that no longer resolves, and is
proven by a real restart (#1032).

## 6. Consumers, in order

1. **The spike strip** (#928): the prototype below, then the service, aimed placement and the road
   probe.
2. **The police roadblock** (#647 with #1031): cones and barriers as one composition.
3. **The market stall** (#1034): a composition on the square for a pitch fee.
4. **House furniture** (#1033): floor and wall snap inside an owned property, persistent.

## 7. The prototype in the tree (#928)

- `Feature/Placement/LF_Placement.c`: `LF_EPlacementVerdict`, `LF_PlacementProbe`,
  `LF_PlacementRules` (`Judge`, `Reason`, `FindBlocker`, `SurfaceBlocker`) and
  `LF_PlacementGhost`.
- `LF_SpikeStrips.Plan` (since #1026 `LF_PlacementService.Plan`) measures the spot into a probe. `LF_SpikeStrips.Deploy` (server) and
  `LF_SpikeStripPreview` (client, every 50 ms) both use it, so the colour the officer sees is the
  answer the server gives. Deploy now also refuses a strip on top of another, under a car, or
  against a post.
- The flag: the preview runs only while the game mode has dev tools on (DebugWorld). Selecting the
  folded strip's Deploy action shows the ghost; the harness verbs `spikepreview` and
  `spikepreviewoff` do the same for a proof.
- Proof: LOGIC `placement/validity`; WORLD `police/spike-strip` (a second strip on the same spot is
  refused, "SpikeStrip is in the way"); `session.mjs placement-preview-proof` with the pictures in
  `website/public/docs-images/928/`.
- Found on the way: the AI road network has no road at the DebugWorld main street (`plan: road 0`
  on the server and the client), so the strip follows the officer's facing there. That is #1029.

## 7a. The placement service in the tree (#1026)

- `Feature/Placement/LF_PlaceableComponent.c`: the placed prefab, the footprint box (the clearance box,
  lifted off the surface; the ground is sampled at its two z ends and its middle), the reach in front
  of the player, the snap mode (`GROUND`, `ACROSS_ROAD`; `FLOOR` and `WALL` refuse until #1028), who
  (`ANYONE`, `ON_DUTY_POLICE` until the #1030 policy) and the lifetime. It replaced
  `LF_DeployableItemComponent`.
- `Feature/Placement/LF_PlacementService.c`: `Plan` (both sides, the spot by snap mode), `ProbeAt`
  (both sides, the probes at one spot) and `Confirm` (server: who, holder, `LF_RateLimit` SPAWN, the
  0.5 m / 10 degree tolerance against its own plan, the probes at the proposal, spawn, the item used
  up, `[LFDebug:Placement] placed ...`). `LF_PlacementGeometry` holds the pure geometry it shares
  with the strip.
- The proposal travels in the action's own data: `LF_SpikeStripDeployAction.OnSaveActionData` writes
  the ghost's origin and forward on the officer's machine, `OnLoadActionData` keeps it on the server for
  3 s, `PerformAction` confirms it. No RPC. A perform with no proposal confirms the server's own plan.
- `LF_SpikeStrips.Deploy` / `DeployAt` are consumers: they add the deployer and the strip's log line.
- Proof: WORLD `placement/confirm` (own spot places; 3 m off, turned 30 degrees, on a laid strip and
  a civilian are refused with the reason); `session.mjs placement-confirm-proof` (two clients on a
  dedicated server: the strip lies within 1 mm of the ghost on both; pictures in
  `website/public/docs-images/1026/`).

## 8. Core and pack

All of the above is core: a modern-setting server would use it unchanged. The pack decides the
choices: which props are placeable and what they cost, the furniture catalogue and its prices, the
roadblock and stall compositions with a period look, the market zone and the pitch fee. The pack
sets them in `.conf` overrides, so the core needs no pack reference
(`docs/architecture/core-vs-pack.md`).

## 9. Issues

| # | Issue | Milestone |
| --- | --- | --- |
| #928 | Spike strip placement preview (prototype in the tree) | M5 |
| #1026 | Placement service: placeable data and a server-checked confirm | M5 |
| #1028 | Aimed placement: the ghost follows the aim, turns, snaps, places or cancels | M5 |
| #1029 | Runtime placement probes for roads and doors | M5 |
| #1030 | Placement rules: who may place, where, and what it costs | M5 |
| #1031 | Composition placement: one ghost and one confirm for a set of props | M5 |
| #1032 | Persistent placements: saved, restored after a restart, picked up, capped per owner | M8 |
| #1033 | House furniture placed on floors and against walls | M8 |
| #1034 | Market stall placed on the square for a pitch fee | M6 |
