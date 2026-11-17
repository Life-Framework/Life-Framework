// red-proof: 2026-11-17, `tools\cli test --no-build --red LF_Test_QuestTurnInGuards` (RedProof() gives the body a uid and
// leaves it alive, so the old behaviour shows): the uid-less turn-in pays and the dead character still reads alive.

// tier: WORLD
//! Quest turn-in guards (#2403): a body no player controls has no uid to save the turn-in under, so the
//! turn-in is refused and nothing is paid (it used to pay and be forgotten at the next respawn). With a uid the
//! turn-in is recorded and paid once, and a dead or downed character is not alive for a talk or a hand-in.
class LF_Test_QuestTurnInGuards : LF_Test
{
	protected static const string QUEST_ID = "hardening_turnin";
	protected static const string UID = "hardening-turnin-uid";

	override string GetName()
	{
		return "quests/turn-in-guards";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_QuestManager manager = LF_QuestManager.GetInstance();
		LF_QuestObjective objective = new LF_QuestObjective();
		objective.m_eType = LF_EQuestEventType.TALK;
		objective.m_sFilter = "hardening_giver";
		objective.m_iTarget = 1;

		LF_QuestDefinition def = new LF_QuestDefinition();
		def.m_sQuestId = QUEST_ID;
		def.m_iRewardMoney = 50;
		def.m_aObjectives = {};
		def.m_aObjectives.Insert(objective);
		manager.RegisterDefinition(def);

		IEntity body = LF_ExploitGuardsWorldFixture.SpawnCharacter(ctx, "100 0 100", true);
		if (!body)
			return;

		LF_QuestTrackerComponent tracker = LF_Component<LF_QuestTrackerComponent>.Find(body);
		ctx.NotNull(tracker, "the test character carries the quest tracker");
		if (!tracker)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(body);
			return;
		}

		RunNoUid(ctx, manager, body, tracker);
		RunWithUid(ctx, body, tracker);
		RunDeadBody(ctx, body);

		LF_Progression.Forget(UID);
		SCR_EntityHelper.DeleteEntityAndChildren(body);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunNoUid(LF_TestContext ctx, LF_QuestManager manager, IEntity body, LF_QuestTrackerComponent tracker)
	{
		// No player is behind the body: no uid.
		tracker.SetUidForTests("");
		if (RedProof())
			tracker.SetUidForTests(UID);

		ctx.True(tracker.AcceptQuest(QUEST_ID), "the quest is accepted");
		manager.ReportTalk(body, "hardening_giver");
		ctx.True(tracker.GetTracker().GetStatus(QUEST_ID) == LF_EQuestStatus.COMPLETED, "the talk completes it");

		int cash = LF_MoneyUtils.GetCash(body);
		ctx.False(tracker.TurnInQuest(QUEST_ID), "a body with no uid cannot turn in");
		ctx.True(tracker.GetTracker().GetStatus(QUEST_ID) == LF_EQuestStatus.COMPLETED, "the quest stays completed, to be handed in properly");
		ctx.Equal(cash, LF_MoneyUtils.GetCash(body), "nothing was paid");
		ctx.False(LF_Progression.HasTurnedIn(UID, QUEST_ID), "nothing was remembered");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunWithUid(LF_TestContext ctx, IEntity body, LF_QuestTrackerComponent tracker)
	{
		tracker.SetUidForTests(UID);
		int cash = LF_MoneyUtils.GetCash(body);
		ctx.True(tracker.TurnInQuest(QUEST_ID), "with a uid the turn-in goes through");
		ctx.True(tracker.GetTracker().GetStatus(QUEST_ID) == LF_EQuestStatus.TURNED_IN, "the quest is turned in");
		ctx.True(LF_Progression.HasTurnedIn(UID, QUEST_ID), "the turn-in is remembered for the next respawn");
		ctx.Equal(cash + 50, LF_MoneyUtils.GetCash(body), "the reward was paid once");

		ctx.False(tracker.TurnInQuest(QUEST_ID), "a turned-in quest cannot be paid again");
		ctx.Equal(cash + 50, LF_MoneyUtils.GetCash(body), "the second try paid nothing");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunDeadBody(LF_TestContext ctx, IEntity body)
	{
		ctx.False(LF_Utils.IsAliveState(null), "no body is not alive");
		ctx.True(LF_Utils.IsAliveState(body), "a standing character is alive");

		SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(body.FindComponent(SCR_CharacterDamageManagerComponent));
		ctx.NotNull(damage, "the character has a damage manager");
		if (!damage)
			return;

		if (!RedProof())
			damage.Kill(Instigator.CreateInstigator(null));

		ctx.False(LF_Utils.IsAliveState(body), "a dead or downed character is not alive for a talk or a hand-in");
	}
}
