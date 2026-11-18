// red-proof: observed 2026-11-18 with `toolscli test --no-build --red LF_Test_ReputationContract`: RED PROOF OK, first `a timed-out contract takes the stake and awards nothing (expected -15, got 0)`. Green with `--only`: passed=4 failed=0. Review (#2817): `RunWipeThenPlainRun` failed on the unfixed code (expected 40, got 60) and passed once ForgetCareer drops the contract.
//! Contracts (#2702, docs/architecture/reputation.md "Spending standing: contracts" and section 8): a
//! finished contract pays the run's base times its multiplier once and awards its points; an abandoned
//! or timed-out one pays no contract money and takes the stake; one contract at a time per character,
//! with a cooldown; the multiplier clamps to 1.3..1.6; a standing gate refuses in the faction's words.
//! RedProof leaves the contract untaken, so the run is a plain run and every contract assertion fails.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_ReputationContract : LF_Test
{
	static const string UID = "test-reputation-contract";
	static const float COOLDOWN = 100;

	override string GetName()
	{
		return "reputation/contract";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_ReputationConfig MakeStanding()
	{
		array<int> thresholds = {-300, -100, 0, 100, 400, 1000};
		LF_ReputationConfig config = new LF_ReputationConfig();
		config.m_aFactions = {};
		config.m_aFactions.Insert(LF_ReputationFactionDef.Make("TOWN", thresholds, 0));
		config.m_aFactions.Insert(LF_ReputationFactionDef.Make("POLICE", thresholds, 0));
		config.m_aAwards = {};
		config.m_aGates = {};
		config.m_aGates.Insert(LF_ReputationGateDef.Make("contract:police", "POLICE", 1));
		return config;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_ReputationContractsConfig MakeContracts()
	{
		LF_ReputationContractsConfig config = new LF_ReputationContractsConfig();
		config.m_fCooldownSeconds = COOLDOWN;
		config.m_aContracts = {};
		config.m_aContracts.Insert(LF_ReputationContractDef.Make("c_town", "PARCEL", "", "TOWN", 1.5, 10, 15));
		config.m_aContracts.Insert(LF_ReputationContractDef.Make("c_timed", "PARCEL", "", "TOWN", 1.5, 10, 15, 60));
		config.m_aContracts.Insert(LF_ReputationContractDef.Make("c_police", "PARCEL", "contract:police", "POLICE", 1.4, 10, 15));
		config.m_aContracts.Insert(LF_ReputationContractDef.Make("c_taxi", "TAXI", "", "TOWN", 1.5, 10, 15));
		return config;
	}

	//------------------------------------------------------------------------------------------------
	protected void Clean()
	{
		LF_CivilianJob.ForgetCareer(UID);
		LF_Reputation.Forget(UID);
		LF_ReputationContracts.Forget(UID);
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
	protected void TakeContract(LF_TestContext ctx, string id, float now, string what)
	{
		bool taken = false;
		if (!RedProof())
			taken = LF_ReputationContracts.Take(UID, id, now);

		ctx.True(taken, what + ": the contract is taken");
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		Clean();
		LF_ReputationConfig.UseForTest(MakeStanding());
		LF_ReputationContractsConfig.UseForTest(MakeContracts());

		RunClamp(ctx);
		RunFinish(ctx);
		RunCooldownAndAbandon(ctx);
		RunTimeout(ctx);
		RunRefusals(ctx);
		RunWipeThenPlainRun(ctx);

		Clean();
		LF_ReputationConfig.Reload();
		LF_ReputationContractsConfig.Reload();
		RunShipped(ctx);
		Clean();
		LF_ReputationContractsConfig.Reload();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunClamp(LF_TestContext ctx)
	{
		ctx.EqualFloat(1.3, LF_ReputationContractDef.Make("a", "PARCEL", "", "TOWN", 1.0, 0, 0).Multiplier(), 0.001, "a multiplier below 1.3 clamps up");
		ctx.EqualFloat(1.6, LF_ReputationContractDef.Make("a", "PARCEL", "", "TOWN", 2.5, 0, 0).Multiplier(), 0.001, "a multiplier above 1.6 clamps down");
		ctx.EqualFloat(1.45, LF_ReputationContractDef.Make("a", "PARCEL", "", "TOWN", 1.45, 0, 0).Multiplier(), 0.001, "a multiplier inside the range stays");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunFinish(LF_TestContext ctx)
	{
		Clean();
		float now = LF_JobGuards.Now();
		if (!StartParcel(ctx, "finish"))
			return;

		TakeContract(ctx, "c_town", now, "finish");
		ctx.EqualStr("c_town", LF_ReputationContracts.ActiveId(UID), "the run carries the contract");
		ctx.False(LF_ReputationContracts.Take(UID, "c_town", now), "a second contract at once is refused");

		int base = LF_CivilianJob.PAY_PARCEL;
		int paid = LF_CivilianJob.CompleteRun(UID);
		ctx.Equal(Math.Round(base * 1.5), paid, "a finished contract pays base times the multiplier");
		ctx.Equal(Math.Round(base * 1.5) - base, LF_ReputationContracts.TakeLastBonus(UID), "the bonus part is kept for the standing-bonus ledger reason");
		ctx.Equal(10, LF_Reputation.Points(UID, "TOWN"), "a finished contract awards its points");
		ctx.EqualStr("", LF_ReputationContracts.ActiveId(UID), "the contract ends with the run");
		ctx.Equal(0, LF_CivilianJob.CompleteRun(UID), "the contract pays once");
		ctx.Equal(10, LF_Reputation.Points(UID, "TOWN"), "and awards once");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunCooldownAndAbandon(LF_TestContext ctx)
	{
		// Continues from RunFinish: TOWN 10, the cooldown running since `now`.
		float now = LF_JobGuards.Now();
		if (!StartParcel(ctx, "cooldown"))
			return;

		string word;
		string refusal = LF_ReputationContracts.Refusal(UID, "c_town", now, word);
		if (RedProof())
			refusal = string.Empty;

		ctx.EqualStr(LF_ReputationContracts.KEY_COOLDOWN, refusal, "a new contract inside the cooldown is refused");
		TakeContract(ctx, "c_town", now + COOLDOWN + 1, "after the cooldown");

		ctx.True(LF_CivilianJob.AbandonRun(UID), "the contract run is abandoned");
		ctx.Equal(10 - 15, LF_Reputation.Points(UID, "TOWN"), "an abandoned contract takes the stake");
		ctx.EqualStr("", LF_ReputationContracts.ActiveId(UID), "the abandoned contract ends");
		ctx.Equal(0, LF_ReputationContracts.TakeLastBonus(UID), "an abandoned contract pays no bonus");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunTimeout(LF_TestContext ctx)
	{
		Clean();
		float now = LF_JobGuards.Now();
		if (!StartParcel(ctx, "timeout"))
			return;

		// Taken 1000 s ago with a 60 s limit: finishing now is too late.
		TakeContract(ctx, "c_timed", now - 1000, "timeout");
		int paid = LF_CivilianJob.CompleteRun(UID);
		ctx.Equal(LF_CivilianJob.PAY_PARCEL, paid, "a timed-out contract pays no contract money, only the run's base");
		ctx.Equal(-15, LF_Reputation.Points(UID, "TOWN"), "a timed-out contract takes the stake and awards nothing");
		ctx.Equal(0, LF_ReputationContracts.TakeLastBonus(UID), "a timed-out contract has no bonus");

		// A plain run with no contract is untouched.
		Clean();
		if (!StartParcel(ctx, "plain"))
			return;

		ctx.Equal(LF_CivilianJob.PAY_PARCEL, LF_CivilianJob.CompleteRun(UID), "a run with no contract pays its base");
		ctx.Equal(0, LF_Reputation.Points(UID, "TOWN"), "a plain run moves no contract standing");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunRefusals(LF_TestContext ctx)
	{
		Clean();
		float now = LF_JobGuards.Now();
		string word;
		ctx.EqualStr(LF_ReputationContracts.KEY_UNKNOWN, LF_ReputationContracts.Refusal(UID, "no_such", now, word), "an unknown contract is refused");
		ctx.EqualStr("#LF-Reputation_Refuse_Police", LF_ReputationContracts.Refusal(UID, "c_police", now, word), "the standing gate refuses in the faction's words");
		ctx.False(LF_ReputationContracts.Take(UID, "c_town", now), "no contract without an active run");
		if (!StartParcel(ctx, "refusals"))
			return;

		ctx.False(LF_ReputationContracts.Take(UID, "c_taxi", now), "a contract for another job does not bind to this run");
		ctx.False(LF_ReputationContracts.Take(UID, "c_police", now), "a gated contract is refused below the tier");
		LF_Reputation.Add(UID, "POLICE", 100, "test");
		TakeContract(ctx, "c_police", now, "at the gate's tier");

		// The run vanishes outside the run paths (a wipe): the contract is dropped with no stake.
		LF_CivilianJob.ForgetCareer(UID);
		ctx.EqualStr("", LF_ReputationContracts.ActiveId(UID), "a contract whose run is gone is dropped");
		ctx.Equal(100, LF_Reputation.Points(UID, "POLICE"), "dropping a stale contract takes no stake");
	}

	//------------------------------------------------------------------------------------------------
	//! Review of #2817: a wipe drops the run mid-contract and the character starts a plain run before
	//! anything reads the contract. The plain run must not inherit the old contract's multiplier and award.
	protected void RunWipeThenPlainRun(LF_TestContext ctx)
	{
		Clean();
		float now = LF_JobGuards.Now();
		if (!StartParcel(ctx, "wipe"))
			return;

		TakeContract(ctx, "c_town", now, "wipe");
		LF_CivilianJob.ForgetCareer(UID);
		if (!StartParcel(ctx, "plain after wipe"))
			return;

		ctx.Equal(LF_CivilianJob.PAY_PARCEL, LF_CivilianJob.CompleteRun(UID), "a plain run after a wipe pays its base, not the wiped contract's multiplier");
		ctx.Equal(0, LF_Reputation.Points(UID, "TOWN"), "a plain run after a wipe awards no contract standing");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunShipped(LF_TestContext ctx)
	{
		LF_ReputationContractDef def = LF_ReputationContracts.FindDef("test_parcel");
		ctx.NotNull(def, "the core ships the neutral test contract");
		if (!def)
			return;

		ctx.Equal(LF_EJobType.PARCEL, def.Job(), "the shipped contract is a parcel run");
		ctx.True(def.m_iStakePoints > 0, "the shipped contract has a stake");
		ctx.True(def.Multiplier() >= 1.3 && def.Multiplier() <= 1.6, "the shipped multiplier is in range");
	}
}
