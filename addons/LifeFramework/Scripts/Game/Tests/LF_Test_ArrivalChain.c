// red-proof: Observed 2026-11-08 with `tools\cli test --no-build --only LF_Test_ArrivalChain`: made
// LF_QuestManager.ReportSignal route a TALK event; got `[ELTEST] FAIL quests/arrival-chain`,
// `- step 2 turns in on its own`, `- step 3 starts on its own`, `- a deposit moves step 4`. Restored; green.
// tier: WORLD
//! The town clerk in Quests.layer runs the arrival chain end to end through the public quest
//! API: talk and register, read the town board, deliver a parcel, bank cash, return. Each
//! SIGNAL moves only its own step, a returning player is never offered the chain again, and
//! the ID card is handed over once.
class LF_Test_ArrivalChain : EL_Test
{
	// Quests.layer: Quest_Clerk stands at 134 0 243.5; the test bodies spawn a meter from him.

	override string GetName()
	{
		return "quests/arrival-chain";
	}

	override void Run(EL_TestContext ctx)
	{
		LF_QuestManager manager = LF_QuestManager.GetInstance();
		ctx.EqualStr("clerk", manager.GetGiverOfQuest("arrival_register"), "the clerk offers step 1");
		ctx.EqualStr("clerk", manager.GetGiverOfQuest("arrival_done"), "the clerk offers step 5");
		LF_QuestDefinition register = manager.GetDefinition("arrival_register");
		ctx.True(register != null && register.m_bRewardIdCard, "step 1 hands over the ID card");
		if (ctx.FailureCount() > 0)
			return;

		IEntity newcomer = EL_ExploitGuardsWorldFixture.SpawnCharacter(ctx, "134 0 242.5", true);
		IEntity returning = EL_ExploitGuardsWorldFixture.SpawnCharacter(ctx, "134 0 242.5", true);
		if (!newcomer || !returning)
			return;

		RunChain(ctx, manager, newcomer);
		RunReturningPlayer(ctx, returning);
		RunIdCardOnce(ctx, newcomer);

		SCR_EntityHelper.DeleteEntityAndChildren(newcomer);
		SCR_EntityHelper.DeleteEntityAndChildren(returning);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunChain(EL_TestContext ctx, LF_QuestManager manager, IEntity player)
	{
		LF_QuestTrackerComponent tracker = LF_QuestTrackerComponent.Cast(player.FindComponent(LF_QuestTrackerComponent));
		ctx.True(tracker != null, "the newcomer carries a quest tracker");
		if (!tracker)
			return;

		// Step 1: one talk with the clerk registers the player and hands the card over.
		ctx.True(manager.GetGiver("clerk") != null && manager.GetGiver("clerk").IsConverse(), "the clerk talks in person, no quest menu");
		ctx.True(tracker.RequestConverse("clerk"), "talking to the clerk moves the chain");
		ctx.True(Status(tracker, "arrival_register") == LF_EQuestStatus.TURNED_IN, "one talk registers the player (step 1 done)");
		ctx.True(Status(tracker, "arrival_board") == LF_EQuestStatus.ACTIVE, "step 2 starts on its own");
		ctx.True(Status(tracker, "arrival_work") == LF_EQuestStatus.AVAILABLE, "step 3 waits for step 2");

		// Another sign, a sign that is not there, and other signals do not count for step 2.
		ctx.False(tracker.RequestSignRead("no_such_sign"), "a sign id with no sign beside the reader does not count");
		ctx.False(tracker.RequestSignRead("post_office"), "the post office sign is not beside the clerk, so its read does not count");
		ctx.False(manager.ReportSignal(player, LF_QuestSignals.SignRead("post_office")), "reading the post office sign does not move step 2");
		ctx.False(manager.ReportSignal(player, LF_QuestSignals.ATM_DEPOSIT), "an early deposit does not move step 2");
		ctx.True(Status(tracker, "arrival_board") == LF_EQuestStatus.ACTIVE, "step 2 still runs");
		ctx.False(tracker.RequestConverse("clerk"), "talking to the clerk mid-errand only repeats the errand");
		ctx.True(Status(tracker, "arrival_board") == LF_EQuestStatus.ACTIVE, "and leaves step 2 running");

		// Step 2 -> 3 -> 4 -> 5, each on its own signal, in order, with no trip back to the clerk.
		// Signs.layer: Sign_TownMap stands at 130.5 0 246.5, about 5 m from the clerk.
		ctx.True(tracker.RequestSignRead("town_map"), "reading the town board beside the clerk counts");
		ctx.True(Status(tracker, "arrival_board") == LF_EQuestStatus.TURNED_IN, "step 2 turns in on its own");
		ctx.True(Status(tracker, "arrival_work") == LF_EQuestStatus.ACTIVE, "step 3 starts on its own");

		ctx.False(manager.ReportSignal(player, LF_QuestSignals.ATM_DEPOSIT), "a deposit before the parcel run does not move step 3");
		ctx.False(manager.ReportSignal(player, LF_QuestSignals.JobRunComplete(EL_EJobType.TAXI)), "a taxi fare is not the parcel run");
		ctx.True(manager.ReportSignal(player, LF_QuestSignals.JobRunComplete(EL_EJobType.PARCEL)), "a paid parcel run moves step 3");
		ctx.True(Status(tracker, "arrival_work") == LF_EQuestStatus.TURNED_IN, "step 3 turns in on its own");
		ctx.True(Status(tracker, "arrival_bank") == LF_EQuestStatus.ACTIVE, "step 4 starts on its own");

		ctx.True(manager.ReportSignal(player, LF_QuestSignals.ATM_DEPOSIT), "a deposit moves step 4");
		ctx.True(Status(tracker, "arrival_bank") == LF_EQuestStatus.TURNED_IN, "step 4 turns in on its own");
		ctx.True(Status(tracker, "arrival_done") == LF_EQuestStatus.ACTIVE, "step 5 starts on its own");

		// Step 5: the clerk, in person.
		ctx.False(tracker.RequestTurnIn("arrival_done"), "step 5 is not done before the player is back");
		int cashBefore = EL_MoneyUtils.GetCash(player);
		ctx.True(tracker.RequestConverse("clerk"), "back at the clerk, the talk finishes step 5");
		ctx.True(Status(tracker, "arrival_done") == LF_EQuestStatus.TURNED_IN, "the clerk takes step 5 back");
		ctx.False(tracker.RequestConverse("clerk"), "after the chain the clerk has nothing more to hand out");
		ctx.Equal(cashBefore + manager.GetDefinition("arrival_done").m_iRewardMoney, EL_MoneyUtils.GetCash(player), "the welcome cash lands in the pockets");
		ctx.True(manager.GetDefinition("arrival_done").m_iRewardMoney > 0, "the clerk gives a welcome");

		// After step 5 nothing else is pushed.
		foreach (string questId, LF_QuestState state : tracker.GetTracker().GetStates())
		{
			ctx.True(state.GetStatus() == LF_EQuestStatus.TURNED_IN, "nothing runs after the chain: " + questId);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! A returning player's turn-ins come back from progression (M1-10): the clerk offers nothing.
	protected void RunReturningPlayer(EL_TestContext ctx, IEntity player)
	{
		LF_QuestTrackerComponent tracker = LF_QuestTrackerComponent.Cast(player.FindComponent(LF_QuestTrackerComponent));
		if (!tracker)
		{
			ctx.Fail("the returning player carries no quest tracker");
			return;
		}

		array<string> chain = {"arrival_register", "arrival_board", "arrival_work", "arrival_bank", "arrival_done"};
		ctx.Equal(5, tracker.RestoreTurnedIn(chain), "the five turn-ins come back");

		LF_QuestManager manager = LF_QuestManager.GetInstance();
		foreach (string questId : chain)
		{
			ctx.True(Status(tracker, questId) == LF_EQuestStatus.TURNED_IN, "restored as turned in: " + questId);
			ctx.False(manager.GetDefinition(questId).IsListed(Status(tracker, questId), true), "the clerk does not list " + questId + " again");
		}

		ctx.False(tracker.RequestAccept("arrival_register"), "the clerk does not register a returning player twice");
		ctx.False(tracker.RequestConverse("clerk"), "talking to the clerk offers a returning player nothing");

		// A player who left after step 2 picks up at step 3, not at step 1.
		IEntity midway = EL_ExploitGuardsWorldFixture.SpawnCharacter(ctx, "60 0 60", false);
		if (!midway)
			return;

		LF_QuestTrackerComponent midwayTracker = LF_QuestTrackerComponent.Cast(midway.FindComponent(LF_QuestTrackerComponent));
		if (midwayTracker)
		{
			array<string> firstTwo = {"arrival_register", "arrival_board"};
			midwayTracker.RestoreTurnedIn(firstTwo);
			ctx.True(Status(midwayTracker, "arrival_work") == LF_EQuestStatus.ACTIVE, "a player back mid-chain gets step 3 again");
			ctx.True(Status(midwayTracker, "arrival_bank") == LF_EQuestStatus.AVAILABLE, "and not step 4");
		}

		SCR_EntityHelper.DeleteEntityAndChildren(midway);
	}

	//------------------------------------------------------------------------------------------------
	//! The clerk hands the registered card over once; a second hand-over adds nothing.
	protected void RunIdCardOnce(EL_TestContext ctx, IEntity player)
	{
		array<LF_IdCardComponent> cards = {};
		LF_IdCard.FindCarried(player, cards);
		ctx.Equal(0, cards.Count(), "a test body with no account was handed no card");

		EL_PlayerAccount account = EL_PlayerAccount.Create("quests-arrival-chain");
		account.AddCharacter(EL_PlayerCharacter.Create("Prefabs/Characters/Core/Character_Roleplay.et", "Ada", "Clerk", 30), true);

		ctx.True(LF_QuestRewards.HandIdCard(player, account), "the clerk hands the registered card over");
		ctx.False(LF_QuestRewards.HandIdCard(player, account), "a second hand-over is refused");
		LF_IdCard.FindCarried(player, cards);
		ctx.Equal(1, cards.Count(), "the player carries exactly one card");

		LF_IdCardComponent shown = LF_IdCard.FindShown(player, account.GetActiveCharacter().GetId());
		ctx.True(shown != null && shown.GetFirstName() == "Ada", "the card carries the registered name");

		ctx.False(LF_QuestRewards.HandIdCard(player, null), "no account, no card");
	}

	//------------------------------------------------------------------------------------------------
	protected LF_EQuestStatus Status(LF_QuestTrackerComponent tracker, string questId)
	{
		return tracker.GetTracker().GetStatus(questId);
	}
}
