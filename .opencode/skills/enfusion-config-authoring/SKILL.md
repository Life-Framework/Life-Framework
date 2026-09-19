---
name: enfusion-config-authoring
description: "Use when editing Life Framework .conf, mission, faction, layout, input, or localization resources. Use MCP generators and validators instead of hand-guessing Enfusion serialization."
---

# Config and UI Resources

`AGENTS.md` owns the data rules. For the specific artifact:

- Mission/faction/catalog/editor-placeable: use `config_create`, then inspect
  references and keep the existing Life Framework mission structure.
- Layout: use `layout_create` or `layout_recipe`; inspect the base layout before
  inheriting. Preserve inherited component GUIDs, give widget instances unique
  GUIDs/names, and create the sibling `.layout.meta`.
- Input: confirm the action exists, is in the active context, and has keyboard
  plus `gamepad0:` bindings. Avoid the documented menu/VON collisions.
- Localization: add the key to `Language/everonlife_localization.st`, every
  declared language, and its translator comment. Reference `#KEY` in UI/script.

Before citing a resource, use `asset_search` or `wb_read_props`. After text
resource changes, run `tools\cli validate`; use the relevant boot/runtime test
when the resource is loaded by gameplay.
