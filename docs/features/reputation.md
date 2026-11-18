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
- ✅ **Standing listens to the event bus** (#2697, merged, in-dev): `LF_ReputationSubscriber` (`Feature/Reputation/LF_ReputationSubscriber.c`, subscribed from the game mode) awards a config row per event, so no feature calls standing. A paid civilian run (`LF_OnJobCompleted`, not police or medic duty; a `job_completed:<JOB>` row overrides the general one) raises town. An arrest costs town and police and gives the underworld a little. `LF_OnSuspectIdentified` costs town. A player witness's statement raises police; one that describes a suspect also costs the witness 5 underworld (outline R4); a town NPC witness earns nothing. `LF_OnVehicleRecovered` raises the officer's police standing, `LF_OnFlaggedLogout` lowers it. `LF_OnFenceSale`, `LF_OnVehicleChopped`, a chain `collect` and `LF_OnHeistLoot` raise the underworld. Loss needs proof: a wanted level changing or a cleared accusation moves nothing. Test `reputation/events`.
- 🔲 Job gates, contracts, owner RPC and screens, the pack's words,
  pack overlay and the owner `config.json` section `reputation` (later cards of #1018).
