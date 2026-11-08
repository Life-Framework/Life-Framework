# CharacterCreation — Design Requirements

> Context file for the first-join / intake flow. Written against
> `docs/design.md`.

## Intent

First join should feel like **registering your presence in the world** — pick a
name, an age, a faction, then you spawn into it. It is a gate, not a lifestyle.
Character creation is one of the few flows with no physical analog at scale, so
a menu is acceptable — but it should be framed as intake and stay minimal.

## Interaction pattern

Rung 4 today (menus): no account → create → faction selection → registration
form R-89 → spawn carrying an ID card. Typing a name is a correct UI, so the
form is a 1989 paper sheet on `LF_UIScreen`, not a desktop dialog. The ID card
is a real item (`IDCard.et` + `LF_IdCardComponent`); `EL_ShowIDAction` holds
it up to the player you face, who sees the card.

Target: rung 3 — a world-anchored intake (a registration desk at a faction
office). Low priority; the menu flow is fine for V1 as long as it is honest.

## V1 (shippable)

1. ✅ Done: `CreateAndSpawnDefaultCharacter` uses
   `{9B5BB216CC7FF18E}Prefabs/Characters/Core/Character_Roleplay.et`; the
   placeholder prefab and hard-coded coordinates are gone.
2. ✅ Done (M1-08): validation failures are written on the form (client) and
   the server sends the form back with its refusal.
3. Account flow stays re-entrant-safe (the `SaveAndReleaseAccount` converge
   path is already correct — keep it).

## Iteration path

- **V2** — intake desk at the faction office (rung 3).
- **V3** — appearance preview in-world before confirming.

## Current state

- `EL_CharacterCreationManager` — flow driver; ✅ real roleplay character
  prefab.
- `EL_FactionSelectionMenu` — faction menu (removed by M1-11; police clock in
  at the station instead). `EL_CharacterCreationRules.RefuseCharacter` still
  requires a chosen faction; M1-11 drops that gate with the menu.
- `LF_RegistrationForm` / `LF_RegistrationRules` — form R-89 and its checks.
- `LF_IdCardComponent` / `LF_IdCard` — the card's printed identity and the
  grant (`EL_SpawnLogic.m_bGrantIdAtSpawn` until M1-18 hands it over).
- `EL_ShowIDAction` / `LF_IdCardView` — hold the card up; the viewer sees it.

## Dependencies

- `Account` (the record it writes), `Character`/`Spawning` (the entity it
  spawns), `Banking`/`ATM` + `Survival` (init on created character).