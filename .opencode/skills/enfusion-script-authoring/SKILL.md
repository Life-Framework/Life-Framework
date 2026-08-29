---
name: enfusion-script-authoring
description: "Use when writing, editing, or reviewing Enforce Script (.c) files in this mod. Encodes the EL_ conventions plus the hard EnforceScript lessons that only surface after they break: no ternaries, ref/strong-ref rules, EntityID vs RplId, RplProp/BumpMe, RPC direction, persistence serializer config-binding. Trigger on 'write a script', 'new component', 'new manager', 'edit this EL_ class', 'RPC', 'replicated', 'save/load', 'persistence'. Use ONLY when the file lives under addons/LifeFramework/Scripts."
---

# Enforce Script Authoring

The conventions this mod follows, plus the EnforceScript constraints that do not forgive. The Overthrow mod's bugs paid for these lessons; they transfer to this codebase because the engine is the same.

**Canonical source.** The factual rules here are mirrored in `AGENTS.md` ("Hard EnforceScript lessons"), which is the portable contract every AI tool reads. When they diverge, AGENTS.md wins. This skill adds the workflow: how to apply the rules when authoring.

## Language constraints that will not forgive

- **No ternary operator.** `condition ? a : b` is a compile error. Write the full `if/else`.
- **No null-coalescing operator.** Initialize in the declaration or guard with `if (!x)`.
- **Strict typing, limited inference.** Declare the type; do not rely on inference.
- Managed-class fields that must survive a frame **need `ref`** or the garbage collector reclaims them at end of frame.

## Naming and structure

- Prefix every mod class with `EL_`. Base-game overrides keep their `SCR_` name but are declared `modded class SCR_...` in this addon.
- One class per concern. A file named `EL_ATMManager.c` defines `EL_ATMManager` (and its `...Class` editor props where the editor needs it).
- Folder layout: `Scripts/Game/Core/` for shared infrastructure (`EL_Component`, `EL_Utils`, `EL_GameModeRoleplay`), `Scripts/Game/Feature/<area>/` for gameplay features, `Scripts/Game/UI/` for UI logic, `Scripts/Game/Tests/` for tests.
- A class attached to an entity extends `ScriptComponent` (or `SCR_` component). `[EntityEditorProps(category: "EveronLife/...")]` registers it in the editor with a `Class` sibling.

## Memory and strong references

- **Strong refs are the default expectation.** Single Managed field: `ref EL_Foo m_Foo;`. Collection: `ref array<ref EL_Foo> m_aFoo;` and `ref map<string, ref EL_Foo> m_mFoo;` — `ref` on **both** the collection and the elements. A missing element `ref` shows up as nulls one frame later, not at assignment.
- **Store `EntityID`, not `IEntity`, for anything long-lived.** Entities are deleted at any time. `IEntity target = GetGame().GetWorld().FindEntityByID(m_EntityId); if (!target) return;` — check existence every use. `EntityID.INVALID` is the "none" state. The same applies to collections: keep `ref array<EntityID>`, prune deleted ids on a timer, iterate backwards when removing.
- **Never store `IEntity` in a UI context or a manager that outlives the entity.**
- Null collections in cleanup (`OnDelete`, `OnClose`) to release strong refs.
- `Init()` is **not** called automatically by the engine. Initialize collections in the declaration or call `Init()` yourself from the game mode or owning manager. This mod's `EL_GameModeRoleplay` creates managers lazily on `OnPlayerConnected`; that is the pattern.

## Component patterns

- Use `EL_Component<Class T>` for typed lookups: `EL_ATMManager atm = EL_Component<EL_ATMManager>.Find(entity);`. It returns null when missing; check it at the boundary.
- Components own behavior; managers (`EL_*Manager`) own server-wide state and are singletons reached via `GetInstance()`; config classes (`EL_*Config`, `EL_*Settings`) own data.
- **Always null-check `GetInstance()` results.** A manager singleton returns null until the game mode has the component and is loaded.
- Script components replicate with the entity when attached in the prefab. State in the class doc which side it exists on.

## Replication and networking

- **`EntityID` differs between server and client. Never send it across the network.** Use `RplId` — it is the same on both sides. Resolve with `Replication.FindItem(rplId)` → `RplComponent` → `GetEntity()`.
- **`RplProp` replicates simple types only** (int, float, bool, short strings). Arrays, maps, and objects do not replicate through it. Use an RPC with the payload, or `RplSave`/`RplLoad` for join-in-progress state.
- **A changed `RplProp` value does not broadcast itself.** Call `Replication.BumpMe()` after changing it. Forgetting is the classic "state only works on the server" bug.
- **Throttle replication.** BumpMe on a significance threshold (`Math.AbsFloat(diff) > 0.1`) or batch related values into one RPC. Per-frame BumpMe floods the network.
- **RPC direction:** `RplRcver.Server` = client→server (name it `RpcAsk_...`); `RplRcver.Broadcast`/`Owner` = server→client (name it `RpcDo_...`). Use `Reliable` for critical data, `Unreliable` for frequent updates.
- **Host check before a client→server RPC.** When the server is also a client (host), call the handler directly: `if (Replication.IsServer()) { RpcAsk_Action(); } else { Rpc(RpcAsk_Action); }`.
- **A client can only RPC on entities it owns** (its own controller). Inter-player communication goes through the server.
- Server-authoritative for money, inventory, and persistence. Client requests, server validates and decides. A client-forged RPC must not be able to mint money.
- JIP: replicate complex state via `RplSave`/`RplLoad` so a late joiner gets the same state as the first player. Test late-join explicitly — it is the most commonly missed regression class.

## Persistence

This mod's persistence (`Feature/*/Persistence/EL_*Persistence`) reads and writes typed data. The hard-won rules:

- **A serializer/loader that compiles but is not registered is never called.** If this mod uses a config- or rule-bound system, the entry in the config is load-bearing. Check it, and check that a renamed component still matches its binding.
- **Never persist `IEntity`, and never persist a session-local `EntityID` across a save.** Persist a stable key and re-resolve on load, tolerating "not there". A saved id from one session means nothing in the next.
- **Write a version first, and tolerate "no payload" on load.** Binary save contexts are positional: write order must equal read order. `WriteValue("name", …)` does not make the format order-independent.
- **Load must be idempotent.** It runs both from a fresh load and when re-applying save data to a live session. Plain assignments are safe; starting a non-re-entrant sequence twice is not.
- **In-session round trips prove nothing about restarts.** A save that commits in-session can still fail to restore after a real restart. The restart path is the ground truth (see **enfusion-verify**).
- No console guards around persistence. Consoles are handled internally by the engine.

## Data, config, and localization

- Data is config, not code. `EL_JobConfig`, `EL_VehicleSettings` mirror a config source one field per key.
- User-facing strings are localization keys, not literals. Keys live in `Language/everonlife_localization.st`, runtime files per language. Resolve via the string table, never hardcode.

## Style

- Existing files use `//---` separator banners and `//!` doc comments (`\param`, `\return`, `\brief`). Match that.
- No narrating comments. The assertion or log string is the doc. Keep a comment only for a non-obvious why.
- `tools\cli validate` compiles the addon headless. Run it after script changes. The DebugWorld is the runtime check.

## Tests

- Pure logic gets an `EL_Test` subclass in `Scripts/Game/Tests/`, registered in `EL_TestManager.CollectTests()`. See the **el-tdd** skill.