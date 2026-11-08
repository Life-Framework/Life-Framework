// red-proof: pending
// tier: LOGIC
//! SIGNAL objectives advance only on their own key, chain steps open one at a time, and a
//! returning player (turn-ins restored) is offered none of the chain again. Pure tracker logic.
class LF_Test_QuestSignal : EL_Test
{
	override string GetName()
	{
		return "quests/signal-event";
	}

	override void Run(EL_TestContext ctx)
	{
		TestSignalKeys(ctx);
		TestSignalMatchesOnlyItsKey(ctx);
		TestChainOrder(ctx);
		TestReturningPlayer(ctx);
	}

	//------------------------------------------------------------------------------------------------
	protected void TestSignalKeys(EL_TestContext ctx)
	{
		ctx.EqualStr("sign.read:town_map", LF_QuestSignals.SignRead("town_map"), "sign read key");
		ctx.EqualStr("job.run_complete:parcel", LF_QuestSignals.JobRunComplete(EL_EJobType.PARCEL), "parcel run key is the job name in lower case");
		ctx.EqualStr("atm.deposit", LF_QuestSignals.ATM_DEPOSIT, "deposit key");
	}

	//------------------------------------------------------------------------------------------------
	protected void TestSignalMatchesOnlyItsKey(EL_TestContext ctx)
	{
		LF_QuestDefinition board = MakeStep("t_board", "", false, false, LF_EQuestEventType.SIGNAL, "sign.read:town_map");
		map<string, ref LF_QuestDefinition> defs = new map<string, ref LF_QuestDefinition>();
		defs.Set(board.m_sQuestId, board);

		LF_QuestTracker tracker = new LF_QuestTracker();
		tracker.Accept(board, 1, EL_EJobType.UNEMPLOYED, "", 0, new array<string>());

		ctx.False(tracker.ApplyEvent(MakeEvent(LF_EQuestEventType.SIGNAL, "sign.read:post_office"), defs), "another sign's read does not move the board step");
		ctx.False(tracker.ApplyEvent(MakeEvent(LF_EQuestEventType.SIGNAL, "atm.deposit"), defs), "a deposit does not move the board step");
		ctx.False(tracker.ApplyEvent(MakeEvent(LF_EQuestEventType.TALK, "sign.read:town_map"), defs), "a talk event with the same text is not a signal");
		ctx.True(tracker.GetStatus("t_board") == LF_EQuestStatus.ACTIVE, "the board step is still running");

		ctx.True(tracker.ApplyEvent(MakeEvent(LF_EQuestEventType.SIGNAL, "sign.read:town_map"), defs), "reading the town board moves the board step");
		ctx.True(tracker.GetStatus("t_board") == LF_EQuestStatus.COMPLETED, "the board step is complete");
	}

	//------------------------------------------------------------------------------------------------
	//! register (manual, talk) -> board (auto, signal) -> bank (auto, signal) -> done (auto accept, talk).
	protected void TestChainOrder(EL_TestContext ctx)
	{
		map<string, ref LF_QuestDefinition> defs = MakeChain();
		LF_QuestTracker tracker = new LF_QuestTracker();
		array<string> ids = {};

		tracker.CollectAutoAccepts(defs, ids);
		ctx.Equal(0, ids.Count(), "nothing starts on its own before the clerk is met");
		ctx.False(tracker.CanAccept(defs.Get("c_bank"), 1, EL_EJobType.UNEMPLOYED, "", 0, tracker.GetCompletedQuestIds()), "a later step cannot be taken out of order");

		ctx.True(tracker.Accept(defs.Get("c_register"), 1, EL_EJobType.UNEMPLOYED, "", 0, tracker.GetCompletedQuestIds()), "step 1 is accepted at the clerk");
		tracker.ApplyEvent(MakeEvent(LF_EQuestEventType.TALK, "clerk"), defs);
		ctx.True(tracker.TurnIn("c_register", 1), "step 1 turns in");

		tracker.CollectAutoAccepts(defs, ids);
		ctx.Equal(1, ids.Count(), "exactly one step opens after step 1");
		if (ids.Count() == 1)
			ctx.EqualStr("c_board", ids[0], "the board step opens after step 1");
		AcceptAll(tracker, defs, ids);

		// The deposit comes too early: the bank step is not running, so it moves nothing.
		ctx.False(tracker.ApplyEvent(MakeEvent(LF_EQuestEventType.SIGNAL, "atm.deposit"), defs), "an early deposit moves nothing");
		tracker.CollectAutoTurnIns(defs, ids);
		ctx.Equal(0, ids.Count(), "nothing is ready to turn in after an early deposit");

		tracker.ApplyEvent(MakeEvent(LF_EQuestEventType.SIGNAL, "sign.read:town_map"), defs);
		tracker.CollectAutoTurnIns(defs, ids);
		ctx.Equal(1, ids.Count(), "the read board step turns in on its own");
		foreach (string boardId : ids)
			tracker.TurnIn(boardId, 2);

		tracker.CollectAutoAccepts(defs, ids);
		ctx.Equal(1, ids.Count(), "the bank step opens after the board");
		if (ids.Count() == 1)
			ctx.EqualStr("c_bank", ids[0], "the bank step is next");
		AcceptAll(tracker, defs, ids);

		tracker.ApplyEvent(MakeEvent(LF_EQuestEventType.SIGNAL, "atm.deposit"), defs);
		tracker.CollectAutoTurnIns(defs, ids);
		foreach (string bankId : ids)
			tracker.TurnIn(bankId, 3);
		tracker.CollectAutoAccepts(defs, ids);
		AcceptAll(tracker, defs, ids);
		ctx.True(tracker.GetStatus("c_done") == LF_EQuestStatus.ACTIVE, "the return-to-clerk step runs after the deposit");

		tracker.CollectAutoTurnIns(defs, ids);
		ctx.Equal(0, ids.Count(), "the last step waits for the clerk: it is not auto turned in");
		tracker.ApplyEvent(MakeEvent(LF_EQuestEventType.TALK, "clerk"), defs);
		ctx.True(tracker.GetStatus("c_done") == LF_EQuestStatus.COMPLETED, "talking to the clerk completes the last step");
		tracker.CollectAutoTurnIns(defs, ids);
		ctx.Equal(0, ids.Count(), "the clerk turns the last step in, not the tracker");
	}

