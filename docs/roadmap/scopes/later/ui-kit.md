# UI kit

Phase: After release
Extends: the workspace-widget screens already used by faction pick, character creation, and the death screen. Those screens stay as they are.

## Player loop

A menu is a last resort, and only while the player is at the place that opened it. When a later screen does need one, it is assembled from these parts instead of a new desktop layout. Nothing in this slice opens a screen. The player the release is about to ship does not see it.

## In scope

- Shared part layouts: a sized frame (blur, paper, metal), a title, a body line, a `PauseMenuButton` with `SCR_ButtonTextComponent`, a WLib edit box, and a list row.
- One 1989 theme. Type, padding, and colors are declared in `LF_UITheme`: Roboto Condensed Bold, carbon ink, aged paper, blued steel, a dull brass rule. Not neon. The part layouts are stamped with those same values. `LF_UIScreen` applies the theme again when it builds a screen.
- `LF_UIScreen` builds a screen from those parts with `WorkspaceWidget.CreateWidgets`. It does not use MenuManager. While it is open it activates `MenuContext` every frame, and `MenuTextEditContext` while an edit box is focused. Close removes the widgets and that call.
- `LF_UISampleScreen` lists every part. No action, menu preset, or world entity calls it.

## Out of scope

- Migrating the release menus (faction, character creation, death, shop, ATM, quests, police, spawn).
- `chimeraMenus.conf`, `ChimeraMenuPreset`, Roadmap layers, and DebugWorld.
- New textures or sounds. Vanilla widgets are enough.

## Assets

`{352322057FD78AE1}UI/layouts/WidgetLibrary/BaseElements/WLib_Blur.layout`, `{9ECCD201BCF07E95}UI/layouts/Menus/PauseMenu/PauseMenuButton.layout`, `{0022F0B45ADBC5AC}UI/layouts/WidgetLibrary/EditBox/WLib_EditBox.layout`, and `{EABA4FE9D014CCEF}UI/Fonts/RobotoCondensed/RobotoCondensed_Bold.fnt`. All base game. No downloaded art.

## First slice

The parts, the theme, the assembler, and the unwired sample. Release menus stay as they are until a later pass.

## Depends on

The workspace-widget pattern. Not on any row in the after-release table.

## Proof

- `ui-kit/parts` builds the sample part list and checks names and order. It does not spawn a widget.
- Play mode is unchanged: nothing in the world opens the sample.
