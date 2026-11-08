# Signs

> Context file for diegetic signs. Written against `docs/design.md` (in-world
> first) and section c of `docs/roadmap/m1-playtest-town.md`.

## Intent

A sign is a 1989 board, pub sign, notice, or chalkboard that belongs in the
scene. The title is printed on the prop (a paper sheet pinned to the cork
board, or the A4 notice itself), and looking at it shows `Read: <title>` in
the interaction prompt. Read opens a short paper card: What, How, Needs, Rules.

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
  vanilla ballistic-table pattern. The board spawns a local `LF_SignFace`
  child (the vanilla A4 poster mesh) on its -Z face, where vanilla filled
  boards pin their posters. The notice retargets its own poster material
  (`m_bFaceOnOwner`). A dedicated server, or a client with no workspace,
  skips it. The render target is the layout size times the screen DPI
  scale while its widgets lay out one unit per pixel, so `FaceTitleRect`
  sizes the title by `DPIScale(1)`; anchored to the full sheet it was cut
  off in a small window. Pub signs and chalkboards keep `m_bShowFace 0` until a rendering
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

## Tests

LOGIC `signs/catalog`, `signs/missing-id`, `signs/localization`,
`signs/face-fit`. WORLD
`signs/prefabs`.
