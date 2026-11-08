# Signs

> Context file for diegetic signs. Written against `docs/design.md` (in-world
> first) and section c of `docs/roadmap/m1-playtest-town.md`.

## Intent

A sign is a 1989 board, pub sign, notice, or chalkboard that belongs in the
scene. The title is printed on the prop (a paper sheet pinned to the cork
board, or the A4 notice itself), and looking at it shows `Read: <title>` in
the interaction prompt. Read opens a short paper card: What, How, Needs, Rules, Next.

## Shape

- `LF_SignComponent` on the prop names one id (`m_sSignId`).
- `LF_SignCatalog` loads `Configs/Signs/LF_Signs.conf` once. A field that
  starts with `#` is a localization key; anything else is shown as written, so
  a server owner can edit a sign in config without touching the string table.
  Empty sections are left off the card.
- `LF_SignReadAction` is local-effect only (`HasLocalEffectOnlyScript`). It
  names itself `Read: <title>` and opens `LF_SignCard` on the reading client.
  No RPC: the content is static config.
- `LF_SignCard` builds the card on `LF_UIScreen` (UI kit). Close, MenuBack,
  death, losing the character, or walking 4 m from the sign closes it.
- **Printed title.** On a rendering client `LF_SignComponent` creates
  `UI/Layouts/Signs/LF_SignFace.layout` (paper, ink title in capitals) and
  points its `RTTextureWidget` at a mesh whose material is
  `Assets/Signs/Data/LF_SignFace.emat` (`BCRMap "$rendertarget"`), the
  vanilla ballistic-table pattern. The sheet reads from the board's local
  +Z side; placed signs turn that side to the street. The board spawns a local `LF_SignFace`
  child (the vanilla A4 poster mesh) at `m_vFaceOffset`, where vanilla filled
  boards pin their posters. The notice retargets its own poster material
  (`m_bFaceOnOwner`). A dedicated server, or a client with no workspace,
  skips it. The render target is the layout size times the screen DPI
  scale while its widgets lay out one unit per pixel, so in Workbench
  `FaceTitleRect` sizes the title by `DPIScale(1)`; anchored to the full
  sheet it was cut off in a small window. That branch is Workbench only
  (`#ifdef WORKBENCH`). A game client uses `ClientTitleRect`: at 640x360 and
  at 1920x1080 alike only about 328 of the 512 units across reach the paper
  (full height shows), so the title box is x 20..310, and the font is 96 or
  smaller so the longest word fits (0.62 em per capital; a word does not
  wrap). The DPI-scaled box printed in the sheet's top-left third; the full
  box at 96 cut WELCOME to WELCO. Pub signs and chalkboards keep `m_bShowFace 0` until a rendering
  pass places their sheet; the mining and whitelist plates keep their painted
  vanilla text.
- An unknown or empty id, or a sign with no title, logs
  `EL_Debug.Error("Signs", ...)` once and the Read action hides.

## Prefabs

`Prefabs/Props/Signs/`: `LF_Sign_Board` (cork notice board), `LF_Sign_Pub`
(hanging shop sign), `LF_Sign_Notice` (A4 notice), `LF_Sign_Chalkboard`
(standing chalkboard; the vanilla `CardboardSign_01` is Rally-addon content). Each inherits the vanilla prop and adds the component, an
`LF_Sign` action context on the face, and the Read action. A placed instance
sets `m_sSignId`. `EL_MiningSign` (`mining`) and `EL_WhitelistSign`
(`police_whitelist`) carry the same pair.

## Sign types (2026-11-08)

Official signs belong to a building or stand on posts; nothing official stands on the grass.
Each type is a vanilla sign body whose face material is `LF_SignFace.emat` (`$rendertarget`), painted with
`UI/Layouts/Signs/LF_SignPaintFace.layout`:

| Prefab | Vanilla body | Face area (units of the 1024 x 1024 target the UVs show) | Used for |
|---|---|---|---|
| `LF_Sign_Plate` | `SignWall_01_115x67` | 1024 x 596 | police (blue), post office (yellow, beside the post box), ATM, garage, jail, hospital (beside a red cross), organization office |
| `LF_Sign_PlateSmall` | `SignWall_01_90x45` | 1024 x 512 | spare; reads only up close |
| `LF_Sign_Fascia` | `SignCommercial_01` (4.3 m) | 1024 x 213 | bank, general store |
| `LF_Sign_Posts` | `SignLarge_01` on two posts | 1024 x 700 | town map, market, vehicle lot |
| `LF_Sign_PolePlate` | `SignGeneric_01` on `Sign_Pole_A` (`m_bFaceOnChild`) | 1024 x 1024 | safe zones, impound, fuel, farm |
| `LF_Sign_Board` | cork `NoticeBoardCork_01` | turned A4 sheet | notices: wanted board (in the station porch), convoy |
| `LF_Sign_Pub` | `SignPub_01` A-frame | none (painted body) | general store and market pavement boards |
| `LF_Sign_Chalkboard` | `ChalkBoard_01` | none | contraband barn (hideout), back room |

Faces read from the body's local +Z. Wall plates are centred on their origin (back at -0.002 m): place them on the
facade with the face out; the fascia's origin is its bottom edge. A placement sets `m_vRimColor`, `m_vPaintColor`,
`m_vTextColor` and may scale the body (`scale 1.4` on a plate). The face areas were measured with a quadrant test card
on a 1080p client. Panel widgets do not draw inside a render target: the faces use image widgets. Each vanilla body
already has an `RplComponent`; a second one breaks the entity ("cannot be combined"). `SCR_DestructionMultiPhaseComponent`
is disabled so a shot does not take a sign away until restart.

## Tests

LOGIC `signs/catalog`, `signs/missing-id`, `signs/localization`,
`signs/face-fit`, `signs/face-client`, `signs/board-face`. WORLD `signs/prefabs`,
`signs/coverage` (every placed sign resolves; every catalog row is placed).

## Placement

`Worlds/DebugWorld/DebugWorld_Layers/Signs.layer`, one per town manifest site
(`agent-box/town.mjs`, `requireSigns` true), within 8 m and facing the street.
