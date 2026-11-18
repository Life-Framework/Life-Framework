// red-proof: observed 2026-11-18 with `toolscli test --no-build --red LF_Test_ReputationPayBonus`: RED PROOF OK, first `a contract pays base times the multiplier, plus the tier bonus on the base only (expected 66, got 60)`. Green with `--only`: passed=2 failed=0 (with LF_Test_ReputationContract).
//! Standing pay bonus (#2703, docs/architecture/reputation.md section 8): the percent by tier is
//! `0 0 0 5 10 15` from config; a tier-3 parcel run pays base plus 15 %, the bonus kept apart so the
//! drop-off books it as its own `standing-bonus` ledger line; another faction's tier pays nothing on a
//! civilian run; a contract's multiplier does not compound the bonus; a config above 15 clamps.
//! RedProof leaves the character with no standing, so the bonus is unwired and every bonus assertion fails.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_ReputationPayBonus : LF_Test
{
	static const string UID = "test-reputation-pay-bonus";

	override string GetName()
	{
		return "reputation/pay-bonus";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_ReputationConfig MakeStanding()
	{
		array<int> thresholds = {-300, -100, 0, 100, 400, 1000};
		array<int> percents = {0, 0, 0, 5, 10, 15};
		LF_ReputationConfig config = new LF_ReputationConfig();
		config.m_aFactions = {};
		config.m_aFactions.Insert(LF_ReputationFactionDef.Make("TOWN", thresholds, 0));
		config.m_aFactions.Insert(LF_ReputationFactionDef.Make("POLICE", thresholds, 0));
		config.m_aAwards = {};
		config.m_aGates = {};
		config.m_aPays = {};
		config.m_aPays.Insert(LF_ReputationPayDef.Make("TOWN", percents));
		config.m_aPays.Insert(LF_ReputationPayDef.Make("POLICE", percents));
		return config;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_ReputationContractsConfig MakeContracts()
	{
		LF_ReputationContractsConfig config = new LF_ReputationContractsConfig();
		config.m_fCooldownSeconds = 0;
		config.m_aContracts = {};
		config.m_aContracts.Insert(LF_ReputationContractDef.Make("c_town", "PARCEL", "", "TOWN", 1.5, 10, 15));
		return config;
	}

	//------------------------------------------------------------------------------------------------
	protected void Clean()
	{
		LF_CivilianJob.ForgetCareer(UID);
		LF_Reputation.Forget(UID);
		LF_ReputationContracts.Forget(UID);
		LF_ReputationPay.Forget(UID);
	}

	//------------------------------------------------------------------------------------------------
	//! Puts the character at `points` with `faction` (RedProof gives them nothing).
	protected void SetStanding(string faction, int points)
	{
		if (RedProof())
			return;

		LF_Reputation.Add(UID, faction, points, "test");
	}

	//------------------------------------------------------------------------------------------------
	protected bool StartParcel(LF_TestContext ctx, string what)
	{
		LF_CivilianJob.SignUp(UID, LF_EJobType.PARCEL);
		bool started = LF_CivilianJob.StartRun(UID, LF_EJobType.PARCEL);
		ctx.True(started, what + ": the parcel run starts");
		return started;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		Clean();
		LF_ReputationConfig.UseForTest(MakeStanding());
		LF_ReputationContractsConfig.UseForTest(MakeContracts());

		RunPercentByTier(ctx);
		RunTierThreeRun(ctx);
		RunOtherFaction(ctx);
		RunContract(ctx);
		RunClamp(ctx);

		Clean();
		LF_ReputationConfig.Reload();
		LF_ReputationContractsConfig.Reload();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunPercentByTier(LF_TestContext ctx)
	{
		array<int> points = {-300, -100, 0, 100, 400, 1000};
		array<int> percents = {0, 0, 0, 5, 10, 15};
		foreach (int i, int held : points)
		{
			Clean();
			SetStanding("TOWN", held);
			int expected = percents[i];
			if (RedProof())
				expected = percents[i] + 1;

			ctx.Equal(expected, LF_ReputationPay.Percent(UID, "TOWN"), string.Format("tier %1 pays %2 percent", i - 2, percents[i]));
		}

		Clean();
		ctx.Equal(0, LF_ReputationPay.Percent(UID, "UNDERWORLD"), "a faction with no pay row pays nothing");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunTierThreeRun(LF_TestContext ctx)
	{
		Clean();
		SetStanding("TOWN", 1000);
		ctx.Equal(3, LF_Reputation.Tier(UID, "TOWN"), "the character is at the top town tier");
		if (!StartParcel(ctx, "tier 3"))
			return;

		int base = LF_CivilianJob.PAY_PARCEL;
		int bonus = Math.Round(base * 0.15);
		int paid = LF_CivilianJob.CompleteRun(UID);
		ctx.Equal(base + bonus, paid, "a tier-3 parcel run pays base plus 15 percent");
		ctx.Equal(bonus, LF_ReputationPay.TakeLastBonus(UID), "the bonus part is kept for the standing-bonus ledger line");
		ctx.Equal(0, LF_ReputationPay.TakeLastBonus(UID), "and taken once");
		ctx.Equal(base, paid - bonus, "the other ledger line is exactly the base");
		ctx.Equal(0, LF_CivilianJob.CompleteRun(UID), "the run pays once");

		// Below the bonus tiers the same run pays the plain base.
		Clean();
		SetStanding("TOWN", 0);
		if (!StartParcel(ctx, "tier 0"))
			return;

		ctx.Equal(base, LF_CivilianJob.CompleteRun(UID), "a tier-0 run pays the plain base");
		ctx.Equal(0, LF_ReputationPay.TakeLastBonus(UID), "and keeps no bonus");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunOtherFaction(LF_TestContext ctx)
	{
		// Police (and medic) duty standing pays nothing on a civilian run: only the town's tier counts.
		Clean();
		SetStanding("POLICE", 1000);
		if (!StartParcel(ctx, "police tier"))
			return;

		ctx.Equal(LF_CivilianJob.PAY_PARCEL, LF_CivilianJob.CompleteRun(UID), "a police tier-3 character's civilian run pays the plain base");
		ctx.Equal(0, LF_ReputationPay.TakeLastBonus(UID), "no standing bonus from another faction's tier");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunContract(LF_TestContext ctx)
	{
		Clean();
		SetStanding("TOWN", 1000);
		if (!StartParcel(ctx, "contract"))
			return;

		bool taken = LF_ReputationContracts.Take(UID, "c_town", LF_JobGuards.Now());
		ctx.True(taken, "the contract is taken");

		int base = LF_CivilianJob.PAY_PARCEL;
		int contractPay = Math.Round(base * 1.5);
		int bonus = Math.Round(base * 0.15);
		int paid = LF_CivilianJob.CompleteRun(UID);
		ctx.Equal(contractPay + bonus, paid, "a contract pays base times the multiplier, plus the tier bonus on the base only");
		ctx.Equal(contractPay - base, LF_ReputationContracts.TakeLastBonus(UID), "the contract bonus part is unchanged");
		ctx.Equal(bonus, LF_ReputationPay.TakeLastBonus(UID), "the tier bonus is 15 percent of the base, not of the multiplied pay");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunClamp(LF_TestContext ctx)
	{
		array<int> wild = {0, 0, 0, 5, 10, 90};
		LF_ReputationPayDef def = LF_ReputationPayDef.Make("TOWN", wild);
		int clamped = def.PercentFor(3);
		if (RedProof())
			clamped = 0;

		ctx.Equal(15, clamped, "a config above 15 percent clamps to 15");
		ctx.Equal(0, def.PercentFor(7), "a tier past the table pays nothing");
		ctx.Equal(0, def.PercentFor(-9), "a tier below the table pays nothing");
	}
}
