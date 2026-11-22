// red-proof: 2026-11-22 `tools/cli.mjs test --no-build --red LF_Test_NextActivities,LF_Test_NextActivitiesWorld (with the pack test)`: RED-OK activities/next-activities-world (fails with the hook on: RedProof() skips Start, 34 assertions run).
//! Records the waypoints LF_ActivityGuide sends: "mark:<uid>:<id>:<label>" and "clear:<uid>:<id>".
class LF_ActivityTestWaypoints : LF_JobWaypointSink
{
	ref array<string> m_aSent = {};

	override void Mark(string uid, string id, string label, int x, int z)
	{
		m_aSent.Insert(string.Format("mark:%1:%2:%3", uid, id, label));
	}

	override void Clear(string uid, string id)
	{
		m_aSent.Insert(string.Format("clear:%1:%2", uid, id));
	}

	string Last()
	{
		if (m_aSent.IsEmpty())
			return string.Empty;

		return m_aSent[m_aSent.Count() - 1];
	}
}

//------------------------------------------------------------------------------------------------
//! What to do next, on a real character and the real quest system: "Start" begins an activity once;
//! a step whose check already holds (the item carried) is passed at once and the next step starts on
//! its own; a step whose check does not hold waits until it does; an objective step finishes on its
//! event; each step moves the waypoint to its own target and the end clears it; a done activity is not
//! started again. The table is built in script; the ATM target comes from the test world.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_NextActivitiesWorld : LF_Test
{
	static const string UID = "next-activities-world-uid";
	static const ResourceName APPLE = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";
	static const ResourceName WATER = "{A231A5F8D479B5DC}Prefabs/Items/Drinks/WaterBottle.et";
	static const string PING = "activity.test.ping";

	override string GetName()
	{
		return "activities/next-activities-world";
	}

	//------------------------------------------------------------------------------------------------
	static LF_ActivityStep Step(string id, LF_EActivityCheck check, string param, string targetKind)
	{
		LF_ActivityStep step = new LF_ActivityStep();
		step.m_sStepId = id;
		step.m_sTitleKey = "#LF-Activity_Start";
		step.m_sNoteKey = "#LF-Activity_Started";
		step.m_sTargetLabelKey = "label_" + id;
		step.m_eCheck = check;
		step.m_sCheckParam = param;
		step.m_iCheckCount = 1;
		step.m_sTargetKind = targetKind;
		step.m_fArriveRadius = 30;
		step.m_aObjectives = {};
		return step;
	}

	//------------------------------------------------------------------------------------------------
	static LF_ActivityConfig Table()
	{
		LF_ActivityConfig config = new LF_ActivityConfig();
		config.m_aActivities = {};
		LF_ActivityDef def = LF_ActivityDef.Make("worldtest");
		def.m_sNameKey = "#LF-Activity_Tab";
		def.m_sPitchKey = "#LF-Activity_Summary";
		def.m_sPlaceKind = "ATM";
		def.m_aSteps.Insert(Step("apple", LF_EActivityCheck.ITEM, APPLE, "ATM"));
		def.m_aSteps.Insert(Step("water", LF_EActivityCheck.ITEM, WATER, "ATM"));
		LF_ActivityStep ping = Step("ping", LF_EActivityCheck.NONE, string.Empty, "ATM");
		LF_QuestObjective signal = new LF_QuestObjective();
		signal.m_eType = LF_EQuestEventType.SIGNAL;
		signal.m_iTarget = 1;
		signal.m_sFilter = PING;
		ping.m_aObjectives.Insert(signal);
		def.m_aSteps.Insert(ping);
		config.m_aActivities.Insert(def);
		return config;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity body = LF_TestCharacterFixture.Spawn(ctx);
		if (!body)
			return;

		LF_QuestTrackerComponent tracker = LF_QuestTrackerComponent.Cast(body.FindComponent(LF_QuestTrackerComponent));
		ctx.NotNull(tracker, "the fixture carries a quest tracker");
		if (tracker)
		{
			LF_Utils.SetUidForTests(body, UID);
			tracker.SetUidForTests(UID);
			LF_MapShares.ResetStatics();
			LF_ActivityGuide.ResetStatics();
			LF_ActivityTestWaypoints waypoints = new LF_ActivityTestWaypoints();
			LF_ActivityGuide.SetSink(waypoints);
			LF_ActivityTable.SetForTests(Table());
			ctx.Equal(3, LF_ActivityTable.RegisterQuests(), "the three steps are quests the quest system knows");
			RunActivity(ctx, body, tracker, waypoints);
		}

		LF_ActivityGuide.ResetStatics();
		LF_ActivityTable.ResetStatics();
		LF_MapShares.ResetStatics();
		LF_MapShares.EnsureStarted();
		LF_Utils.SetUidForTests(null, string.Empty);
		LF_TestCharacterFixture.Delete(body);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunActivity(LF_TestContext ctx, IEntity body, LF_QuestTrackerComponent tracker, LF_ActivityTestWaypoints waypoints)
	{
		string apple = LF_ActivityTable.QuestIdOf("worldtest", "apple");
		string water = LF_ActivityTable.QuestIdOf("worldtest", "water");
		string ping = LF_ActivityTable.QuestIdOf("worldtest", "ping");
		LF_QuestTracker states = tracker.GetTracker();

		// The list: one row, available, with a distance to the test world's ATM.
		LF_ActivityConfig table = LF_ActivityTable.Get();
		LF_ActivityData before = LF_ActivityData.Build(table.m_aActivities, new LF_ActivityLiveFacts(body), body.GetOrigin(), true);
		ctx.Equal(1, before.m_aRows.Count(), "the activity is listed");
		if (!before.m_aRows.IsEmpty())
		{
			ctx.Equal(LF_EActivityState.AVAILABLE, before.m_aRows[0].m_eState, "it is available");
			ctx.True(before.m_aRows[0].m_iDistance >= 0, "its distance is measured to a placed ATM");
		}

		// The apple is already carried: Start passes step one at once and step two starts on its own.
		ctx.True(LF_InventoryUtils.AddAmount(body, APPLE, 1) >= 1, "the character carries an apple");
		string outcome = "skipped";
		if (!RedProof())
			outcome = LF_ActivityGuide.Start(body, "worldtest");

		ctx.EqualStr(string.Empty, outcome, "Start begins the activity");
		ctx.Equal(LF_EQuestStatus.TURNED_IN, states.GetStatus(apple), "a step whose check already holds is passed at once");
		ctx.Equal(LF_EQuestStatus.ACTIVE, states.GetStatus(water), "the next step starts on its own");
		ctx.Equal(LF_EQuestStatus.AVAILABLE, states.GetStatus(ping), "the step after it has not started");
		ctx.EqualStr("mark:" + UID + ":" + LF_ActivityGuide.WAYPOINT_ID + ":label_water", waypoints.Last(), "the waypoint is on step two's target");
		ctx.True(LF_MapShares.Has(LF_ActivityGuide.KEY_PREFIX + UID), "the character's own map mark stands");
		ctx.EqualStr("running", LF_ActivityGuide.Start(body, "worldtest"), "Start does not begin it twice");

		// Step two waits for its item.
		ctx.Equal(0, LF_ActivityGuide.CheckBody(body), "without water the step waits");
		ctx.Equal(LF_EQuestStatus.ACTIVE, states.GetStatus(water), "step two still runs");
		ctx.True(LF_InventoryUtils.AddAmount(body, WATER, 1) >= 1, "the character picks up water");
		ctx.Equal(1, LF_ActivityGuide.CheckBody(body), "with water the step passes");
		ctx.Equal(LF_EQuestStatus.TURNED_IN, states.GetStatus(water), "step two is done");
		ctx.Equal(LF_EQuestStatus.ACTIVE, states.GetStatus(ping), "the objective step starts");
		ctx.EqualStr("mark:" + UID + ":" + LF_ActivityGuide.WAYPOINT_ID + ":label_ping", waypoints.Last(), "the waypoint moved to step three");

		LF_ActivityData during = LF_ActivityData.Build(table.m_aActivities, new LF_ActivityLiveFacts(body), body.GetOrigin(), true);
		if (!during.m_aRows.IsEmpty())
		{
			ctx.Equal(LF_EActivityState.ACTIVE, during.m_aRows[0].m_eState, "the list shows it running");
			ctx.Equal(2, during.m_aRows[0].m_iStep, "on step three");
		}

		ctx.True(LF_ActivityGuide.Focus(body, "worldtest"), "Guide me puts the waypoint back on the running step");

		// The objective step finishes on its event; the activity is done and the waypoint clears.
		LF_QuestManager.GetInstance().ReportSignal(body, PING);
		ctx.Equal(LF_EQuestStatus.TURNED_IN, states.GetStatus(ping), "the objective step is done on its event");
		ctx.EqualStr("clear:" + UID + ":" + LF_ActivityGuide.WAYPOINT_ID, waypoints.Last(), "the waypoint clears at the end");
		ctx.False(LF_MapShares.Has(LF_ActivityGuide.KEY_PREFIX + UID), "the map mark goes at the end");
		ctx.EqualStr("done", LF_ActivityGuide.Start(body, "worldtest"), "a done activity is not started again");
		ctx.EqualStr("unknown", LF_ActivityGuide.Start(body, "no-such-activity"), "an unknown id is refused");
	}
}
