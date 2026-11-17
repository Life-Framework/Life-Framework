# Reputation (standing)

Design: [`architecture/reputation.md`](../architecture/reputation.md). Player-facing it is called standing.

### `LF_Reputation` — `Feature/Reputation/LF_Reputation.c`
- ✅ **Store, config and award rules** (#2696, merged, in-dev): server-side points per (character record key,
  faction: `TOWN`, `POLICE`, `UNDERWORLD`). The tier is derived on every read from the faction's thresholds
  (`-300 -100 0 100 400 1000` is tiers -2..3); points clamp to the first and last threshold. `Add` takes a
  signed amount, `Award` takes a config action (faction, points, cooldown). A faction's daily gain cap
  counts the in-game day (2 real hours) and compares the stored day key on the next gain, with no tick; a
  loss is never capped. Cooldowns and caps are session state. Unknown faction or action: `LF_Debug.Warn`, no
  change. Test `reputation/rules`.
- ✅ `LF_OnReputationChanged` (`LF_ReputationEvents.c`) publishes only when a tier changes.
- ✅ Config `Configs/Reputation/LF_Reputation.conf` (`LF_ReputationConfig`): three factions with neutral
  `#LF-Reputation_*` labels, a 60 a day cap, the underworld not shown, three conservative awards.
- 🔲 Event subscriptions, persistence payload, job gates, contracts, owner RPC and screens, the pack's words,
  pack overlay and the owner `config.json` section `reputation` (later cards of #1018).
