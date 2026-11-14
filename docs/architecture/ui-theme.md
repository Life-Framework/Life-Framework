# The UI theme and how a pack overrides it

Issue #966, under the pack epic #223. A modern pack must be able to change how every sheet looks
without touching a layout. The 1989 paper look is the flagship's; the core only ships the shape.

## Where the values are

`Configs/UI/LF_UITheme.conf`, class `LF_UIThemeConfig`: a list of `LF_UIThemeRecord`, one per
theme id.

| Id | Feeds | Looks like |
|---|---|---|
| `paper` | `LF_UISkin.Paper()`, the sign card, every sheet without its own look | Warm form paper, carbon ink, brass rule, blued-steel rim |
| `id-card` | `IdCard()`, `IdCheck()` | Pale registry paper, slate rim, state-blue accent |
| `terminal` | `Terminal()` (the ATM) | Green phosphor on a black tube, amber focus, mono face |
| `price-list` | `PriceList()` (shop and vehicle lists) | Paper in a mono face |

A field a theme leaves empty (or 0) takes the `paper` value, so patching `paper` restyles the base
and every theme that inherits from it. The accent and the paper texture are never inherited: a
flat terminal does not pick up the paper's texture.

## Tokens

- **Colours** (`#RRGGBB` or `#RRGGBBAA`, sRGB bytes): `m_sMetal` (rim), `m_sPaper` (the sheet's face,
  opaque), `m_sInk`, `m_sRule`, `m_sBlur` (tint of the dimmed world), `m_sRow` (list strip),
  `m_sButtonText`, `m_sButtonFocus`, `m_sAccent` (section labels and highlighted words).
- **Faces**: `m_Font` (print), `m_FontRegular` (a notice's body).
- **Sizes**: `m_iTitleSize`, `m_iBodySize`, `m_iButtonSize`, `m_iFieldSize`, `m_iCaptionSize`
  (the printed name above a blank), `m_iHeadingSize`.
- **Texture**: `m_PaperTexture` (an `.edds` drawn on the sheet's face, tinted by the paper colour;
  empty is the flat sheet the flagship has).

Not tokens: the edit-box well (`LF_UIEditBox.layout` writes it in linear space for the vanilla
component), the sheet padding, and the semantic alert colours of the toasts.

## Reading it

Everything reads through `LF_UITheme` (`Get(id)`, `Metal()`, `TitleSize()`, ...) and
`LF_UISkin.FromTheme(id)`. A feature never reads the config and never writes a colour, a face or a
size. `validate-ui-kit` fails a colour literal in `Feature/UIKit/*.c` outside the two theme files,
and keeps failing a new layout outside the kit.

If the config does not load, `LF_UITheme.BuiltIn()` holds the same 1989 values in code, so no sheet
breaks. `LF_Test_UIThemePack` proves the core file equals the built-in, field for field.

## How a pack overrides it

A pack ships a `.conf` of class `LF_UIThemeConfig` and lists it in its mission header's
`m_aLF_ConfigOverlays` (the same seam as every other section, `Core/Config/Feature.md`). A record
names a theme by id and a verb (`docs/config/override-model.md`):

```
LF_UIThemeConfig {
 m_aThemes {
  LF_UIThemeRecord "{...}" {
   m_sOp "patch"
   m_sThemeId "paper"
   m_sPaper "#E8F1F7"
   m_sRule "#00B8A9"
   m_sAccent "#00B8A9"
   m_iTitleSize 32
  }
 }
}
```

Only the fields written change. The test fixture is `Configs/Tests/LF_Test_UIThemePack.conf`. There
is no owner `config.json` section: a look is a pack's job, and the client would need the config
stream to see it. Packs load on every client, so none is needed.

A malformed colour logs `[LFDebug:UITheme]` once when the theme loads and that token takes the
built-in paper colour; the sheet still opens.

## Pictures

`node agent-box/harness.mjs flow ui-theme` in one `--profile full` session: the kit sheet and the
ATM terminal with the flagship theme, then with the test pack. Embedded in the PR.
