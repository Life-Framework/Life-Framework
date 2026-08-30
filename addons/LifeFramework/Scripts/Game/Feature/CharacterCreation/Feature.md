# CharacterCreation — Design Requirements

> Context file for the first-join / intake flow. Written against
> `docs/design-philosophy.md`.

## Intent

First join should feel like **registering your presence in the world** — pick a
name, an age, a faction, then you spawn into it. It is a gate, not a lifestyle.
Character creation is one of the few flows with no physical analog at scale, so
a menu is acceptable — but it should be framed as intake and stay minimal.

## Interaction pattern

Rung 4 today (menus): no account → create → faction selection → character
creation → spawn. `EL_ShowIDAction` shows "First Last, Age: N" in-world, which
is the physical counterpoint (your ID is a real thing you show people).

Target: rung 3 — a world-anchored intake (a registration desk at a faction
office). Low priority; the menu flow is fine for V1 as long as it is honest.

## V1 (shippable)

1. Replace the placeholder prefab GUID (`{YourTempCharacterPrefab}`) and
   hard-coded coordinates — the temp-character path fails if reached.
2. Validation failures (`EL_CharacterCreationMenu` silently early-returns on
   bad name/age) must show the reason, not no-op.
3. Account flow stays re-entrant-safe (the `SaveAndReleaseAccount` converge
   path is already correct — keep it).

## Iteration path

- **V2** — intake desk at the faction office (rung 3).
- **V3** — appearance preview in-world before confirming.

## Current state

- `EL_CharacterCreationManager` — flow driver; ❌ placeholder prefab + hardcoded
  coords.
- `EL_CharacterCreationMenu` / `EL_FactionSelectionMenu` — menus; ⚠️ silent
  validation failure.
- `EL_ShowIDAction` — in-world ID display; ⚠️ `CanBePerformedScript` returns
  true unconditionally.

## Dependencies

- `Account` (the record it writes), `Character`/`Spawning` (the entity it
  spawns), `Banking`/`ATM` + `Survival` (init on created character).