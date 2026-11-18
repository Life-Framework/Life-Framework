# Signs

> Context file for diegetic signs. Written against `docs/design.md` (in-world
> first) and section c of `docs/roadmap/m1-playtest-town.md`.

## Intent

A sign is a 1989 board, pub sign, notice, or chalkboard that belongs in the
scene. The title is printed on the prop (a paper sheet pinned to the cork
board, or the A4 notice itself), and looking at it shows `Read: <title>` in
the interaction prompt. Read opens a printed notice: a large title and rule, then What it is, How, You need, Rules, Next, each a small label over a body.

## Shape

- `LF_SignComponent` on the prop names one id (`m_sSignId`).
- `LF_SignCatalog` loads `Configs/Signs/LF_Signs.conf` once. A field that
  starts with `#` is a localization key; anything else is shown as written, so
  a server owner can edit a sign in config without touching the string table.
  Empty sections are left off the card.
- `LF_SignReadAction` is local-effect only (`HasLocalEffectOnlyScript`). It
  names itself `Read: <title>` and opens `LF_SignCard` on the reading client.
  No RPC: the content is static config.
- **Card format.** `LF_SignCatalog.BuildParts` makes TITLE, RULE, then HEADING + RICH per filled section, CLOSE. A section string is plain prose, or a list: `|` or a newline between items, or lines starting `- `, give hyphen bullets. `*word*` draws the word in the accent colour (RichTextWidget `<color rgba='r,g,b,a'>`; `<br/>` between items; bold is not available on the bitmap faces). Typed `<` and `>` are dropped. The skin is `LF_UISkin.Notice`: a painted sign (`HasPaintedFace`: `m_vFaceArea` set) frames the card in `m_vPaintColor` and takes the darkest of paint, lettering and rim as the accent; the board and notice keep the steel frame and an oxblood accent. `FitScale` shrinks type (down to 0.7) so a long card fits the 920 high sheet.
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
  box at 96 cut WELCOME to WELCO. Pub signs and chalkboards print a painted sheet (#116; values on the prefabs);
  the mining and whitelist plates keep their painted
  vanilla text.
- An unknown or empty id, or a sign with no title, logs
  `LF_Debug.Error("Signs", ...)` once and the Read action hides.

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
| `LF_Sign_Posts` | `SignLarge_01` on two posts | 1024 x 700 | market, vehicle lot (the town map is the town board since 2026-11-16, `Feature/TownBoard`) |
| `LF_Sign_PolePlate` | `SignGeneric_01` on `Sign_Pole_A` (`m_bFaceOnChild`) | 1024 x 733 (the plate shows the top 733 rows; at 1024 the text sat low and the bottom rim was cut) | safe zones, impound, fuel, farm |
| `LF_Sign_Board` | cork `NoticeBoardCork_01` | turned A4 sheet | notices: wanted board (in the station porch), convoy |
| `LF_Sign_Pub` | `SignPub_01` A-frame | none (painted body) | general store and market pavement boards |
| `LF_Sign_Chalkboard` | `ChalkBoard_01` | none | contraband barn (hideout), back room |

Faces read from the body's local +Z. Wall plates are centred on their origin (back at -0.002 m): place them on the
facade with the face out; the fascia's origin is its bottom edge. A placement sets `m_vRimColor`, `m_vPaintColor`,
`m_vTextColor` and may scale the body (`scale 1.4` on a plate). The face areas were measured with a quadrant test card
on a 1080p client. Panel widgets do not draw inside a render target: the faces use image widgets. Each vanilla body
already has an `RplComponent`; a second one breaks the entity ("cannot be combined"). `SCR_DestructionMultiPhaseComponent`
is disabled so a shot does not take a sign away until restart.

## Places

Every `LF_SignComponent` with an id claims it in `LF_Places` as kind `Sign` (`LF_Places.Claim`: the first prop of an id stands for it, a second, such as the other safe zone sign, is skipped without an error). A reader finds the place a sign stands for by component, never by entity name or coordinates; the town board points its cards at sign ids this way.

## Tests

LOGIC `signs/catalog`, `signs/missing-id`, `signs/localization`,
`signs/face-fit`, `signs/face-client`, `signs/board-face`, `signs/image`. WORLD `signs/prefabs`,
`signs/coverage` (every placed sign resolves; every catalog row is placed).

## Picture

A row may set `m_sImage` (imageset) and `m_sImageName`; the card then shows the
picture under the title. Made with `tools\cli ui-image`; see
`docs/research/ui-illustration-pipeline.md`.

## Placement

`Worlds/DebugWorld/DebugWorld_Layers/Signs.layer`, one per town manifest site
(`agent-box/town.mjs`, `requireSigns` true), within 8 m and facing the street.

## World Editor preview (#1819)

The edit view never draws the face's render target, so a sign there was blank. Under
`#ifdef WORKBENCH`, `LF_SignComponent` writes the printed title on the face as world-space
debug text every editor frame while the sign is in view (`_WB_AfterWorldUpdate`,
`CALL_WHEN_ENTITY_VISIBLE`); a selected sign shows its whole card, an unknown id shows red.
`m_sSignId` is a combo box of catalog ids, a changed attribute reprints, and the entity's
right-click menu has Edit sign text (opens `LF_Signs.conf`) and Reload sign text. BuildFace
does run in edit mode, but the faces still draw blank there, so the editor does not build them.
Guide: `docs/tooling/sign-editing.md`.

## Board provider (#1945)

One board component for every server-answered board: `LF_BoardProvider` (a kind: `GetKind`,
`GetReadAction`, server `Answer(playerId)`, client `Paint(answer)`), registered by a
`modded class LF_BoardProviderSet` in the feature's own file, found through `LF_BoardProviders`.
The read is one RPC pair on `SCR_PlayerController` (`LF_AskBoard(kind)` ->
`RpcAsk_LF_Board`; server checks the kind exists and the reader stands at the provider's read
action; `RpcDo_LF_ShowBoard` carries `LF_BoardAnswer`: a flag plus flat text and int arrays).
Nothing is cached: an answer is built per read and dropped after painting. The wanted board is
the first kind (`LF_WantedBoardProvider`, kind `wanted`, #758). Test `boards/provider`.

The priced sign (#1946, `m_sPriceLotShopId`) is the second kind: `LF_PricedSignProvider`, kind
`priced`, in `LF_SignComponent.c`. `AnswerFor(lot, vehicle)` is its answer (offer name key, price);
the sign fills its title from it. It names no read action (the face is painted on each client, no
RPC), so a client cannot ask the server for it. Test `signs/priced-provider`.

## Styled faces (#3296, 2026-11-18)

The overhaul's model (`docs/architecture/sign-system.md`): a face is style x size x content x finish.
`LF_SignComponent.m_sStyle` and `m_sSize` name rows of `Configs/Signs/LF_SignStyles.conf`
(`LF_SignStyle.c`; pack overlay `signstyles`); a sign row's own `m_sStyle` wins over the prop's. With a
style set, `LF_SignStyledFace` spawns the size's carrier (a one-mesh `$rendertarget` quad, `LF_BoardFace.et`
for `quad16x9`) at the prop's face offset, angles and scale, and paints the style layout from
`LF_SignFacePlan.Compose`: the title (capitals, hand tilt, a faint shifted ghost) and up to 8 `m_aRows`
(label, dot leader, right-aligned value, one font for every row). A style or size that does not resolve logs
`[LFDebug:Signs]` and the legacy face is drawn; an empty style is the legacy face unchanged. The log line is
`styled face printed: <id> style=<s> size=<z> rows=<n>`.

First style: `chalk` on `LF_Sign_Chalkboard` (face `-90 0 0`, offset `0 1.53 0.3`, scale 1.15 covers the slate).
Sample sign `chalk_sample` (Today's prices, four rows) stands in GridWorld cell `sign-overhaul`. Tests LOGIC
`signs/face-plan`, `signs/styles`. Proof of a face is a full-profile session shot (`look` draws render targets white).

## Shop fascias (#3301)

`LF_Sign_Fascia` is migrated to the styled face (`m_sStyle "fascia"`, `m_sSize "fascia"`): bold condensed capitals,
a dark drop shadow (`LF_SignStyleDef.m_vGhostColor` at the ghost alpha), ground and ink from the placement's
`m_vPaintColor` and `m_vTextColor` (`LF_SignStyleDef.m_bPlacementColors`, `LF_SignStyledFace.FaceColors`). The `fascia`
size has no carrier: the vanilla body samples `$rendertarget` itself and the style shows 1024 x 213 of the target.
The layout is `UI/Layouts/Signs/Styles/LF_SignStyle_Fascia.layout`. Test `signs/fascia`.
