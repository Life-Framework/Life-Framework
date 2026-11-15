# Polishing a building by hand

Every building is made once and polished once, by hand, then iterated in the game. Polish is per
building: there are no rules a building has to pass, and a building type has no rule set of its own
(we build one supermarket, not a supermarket generator). This page is the workflow, the pictures to
look at, and the patterns that have gone wrong before. The pipeline is in [README.md](README.md).

```
tools\cli building run <slug>                         spec .. generate .. review .. polish .. export .. import .. prefab
tools\cli building adopt <slug> [<file.blend>]        keep a scene edited by hand as the export source
tools\cli building review <slug> --shots              Blender review renders and a contact sheet, about 20 s, no game
```

## The loop

1. **Run the pipeline** (`tools\cli building run <slug>`). The `polish` stage renders what you will
   judge into `tmp/buildings/<slug>/polish/`: `plan.png` (a section 2.2 m above the floor, seen from
   above: rooms, doors, fixtures), `front/back/left/right.png` from eye height, `street.png` (the
   street corner) and `inside.png` (from inside the entrance, facing the back wall). It prints the hand
   edits the spec lists and nothing else; it never fails a building.
2. **Settings first.** Most fixes are ArchBuilder settings in `spec.archbuilder` (size, seed,
   `custom_dims` + `storey_h` + `wall_thick`, materials, roof). Change them and rerun `--from generate`.
   A setting survives every regenerate; a hand edit does not (below).
3. **Then the scene, by hand**, for what no setting does:
   - The maintainer's Blender: open `tmp/buildings/<slug>/building.blend` (or the building's
     `polished.blend`) with File > Open in Blender 5.3 with ArchBuilder. Edit, File > Save.
   - An agent: a Blender script on the file, headless (`blender --background <file> --python
     <edit.py> -- <out.blend>`), kept beside the spec as `tools/buildings/<slug>/polish-*.py` so it can
     be run again. Example: [`tools/buildings/supermarket/polish-dado.py`](../../tools/buildings/supermarket/polish-dado.py).
   - Keep ArchBuilder's Live Update off while editing: changing a setting in its panel regenerates
     the building and throws the scene edits away.
4. **Adopt it**: `tools\cli building adopt <slug> [<file.blend>]` copies the file to
   `tools/buildings/<slug>/polished.blend` (tracked) after checking it carries no ArchBuilder licence
   watermark (the repo is public). List each edit in `spec.polish.edits` in one line.
5. **Rerun `--from polish`.** With a `polished.blend` there, the export uses ArchBuilder's scene export
   (`ab.rf_export_scene`) on it instead of the generated building, so the hand edits reach the game.
6. **Look in Blender, every iteration** (`tools\cli building review <slug> --shots`, next section), and
   go round again from step 2 or 3. **Look in the game only once the Blender pictures are right**: the
   final proof and the pictures for the PR (pictures below). A game session takes minutes to start;
   a canopy, a sign, a wall colour or a room's layout is judged from the Blender shots in seconds.

### Review shots (Blender, no game)

`tools\cli building review <slug> --shots` renders the building straight from its `.blend` (the
`polished.blend` if there is one, else the generated one) in headless Blender 5.3 with the Workbench
engine: no ArchBuilder, no game, no pool slot. The supermarket takes about 20 s. It writes
`tmp/buildings/<slug>/review/contact.png` (read this first) and the single shots beside it:

| Shot | What it shows |
|---|---|
| `street_front.png` | the front from the street at eye height, the whole width: canopy, sign, windows, roof line |
| `street_3q.png` | three-quarter view from across the street: depth of the canopy, the side wall, the roof |
| `entrance.png` | the entrance door close up, canopy and fixtures beside it |
| `hall.png`, `hall_back.png` | the biggest room at eye height, from the entrance and from the far end |
| `room1.png` ... | each back room, from its far end looking at the door |
| `plan.png` | a section 2.2 m above the floor, rooms labelled with their size, the entrance marked |

Where each camera stands comes from the building's bounds, the gap in its front wall (the entrance)
and the rooms found in a plan slice at 1.2 m (the distance to the nearest wall, partition or frame, with
door gaps closed), never from numbers kept per building. `shots.json` lists every camera in Blender and
prefab space (prefab `x z y` is Blender `x y z`, so Blender -Y is the prefab's -Z), so a game picture of
the same view is `shot world` with the camera in prefab space added to the placement.

What the pictures can and cannot say: shape, proportions, colours, which door opens where, what stands
in the way of a door, whether the sign is behind the canopy. Colours are the game's (each material's
Blender colour times the scale the export gives it, `materials.mjs`) lit by Workbench's studio light,
with the exposure raised for the sunlit shots so a wall reads about as bright as the game's. It does
not show textures, glass reflections, the game's light or the vanilla door and window sets (they are
the model's holes here). A ceiling reads darker than in the game. Trust it for form, and use the game
for the final look.

