# Sign system: style, size, content, finish

The design for the sign overhaul (maintainer, 2026-11-18: "the current sign system is pretty bad; it needs a
massive overhaul to support different types, sizes, textures etc. using different prefabs to display on").
Feature doc: `addons/LifeFramework/Scripts/Game/Feature/Signs/Feature.md`. Player-facing rows:
`docs/features/ui-layouts-localization.md`.

## What exists (2026-11-18)

| Piece | Where | Problem |
|---|---|---|
| `LF_SignComponent` | `Feature/Signs/LF_SignComponent.c`, 941 lines | One component draws every sign kind through two hard-coded paths (paper sheet, painted face) with a dozen per-prefab attributes (`m_vFaceArea`, `m_vTitlePos`, `m_fTitleRotation`, rim/paint/text colours). A new kind means new branches |
| `LF_SignDef` | `Configs/Signs/LF_Signs.conf` + pack overlay | Only a title and the Read card's five sections. The face can print one title line, nothing else: no price rows, no lines, no icon |
| Carriers | `LF_SignFace.et` (vanilla A4 poster, UVs `u 0..0.667`), `LF_BoardFace.et` (our 16:9 quad), vanilla plates re-materialed | The A4 sheet is portrait, so landscape boards (chalkboard, A-frame) turn it 90 degrees and rotate the text; a game client shows about 328 of the 512 units across. Each prefab hand-tunes offset, angle, scale |
| Faces | `LF_SignFace.layout`, `LF_SignBoardFace.layout`, `LF_SignPaintFace.layout` | All RobotoCondensed Bold on a flat colour. A chalkboard looks like a green plastic panel with a printed label |
| Price board | `Prefabs/Props/Market/LF_PriceBoard.et` | Inherits the chalkboard but sets no sign id, so its face is blank; the prices are only on the Read card |
| Town board | `Feature/TownBoard/LF_TownBoardFace.c` | Its own face code; keeps it (it is a map, not a sign) |

Findings that shape the design:

- **A render target needs a one-mesh carrier whose bake uses the `$rendertarget` material.** The licence plate was
  white because its xob drew the window face as a second mesh object with the box material (#2476, #3039). A
  material override over a placeholder or a multi-object bake renders white. `LF_BoardFace.xob` is one quad, one
  material, UV 0..1 over the whole face: the proven carrier (the wanted board, `docs/architecture/visual-boards.md`).
- **Workbench never draws a render target.** The `look` rig and the World Editor view show every sign face white or
  blank (#3075). `look` proves placement and the carrier mesh, never the face.
- Workbench renders a target at layout size times the DPI scale while widgets lay out one unit per pixel
  (`FaceTitleRect`); a game client does not. Anything measured in Workbench is not a client measurement.
- The base game has no handwriting font: `UI/Fonts` holds Roboto, RobotoCondensed, NotoSans and a mono MSDF font.

## The model

A sign on screen is **style x size x content x finish**. Each axis is data; the prefab only picks a carrier.

| Axis | Decides | Lives in | Who adds one |
|---|---|---|---|
| **Style** | How lettering is made: layout (and so the font), ink colour and alpha, ground colour or texture, title case, the ghost of old chalk, hand tilt, row leader, margins | `LF_SignStyleDef` rows in `Configs/Signs/LF_SignStyles.conf`; a style names its layout `UI/Layouts/Signs/Styles/LF_SignStyle_<Id>.layout` and row layout | core ships the generic styles; a pack overlay adds or patches (`signstyles`) |
| **Size** | Which carrier mesh the face is drawn on and how many layout units it shows | `LF_SignSizeDef` rows in the same file: carrier prefab, render target units, the shown area | core (a carrier needs a mesh) |
| **Content** | What it says: title, lines, price rows (label, value), optional icon | `LF_SignDef` (existing ids, new `m_aRows`, `m_sStyle`) in `LF_Signs.conf` and its pack overlay | core neutral content; the pack the place names and prices |
| **Finish** | Wear on top of the style: weathering, tape, chips, fading | a style's optional `m_sFinishImage` imageset (later: a finish row a placement can pick) | core |

The prefab says which **size** it carries and its default **style**; a sign row may name another style (a pack restyles
a placed sign without touching the layer). The component keeps `m_sSignId`, the place claim and the Read action.

### Styles (target set)

| Style id | Look | Carrier |
|---|---|---|
| `chalk` | Dark smudged slate, chalk-white lettering at 0.93 alpha (the title in RobotoCondensed Bold, up to 240 units; rows Regular), a faint ghost of rubbed-out writing, a slight hand tilt, dot leaders on price rows | the chalkboard, the A-frame pavement board, the price board |
| `slate` | The chalk layouts on a near-black school slate (no teal), chalk-white ink at 0.86 alpha, a stronger ghost, the title held to a quarter of the face (160 units) so the rows lead | the outpost stock board (#4675) |
| `poster` | Printed paper, off-white, black ink, a red band, worn edges and tape at the corners | notices, wanted and event posters |
| `wood` | Painted wooden board, cream letters on a stained ground, grain texture | farm, hideout, hand-made signs |
| `enamel` | Enamel metal plate: white rim, deep colour, chipped corners | civic plates (police, post, hospital), pole plates |
| `fascia` | Shop front lettering: large condensed capitals with a drop shadow on the fascia colour | bank, stores, garage |
| `card` | A small printed notice or price card: one title, up to six rows | shop counters, shelves |
| `plaque` | Engraved door plaque: brass ground, dark letters | houses, offices |
| `road` | Road and direction sign: white on highway green, capitals, a compass letter and miles per row (`LF_SignStyles_Road.conf`, #4478) | junctions, the town entry |

### Sizes (carriers)

Every face is drawn on a one-mesh quad with UV 0..1, so the layout's whole area is the shown area and no text is
rotated. Size rows today: `quad16x9` (`LF_BoardFace.et`, 1920 x 1080 units) and the carrier kit (#3307), Blender quads
at 1:1, 4:3, 3:2, 2:1, 4:1 and 1:2 (one mesh, `LF_SignFace.emat` baked, the merge-face rule from #3039), each with a
size row named `quad<W>x<H>`. A carrier is 1 m tall (the 4:1 is 4 m wide, the 1:2 half a metre) and faces +X like
`LF_BoardFace`; the prefab's scale sets the real size. `node tools/assets/sign-carriers/build.mjs` rebuilds the six
FBX files from `LF_BoardFace.fbx` and writes their metas and prefabs (`tools\cli rebuild-fbx` then bakes the `.xob`).
A new aspect is one entry in `build.py` plus a size row; `LF_Test_SignCarriers` holds every `quad*` row to its id's
shape. Plates whose vanilla mesh is the face (`SignWall_01`, `SignCommercial_01`) keep their re-materialed body
as the carrier and get a size row with the measured shown area (from the 2026-11-08 quadrant cards in the feature doc).

## Rendering

`LF_SignStyledFace` (client only, `Feature/Signs/LF_SignStyledFace.c`):

1. Resolve style (sign row, else prefab) and size; an unknown id logs `LF_Debug.Error("Signs", ...)` and the sign falls
   back to the legacy face. A dedicated server, or a client with no workspace, draws nothing.
2. Spawn the size's carrier as a local child at the prefab's offset, angles and scale.
3. Build the style layout, then the plan: `LF_SignFacePlan.Compose(shown area, style, title, rows)` is pure script
   (LOGIC-tested): margins, the title box and font, one box per row with label, leader and value, the ghost lines.
4. Paint: ground colour or texture, ghost, title, rows from the style's row layout (fonts live in layouts, so a pack
   style brings its own font).
5. `SetRenderTarget(carrier)`, one frame a second (`SetMaxFPS(1)`): signs do not move.

The legacy path (`m_sStyle` empty) is unchanged until each prefab migrates.

## Config and the pack

**A new pack sign goes in its own file.** Put it under `Configs/Signs/LF_Signs_Everon1989/`
(`LF_Signs_<Shop>.conf`, root `LF_SignConfig`, `add` verb, plus a `.meta` with a fresh GUID). The pack overlay
loads the folder after `LF_Signs_Everon1989.conf` with no header line and no append to the shared file, so two
shop-fit PRs never conflict on the signs conf (#3555; `docs/config/override-model.md`). `pharmacy` is the first.

`LF_SignStyles.conf` follows the sign catalog's overlay model (`LF_ConfigOverlay`, verbs add, replace, patch, remove;
`docs/config/override-model.md`), overlay name `signstyles`. A pack style is a row plus its own layout under the pack's
`UI/Layouts/Signs/Styles/`. Content stays in `LF_Signs.conf`; a pack row may `patch` a core sign's `m_sStyle`. Price
rows a pack writes are literal values; live rows (the market book, a shop's price list) come from a row source card
(S2), never from text typed into config that can go stale.

## Migration

- **No prefab moves, no GUID changes.** Each legacy prefab (`LF_Sign_Chalkboard`, `_Pub`, `_Fascia`, `_Plate`,
  `_PlateSmall`, `_PolePlate`, `_Posts`, `_Board`, `_Notice`, `LF_PlotSign`, `LF_PriceBoard`) migrates in place by
  setting `m_sStyle` and `m_sSize` on its `LF_SignComponent`; every placed layer entry keeps working and picks up the
  new face. The old per-prefab paint attributes stay readable until the last prefab moves, then go.
- The mapping old prefab -> style and size is the table below; the validator card (S9) fails a placed sign whose style
  or size does not resolve in core plus pack, a style layout missing `RTTexture0`, `Ground` or `Title`, and any change
  to the GUID of a prefab in this table.

| Prefab | Style | Size | Card |
|---|---|---|---|
| `LF_Sign_Chalkboard`, `LF_PriceBoard` | `chalk` | `quad16x9` | S1 (this PR), S2 |
| `LF_Sign_Pub` (A-frame) | `chalk` | `quad16x9` at the lean now (merged, #3305); kit `4:3` later | S5 |
| `LF_Sign_Fascia` | `fascia` | `fascia` (vanilla body) | S3 |
| `LF_Sign_Plate`, `_PlateSmall`, `_PolePlate` | `enamel` | vanilla bodies | S4 (`LF_Sign_HoursPlate` is the first: style `enamel`, size `plate` 1024 x 512, `LF_SignStyles_Plate.conf`, #4834; the three legacy plates still print the paint face) |
| `LF_Sign_Board`, `LF_Sign_Notice` | `poster` | kit `1:2` / vanilla A4 | S6 |
| `LF_Sign_Posts`, `LF_PlotSign` | `wood` / `road` | `quad16x9`, kit | S10 |

## Proving a sign

| Question | Tool |
|---|---|
| Is the carrier placed, upright, flush, facing the street? | `tools\cli look grid:<cell>` (the face is white there by design) |
| Does the face read? Lettering, fit, style, colour | a rendering client: `session up --profile full`, `shot world` from 2 to 7 m in front of the face |
| Does the plan fit (no overflow, font floors, row count)? | LOGIC tests on `LF_SignFacePlan` |
| Does every style, size and placed sign resolve, core and pack? | WORLD test `signs/styles` and the S9 validator |

PR pictures of a face come from the session shot; the placement picture from `look`. A white face in `look` is not a
defect (`docs/features/ui-layouts-localization.md`, #3075).

## Slices

Epic #3286; cards are its sub-issues (`tools\cli slice list 3286 --ready`). `weekend` marks what players see on
Saint-Philippe this weekend.

| Key | Issue | Card | Depends |
|---|---|---|---|
| S1 | #3296 | Foundation: style, size and rows, the chalk style end to end (this PR) | |
| S2 | #3299 | The trader price board prints today's market prices on its face (weekend; done, merged in-dev) | S1 |
| S3 | #3301 | Shop fascias in painted shop lettering, per-placement colours (weekend) | S1 |
| S4 | #3303 | Enamel plates for civic signs | S3 |
| S5 | #3305 | A-frame pavement boards in chalk with specials (weekend) | S1 |
| S6 | #3306 | Paper posters and notices: printed, worn, taped; the finish image | S1 |
| S7 | #3307 | Carrier kit: one-mesh quads at six aspects | S1 |
| S8 | #3308 | Chalk handwriting font (OFL) | S1 |
| S9 | #3309 | Sign validator and migration guard | S1 |
| S10 | #3310 | Wooden boards, door plaques and road signs; row icons | S7 |

Placing a migrated sign in town is never a card: the prefabs keep their GUIDs, so placed town signs change look in
place. A new board on Saint-Philippe (the price board) goes through a `town-` worktree.
