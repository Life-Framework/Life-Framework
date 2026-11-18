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
- ✅ **Job sign-up and run start check standing** (#2701, merged, in-dev): `LF_Reputation.Passes(uid, gateId)`
  reads a gate row (`LF_ReputationGateDef`: gate id, faction, `m_iMinTier`, in `Configs/Reputation/LF_Reputation.conf`
  `m_aGates`; code in `LF_ReputationGate.c`); a gate id with no row passes. `LF_JobSignUpAction` reads `job:<JOB>`
  and `LF_JobStartRunAction` reads `run:<JOB>` (`LF_JobStandingGate`). The core ships fuel and taxi at the town, tier 0.
  A refusal is the `#LF-Reputation_Refuse_<Faction>` key with the minimum tier's word as the argument, never a number
  (the underworld gives no tier word). The server refuses and notifies in `PerformAction`; the greyed offer reads
  standing only on the server host (a dedicated client waits for the owner RPC). The sign-up offer's level refusal
  now uses `LF_JobLevelWords` (#2700). Test `reputation/gate`.
- ✅ **The owner learns their own tiers** (#2704, merged, in-dev): `LF_ReputationSync.c` packs the three tiers into one int and
  sends it on spawn and on each tier change to that player's own controller only (`RpcDo_LF_StandingTiers`, `RplRcver.Owner`,
  never an RplProp; points never leave the server). The client keeps them in `LF_ReputationSync.ClientTier` for greyed offers;
  no screen draws the underworld. A town or police tier change sends one toast in words through `NotifyPlayer`
  (`#LF-Reputation_Toast_<Faction>_Up|Down`), the underworld none. Test `reputation/sync`.
- 🔲 Contracts, client greying from `ClientTier`, screens, the pack's words,
  pack overlay and the owner `config.json` section `reputation` (later cards of #1018).