### What a scene edit can change

ArchBuilder's scene export ships the scene's **walls, slabs (floors and ceilings), roof, partitions
(`interior`) and wall finishes (`lining`)**, and any mesh you add without an ArchBuilder part. It takes
everything else from its own plan, whatever the scene shows: **window frames, glass, doors, interior
doors, furniture, lamps, `detail` (rooftop units, pipes, canopy posts) and `trim` (canopies, fascia
boards, cornices)**. Editing those in Blender changes the Blender view and nothing in the game.

For those:
- **Game entities** (the shop sign, lamps, props, switches) move with a nudge in the spec, applied
  to the exported prefabs on every export:
  ```json
  "polish": { "entities": [ { "prefab": "SignCommercial_01_GroceryStore", "move": [-5.1, 0, 0], "why": "..." } ] }
  ```
  `prefab` is the vanilla prefab's file name; every child of the building or furniture prefab with
  that name moves by `move` (metres, prefab space: -Z is the street front). A name the export did
  not place fails the export. Do not nudge door sets or windows: their hole in the wall stays put.
- **Props ArchBuilder never places** (stock on the stockroom shelves, boxes, a punch clock, files on a desk) go in
  `spec.polish.props`, appended to the furniture prefab on every export:
  ```json
  "polish": { "props": [ { "prefab": "{C76C3AF337109066}Prefabs/Props/Crates/CardboardBoxStack_01.et", "at": [-3.6, 0.1, 6.05], "why": "..." } ] }
  ```
  `at` is building space like a nudge, `yaw` optional; the prefab is named with its `{GUID}` (`toolscli call
  asset_search` finds the path, the game's resource database the GUID) and the prefab stage checks it. Floor is
  y 0.1. Heights on shelves come from the model: `tools\cli measure <prefab> --surfaces` (the shop fixtures' are cached by `tools\cli fixtures` in `tools/buildings/pipeline/fixtures.json`); a
  `look item` picture for the rest, then the game picture.
- **Plan decisions** (which rooms, where doors go) are ArchBuilder settings, or a small change to
  ArchBuilder itself, listed in [README.md "Changes to ArchBuilder"](README.md#changes-to-archbuilder).

### Changing settings later

The polished blend wins over the spec. A setting changed in the spec after the polish does nothing
until the blend is made again, and the `polish` stage warns with each setting that differs. To take a
new setting: `git rm tools/buildings/<slug>/polished.blend`, rerun `--from generate`, redo the edits
(run the `polish-*.py` scripts), adopt again.

## Pictures

**Blender**: `building review --shots` (above) for every iteration; the `polish` stage's renders
(`plan`, `street`, `inside`) are the older, fixed set. Put the Blender picture of a view beside the
game's in the PR.

**Game**, always on a full-quality session (`--profile full`; lowspec textures hide materials and
glass). For a building at `x y z` with its front on -Z (an unrotated placement):

```
node agent-box/session.mjs up --profile full
node agent-box/harness.mjs type RegFirstName Ada      (a fresh client opens form R-89 first: sign it)
node agent-box/harness.mjs type RegLastName Grocer
node agent-box/harness.mjs type RegAge 40
node agent-box/harness.mjs click SignButton
node agent-box/harness.mjs shot world b_front  <x> 2.5 <z>  0 0 -21     four sides, about 20 m out
node agent-box/harness.mjs shot world b_back   <x> 2.5 <z>  0 0 21
node agent-box/harness.mjs shot world b_left   <x> 2.5 <z>  -20 0 0
node agent-box/harness.mjs shot world b_right  <x> 2.5 <z>  20 0 0
node agent-box/harness.mjs shot world b_street <x> 3.2 <front z>  -8 -1.5 -9     the street corner, the sign
node agent-box/harness.mjs shot world b_inside <x> 1.6 <back z - 0.5>  0 0 -11  from the entrance to the back wall
node agent-box/harness.mjs shot world b_ref    156 1.6 262  0 0 4      a vanilla interior, same light (House_Town_E_2I01)
node agent-box/session.mjs building-proof --entity <Entity> --side s9
node agent-box/session.mjs pull
node agent-box/session.mjs down
```

`shot world` puts the character at target + offset facing the target, at noon in clear weather, so
pictures from two sessions compare. The captures are `.bmp`; convert to `.jpg` for the PR.

## Patterns

Things that went wrong on a real building. Check each against the pictures; none of them is a gate.

| Pattern | Look at | What we did |
|---|---|---|
| **Doors in a row.** Back rooms each opening onto the main room give a row of doors on one wall (the supermarket had four: stockroom, office, two WCs). | `plan`, `inside` | The export prints `WARN  N door sets in a row on the wall at ...` for three or more on one wall line. In a hall program, ArchBuilder's `@` room is the one back room that opens onto the hall, and its neighbours open into it. |
| **Too tall.** ArchBuilder scales its profile storey by 1.15 for Reforger proportions: the SUPERMARKET's 5 m storey became 5.75 m, plus a 1 m parapet. | `front`, against the table below | `custom_dims: true` with `storey_h` and `wall_thick: 0.45` (the vanilla wall the game windows are made for). |
| **Out of scale with Everon.** Footprint and height far from the buildings it stands beside. | the four sides; `tools\cli shots area <x,z>` for neighbours | Compare with the reference table below. |
| **Sign behind a canopy.** A fascia 0.1 m above a 3.45 m deep canopy is hidden from anywhere near the street. | `street` | Nudge the sign entity sideways, over a shop window. |
| **Door onto a fixture.** A shelf, cooler or counter in front of a door, either side. | `plan`, in game | Move the fixture with an entity nudge, or reseed. |
| **Entrance not on the street.** ArchBuilder builds the front on Blender -Y, the prefab's -Z. | `front` | Rotate the placement, not the building. |
| **Interior too bright.** Walls and ceiling read white, and from outside the shop glass reads milky because the hall behind it glows. | `inside`, `b_ref` | See "Interior brightness". |
| **Blank walls, missing doors.** A shop's stockroom with no delivery door, a side with nothing on it. | `back`, `left`, `right` | Supermarket: still open (no ArchBuilder setting adds a back door). |
| **Rooftop clutter.** Units and ladders above the parapet read from the street. | `front`, `street` | They are `detail`: settings or ArchBuilder, not the scene. |
| **A flimsy canopy.** A thin plate over the door on two thin posts reads as a petrol station, not a shop. | `street`, `front` | Supermarket: `canopy='SHOPFRONT'` in ArchBuilder, a slab along the whole glazed front with a painted fascia. |
| **Public fittings in staff rooms.** A row of WC cubicles, living-room shelves in a stockroom. | `plan`, in game | Staff back rooms (behind the `@` hub) get one toilet and stock shelving (README "Changes to ArchBuilder"); the stock itself is `polish.props`. |

### Interior brightness

What was wrong (#1143, 2026-11-15, full profile, noon): every supermarket interior picture had a near-white
ceiling (204 of 255) and walls at 160, flat, with no light falloff; the base game's interiors read about 105
and 98 (a vanilla food store, `d_food2`). What it is not, each tried in a session with the variants placed
side by side:

- **Not the probe.** The building's `ProbeHouse` (`GameEnvironmentProbeEntity`) has no visible effect here: the
  vanilla-house settings, `LVDiffuseBrightness -6.5` and `-12`, `InteriorProbe 0` and no probe at all all gave
  the same ceiling (204.3 +- 0.2). The base game's own shop uses the same default probe and reads dark, so the
  room is lit by something the probe does not control; tuning probe numbers is wasted time.
- **Not the lamps.** ArchBuilder's ceiling lamps (`LampFluorescent_*`, `LightRectangle_01`) are meshes with no
  light in the base game too, so the hall had no light of its own: two dead panels in a 17 x 9 m hall. A
  `LightEntity` added by day changes nothing in the picture (ceiling 139 with and without), because the sky
  ambient is far brighter than an interior lamp; at night the same lights give a lit floor and pools on the
  walls (`LV 6`, below), where the old hall was black.
- **Albedo is the one lever that works by day.** The first calibration (interior white 0.30) left the
  ceiling at 204. Halving again (0.15) read 162, 0.10 with a warm tint reads 134 (the ambient is blue, so a
  neutral albedo reads blue-grey).

What the export does now (`tools/buildings/pipeline/materials.mjs`, `lighting.mjs`):

- A **room white**, a light finish (Blender luminance 0.6 or more) on a plaster-like texture (mean 0.5 or more)
  whose faces look sideways or down (not a floor, not an outside wall), is calibrated to albedo 0.10 instead of
  0.30. A near-neutral one (saturation under 0.15, the plaster) also gets a warm tint (`1 0.93 0.8`); cream and
  blue paint keep their hue. A tile, a floor or a dark finish keeps the interior scale. The export prints
  `room` on those lines.
- **Ceiling fixtures from the room sizes.** The materials job also returns the building's rooms
  (`blender/lf_rooms.py`: ArchBuilder keeps no room list, so it flood-fills the free floor inside the ceiling
  slab behind the partitions, splits it at the doors and takes each blob's box). Each room gets a grid of
  fixtures about 4.5 m apart (at most 12): the vanilla `LampFluorescent_02` panel plus a spot `LightEntity`
  (`LV 6`, radius 7, 140 degrees, warm white) 0.2 m under the ceiling. They replace ArchBuilder's own ceiling
  lamps. The base game's `Lamp_LightEntity_base` is not used: it is switched off (`Visualization Disabled`).
  A building changes them with a `lighting` block in its spec (`spacing`, `max`, `lv`, `radius`, `angle`,
  `color`, `drop`) or turns the preset off with `"lighting": { "off": true }`.

Supermarket, same cameras (hall from the entrance, entrance from the hall, back room), noon, ceiling / wall luminance:

| | Before | After | Vanilla food store |
|---|---|---|---|
| Hall | 204 / 161 | 139 / 101 | 105 / 98 |
| Entrance | 204 / 181 | 131 / 130 | |
| Back room | 182 / 163 | 115 / 99 | |

Pictures: `website/public/docs-images/1143/lighting-*-before|after.jpg`, `lighting-vanilla-shop-ref.jpg`,
`lighting-*-night-after.jpg`. A night picture: `harness.mjs scene start street 22 Clear`, then `shot world`.
What is still flat: by day the whole room is lit by one even sky ambient, so there is no falloff to see. The
lights are what give a room shape at night (and a power cut something to switch off, later).

## Everon reference

The base game's own buildings, measured from the game data (game 1.9): the model's bounds from its
`.xob` (`tools/assets/pipeline/xob.mjs`), and the height of its door and window sockets. Footprint is
the model's bounds, so it includes porches, terraces and plinths. Height is the top of the model above
the ground-floor door. Storey height is the step between the window sockets of one window type on two
floors. Numbers only: look at a building beside them, do not test against them.

| Building | Kind | Footprint (m) | Height (m) | Storey (m) |
|---|---|---|---|---|
| `ShopModern_E_01` | the vanilla grocery (GroceryStore sign), one storey, flat roof, L-shaped with a garage wing | 24.4 x 19.8 | 5.8 | one storey |
| `ShopHouse_E_2I01t` | town shop with a flat above | 12.4 x 16.8 | 14.9 | 4.1 |
| `PubVillage_E_1L01` | village pub, one storey and attic | 16.5 x 13.8 | 10.9 | |
| `PubVillage_E_2L01` | village pub, two storeys | 17.9 x 17.0 | 11.1 | 3.5 |
| `House_Town_E_2I01` / `2I02` / `2I03` | town houses, two storeys and attic | 12.4-20.3 x 12.6-16.8 | 14.9-16.5 | 4.1 / - / 3.3 |
| `House_Village_E_1I01` .. `1I08`, `1L01` .. `1L03` | village houses, one storey and attic | 9.9-26.7 x 10.2-25.6 | 10.0-12.5 | |
| `FarmHouse_E_1L01` | farmhouse | 16.2 x 21.2 | 11.0 | |
| `MunicipalOffice_E_01` | town hall | 24.1 x 17.8 | 14.6 | 4.0 |

| Opening (vanilla door or window set) | Frame (m) |
|---|---|
| `DoorSet_ShopModern_E_EXT_A` (the grocery's shop door) | 2.78 x 3.20 |
| `DoorSet_Services_E_A_Double_UNI_B_EXT` (our supermarket's glass double door) | 2.70 x 3.00 |
| `DoorSet_Village_E_01_*_EXT_COV_B`, `_INT_COV_B` (house doors, inside and out) | 1.61 x 2.55 |
| `Win_ShopModern_E_A_305x240_1_B` (the grocery's shop window) | 3.12 x 2.46 |

The supermarket after this polish: 18 x 14 m, 3.8 m storey, parapet top 4.9 m, rooftop unit 6.9 m
(it was a 5.75 m storey, parapet top 6.9 m, rooftop unit 8.8 m).
