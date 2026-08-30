# Account — Design Requirements

> Context file for the per-player persistent identity. Written against
> `docs/design-philosophy.md`.

## Intent

The account is the player's **persistent slice of the world**: faction,
character roster, on-duty flag, wanted level. It exists in the background so
every other feature has a stable identity to key on. It is not an interaction
surface — the player should never "open their account."

## Interaction pattern

Background (rung 4, accepted). Other features read/write it:
`EL_SpawnLogic` (respawn for the active character), character creation
(faction), `Police`/`Crime` (wanted, duty).

## V1 (shippable)

Keep the account stable and correct — it is the keystone every feature keys
off:

1. Fix the cache-eviction churn in `EL_PlayerAccountManager` — every crime,
   duty toggle, arrest, and fine evicts the account, so the next read returns
   null until an async reload, silently degrading consecutive events per
   player.
2. Guard `RemoveCharacter` (stale `m_iActiveCharacterIdx` is an out-of-bounds
   read) and `SetActiveCharacter` with a non-member.

## Iteration path

- **V2** — richer identity (appearance, playtime, criminal record) feeding
  `Police` and `Job`s.

## Current state

- `EL_PlayerAccount` / `EL_PlayerCharacter` (`Feature/Account/`) — faction,
  duty flag, wanted (clamped [0,5]), character roster. ⚠️ stale-index and
  unguarded-set bugs; fields effectively public (BIS ticket T174113
  workaround).
- `EL_PlayerAccountManager` — async load + cache + save/release. ⚠️ eviction
  churn.
- Persistence: `Feature/Account/Persistence/` — ❌ `ApplyTo` returns the wrong
  enum (`EPF_EReadResult.OK`), no version field.

## Dependencies

- Everything: `CharacterCreation`, `Spawning`, `Police`, `Crime`, `Jobs`,
  `Money`.