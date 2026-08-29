---
name: enfusion-config-authoring
description: "Use when creating, editing, or reviewing config (.conf), mission, faction, layout (.layout), keybind/input, or localization (.st / string table) files for this mod. Trigger on 'mission header', 'faction config', 'game mode conf', 'layout file', 'keybind', 'input action', 'localization key', 'string table', 'editor placeables', 'entity catalog'. Use ONLY when the file lives under addons/LifeFramework (Missions/, Language/, UI/, Configs/, or a .conf config)."
---

# Config, Mission, Layout, Keybind, and Localization Authoring

The text resources of the mod. They compile like data and fail at runtime when wrong: a mission that does not load, a layout that cannot find its widget, a keybind that never fires, a string table key the script never resolves. The hard lessons below were paid for by real bugs in a sibling Reforger mod; the engine is the same.

**Canonical source.** The factual rules here are mirrored in `AGENTS.md` ("Enfusion data rules"), the portable contract every AI tool reads. When they diverge, AGENTS.md wins. This skill adds the workflow: how to author each file type correctly.

## Mission and game mode (.conf)

- Mission headers and game mode configs use Enfusion Text serialization. The MCP `config_create` tool generates valid blocks for `mission-header` (Conflict mode `SCR_MissionHeaderCampaign`, or `SF` for Scenario Framework), `faction`, `entity-catalog`, and `editor-placeables`.
- The mission header references the world by full resource ref (`{GUID}worlds/...`). The world must exist and the GUID must match. `tools\cli test` boots the DebugWorld and catches a broken world reference.
- The addon's game mode lives in `Prefabs/MP/Modes/Roleplay/GameMode_Roleplay.et` and `Missions/EveronLifeGameMode.conf`. Extend what is there; do not create a parallel mission system.
- **Same-GUID `.conf` overrides are deltas, not replacements.** A config that overrides an inherited file merges over it. An empty override block can quietly keep an inherited value you expected to clear.

## Factions

- Faction definitions are `faction` configs with a key, color, flag texture, and optional catalog. Follow the vanilla `Faction_*` config shape. `api_search` / `component_search` for `SCR_FactionManager` when wiring faction logic.
- The DebugWorld `Factions.layer` shows the factions the mod actually instantiates. Keep config and layer in agreement.

## UI layouts (.layout)

The layout GUID rules are unforgiving. Three different kinds of GUID behave differently:

| Kind | Example | Rule |
|---|---|---|
| Widget instance | `ButtonWidgetClass "{GUID}"` | **Unique within the file.** Two widgets with the same GUID corrupt lookup. |
| Slot | `Slot LayoutSlot "{GUID}"` | **Repeats freely.** It identifies the slot type binding. Copy whatever the sibling widgets use. |
| Inherited component | `SCR_InputButtonComponent "{GUID}"` | **Must equal the GUID in the base layout you inherited from.** A fresh GUID here adds a second, unconfigured component and the widget silently does nothing. This is the single most common layout bug. |

- **Every new `.layout` needs a sibling `.layout.meta`, or the engine will not resolve it.** The meta declares the resource GUID (the one referenced from prefabs and scripts) and keeps all five platform configs (`PC`, `XBOX_ONE`, `XBOX_SERIES`, `PS4`, `HEADLESS`). Never create a layout without its meta.
- Use `layout_create` for a full tree (root + nested children with inferred Slot types) or `layout_recipe` for proven HUD blueprints.
- Every widget the script looks up by name must carry that name. `FindAnyWidget` fails silently on a mismatch; it is also case-sensitive. Rename a widget and the script in the same wave.
- Inheriting a base layout (`: "{GUID}path"` suffix) pulls everything from it; the body overrides. Read the base layout before overriding it, because that is where the component GUID you must reuse lives.
- Fonts are resource refs; the engine does not fall back gracefully. Use the same font resource the sibling layouts use.
- `m_sActionName` on a button must match an `Action` in the input conf **and** that action must be listed in the screen's `ActionContext`. Without the context entry the glyph still draws and the key never fires.

## Keybinds and input contexts (chimeraInputCommon.conf)

- **An action does nothing until (a) a context lists it and (b) that context is activated.** This mod's menus must activate their input context every frame while open, or only mouse clicks work. `MenuBack` must always be listed, and `MenuUp/Down/Left/Right` + `MenuSelect` must be listed or the menu cannot be driven by a stick or d-pad.
- **The WASD trap.** `W A S D` are menu navigation, not free letters. A "Sell" button on `KC_S` collides with `MenuDown` in any menu that supports pad navigation. A real mod shipped that exact bug.
- **Never bind a menu verb to `a`** (it is `MenuSelect`, so pressing `a` on the focused button fires twice), **never to the d-pad**, and **never to `shoulder_left`** (VON owns it at priority 110 and eats the press). Safe-ish keyboard letters: `B F G H I J K L N O P R U V X Y` minus what the screen already took, and never `T` (VON push-to-talk).
- **Bind both a keyboard and a `gamepad0:` source.** A keyboard-only action is invisible and unusable on console.
- **Hiding or disabling a button also kills its shortcut.** `SCR_InputButtonComponent` refuses the keybind when the widget is not visible or not enabled in the hierarchy. This is the supported way to retire a shortcut contextually, and it is what makes two verbs on one pad button safe when only one is visible. Rely on it and say so in a comment at the call site.
- If this mod ships input conf changes, grep both the mod and base confs for the raw input string before choosing it. Priorities matter: an always-live base context of higher priority swallows the press.

## Localization (.st / string tables)

- The addon's string table is `Language/everonlife_localization.st`, with per-language runtime `.conf` files declared in `LifeFramework.gproj`.
- Keys are stable identifiers. The script and the layout reference the key; only the `.st` holds the string. A key used in script but missing from the table renders as the key itself at runtime.
- This mod already ships en_us, es_es, de_de, fr_fr, it_it, pt_pt, pt_br. Add a string to every declared language or the language falls back or shows the key. Add the missing-language entries in the same wave as the new key.
- **Fill in the `Comment` field for new strings.** It is the only context a translator gets.
- The Workbench Localization Editor (MCP `wb_localization`) is the mechanical way to add keys across all columns.
- `TextWidget.SetText("#KEY")` resolves at draw time; use `WidgetManager.Translate(key)` when you need the text in script (sorting, comparison).

## Encode the lesson

A config or layout that fails at runtime is a silent failure. After any text-resource change, `tools\cli validate` (repo hygiene) and `tools\cli test` (boot smoke) are the cheap gates. Use them.