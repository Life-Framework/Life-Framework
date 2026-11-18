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
- ✅ A character wipe resets standing (#2771, merged, in-dev): the admin wipe chain `LF_Progression.Forget`
  calls `LF_Reputation.Forget`. Test `reputation/wipe`.
- ✅ `LF_OnReputationChanged` (`LF_ReputationEvents.c`) publishes only when a tier changes.
- ✅ Config `Configs/Reputation/LF_Reputation.conf` (`LF_ReputationConfig`): three factions with neutral
  `#LF-Reputation_*` labels, a 60 a day cap, the underworld not shown, three conservative awards.
- ✅ **Standing survives a restart** (#2698, merged, in-dev): the `reputation` payload (save version 53,
  `LF_ReputationRow`: row version, character record key, faction, points). 0 rows are not written; on load a
  removed faction is skipped with a warning and points clamp to the current thresholds. Cooldowns and daily caps
  stay session state. Proof: `LF_Test_ReputationPersist` and `tools/cli wt e2e`.
- ✅ **The owner learns their own tiers** (#2704, merged, in-dev): `LF_ReputationSync.c` packs the three tiers into one int and
  sends it on spawn and on each tier change to that player's own controller only (`RpcDo_LF_StandingTiers`, `RplRcver.Owner`,
  never an RplProp; points never leave the server). The client keeps them in `LF_ReputationSync.ClientTier` for greyed offers;
  no screen draws the underworld. A town or police tier change sends one toast in words through `NotifyPlayer`
  (`#LF-Reputation_Toast_<Faction>_Up|Down`), the underworld none. Test `reputation/sync`.
- 🔲 Event subscriptions, job gates, contracts, screens, the pack's words,
  pack overlay and the owner `config.json` section `reputation` (later cards of #1018).
