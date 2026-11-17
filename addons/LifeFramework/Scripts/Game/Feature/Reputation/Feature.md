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
- `LF_OnReputationChanged` (`LF_ReputationEvents.c`) fires only when a tier changes.

## Not built yet (later cards of chapter #1018)

Event subscriptions, the `reputation` persistence payload, gates in job sign-up, contracts, the owner
RPC and screens, the pack's words, and the pack overlay and owner `config.json` section for the config.

## Dependencies

`Core/Config` (the record base), `Core/Events` (the bus), `Jobs` (`LF_JobGuards.Now`).
