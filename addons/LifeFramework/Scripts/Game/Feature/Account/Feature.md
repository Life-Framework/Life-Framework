# Account — Design Requirements

> Context file for the per-player persistent identity. Written against
> `docs/design.md`.

## Intent

The account is the player's **persistent slice of the world**: faction,
character roster, on-duty flag, wanted level. It exists in the background so
every other feature has a stable identity to key on. It is not an interaction
surface — the player should never "open their account."

## Interaction pattern

Background (rung 4, accepted). Other features read/write it:
`LF_SpawnLogic` (respawn for the active character), character creation
(faction), `Police`/`Crime` (wanted, duty).

## V1 (shippable)

Keep the account stable and correct — it is the keystone every feature keys
off:

1. Done: the cache-eviction churn in `LF_PlayerAccountManager` is fixed;
   `SaveAndReleaseAccount` keeps the account cached.
2. Done: `RemoveCharacter` re-clamps `m_iActiveCharacterIdx` and
   `SetActiveCharacter` ignores a non-member.

## Iteration path

- **V2** — richer identity (appearance, playtime, criminal record) feeding
  `Police` and `Job`s.

## Current state

- `LF_PlayerAccount` / `LF_PlayerCharacter` (`Feature/Account/`) — faction,
  duty flag, wanted (clamped [0,5]), character roster. `RemoveCharacter`
  re-clamps the active index and `SetActiveCharacter` ignores a non-member.
  Fields effectively public (BIS ticket T174113 workaround).
- `LF_PlayerAccountManager` — load + cache + save/release.
  `SaveAndReleaseAccount` keeps the account resident (no eviction churn).
- Persistence: `Feature/Account/Persistence/LF_PlayerAccountRecord.c` — a
  dedicated record (no EPF). `LF_PlayerAccountManager.ExportAll` / `ApplyAll`
  feed `LF_PersistenceComponentSerializer`, which writes a `version` header
  first.
- `LF_Progression` + `Persistence/LF_ProgressionRecord.c` — per-uid progression
  (career, police rank and duty, benefit cooldown, level, licences, quest
  turn-ins), the `progression` payload at save version 7. Hidden: no UI.

## Dependencies

- Everything: `CharacterCreation`, `Spawning`, `Police`, `Crime`, `Jobs`,
  `Money`.