	//------------------------------------------------------------------------------------------------
	protected void TestReturningPlayer(EL_TestContext ctx)
	{
		map<string, ref LF_QuestDefinition> defs = MakeChain();
		LF_QuestTracker tracker = new LF_QuestTracker();
		foreach (string questId, LF_QuestDefinition def : defs)
			tracker.RestoreTurnedIn(questId);

		array<string> ids = {};
		tracker.CollectAutoAccepts(defs, ids);
		ctx.Equal(0, ids.Count(), "a returning player gets no chain step started again");

		foreach (string listedId, LF_QuestDefinition listed : defs)
		{
			ctx.False(tracker.CanAccept(listed, 1, EL_EJobType.UNEMPLOYED, "", 1000, tracker.GetCompletedQuestIds()), "a returning player cannot take " + listedId + " again");
			ctx.False(listed.IsListed(tracker.GetStatus(listedId), true), "the clerk does not list " + listedId + " to a returning player");
		}

		// A player who left after step 1 gets step 2 back, and the clerk lists only that step.
		LF_QuestTracker midway = new LF_QuestTracker();
		midway.RestoreTurnedIn("c_register");
		midway.CollectAutoAccepts(defs, ids);
		ctx.Equal(1, ids.Count(), "a player who left after step 1 gets the board step back");
		ctx.False(defs.Get("c_bank").IsListed(LF_EQuestStatus.AVAILABLE, false), "a step whose previous step is not done is not listed");
	}

	//------------------------------------------------------------------------------------------------
	protected void AcceptAll(LF_QuestTracker tracker, map<string, ref LF_QuestDefinition> defs, array<string> ids)
	{
		foreach (string questId : ids)
			tracker.Accept(defs.Get(questId), 1, EL_EJobType.UNEMPLOYED, "", 0, tracker.GetCompletedQuestIds());
	}

	//------------------------------------------------------------------------------------------------
	protected map<string, ref LF_QuestDefinition> MakeChain()
	{
		map<string, ref LF_QuestDefinition> defs = new map<string, ref LF_QuestDefinition>();
		LF_QuestDefinition register = MakeStep("c_register", "", false, false, LF_EQuestEventType.TALK, "clerk");
		LF_QuestDefinition board = MakeStep("c_board", "c_register", true, true, LF_EQuestEventType.SIGNAL, "sign.read:town_map");
		LF_QuestDefinition bank = MakeStep("c_bank", "c_board", true, true, LF_EQuestEventType.SIGNAL, "atm.deposit");
		LF_QuestDefinition done = MakeStep("c_done", "c_bank", true, false, LF_EQuestEventType.TALK, "clerk");
		defs.Set(register.m_sQuestId, register);
		defs.Set(board.m_sQuestId, board);
		defs.Set(bank.m_sQuestId, bank);
		defs.Set(done.m_sQuestId, done);
		return defs;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_QuestDefinition MakeStep(string questId, string previous, bool autoAccept, bool autoTurnIn, LF_EQuestEventType type, string filter)
	{
		LF_QuestObjective objective = new LF_QuestObjective();
		objective.m_eType = type;
		objective.m_iTarget = 1;
		objective.m_eMode = LF_EQuestProgressMode.ADD;
		objective.m_sFilter = filter;

		LF_QuestDefinition def = new LF_QuestDefinition();
		def.m_sQuestId = questId;
		def.m_sRequiredPreviousQuestId = previous;
		def.m_bAutoAccept = autoAccept;
		def.m_bAutoTurnIn = autoTurnIn;
		def.m_eRequiredJob = EL_EJobType.UNEMPLOYED;
		def.m_aObjectives = new array<ref LF_QuestObjective>();
		def.m_aObjectives.Insert(objective);
		return def;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_QuestEvent MakeEvent(LF_EQuestEventType type, string filter)
	{
		LF_QuestEvent ev = new LF_QuestEvent();
		ev.m_eType = type;
		ev.m_sFilter = filter;
		ev.m_iValue = 1;
		return ev;
	}
}
