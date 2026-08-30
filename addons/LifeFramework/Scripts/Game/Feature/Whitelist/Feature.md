# Whitelist — Design Requirements

> Context file for access control. Written against
> `docs/design-philosophy.md`.

## Intent

Whitelist is **admin infrastructure**, not a player-facing feature. It answers
one question: "is this player allowed to be in faction/job X?" Restricted
factions (POLICE, MILITARY, MAFIA) and restricted jobs (POLICE) route through
it. The player never interacts with it directly — they either get into the
faction or they don't.

## Interaction pattern

No player-facing rung. It is a gate that other features call.

## V1 (shippable)

1. **`EL_EFactionType` is referenced 16× but defined nowhere in the repo** —
   the file cannot compile as-is unless the type lives in another addon. Define
   it or route the checks through `Account`'s faction representation.
2. Persist whitelists — they reset every server start today.
3. Getter returns the live internal array (callers can mutate without logging) —
   return a copy, log changes through the manager.
4. Make `Jobs`/`License` route through this (the hard-coded POLICE
   special-cases are dead weight once this works).

## Iteration path

- **V2** — server-owner managed whitelist surface (admin commands / config).

## Current state

- `EL_WhitelistManager`, `EL_Whitelist`, `EL_WhitelistAction`,
  `EL_WhitelistComponent`, `EL_WhitelistManagerComponent`,
  `EL_WhitelistRestrictionComponent` (`Feature/Whitelist/`).
- ❌ unresolved `EL_EFactionType` (compile risk), no persistence, live-array
  getter.

## Dependencies

- `Account` (faction identity), `Jobs`, `License` (the gates it backs).