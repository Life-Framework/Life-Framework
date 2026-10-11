# UI kit

Phase: After release
Extends: the workspace-widget screens already used by faction pick, character creation, and the death screen. Those screens stay as they are.

## Player loop

A menu is a last resort, and only while the player is at the place that opened it. When a later screen does need one, it is assembled from these parts instead of a new desktop layout. Nothing in this slice opens a screen. The player the release is about to ship does not see it.

## In scope

- Shared part layouts: a sized frame (blur, paper, metal), a title, a body line, a `PauseMenuButton` with `SCR_ButtonTextComponent`, a WLib edit box, a list row, a status row with one action, and a quantity (keep and take) on the existing thin slider.
- One 1989 theme. Type, padding, and colors are declared in `LF_UITheme`: Roboto Condensed Bold, carbon ink, aged paper, blued steel, a dull brass rule. Not neon. The part layouts are stamped with those same values. `LF_UIScreen` applies the theme again when it builds a screen.
- `LF_UIScreen` builds a screen from those parts with `WorkspaceWidget.CreateWidgets`. It does not use MenuManager. While it is open it activates `MenuContext` every frame, `MenuTextEditContext` while an edit box is focused, and `MenuWithEditorContext` so MenuUp and MenuDown can move focus. Close removes the widgets and that call.
- A title or body line can be rewritten after open. A status row's action hides when the text is empty. A quantity clamps take to 1 .. total-1. The caller can arm close-on-death, close-when-uncontrolled, and close-beyond a distance from an anchor.
- `LF_UISampleScreen` lists every part. No action, menu preset, or world entity calls it.

## Out of scope

- Migrating the release menus (faction, character creation, death, shop, ATM, quests, police, spawn).
- Clocks on a breach charge, a machine, a crop, or a cell. Documents in the hand or on a vehicle. Hunger, cash, wanted, toasts, and the radio. Those are not this sheet.
- `chimeraMenus.conf`, `ChimeraMenuPreset`, Roadmap layers, and DebugWorld.
- New textures or sounds. Vanilla widgets and the existing thin slider are enough.

## Assets

`{352322057FD78AE1}UI/layouts/WidgetLibrary/BaseElements/WLib_Blur.layout`, `{9ECCD201BCF07E95}UI/layouts/Menus/PauseMenu/PauseMenuButton.layout`, `{0022F0B45ADBC5AC}UI/layouts/WidgetLibrary/EditBox/WLib_EditBox.layout`, `{B7DC3B0D9D536470}UI/Layouts/WidgetLibrary/ThinSliderWidget.layout`, and `{EABA4FE9D014CCEF}UI/Fonts/RobotoCondensed/RobotoCondensed_Bold.fnt`. The slider is already in this addon. The rest are base game. No downloaded art.

## First slice

The parts, the theme, the assembler, mutable lines, the status row, the quantity split, close rules, pad focus, and the unwired sample. Release menus stay as they are until a later pass.

## Depends on

The workspace-widget pattern. Not on any row in the after-release table.

## Proof

- `ui-kit/parts` builds the sample part list and checks names, order, and that a line's text can change.
- `ui-kit/quantity` clamps a split. `ui-kit/close` decides death, lost character, a missing anchor, and range. `ui-kit/focus` steps and wraps. None of them spawn a widget.
- Play mode is unchanged: nothing in the world opens the sample.
