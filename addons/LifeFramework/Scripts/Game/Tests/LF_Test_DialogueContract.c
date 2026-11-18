// red-proof: observed 2026-11-18 with `tools\cli test --no-build --red LF_Test_DialogueContract`: RED PROOF OK, RedProof() skips the pick at the gate's tier, so `the run's deposit was taken once (expected 25, got 50)` fails. The first green run also caught a real bug: the nested JobSignUp Run overwrote the hook's `param`, so Begin got "PARCEL" (`the reply starts the contract (expected 'test_parcel', got '')`).
//! #2913: a dialogue reply offers a contract. `LF_DialogueHook_Contract` with `test_parcel` hides the
//! reply below the contract's standing gate and refuses there in the faction's words with nothing
//! started; at the gate's tier it signs the player up, takes the run's deposit and binds the contract;
//! a second ask while the contract runs is refused as busy and starts nothing more.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_DialogueContract : LF_Test
{
	protected static const string UID = "dialogue-contract-uid";
	protected static const string CONTRACT = "test_parcel";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "dialogue/contract";
	}

	//------------------------------------------------------------------------------------------------
	//! A standing table whose gate for test_parcel needs TOWN tier 1.
	protected LF_ReputationConfig MakeStanding()
	{
		array<int> thresholds = {-300, -100, 0, 100, 400, 1000};
		LF_ReputationConfig config = new LF_ReputationConfig();
		config.m_aFactions = {};
		config.m_aFactions.Insert(LF_ReputationFactionDef.Make("TOWN", thresholds, 0));
		config.m_aAwards = {};
		config.m_aGates = {};
		config.m_aGates.Insert(LF_ReputationGateDef.Make("contract:" + CONTRACT, "TOWN", 1));
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
	//! Runs the reply's action as the server does when the player picks it.
	protected void Pick(IEntity player, notnull array<string> spoken)
	{
		LF_DialogueHook hook = LF_DialogueHook.Find("Contract");
		if (hook)
			hook.Run(player, null, CONTRACT, spoken);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_DialogueHook hook = LF_DialogueHook.Find("Contract");
		ctx.NotNull(hook, "the Contract dialogue hook has its class");
		if (!hook)
			return;

		IEntity player = LF_TestCharacterFixture.Spawn(ctx);
		if (!player)
			return;

		Clean();
		LF_Utils.SetUidForTests(player, UID);
		LF_ReputationConfig.UseForTest(MakeStanding());
		LF_ReputationContractsConfig.Reload();
		LF_JobsConfig.Reload();
		ctx.NotNull(LF_ReputationContracts.FindDef(CONTRACT), "the core ships the test_parcel contract row");

		int deposit = LF_CivilianJob.DepositFor(LF_EJobType.PARCEL);
		LF_MoneyUtils.GiveCash(player, deposit * 2);
		int cash = LF_MoneyUtils.GetCash(player);

		// Below the gate: no reply, and a forced pick is refused in words with nothing started.
		ctx.False(hook.Allows(player, null, CONTRACT), "the reply is hidden below the gate's tier");
		ctx.False(hook.Allows(player, null, "no_such_contract"), "a reply naming no contract is hidden");
		// #3050: the locked condition is the other side: offered below the gate, hidden at it and for no contract.
		LF_DialogueHook locked = LF_DialogueHook.Find("ContractLocked");
		ctx.NotNull(locked, "the ContractLocked dialogue hook has its class");
		if (locked)
		{
			ctx.True(locked.Allows(player, null, CONTRACT), "the come-back reply is offered below the gate's tier");
			ctx.False(locked.Allows(player, null, "no_such_contract"), "the come-back reply naming no contract is hidden");
		}
		array<string> spoken = {};
		Pick(player, spoken);
		string tierWord;
		ctx.EqualStr("#LF-Reputation_Refuse_Town", LF_ReputationContracts.Refusal(UID, CONTRACT, LF_JobGuards.Now(), tierWord), "below the gate the refusal is the town's words");
		ctx.EqualStr("", LF_ReputationContracts.ActiveId(UID), "a refused pick starts no contract");
		ctx.False(LF_CivilianJob.HasActiveRun(UID), "a refused pick starts no run");
		ctx.Equal(cash, LF_MoneyUtils.GetCash(player), "a refused pick takes no deposit");

		// At the gate's tier: the act. Red proof: skip the pick.
		LF_Reputation.Add(UID, "TOWN", 100, "test");
		ctx.True(hook.Allows(player, null, CONTRACT), "the reply is offered at the gate's tier");
		if (locked)
			ctx.False(locked.Allows(player, null, CONTRACT), "the come-back reply is hidden at the gate's tier");

		spoken.Clear();
		if (!RedProof())
			Pick(player, spoken);

		ctx.EqualStr(CONTRACT, LF_ReputationContracts.ActiveId(UID), "the reply starts the contract");
		ctx.True(LF_CivilianJob.HasActiveRun(UID), "the contract's parcel run is under way");
		ctx.Equal(LF_EJobType.PARCEL, LF_CivilianJob.GetSignedUpJob(UID), "the player was signed up for the contract's job");
		ctx.Equal(cash - deposit, LF_MoneyUtils.GetCash(player), "the run's deposit was taken once");
		ctx.True(spoken.Contains(LF_DialogueHook_Contract.KEY_STARTED), "the NPC answers that the contract is on");

		// A second ask while it runs: refused as busy, nothing more moves.
		spoken.Clear();
		Pick(player, spoken);
		ctx.True(spoken.Contains(LF_ReputationContracts.KEY_BUSY), "a second ask is refused as busy, in words");
		ctx.EqualStr(CONTRACT, LF_ReputationContracts.ActiveId(UID), "the first contract stays");
		ctx.Equal(cash - deposit, LF_MoneyUtils.GetCash(player), "a second ask takes no second deposit");

		// A plain run already under way: refused in words before the job is touched.
		LF_CivilianJob.AbandonRun(UID);
		LF_ReputationContracts.Forget(UID);
		LF_Reputation.Add(UID, "TOWN", 100, "test");	// the abandon took the stake; back above the gate
		ctx.True(LF_CivilianJob.StartRun(UID, LF_EJobType.PARCEL), "a plain parcel run starts");
		spoken.Clear();
		Pick(player, spoken);
		ctx.True(spoken.Contains(LF_DialogueHook_Contract.KEY_RUN_OPEN), "an ask during a plain run is refused, in words");
		ctx.EqualStr("", LF_ReputationContracts.ActiveId(UID), "an ask during a plain run binds no contract");

		LF_CivilianJob.AbandonRun(UID);
		Clean();
		LF_ReputationConfig.Reload();
		LF_ReputationContractsConfig.Reload();
		LF_Utils.SetUidForTests(null, "");
		LF_TestCharacterFixture.Delete(player);
	}
}
