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
- `LF_AccountReplication.Publish` (#716) — an account change writes its player's body at
  once; the player comes from `LF_AccountOwnerIndex` (cached owner checked with one identity
  read, at most one walk over the players per frame) and the police count is recounted
  once at the end of the frame, not per change. 128 changes at 128 players: 32768 identity
  reads before, at most 256 after. `account/publish-cost`.
- `LF_PlayerIdentity` — the key a connected player's account opens under
  (`LF_Utils.GetAccountUID`). Waits ("") while the platform identity can still
  arrive (a listen host gets it about a second after the join), keys by name on
  `-noBackend`, in Workbench, or after 30 s with no answer on a listen host, and on a
  late identity moves the stand-in account onto the real key
  (`LF_PlayerAccountManager.AdoptStandIn`; character record keys stay).
  `persist/late-identity`.
  `SaveAndReleaseAccount` keeps the account resident (no eviction churn).
- Persistence: `Feature/Account/Persistence/LF_PlayerAccountRecord.c` — a
  dedicated record (no EPF). `LF_PlayerAccountManager.ExportAll` / `ApplyAll`
  feed `LF_PersistenceComponentSerializer`, which writes a `version` header
  first.
- Characters per account (#331): `LF_CharacterConfig` (`Configs/Account/LF_Characters.conf`,
  1 shipped) caps the roster. Each `LF_PlayerCharacter` has a record key (slot 1 = the
  account uid, later slots their id) that `LF_Utils.GetPlayerUID` returns for the
  active character and every character-scoped store keys by; `LF_Utils.GetAccountUID`
  is the account. `LF_CharacterStanding` (wanted, rank, duty totals) is per character;
  the account's getters read the active one. Keys and wanted levels are the
  `characters` payload (save version 24); an older save's one character is slot 1.
- `LF_Playtime` + `Persistence/LF_PlaytimeRecord.c` (#692) — seconds played per
  account uid and job (`CIVILIAN`, on-duty `POLICE`/`MEDIC`, a signed-up delivery
  job), credited on the server by a 15 s clock, on disconnect and before each save.
  A gap of more than 300 s between two credits is clamped to 300 s (a wall-clock
  jump or a suspended host is not playtime). The disconnect credit runs from the
  game mode's `GetOnPlayerDisconnected` invoker, after `ReleaseDisconnectedPlayer`;
  the account stays cached there, and if it ever is not, the last stretch goes to
  the job of the previous credit.
  Reads duty and sign-up state through accessors only. The `playtime` payload at
  save version 26. Hidden: read by staff tools and config gates through `Total` /
  `JobSeconds`.
- `LF_Progression` + `Persistence/LF_ProgressionRecord.c` — per-uid progression
  (career, police rank and duty, benefit cooldown, level, licences, quest
  turn-ins), the `progression` payload at save version 7. Hidden: no UI.

## Dependencies

- Everything: `CharacterCreation`, `Spawning`, `Police`, `Crime`, `Jobs`,
  `Money`.