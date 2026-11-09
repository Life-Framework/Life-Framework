---
name: enfusion-blast-radius
description: "Use when you have a small-looking change to this mod and want to know what else it could break. The specific breakage classes in Enfusion: prefab inheritance, GUID references, RPC signatures, layout widget GUIDs, input bindings, save/load compatibility. Trigger on 'what could this break', 'is it safe to', 'check the blast radius', 'before I change X'. Use ONLY when the change is to this mod's addon."
---

# Blast radius

A small-looking change is not small until its references are mapped. Enfusion couples through things that do not show in a diff. Check the change against every breakage class that applies.

## The breakage classes

- **Prefab inheritance.** Editing a base prefab (`Food_Base`, `Drink_Base`, `Character_Base`, `Vehicle_Base`, `EL_Door_Base`) changes every child that inherits it. `prefab inspect` a base before editing it to see who inherits. A child that overrides a component value you are about to change is silently coupled. Override an inherited component by its GUID; adding a second component of the same class (for example a second `ActionsManagerComponent`) shadows the inherited one and drops what it carried.
- **GUID references.** A rename or move breaks every `{GUID}path` reference, including saved worlds and configs. Use the Workbench MCP refactor tools (`refactor-rename`, `refactor-move-resource`, `refactor-replace-guid`) or `find-references` / `list-dependencies`, never a hand edit across a tree.
- **RPC and method signatures.** Renaming or reordering a `RpcAsk_`/`RpcDo_` method breaks every call site, and a changed RPC receiver direction breaks the feature silently. Grep the whole mod for the name and for `.Rpc(`/`Rpc(` call sites.
- **Layout widget GUIDs.** A widget-instance GUID must be unique within the file. An inherited-component GUID must equal the GUID in the base layout; a fresh one adds a second, unconfigured component and the widget silently does nothing. Every new `.layout` needs a sibling `.layout.meta` with the PC/XBOX/PS4/HEADLESS configs. `m_sActionName` must match an `Action` and be listed in the screen's `ActionContext`, or the key never fires. Copy-paste of a layout block is the most common way to break one.
- **Input bindings.** A menu's input context must be activated every frame while the menu is open, and must list `MenuBack` plus `MenuUp/Down/Left/Right` and `MenuSelect` or a pad cannot drive it. Never bind a verb to `W/A/S/D` (menu navigation), `a` (`MenuSelect` double-fire), the d-pad, `T`, or `shoulder_left` (VON owns it at priority 110). Bind a `gamepad0:` source alongside every keyboard source. Grep both the mod and the base input confs for the raw input string before choosing it.
- **Save/load compatibility.** Changing a persisted field, its order, or its identity breaks existing saves. A persisted key that stops being written, or a saved record whose config no longer self-spawns, is how player data disappears. `LF_PersistenceComponentSerializer` writes one version integer and walks `RegisterPersistencePayloads` in order; a new payload is appended and the version bumped, and `validate-persistence-version` fails when the version, the list length, and the read order disagree. When you touch persistence, say what happens to an existing save, and verify the restart path per **enfusion-verify**.
- **Config deltas.** A same-GUID `.conf` override merges over the inherited file; it does not replace it. An empty block can keep an inherited value you expected to clear.
- **Replication state.** Adding a replicated variable or RPC changes network traffic. Every player pays for it. Keep it minimal.
- **Localization.** Removing or renaming a key renders the raw key wherever a layout or script still references it. Grep `#Key` usages first.

## The rule

Map the references before editing, prove the coupling you rely on is safe by running the relevant surface, and run `tools\cli validate` after. A "safe because" claim needs a mechanism (code that prevents the collision, a reference that resolves), not a wish.
