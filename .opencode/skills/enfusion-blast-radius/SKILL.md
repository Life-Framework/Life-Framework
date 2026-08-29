---
name: enfusion-blast-radius
description: "Use when you have a small-looking change to this mod and want to know what else it could break. The specific breakage classes in Enfusion: prefab inheritance, GUID references, RPC signatures, layout widget GUIDs, input bindings, save/load compatibility. Trigger on 'what could this break', 'is it safe to', 'check the blast radius', 'before I change X'. Use ONLY when the change is to this mod's addon."
---

# Blast radius

A small-looking change is not small until its references are mapped. Enfusion couples through things that do not show in a diff. Check the change against every breakage class that applies.

## The breakage classes

- **Prefab inheritance.** Editing a base prefab (`Food_Base`, `Drink_Base`, `Character_Base`, `Vehicle_Base`) changes every child that inherits it. `prefab inspect` a base before editing it to see who inherits. A child that overrides a component value you are about to change is silently coupled.
- **GUID references.** A rename or move breaks every `{GUID}path` reference, including saved worlds and configs. Use the Workbench MCP refactor tools (`refactor-rename`, `refactor-move-resource`, `refactor-replace-guid`) or `find-references` / `list-dependencies`, never a hand edit across a tree.
- **RPC and method signatures.** Renaming or reordering a `RpcAsk_`/`RpcDo_` method breaks every call site, and a changed RPC receiver direction breaks the feature silently. Grep the whole mod for the name and for `.Rpc(`/`Rpc(` call sites.
- **Layout widget GUIDs.** In a layout, a widget-instance GUID must be unique in the file and an inherited-component GUID must match the base layout. A copy-paste or duplicate produces a dead widget. This is the single most common layout breakage.
- **Input bindings.** A new keybind can collide with menu navigation (`W A S D`), with `MenuSelect` (`a`), with VON (`T`, `shoulder_left`), or with an always-live base context of higher priority. Grep both the mod and base input confs for the raw input string before choosing it. See **enfusion-config-authoring**.
- **Save/load compatibility.** Changing a persisted field, its order, or its identity breaks existing saves. A persisted key that stops being written, or a saved record whose config no longer self-spawns, is how player data disappears. When you touch persistence, say what happens to an existing save, and verify the restart path per **enfusion-verify**.
- **Config deltas.** A same-GUID `.conf` override merges over the inherited file; it does not replace it. An empty block can keep an inherited value you expected to clear.
- **Replication state.** Adding a replicated variable or RPC changes network traffic. Every player pays for it every frame. Keep it minimal.

## The rule

Map the references before editing, prove the coupling you rely on is safe by running the relevant surface, and run `tools\cli validate` after. A "safe because" claim needs a mechanism (code that prevents the collision, a reference that resolves), not a wish.