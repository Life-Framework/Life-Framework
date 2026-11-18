// red-proof: observed 2026-11-18 with `tools\cli test --no-build --red LF_Test_ReputationGate`: RED-OK, all 1 listed test(s) fail with the hook on, first `the underworld refuses in its own words (expected '#LF-Reputation_Refuse_Underworld', got '')`. Green with `--only`: passed=1 failed=0.
// tier: LOGIC
//! Standing gates (#2701, docs/architecture/reputation.md section 4): `LF_Reputation.Passes(key, gateId)`
//! is false below a gate row's m_iMinTier and true at it, a gate id with no row passes, the refusal key
//! names the faction and the tier word key and carries no digit, an unshown faction gives no tier word,
//! and the job counters build their gate ids from the job (`job:TAXI`, `run:TAXI`). The shipped config
//! gates fuel and taxi on the town at tier 0.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_ReputationGate : LF_Test
{
	static const string UID = "test-reputation-gate";

	override string GetName()
	{
		return "reputation/gate";
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasDigit(string text)
	{
		string digits = "0123456789";
		for (int i = 0; i < text.Length(); i++)
		{
			if (digits.Contains(text.Get(i)))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! TOWN and POLICE shown, UNDERWORLD not shown, all with the shipped ladder and tier words, no daily cap.
	protected LF_ReputationConfig MakeGateConfig()
	{
		array<int> thresholds = {-300, -100, 0, 100, 400, 1000};
		array<string> words = {"#LF-Reputation_Tier_Neg2", "#LF-Reputation_Tier_Neg1", "#LF-Reputation_Tier_0", "#LF-Reputation_Tier_1", "#LF-Reputation_Tier_2", "#LF-Reputation_Tier_3"};
		LF_ReputationConfig config = new LF_ReputationConfig();
		config.m_aFactions = {};
		array<string> ids = {"TOWN", "POLICE", "UNDERWORLD"};
		foreach (string id : ids)
		{
			LF_ReputationFactionDef faction = LF_ReputationFactionDef.Make(id, thresholds, 0);
			faction.m_aTierLabels = {};
			faction.m_aTierLabels.Copy(words);
			faction.m_bShown = (id != "UNDERWORLD");
			config.m_aFactions.Insert(faction);
		}

		config.m_aAwards = {};
		config.m_aGates = {};
		// RedProof: leave the gate rows out, so every assertion that needs a refusal fails (`--red`).
		if (!RedProof())
		{
			config.m_aGates.Insert(LF_ReputationGateDef.Make("job:TAXI", "TOWN", 0));
			config.m_aGates.Insert(LF_ReputationGateDef.Make("gate:police", "POLICE", 1));
			config.m_aGates.Insert(LF_ReputationGateDef.Make("gate:hidden", "UNDERWORLD", 1));
			config.m_aGates.Insert(LF_ReputationGateDef.Make("gate:ghost", "NO_SUCH_FACTION", 3));
		}

		return config;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_Reputation.Forget(UID);
		LF_ReputationConfig.UseForTest(MakeGateConfig());

		RunPasses(ctx);
		RunRefusal(ctx);
		RunJobIds(ctx);

		LF_Reputation.Forget(UID);
		LF_ReputationConfig.Reload();
		RunShippedRows(ctx);

		LF_Reputation.Forget(UID);
		LF_ReputationConfig.Reload();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunPasses(LF_TestContext ctx)
	{
		ctx.True(LF_Reputation.Passes(UID, "no:such:gate"), "a gate id with no row passes");
		ctx.True(LF_Reputation.Passes(UID, ""), "an empty gate id passes");
		ctx.True(LF_Reputation.Passes(UID, "gate:ghost"), "a row naming an unknown faction passes");

		// TOWN tier 0: a new character is at 0 and passes, a doubted one (tier -1) does not.
		ctx.True(LF_Reputation.Passes(UID, "job:TAXI"), "a new character is at the gate's tier and passes");
		LF_Reputation.Add(UID, "TOWN", -50, "test");
		ctx.Equal(-1, LF_Reputation.Tier(UID, "TOWN"), "the loss puts the character at tier -1");
		ctx.False(LF_Reputation.Passes(UID, "job:TAXI"), "below the gate's tier is refused");
		LF_Reputation.Add(UID, "TOWN", 50, "test");
		ctx.True(LF_Reputation.Passes(UID, "job:TAXI"), "back at the gate's tier passes");

		// POLICE tier 1 (100 points).
		ctx.False(LF_Reputation.Passes(UID, "gate:police"), "tier 0 is below a tier 1 gate");
		LF_Reputation.Add(UID, "POLICE", 99, "test");
		ctx.False(LF_Reputation.Passes(UID, "gate:police"), "99 points is still tier 0");
		LF_Reputation.Add(UID, "POLICE", 1, "test");
		ctx.True(LF_Reputation.Passes(UID, "gate:police"), "exactly the gate's tier passes");
		LF_Reputation.Add(UID, "POLICE", 400, "test");
		ctx.True(LF_Reputation.Passes(UID, "gate:police"), "above the gate's tier passes");
		LF_Reputation.Forget(UID);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunRefusal(LF_TestContext ctx)
	{
		string word;
		ctx.EqualStr("", LF_Reputation.Refusal(UID, "no:such:gate", word), "no refusal for a gate with no row");
		ctx.EqualStr("", LF_Reputation.Refusal(UID, "job:TAXI", word), "no refusal when the gate passes");

		string key = LF_Reputation.Refusal(UID, "gate:police", word);
		ctx.EqualStr("#LF-Reputation_Refuse_Police", key, "the refusal key names the faction");
		ctx.EqualStr("#LF-Reputation_Tier_1", word, "the tier word is the faction's word for the minimum tier");
		ctx.False(HasDigit(key), "no digit in the refusal key");
		ctx.True(key.StartsWith("#"), "the refusal is a localization key");

		LF_Reputation.Add(UID, "TOWN", -50, "test");
		key = LF_Reputation.Refusal(UID, "job:TAXI", word);
		ctx.EqualStr("#LF-Reputation_Refuse_Town", key, "a doubted character hears the town's words");
		ctx.EqualStr("#LF-Reputation_Tier_0", word, "the town wants tier 0, whose word is the neutral baseline");
		ctx.True(key != LF_Reputation.Refusal(UID, "gate:police", word), "factions read differently");

		// The underworld's tier is never shown: its refusal has no tier word.
		key = LF_Reputation.Refusal(UID, "gate:hidden", word);
		ctx.EqualStr("#LF-Reputation_Refuse_Underworld", key, "the underworld refuses in its own words");
		ctx.EqualStr("", word, "a faction that is not shown gives no tier word");

		ctx.EqualStr("#LF-Reputation_Refuse_Any", LF_Reputation.RefusalKey("SOME_FUTURE_FACTION"), "a faction with no words of its own falls back");
		LF_Reputation.Forget(UID);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunJobIds(LF_TestContext ctx)
	{
		ctx.EqualStr("job:TAXI", LF_JobStandingGate.GateId("job", LF_EJobType.TAXI), "sign-up reads job:<JOB>");
		ctx.EqualStr("run:FUEL_DELIVERY", LF_JobStandingGate.GateId("run", LF_EJobType.FUEL_DELIVERY), "a run start reads run:<JOB>");

		string word;
		ctx.EqualStr("", LF_JobStandingGate.Refusal(UID, "job", LF_EJobType.TAXI, word), "a new character passes the taxi sign-up gate");
		LF_Reputation.Add(UID, "TOWN", -50, "test");
		ctx.EqualStr("#LF-Reputation_Refuse_Town", LF_JobStandingGate.Refusal(UID, "job", LF_EJobType.TAXI, word), "the sign-up gate refuses a doubted character with the town's words");
		ctx.EqualStr("", LF_JobStandingGate.Refusal(UID, "job", LF_EJobType.PARCEL, word), "a job with no gate row is open");
		LF_Reputation.Forget(UID);
	}

	//------------------------------------------------------------------------------------------------
	//! The rows the core ships in LF_Reputation.conf: fuel and taxi sign-up and runs need the town at tier 0.
	protected void RunShippedRows(LF_TestContext ctx)
	{
		array<string> gates = {"job:FUEL_DELIVERY", "job:TAXI", "run:FUEL_DELIVERY", "run:TAXI"};
		foreach (string id : gates)
		{
			LF_ReputationGateDef gate = LF_Reputation.FindGate(id);
			ctx.NotNull(gate, "the shipped config has the gate " + id);
			if (!gate)
				continue;

			ctx.EqualStr("TOWN", gate.m_sFaction, id + " reads the town");
			ctx.Equal(0, gate.m_iMinTier, id + " wants tier 0");
			ctx.True(LF_Reputation.Passes(UID, id), "a new character passes " + id);
		}

		LF_Reputation.Add(UID, "TOWN", -50, "test");
		string word;
		ctx.EqualStr("#LF-Reputation_Refuse_Town", LF_JobStandingGate.Refusal(UID, "run", LF_EJobType.TAXI, word), "a doubted character cannot start a taxi run");
		ctx.EqualStr("#LF-Reputation_Tier_0", word, "the shipped refusal names the tier word for tier 0");
		ctx.EqualStr("", LF_JobStandingGate.Refusal(UID, "job", LF_EJobType.PARCEL, word), "the parcel counter has no standing gate");
	}
}
