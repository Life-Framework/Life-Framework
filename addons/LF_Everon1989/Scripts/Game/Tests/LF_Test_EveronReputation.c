// red-proof: #3083 observed 2026-11-18 `--only LF_Test_EveronReputation` on the old +4 award (cap 60) failed `ten parcel runs open the crates gate` and `ten parcel runs take a newcomer to Known (tier 1) (expected 1, got 0)`, then passed on +10 with the town cap at 120. Earlier: observed 2026-11-18 `tools\cli test --no-build --red LF_Test_EveronReputation` printed `RED PROOF OK` (pack leg); with an empty header as the pack source it failed `a newcomer does not pass the crates gate` and `a Known character passes the crates gate` (no gate row, no contract, core tier words). `--only` then passed under the pack header. An earlier green run caught the town's daily cap of 60 holding back the 100 test points, lifted on the loaded copy.
//! #2709 in game: under the Everon 1989 header (the pack leg of `tools\cli test`), the pack's standing overlays
//! replace the core tier words with the pack's own (the pack's own tier keys, three factions of six, the ladder itself
//! untouched), add an award row per civilian job whose action the core subscriber actually raises
//! (`job_completed:<JOB>`), and add the harbour master's early crates: a contract that wraps a real delivery job
//! and names a gate row that exists, closed to a newcomer and open at Known. Every award row in the layered
//! table names an action the core publishes and a faction the table lists. The chop shop's "car to order" is not
//! here: no core job matches it yet.
[LF_TestInfo(LF_TestTier.LOGIC, pack: true)]
class LF_Test_EveronReputation : LF_Test
{
	static const ResourceName OVERLAY = "{FCB4ADCEA8B6732B}Configs/Reputation/LF_Reputation_Everon1989.conf";
	static const ResourceName OVERLAY_CONTRACTS = "{98EDECED0306D0DB}Configs/Reputation/LF_ReputationContracts_Everon1989.conf";
	static const string UID = "test-everon-reputation";
	static const string TIER_KEY_PREFIX = "LF-Everon_Tier_";
	static const string CONTRACT = "harbour_crates";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/reputation";
	}

	//------------------------------------------------------------------------------------------------
	//! The actions the core subscriber awards: its constants and `job_completed:<civilian job>`.
	protected static bool IsPublishedAction(string action)
	{
		array<string> fixedActions = {
			LF_ReputationSubscriber.ACTION_JOB_COMPLETED, LF_ReputationSubscriber.ACTION_ARREST_TOWN,
			LF_ReputationSubscriber.ACTION_ARREST_POLICE, LF_ReputationSubscriber.ACTION_ARREST_UNDERWORLD,
			LF_ReputationSubscriber.ACTION_SUSPECT_IDENTIFIED, LF_ReputationSubscriber.ACTION_WITNESS_STATEMENT,
			LF_ReputationSubscriber.ACTION_WITNESS_UNDERWORLD, LF_ReputationSubscriber.ACTION_VEHICLE_RECOVERED,
			LF_ReputationSubscriber.ACTION_FLAGGED_LOGOUT, LF_ReputationSubscriber.ACTION_FENCE_SALE,
			LF_ReputationSubscriber.ACTION_VEHICLE_CHOPPED, LF_ReputationSubscriber.ACTION_CHAIN_STEP,
			LF_ReputationSubscriber.ACTION_HEIST_LOOT
		};
		if (fixedActions.Find(action) != -1)
			return true;

		string prefix = LF_ReputationSubscriber.ACTION_JOB_COMPLETED + ":";
		if (!action.StartsWith(prefix))
			return false;

		int job = typename.StringToEnum(LF_EJobType, action.Substring(prefix.Length(), action.Length() - prefix.Length())); // lf-bounded: a short action id
		return job > LF_EJobType.UNEMPLOYED && job != LF_EJobType.MEDIC && job != LF_EJobType.POLICE;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MissionHeader header = LF_MissionHeader.GetCurrent();
		ctx.NotNull(header, "the pack leg runs under an LF mission header");
		if (header)
		{
			ctx.True(header.m_aLF_ConfigOverlays && header.m_aLF_ConfigOverlays.Find(OVERLAY) != -1, "the header lists the Everon standing overlay");
			ctx.True(header.m_aLF_ConfigOverlays && header.m_aLF_ConfigOverlays.Find(OVERLAY_CONTRACTS) != -1, "the header lists the Everon contracts overlay");
		}

		// The act: the pack overlays layered over the core files.
		if (RedProof())
		{
			// No pack layer: an empty header as the pack source.
			LF_MissionConfigPackSource none = new LF_MissionConfigPackSource();
			none.SetHeader(new LF_MissionHeader());
			LF_ConfigService.SetPackSource(none);
		}

		LF_ReputationConfig.Reload();
		LF_ReputationContractsConfig.Reload();
		LF_ReputationConfig config = LF_ReputationConfig.Get();
		LF_ReputationContractsConfig contracts = LF_ReputationContractsConfig.Get();
		LF_ReputationContractDef crates;
		if (contracts)
			crates = contracts.Find(CONTRACT);

		LF_ReputationGateDef gate = LF_Reputation.FindGate("contract:" + CONTRACT);
		bool newcomerPasses = true;
		bool knownPasses;
		bool ninePasses = true;
		bool tenPasses;
		int parcelTier = -1;
		if (gate)
		{
			// #3083: finished parcel runs at the pack's award, under the pack's own daily cap (the cooldown
			// is real time between runs, so each run is booked through Add at the award's points).
			LF_ReputationAwardDef parcel = config.FindAward("job_completed:PARCEL");
			int parcelPoints;
			if (parcel)
				parcelPoints = parcel.m_iPoints;

			LF_Reputation.Forget(UID);
			for (int run = 1; run <= 10; run++)
			{
				LF_Reputation.Add(UID, "TOWN", parcelPoints, "job_completed:PARCEL");
				if (run == 9)
					ninePasses = LF_Reputation.Passes(UID, "contract:" + CONTRACT);
			}

			tenPasses = LF_Reputation.Passes(UID, "contract:" + CONTRACT);
			LF_ReputationFactionDef townDef = config.FindFaction("TOWN");
			if (townDef)
				parcelTier = LF_Reputation.Tier(UID, "TOWN");

			// The town's daily cap would hold 100 points back; lift it on this loaded copy (dropped below).
			LF_ReputationFactionDef capped = config.FindFaction("TOWN");
			if (capped)
				capped.m_iDailyCap = 0;

			LF_Reputation.Forget(UID);
			newcomerPasses = LF_Reputation.Passes(UID, "contract:" + CONTRACT);
			LF_Reputation.Add(UID, "TOWN", 100, "test");
			knownPasses = LF_Reputation.Passes(UID, "contract:" + CONTRACT);
			LF_Reputation.Forget(UID);
		}

		// Undo the pack source and drop the edited copy before any assertion can return early.
		if (RedProof())
			LF_ConfigService.Reset();

		LF_ReputationConfig.Reload();
		LF_ReputationContractsConfig.Reload();

		ctx.NotNull(config, "the standing config loads under the pack header");
		ctx.NotNull(contracts, "the contracts config loads under the pack header");
		if (!config || !contracts)
			return;

		// The pack's tier words, three factions of six, the ladder itself core.
		array<string> factionIds = {"TOWN", "POLICE", "UNDERWORLD"};
		foreach (string factionId : factionIds)
		{
			LF_ReputationFactionDef faction = config.FindFaction(factionId);
			ctx.NotNull(faction, factionId + " is still a faction");
			if (!faction)
				continue;

			ctx.Equal(6, faction.m_aTierThresholds.Count(), factionId + " keeps its core ladder of six thresholds");
			ctx.Equal(6, faction.m_aTierLabels.Count(), factionId + " has six tier words");
			foreach (string word : faction.m_aTierLabels)
			{
				ctx.True(word.Contains(TIER_KEY_PREFIX), factionId + " tier word " + word + " is a pack key");
			}
		}

		LF_ReputationFactionDef town = config.FindFaction("TOWN");
		if (town)
			ctx.EqualStr("#LF-Everon_Tier_Town_1", LF_Reputation.TierWordKey(town, 1), "town tier 1 reads as the pack's Known");

		// Every award row names an action the core publishes and a faction the table lists.
		foreach (LF_ReputationAwardDef award : config.m_aAwards)
		{
			ctx.True(IsPublishedAction(award.m_sAction), "award " + award.m_sAction + " names an action the core publishes");
			ctx.NotNull(config.FindFaction(award.m_sFaction), "award " + award.m_sAction + " feeds a faction the table lists");
		}

		array<string> jobs = {"PARCEL", "FUEL_DELIVERY", "TAXI"};
		foreach (string job : jobs)
		{
			LF_ReputationAwardDef jobAward = config.FindAward("job_completed:" + job);
			ctx.NotNull(jobAward, "the pack awards town standing for a finished " + job + " run");
			if (jobAward)
				ctx.True(jobAward.m_iPoints > 0 && jobAward.m_sFaction == "TOWN", job + " run pays town standing");
		}

		// The harbour master's early crates: a real delivery job, a gate that exists, closed to a newcomer.
		ctx.NotNull(crates, "the pack adds the harbour crates contract");
		if (crates)
		{
			ctx.True(crates.Job() != LF_EJobType.UNEMPLOYED, "the crates contract wraps a real delivery job");
			ctx.EqualStr("TOWN", crates.m_sFaction, "the crates earn town standing");
			ctx.True(crates.m_fPayMultiplier >= LF_ReputationContractDef.MIN_MULTIPLIER && crates.m_fPayMultiplier <= LF_ReputationContractDef.MAX_MULTIPLIER, "the crates pay inside the 1.3 to 1.6 band, unclamped");
			ctx.True(crates.m_iAwardPoints > 0 && crates.m_iStakePoints > 0, "the crates carry an award and a stake");
			ctx.True(crates.m_fTimeLimitSeconds > 0, "the crates have a time limit");
		}

		ctx.NotNull(gate, "the crates gate row exists");
		if (gate)
		{
			ctx.EqualStr("TOWN", gate.m_sFaction, "the crates gate reads town standing");
			ctx.Equal(1, gate.m_iMinTier, "the crates open at Known (tier 1)");
		}

		ctx.False(ninePasses, "nine parcel runs do not open the crates gate");
		ctx.True(tenPasses, "ten parcel runs open the crates gate");
		ctx.Equal(1, parcelTier, "ten parcel runs take a newcomer to Known (tier 1)");
		ctx.False(newcomerPasses, "a newcomer does not pass the crates gate");
		ctx.True(knownPasses, "a Known character passes the crates gate");
	}
}
