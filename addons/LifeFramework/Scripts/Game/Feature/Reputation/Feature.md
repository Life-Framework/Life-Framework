# Reputation (standing) - Design Requirements

> Context file for per-faction standing. Design: `docs/architecture/reputation.md`. Player-facing word:
> **standing**. Code name: `LF_Reputation*` (config owner key `reputation`), because
> `LF_CharacterStanding` and `LF_QuestStanding` already exist.

## Intent

How well the town, the police and the underworld know a character. Earned by work, lost by proof, never
bought. Jobs-first in M6: it gates better jobs and pay (later cards). No visible number, no bar.

## Interaction pattern

Rung 4 background: server-only state, no screen in this card. Target: rung 3 for reading it (the clerk's
"My file" line, one line on the ID card sheet), never the underworld tier.

## Current state

- `LF_Reputation` (`LF_Reputation.c`): points per (character record key, faction), tier from the
  faction's thresholds, `Add` (signed, quests and dialogue) and `Award` (config action: faction, points,
  cooldown), a daily gain cap per faction (in-game day, 2 real hours, compared on the next gain), points
  clamped to the first and last threshold.
- `LF_ReputationConfig` (`LF_ReputationConfig.c`, `Configs/Reputation/LF_Reputation.conf`): factions
  (`TOWN`, `POLICE`, `UNDERWORLD`, thresholds `-300 -100 0 100 400 1000` = tiers -2..3, `#` keys, a 60 a
  day cap, the underworld not shown) and three conservative awards. Core labels are neutral keys.
- Persistence (`Persistence/`, #2698): the `reputation` payload, `LF_ReputationRow` rows through a
  `modded class LF_Reputation` seam (`ExportAll`/`ApplyAll`); load replaces the store, skips removed factions and clamps.
- `LF_OnReputationChanged` (`LF_ReputationEvents.c`) fires only when a tier changes.
- `LF_ReputationSync` (`LF_ReputationSync.c`, #2704): on spawn and on each tier change the owner gets their packed
  tiers by an Owner RPC on their controller (`ClientTier` on the client); town and police changes toast in words. A spawn before the account is cached
  retries once (`SendOnSpawn`, `SPAWN_RETRY_MS`).

- Contracts (`LF_ReputationContracts.c`, `Configs/Reputation/LF_ReputationContracts.conf`, #2702): a contract wraps the
  active `LF_CivilianJob` run (`Begin` / `Take`); `CompleteRun` calls `Settle` (multiplier 1.3..1.6, award, `standing-bonus`
  ledger part), `AbandonRun` calls `Forfeit` (the stake, points only). One at a time, a cooldown; session state.
- Standing pay bonus (`LF_ReputationPay.c`, `m_aPays` in `LF_Reputation.conf`, #2703): `CompleteRun` adds the town tier's percent
  (`0 0 0 5 10 15`, max 15) of the run's base pay; `Dropoff` books it with the contract bonus under the ledger reason `standing-bonus`.
- Pack overlays (`LF_ReputationOverlay.c`, #3044): `LF_ReputationConfig.Get()` and `LF_ReputationContractsConfig.Get()` layer each
  mission-header overlay over the core file (factions, awards, gates, pays, contracts, by id: add, replace, patch, remove).
- Contract offer in dialogue (`LF_DialogueHook_Contract.c`, #2913): the dialogue hook "Contract" offers a contract by id and calls
  `LF_ReputationContracts.Begin`; condition and action are both server-side.

## Not built yet (later cards of chapter #1018)

Gates beyond job sign-up and run start (#2701 built those: `LF_ReputationGate.c`),
client greying from `LF_ReputationSync.ClientTier`, screens, the pack's words, and the pack overlay and owner
`config.json` section for the config.

## Dependencies

`Core/Config` (the record base), `Core/Events` (the bus), `Jobs` (`LF_JobGuards.Now`).
