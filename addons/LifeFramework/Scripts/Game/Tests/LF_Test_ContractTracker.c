// red-proof: observed 2026-11-18 with `tools\cli test --no-build --red LF_Test_ContractTracker`: RED-OK, 51 assertions fail with the hook on, first `Begin adds the contract line, for its owner (expected 'contract-tracker-uid|test_parcel|1800|1', got '')`. Green with `--only`: passed=4 failed=0 beside the two contract tests.
//! #2914: a running contract shows in the player's tracker with its time left, in words. The time word
//! moves through its bands (plenty, soon, hurry, gone) and never shows a number; the server adds the
//! line when the contract is taken (Begin) and clears it when the contract is finished, abandoned or
//! times out; only the contract's owner is sent it (an Owner RPC to their own controller); the client
//! counts the time down itself and drops the line when the time is gone.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_ContractTracker : LF_Test
{
	protected static const string UID = "contract-tracker-uid";
	protected static const string OTHER = "contract-tracker-other";
	protected static const string CONTRACT = "test_parcel";

	override string GetName()
	{
		return "reputation/contract-tracker";
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
		LF_CivilianJob.ForgetCareer(OTHER);
		LF_Reputation.Forget(UID);
		LF_Reputation.Forget(OTHER);
		LF_ReputationContracts.Forget(UID);
		LF_ReputationContracts.Forget(OTHER);
	}

	//------------------------------------------------------------------------------------------------
	//! The last send in the journal as "uid|payload", or "" when there is none.
	protected string LastSend()
	{
		array<string> journal = {};
		LF_ContractTracker.GetJournal(journal);
		if (journal.IsEmpty())
			return string.Empty;

		return journal[journal.Count() - 1];
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		RunWords(ctx);
		RunWire(ctx);
		RunClient(ctx);
		RunServer(ctx);
	}

	//------------------------------------------------------------------------------------------------
	//! The word moves through its bands as the time runs down; no limit has no word.
	protected void RunWords(LF_TestContext ctx)
	{
		ctx.Equal(LF_EContractTimeWord.PLENTY, LF_ContractTracker.TimeWord(1000, 1000), "all the time left is plenty");
		ctx.Equal(LF_EContractTimeWord.PLENTY, LF_ContractTracker.TimeWord(501, 1000), "just over half is still plenty");
		ctx.Equal(LF_EContractTimeWord.SOON, LF_ContractTracker.TimeWord(500, 1000), "half is soon");
		ctx.Equal(LF_EContractTimeWord.SOON, LF_ContractTracker.TimeWord(201, 1000), "just over a fifth is still soon");
		ctx.Equal(LF_EContractTimeWord.HURRY, LF_ContractTracker.TimeWord(200, 1000), "a fifth is hurry");
		ctx.Equal(LF_EContractTimeWord.HURRY, LF_ContractTracker.TimeWord(1, 1000), "the last second is hurry");
		ctx.Equal(LF_EContractTimeWord.EXPIRED, LF_ContractTracker.TimeWord(0, 1000), "no time left is expired");
		ctx.Equal(LF_EContractTimeWord.EXPIRED, LF_ContractTracker.TimeWord(-30, 1000), "past the time is expired");
		ctx.Equal(LF_EContractTimeWord.NONE, LF_ContractTracker.TimeWord(5, 0), "a contract with no limit has no time word");

		ctx.EqualStr(LF_ContractTracker.KEY_TIME_PLENTY, LF_ContractTracker.TimeKey(LF_EContractTimeWord.PLENTY), "plenty has its key");
		ctx.EqualStr(LF_ContractTracker.KEY_TIME_SOON, LF_ContractTracker.TimeKey(LF_EContractTimeWord.SOON), "soon has its key");
		ctx.EqualStr(LF_ContractTracker.KEY_TIME_HURRY, LF_ContractTracker.TimeKey(LF_EContractTimeWord.HURRY), "hurry has its key");
		ctx.EqualStr("", LF_ContractTracker.TimeKey(LF_EContractTimeWord.NONE), "no limit shows no word");
		ctx.EqualStr("", LF_ContractTracker.TimeKey(LF_EContractTimeWord.EXPIRED), "expired shows no word: the line is gone");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunWire(LF_TestContext ctx)
	{
		string id;
		int limit;
		bool stake;
		ctx.True(LF_ContractTracker.Decode(LF_ContractTracker.Encode("harbour", 1800, true), id, limit, stake), "a line decodes");
		ctx.EqualStr("harbour", id, "the contract id round trips");
		ctx.Equal(1800, limit, "the limit round trips");
		ctx.True(stake, "the stake flag round trips");
		ctx.True(LF_ContractTracker.Decode(LF_ContractTracker.Encode("harbour", 0, false), id, limit, stake), "a line with no limit decodes");
		ctx.False(stake, "no stake round trips");
		ctx.False(LF_ContractTracker.Decode("", id, limit, stake), "an empty payload is not a line");
		ctx.False(LF_ContractTracker.Decode("harbour|1800", id, limit, stake), "a short payload is not a line");
		ctx.True(LF_ContractTracker.TitleKey("harbour").EndsWith("_harbour"), "the title key carries the contract id");
	}

	//------------------------------------------------------------------------------------------------
	//! The client holds a line, moves its word as time passes, and drops it when the time is gone.
	protected void RunClient(LF_TestContext ctx)
	{
		LF_ContractTracker.ClearClient();
		ctx.False(LF_ContractTracker.ClientHasLine(), "a client with no contract has no line");

		LF_ContractTracker.Receive(LF_ContractTracker.Encode("harbour", 100, true), 0);
		ctx.True(LF_ContractTracker.ClientHasLine(), "the received line is held");
		ctx.True(LF_ContractTracker.ClientHasCost(), "a stake shows the cost line, in words");
		ctx.Equal(LF_EContractTimeWord.PLENTY, LF_ContractTracker.ClientTimeWord(1000), "a fresh line has plenty of time");
		ctx.Equal(LF_EContractTimeWord.SOON, LF_ContractTracker.ClientTimeWord(60000), "after 60 of 100 s it is soon");
		ctx.Equal(LF_EContractTimeWord.HURRY, LF_ContractTracker.ClientTimeWord(90000), "after 90 of 100 s it is hurry");
		ctx.True(LF_ContractTracker.ClientHasLine(), "the line stays while time is left");
		ctx.Equal(LF_EContractTimeWord.EXPIRED, LF_ContractTracker.ClientTimeWord(100000), "at 100 s the time is gone");
		ctx.False(LF_ContractTracker.ClientHasLine(), "the line goes on expiry");

		LF_ContractTracker.Receive(LF_ContractTracker.Encode("harbour", 100, false), 0);
		ctx.False(LF_ContractTracker.ClientHasCost(), "no stake shows no cost line");
		LF_ContractTracker.Receive("", 5);
		ctx.False(LF_ContractTracker.ClientHasLine(), "an empty payload clears the line");

		LF_ContractTracker.Receive(LF_ContractTracker.Encode("open", 0, false), 0);
		ctx.Equal(LF_EContractTimeWord.NONE, LF_ContractTracker.ClientTimeWord(9999999), "a contract with no limit never runs out");
		ctx.True(LF_ContractTracker.ClientHasLine(), "and its line stays");

		LF_ContractTracker.ClearClient();
		LF_ContractTrackerHud.Remove();
	}

	//------------------------------------------------------------------------------------------------
	//! The server adds the line on Begin and clears it on finish, abandon and time-out; only the owner
	//! is addressed.
	protected void RunServer(LF_TestContext ctx)
	{
		IEntity player = LF_TestCharacterFixture.Spawn(ctx);
		if (!player)
			return;

		Clean();
		LF_ContractTracker.ClearJournal();
		LF_Utils.SetUidForTests(player, UID);
		LF_ReputationConfig.UseForTest(MakeStanding());
		LF_ReputationContractsConfig.Reload();
		LF_JobsConfig.Reload();
		LF_ReputationContractDef def = LF_ReputationContracts.FindDef(CONTRACT);
		ctx.NotNull(def, "the core ships the test_parcel contract row");
		if (!def)
		{
			LF_Utils.SetUidForTests(null, "");
			LF_TestCharacterFixture.Delete(player);
			return;
		}

		string started = UID + "|" + LF_ContractTracker.Encode(CONTRACT, Math.Round(def.m_fTimeLimitSeconds), def.m_iStakePoints > 0);
		string cleared = UID + "|";

		// Begin adds the line. Red proof: skip the Begin.
		LF_Reputation.Add(UID, "TOWN", 100, "test");
		LF_MoneyUtils.GiveCash(player, LF_CivilianJob.DepositFor(LF_EJobType.PARCEL) * 4);
		LF_CivilianJob.SignUp(UID, LF_EJobType.PARCEL);
		int deposit;
		string refusal;
		string tierWord;
		if (!RedProof())
			LF_ReputationContracts.Begin(player, UID, CONTRACT, LF_JobGuards.Now(), deposit, refusal, tierWord);

		ctx.EqualStr(started, LastSend(), "Begin adds the contract line, for its owner");

		// Finish clears it.
		LF_CivilianJob.CompleteRun(UID);
		ctx.EqualStr(cleared, LastSend(), "finishing the contract clears the line");

		// Abandon clears it.
		LF_ReputationContracts.Forget(UID);
		LF_ContractTracker.ClearJournal();
		LF_Reputation.Add(UID, "TOWN", 100, "test");
		// Begin is rate limited as a purchase, so the later contracts bind a started run with Take.
		ctx.True(LF_CivilianJob.StartRun(UID, LF_EJobType.PARCEL), "a second parcel run starts");
		ctx.True(LF_ReputationContracts.Take(UID, CONTRACT, LF_JobGuards.Now()), "a second contract binds");
		ctx.EqualStr(started, LastSend(), "a second contract adds its line");
		LF_CivilianJob.AbandonRun(UID);
		ctx.EqualStr(cleared, LastSend(), "abandoning the contract clears the line");

		// A time-out clears it: taken long ago, settled now.
		LF_ReputationContracts.Forget(UID);
		LF_ContractTracker.ClearJournal();
		LF_Reputation.Add(UID, "TOWN", 100, "test");
		ctx.True(LF_CivilianJob.StartRun(UID, LF_EJobType.PARCEL), "a parcel run starts");
		ctx.True(LF_ReputationContracts.Take(UID, CONTRACT, LF_JobGuards.Now() - def.m_fTimeLimitSeconds - 60), "a contract taken long ago binds");
		ctx.EqualStr(started, LastSend(), "taking it adds the line");
		LF_CivilianJob.CompleteRun(UID);
		ctx.EqualStr(cleared, LastSend(), "a timed-out contract clears the line");

		// Only the owner is addressed: another character's contract sends to that character alone.
		LF_ReputationContracts.Forget(OTHER);
		LF_ContractTracker.ClearJournal();
		LF_Reputation.Add(OTHER, "TOWN", 100, "test");
		LF_CivilianJob.SignUp(OTHER, LF_EJobType.PARCEL);
		LF_CivilianJob.StartRun(OTHER, LF_EJobType.PARCEL);
		LF_ReputationContracts.Take(OTHER, CONTRACT, LF_JobGuards.Now());
		array<string> journal = {};
		LF_ContractTracker.GetJournal(journal);
		ctx.Equal(1, journal.Count(), "one contract is one send");
		foreach (string entry : journal)
		{
			ctx.True(entry.StartsWith(OTHER + "|"), "the line is addressed to the contract's owner only");
		}

		LF_CivilianJob.AbandonRun(OTHER);
		LF_CivilianJob.AbandonRun(UID);
		Clean();
		LF_ContractTracker.ClearJournal();
		LF_ReputationConfig.Reload();
		LF_ReputationContractsConfig.Reload();
		LF_Utils.SetUidForTests(null, "");
		LF_TestCharacterFixture.Delete(player);
	}
}
