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
  standing on the server host, and on a dedicated client from `ClientTier` (#2915, below). The sign-up offer's level refusal
  now uses `LF_JobLevelWords` (#2700). Test `reputation/gate`.
- ✅ **The owner learns their own tiers** (#2704, merged, in-dev): `LF_ReputationSync.c` packs the three tiers into one int and
  sends it on spawn and on each tier change to that player's own controller only (`RpcDo_LF_StandingTiers`, `RplRcver.Owner`,
  never an RplProp; points never leave the server). The client keeps them in `LF_ReputationSync.ClientTier` for greyed offers;
  no screen draws the underworld. A spawn that comes before the account is cached waits once (2 s) instead of
  sending the account key's tiers. A town or police tier change sends one toast in words through `NotifyPlayer`
  (`#LF-Reputation_Toast_<Faction>_Up|Down`), the underworld none. Test `reputation/sync`.
- ✅ **Contracts** (#2702, merged, in-dev): `LF_ReputationContracts.c`, rows in `Configs/Reputation/LF_ReputationContracts.conf`
  (`LF_ReputationContractDef`: id, `m_sJob`, `m_sStandingGate`, faction, `m_fPayMultiplier` clamped to 1.3..1.6, award, `m_iStakePoints`,
  time limit; a per-character cooldown, 1800 s). A contract wraps the character's active `LF_CivilianJob` run of its job, one at a
  time: `Begin(user, ...)` refuses a dead or downed body and is rate limited as a purchase, then starts the run and `Take`s it.
  `CompleteRun` pays base times the multiplier once and awards the points; the bonus part reaches the player under the ledger reason
  `standing-bonus`. `AbandonRun`, or finishing past the time limit, takes the stake (`LF_Reputation.Add(-stake)`, reason logged) and pays
  no contract money (a late finish still pays the plain run). Stakes move no money. A contract whose run vanished (a wipe) is dropped with
  no stake. Session state: a restart refunds the run's deposit and drops the contract. A drop-off by a dead or downed body pays nothing.
  Core ships one neutral parcel row (`test_parcel`); the offer that calls `Begin` and the pack's contracts are later cards. Test `reputation/contract`.
- ✅ **Standing pay bonus** (#2703, merged, in-dev): `LF_ReputationPay.c`, rows `m_aPays` in `Configs/Reputation/LF_Reputation.conf`
  (`LF_ReputationPayDef`: faction, percent per tier lowest first, shipped TOWN `0 0 0 5 10 15` for tiers -2..3, clamped to 15). `LF_CivilianJob.CompleteRun`
  adds the town tier's percent of the run's BASE pay (a contract's multiplier does not compound it); the drop-off hands it over as its own
  ledger line, reason `standing-bonus`, beside the contract bonus part. Police and medic duty pay never calls it; another faction's tier pays
  nothing on a civilian run. Test `reputation/pay-bonus`.
- ✅ **Contract offer in dialogue** (#2913, merged, in-dev): `LF_DialogueHook_Contract.c`. A dialogue reply with `m_sAction "Contract"`,
  `m_sCondition "Contract"` and the contract id as `m_sParam` starts the contract: the reply is offered only when the contract's standing
  gate passes (checked on the server, where `Offers` runs), the pick signs the player up for the contract's job if needed (the JobSignUp
  hook), then `LF_ReputationContracts.Begin` (dead or downed, purchase rate limit, gate, busy, cooldown, deposit). The NPC answers
  `#LF-Contract_Started` or the refusal key; a toast repeats it. Test `dialogue/contract`. The harbour master's reply is pack data (#2709).
- ✅ **A job you cannot take yet is greyed in words on a dedicated client** (#2915, merged, in-dev): `LF_ReputationOfferGrey.c`
  reads the gate row and the client's own tier from `LF_ReputationSync.ClientTier`; a modded `LF_JobSignUpAction` and
  `LF_JobStartRunAction` (`CanBePerformedScript`, client branch only) grey the offer with the faction's refusal and the
  tier word, before the press. No tiers received yet shows the offer (the server still refuses on use); a gate with no
  row is open. The host path is unchanged. Test `reputation/offer-grey`.
- ✅ **A running contract shows in the tracker with its time left** (#2914, merged, in-dev): `LF_ContractTracker.c`. `Take` sends
  the contract's own player one line by an Owner RPC on their controller (`RpcDo_LF_ContractLine`; never anyone else), `End`,
  the stale drop and `Forget` clear it. The client counts down itself and shows who it is for and the time left in words
  (`LF_EContractTimeWord`: plenty over half, soon over a fifth, hurry, then the line goes; never a number), and under it
  what walking away costs in words (`#LF-ContractTracker_Cost`, never points) when the contract has a stake. The title
  is the pack's row `Title_<contractId>`, else `Title_Default`. There was no on-screen quest tracker, so the line is a
  paper tag built from UI kit parts (`LF_ContractTrackerHud`, no layout). Test `reputation/contract-tracker`.
- ✅ **The tracker line comes back after a reconnect, with the time left** (#3144, merged, in-dev): `LF_ContractTrackerSubscriber`
  listens for `LF_OnPlayerSpawnedEvent` (a reconnect, a late join, a respawn) and `LF_ContractTracker.Resend` sends a running
  contract's line again over the same Owner RPC, with a fourth field, the seconds left (`limit - (now - takenAt)`, from
  `LF_ReputationContracts.ActiveRun`), so the client's countdown and time word carry on instead of starting full. No new
  RPC and no JIP state; a spawn with no contract, or one already out of time, sends nothing. A contract still ends on a
  server restart. Test `reputation/contract-tracker-resend`.
- ✅ **The rejoin proof reads the receiving client** (#3198, merged, in-dev): `LF_ContractTracker.Receive` logs `[LFDebug:Reputation] contract line received: <id> limit=N left=M` (or `cleared`) on the client, and session proof `tracker-rejoin-proof` (`agent-box/proofs/tracker-rejoin-proof.mjs`, `up --mission island`) takes `harbour_crates`, waits 30 s, runs `session rejoin 1` on the dedicated server and checks the new client process logged the line again with less than the full limit left and no line for another contract. Passed 2026-11-19: 1500 s limit, 1424 s left after the rejoin, server re-sent `harbour_crates|1500|1|1424`. Needs no fix in the tracker.
- 🟡 **The standing tables read pack overlays** (#3044): `LF_ReputationConfig.Get()` and `LF_ReputationContractsConfig.Get()`
  layer each overlay the mission header lists (`LF_ReputationOverlay.c`, load order) over the core file: factions (key `m_sId`),
  awards (`m_sAction`), gates (`m_sGate`), pays (`m_sFaction`) and contracts (`m_sId`) take add, replace, patch and remove
  (`docs/config/override-model.md`). A patch sets only the fields it gives; an array (tier words, pay percents) is replaced whole.
  The contracts file's cooldown stays core. Test `reputation/pack-overlay`.
- ✅ **Standing in words** (#2705, merged, in-dev): `LF_ReputationWords.c`. `Pairs` lists each faction's label key and tier word key
  (the config's `m_sLabel` and `m_aTierLabels`) for the tiers given, skipping a faction whose `m_bShown` is false (the underworld) or
  that has no word for the tier. The ID card sheet (`LF_IdCardView`, your own card only, never one shown to you) prints one line from the
  tiers this client holds (`ClientLine`, "Locals: Known. The law: Unknown."; `#LF-Reputation_File_Line` is "%1: %2."). The clerk's
  "What does my file say?" reply (on the town node, action hook `StandingFile`) says the same, one entry per faction, with the server
  tiers. No number, no bar, no underworld. Test `reputation/words`.
- 🟡 **The 1989 pack speaks standing** (#2709, `Configs/Reputation/LF_Reputation_Everon1989.conf`, `LF_ReputationContracts_Everon1989.conf`, both in
  the mission header's overlays): the three factions' tier words are the pack's (`#LF-Everon_Tier_<Faction>_<Tier>` rows in
  `LF_Everon1989/Language/`, from `docs/architecture/reputation.md` section 1; the underworld's are never shown), a `job_completed:<JOB>` award for
  parcel (10, #3083: ten runs, about 40 minutes, take a newcomer to Known), fuel (6) and taxi (6) runs with a 60 s cooldown, the pack's
  town daily cap raised from the core 60 to 120 so those ten runs fit one in-game day (#3083), the gate `contract:harbour_crates` (town, tier 1, Known) and the harbour master's
  early crates contract (a parcel run, x1.4 pay, +12 on finish, -10 if dropped, 25 min limit; tracker title `LF-ContractTracker_Title_harbour_crates`).
  Test `everon/reputation` (pack leg). Not yet: the chop shop's "car to order" contract (no core job matches it).
- 🧪 **The harbour master offers the harbour crates contract, voiced** (#3050, merged, in-dev): his pack dialogue (`tools/assets/dialogue/harbour_master/`, ElevenLabs
  pipeline, ten recorded lines) greets with "a bigger load?" behind the core `Contract` condition (`harbour_crates`, shown at Known) and, below Known, the same words behind the new
  core condition `ContractLocked` (`LF_DialogueHook_ContractLocked.c`: offered only while the gate does NOT pass), which leads to "I keep the big loads for people I know. Come back when
  the town knows you." Accepting runs the `Contract` action; the node after it has no line of its own, so he answers with the recorded line for the key the hook hands back
  (started, busy, cooldown, run already open). Declining ends the talk. Tests `everon/harbour-master` (pack leg: both replies, the pick starts the contract at Known and nothing
  below it) and `dialogue/contract` (the locked condition).
- ✅ **Harbour crates proof** (#3084, merged, in-dev): session proof `harbour-crates` (`agent-box/proofs/harbour-crates-proof.mjs`, `up --mission island`). A Known character (set with the harness verbs `repgrant <FACTION> <points>` and `repshow <FACTION>` in `LF_ReputationHarnessVerbs.c`, dev-gated like `/licence`, no daily cap booked) talks to Marcel Roux, takes `harbour_crates`, reads the tracker, delivers and the server log shows pay x1.4, the `standing-bonus` ledger line and +12 town. It prints PASS or FAIL per step with the PR that should fix a failure. It passed 11 of 11 on a local merge of the harbour master (#3058) and the parcel drop points (#2973); since #4693 it passes 11 of 11 on `--mission island` as is: the greeting is two replies plus the always-present Goodbye row (`replies=2`), the tracker title is the US "Harbor crates", and the drop is the district mailbox `Fit_saint_philippe_districts_parcel_drops_drop_b34` (the only island `LF_ParcelDrop` 300 m or more from the dock cabin; the dock mailboxes are all under 100 m, so a run from Marcel cannot end there), which the tracker's "Delivery stop" marker points at. `--cmd-timeout <ms>` raises each command's wait on a loaded pool PC.
- 🔲 Screens, and the owner `config.json` section `reputation` (later cards of #1018).
