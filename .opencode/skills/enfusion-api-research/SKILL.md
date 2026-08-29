---
name: enfusion-api-research
description: "Use when about to write or read Enfusion / Arma Reforger API code, base-game classes, prefabs, or components and you are not certain of the exact API. How to look up the truth: api_search, component_search, game_read, wiki_read, game_browse, asset_search, wb_knowledge. Never guess an Enfusion API. Trigger on: 'how does X work in the base game', 'which component', 'what class', 'what does SCR_X inherit', 'find the prefab for'. Use ONLY when the mod's own EL_ conventions are not the question."
---

# Enfusion API Research

The base game is the source of truth. Never write Enfusion code against a guessed API. This skill is the lookup protocol before any Enfusion-facing code.

## The lookup order

1. **Class or method exists?** `api_search` with `type: class` (or `method`/`property`). Use `format: tree` to see the inheritance chain when a class search matters. The result shows inherited members automatically. Check `source: arma` vs `enfusion` for where it lives.
2. **Which component to attach?** `component_search`. Filter by category (character, vehicle, weapon, damage, inventory, ai, ui, general) and by the event handler you need (`OnDamage`, `EOnFrame`, `OnPlayerConnected`). This is the tool for "what do I put on the entity".
3. **How does the vanilla class behave?** `game_read` on the actual source, e.g. `Scripts/Game/Character/SCR_CharacterControllerComponent.c`. Read the real implementation over an API summary when behavior matters.
4. **Engine concept (replication, RPC, UI, event system)?** `wiki_search` + `wiki_read` for the full page, or `wb_knowledge` for distilled modding patterns (query 'replication', 'scenario framework', 'audio signals', 'weapon suppressor', and so on).
5. **Which asset / prefab?** `asset_search` for prefabs, models, textures, scripts, configs by name. Returns the `{GUID}path` resource form you can use in references.
6. **Browse structure.** `game_browse` to see what exists under a folder (e.g. `Prefabs/Weapons`, `Scripts/Game/Character`) before searching blind.

## Rules

- Cite what you looked up. A claim about an API without a lookup behind it is a guess. Record the lookup (class, path, page) so the reader can verify.
- When a class is not found, say "not found" and try the base class, the other source (arma vs enfusion), or the wiki. Do not invent a signature.
- Prefer reading the vanilla source for behavior questions. The API index says a method exists; the source says what it does.
- When the mod overrides a vanilla class (`modded class SCR_...`), read both the vanilla source and the mod's override. The interaction is where bugs live.
- Use the Workbench MCP for resolved truth: `wb_read_props` reads a prefab's property through the engine (inheritance already resolved). It beats reading `.et` text for a single value.

## Grounding in this repo

- Base-game classes are prefixed `SCR_` (Conflict) or engine `Enfusion` types. The mod's own classes are `EL_`. When you search, know which side you are asking about.
- The mod extends the Conflict framework (`SCR_BaseGameMode`, `SCR_InventoryStorageComponent`, `SCR_CharacterControllerComponent`). Vanilla Conflict source is the reference for what those provide.
- Assets the mod references live under the base game's `Prefabs/`, `Worlds/`, `Materials/`. `asset_search` returns the GUID-prefixed paths for prefab references.