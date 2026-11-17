// red-proof: 2026-11-18, `tools\cli test --no-build --only LF_Test_QuestTrackerIdle` on unfixed main: the tracker
// of a fresh body with no quest still carried the FRAME event (expected 0, got 8) and a turned-in quest left it on.

// tier: WORLD
//! Quest tracker idle cost (#2537): the tracker's timed check (expiry, area entry) is an EOnFrame on every body on
//! the server. It runs only while the body holds a quest that can expire or fire an area, starts with the accept and
//! stops with the last quest's turn-in.
class LF_Test_QuestTrackerIdle : LF_Test
{
	protected static const string QUEST_ID = "idle_tracker_quest";
	protected static const string GIVER_ID = "idle_tracker_giver";
	protected static const string UID = "idle-tracker-uid";

	override string GetName()
	{
		return "quests/tracker-idle";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_QuestManager manager = LF_QuestManager.GetInstance();
		LF_QuestObjective objective = new LF_QuestObjective();
		objective.m_eType = LF_EQuestEventType.TALK;
		objective.m_sFilter = GIVER_ID;
		objective.m_iTarget = 1;

		LF_QuestDefinition def = new LF_QuestDefinition();
		def.m_sQuestId = QUEST_ID;
		def.m_aObjectives = {};
		def.m_aObjectives.Insert(objective);
		manager.RegisterDefinition(def);

		IEntity body = LF_ExploitGuardsWorldFixture.SpawnCharacter(ctx, "105 0 105", false);
		if (!body)
			return;

		LF_QuestTrackerComponent tracker = LF_Component<LF_QuestTrackerComponent>.Find(body);
		ctx.NotNull(tracker, "the test character carries the quest tracker");
		if (!tracker)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(body);
			return;
		}

		tracker.SetUidForTests(UID);

		if (RedProof())
			tracker.SetEventMask(body, EntityEvent.FRAME);
		ctx.Equal(0, tracker.GetEventMask() & EntityEvent.FRAME, "a body with no quest has no frame event");

		bool accepted = tracker.AcceptQuest(QUEST_ID);
		ctx.True(accepted, "the quest is accepted");
		ctx.True(tracker.IsTicking(), "with an accepted quest the timed check runs");

		manager.ReportTalk(body, GIVER_ID);
		ctx.True(tracker.IsTicking(), "a completed quest still waits for its turn-in: the check keeps running");

		ctx.True(tracker.TurnInQuest(QUEST_ID), "the quest is turned in");
		if (RedProof())
			tracker.SetEventMask(body, EntityEvent.FRAME);
		ctx.False(tracker.IsTicking(), "with the last quest turned in the timed check stops");
		ctx.Equal(0, tracker.GetEventMask() & EntityEvent.FRAME, "the frame event is gone");

		LF_Progression.Forget(UID);
		SCR_EntityHelper.DeleteEntityAndChildren(body);
	}
}
