# Action-context anchors

`tools/actions/place-action-contexts.mjs` puts a `UserActionContext` point
(`PointInfo` `Offset`) and `Radius` on a mesh instead of by eye. With no
arguments it fixes door-default interactables (unchanged). With arguments it
anchors one entity or checks all of them. Code: `tools/actions/action-anchor.mjs`.

## Bounds

Mesh bounds are read from the `.xob` header (min/max, entity-local space, the
same space a `PointInfo` Offset uses), from the addon or the game paks. An
entity's mesh is the first `MeshObject` found up its prefab chain. An entity
with no mesh (an empty marker such as `Roadmap_Garage`) needs `--anchor custom`.

## Anchors

| Anchor | Offset | Radius |
|---|---|---|
| `center` | bounds centre | half the larger footprint side + 1.25 m, 1.5 to 4 |
| `front` | centre of the +Z (or `--front -z`) face, at `--height` (default 1.0), clamped to the mesh top | 1.5 |
| `top` | top face centre | as `center` |
| `custom` | `--offset x y z` | `--radius`, default 1.5 |

The radius is picked so a player standing 1.0 to 1.5 m off the mesh reaches the
point. Override with `--radius`. The edit rewrites the entity's own
`UserActionContext` (or adds one using the ancestor's GUIDs) and removes a
`Height` so the volume stays a sphere.

## Use

```
node tools/actions/place-action-contexts.mjs --anchor front --front -z Roadmap_Fuel_Till
node tools/actions/place-action-contexts.mjs --anchor center "Prefabs/Police/LF_PoliceDutyDesk.et#DutyClock"
node tools/actions/place-action-contexts.mjs --anchor custom --offset 0 1.2 0 --radius 2.5 Roadmap_Garage
node tools/actions/place-action-contexts.mjs --batch [--dry]
node tools/actions/place-action-contexts.mjs --check
```

Target: a layer entity name, `File.layer#Name`, a prefab path, or
`Prefab.et#Child`. `--dry` prints the change without writing. `--context name`
picks one of several contexts. `tools/actions/anchors.json` lists per-prefab
anchors for `--batch`; keep an entry there for anything you anchor so a rerun
reproduces it.

## Check

`--check`, run by `validate-action-reach`, looks at every context written on an
entity with a known mesh. WARN: the point is more than 0.5 m outside the mesh
bounds, or the radius does not reach a player standing 0.5 m off the footprint
at chest height (1.0 m). ERROR (fails validate): the point is more than 3 m
outside the bounds. Tune with `--tolerance` and `--fail-gap`. Character meshes
(NPC talk points) show up as reach warnings; they are known and harmless.

Bounds say where the mesh is, not which side faces the street. Choose `--front`
by looking at the entity in the world, or use `center`.

## Counter NPCs

A clerk, teller or desk officer stands behind a counter; the customer stands in front. The talk context on an NPC is a sphere on its `Spine5` bone, so its `Radius` is how far across the counter the prompt shows. The old 0.6 m put the prompt behind the counter ("you couldn't really talk to the clerk across the counter", live play 2026-11-22). Counter staff now use **2.4 m** (the `RoleTalk` context on `Npc_Base.et` too): a customer 1.5 to 2.2 m away across a counter has the prompt without walking round. `LF_DialogueTalkAction` and `LF_QuestAction` still show only within 3 m of the NPC and only on the same level (a storey is about 2.8 m), so the larger sphere does not show through a ceiling.

`validate-action-reach` fails when it is wrong (`tools/actions/counter-npc.mjs`):

- every layer NPC (a prefab with a Talk, Quest or RoleTalk context) with a counter prop in front of it (name or prefab matching counter, till, desk, stall, kiosk, window or teller, bounds from the `.xob`, turned by its yaw): the customer stands 0.35 m past the counter's far face, and the NPC's radius must reach that spot;
- every `LF_ResidentAnchorComponent` `counter` anchor, against the smallest talk radius of a resident whose work kind is a counter, till or desk (`LF_Residents_*.conf`);
- the counter-staff prefabs themselves (`LF_Shopkeeper`, `LF_FuelClerk`, `QuestGiver_Clerk` and every resident prefab at a counter work kind): a talk radius of at least 2.0 m.

A new counter NPC prefab sets its `Talk` (or `Quest`) context `Radius` to 2.4; `LF_Test_CounterNpcTalkReach` (core) and `LF_Test_EveronCounterTalkReach` (pack) cover the shipped ones.
