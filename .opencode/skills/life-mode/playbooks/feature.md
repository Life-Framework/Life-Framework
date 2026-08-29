# Feature

New or changed gameplay behavior, built from a named data shape. The Enfusion way is data first: the prefab, component, and config define the shape; the script implements the behavior.

1. Name the data shape first. Answer these before any code: what entity owns this behavior (a prefab, a component on an existing prefab, a manager singleton)? What config or `EL_*Settings` table drives it? What existing base does it extend (a base-game class, an `EL_` base prefab)? Write the shape down in one paragraph. Per **principle-foundational-thinking**, this paragraph is the spec.
2. Research the base game. Use **enfusion-api-research**: `component_search` for the script component to attach, `api_search` for the methods to override, `game_read` on the vanilla class you are extending. Confirm your base class exists and what it already provides. Reuse the base game's systems before building your own. The base game already handles inventory, damage, vehicles, and most of what a Life mod touches. Do not reinvent a vanilla system.
3. Research the mod conventions. Read a sibling feature under `Scripts/Game/Feature/<area>/` for structure, the matching base prefab under `Prefabs/`, and any localization keys it needs. Follow **enfusion-script-authoring**, **enfusion-prefab-authoring**, and **enfusion-config-authoring**.
4. Subtract before you add. Is there dead weight in the path you are extending? Remove it first per **principle-subtract-before-you-add**. Does a partial feature stub already exist? Finish or delete it, do not build around it.
5. Build the smallest working slice end to end. One feature, one commit unit. Wire the prefab component, the scripted logic, the config values, and the localization key as one verifiable unit.
6. Verify in the running game per **enfusion-verify** and **principle-prove-it-works**. Drive the feature in the DebugWorld. Exercise the actual path: spawn the item, trigger the action, read the log or UI state. "It compiles" is not a pass.
7. Add an `EL_Test` for any pure logic the feature introduces (calculations, formatting, state transitions) via **el-tdd**. Register it in `EL_TestManager.CollectTests()`.
8. Check **enfusion-blast-radius** for what the new prefab or component could break (children inheriting from a base you edited, config references, replication).
9. Keep the diff as small as the feature allows. Delete scaffolding and debug logging before declaring done.

**Reply:** the data shape, the base-game research that justified it, what you built, how you verified it in-game, and what a maintainer inherits.