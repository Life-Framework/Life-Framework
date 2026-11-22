// red-proof: 2026-11-22 `tools/cli.mjs test --no-build --red LF_Test_NextActivities,LF_Test_NextActivitiesWorld,LF_Test_EveronActivities`: RED-OK activities/next-activities (fails with the hook on: RedProof() skips the filtering, 55 assertions run).
//! Hand-filled facts for the activity rules.
class LF_TestActivityFacts : LF_ActivityFacts
{
	ref map<string, int> m_mLevels = new map<string, int>();
	ref map<string, int> m_mStatus = new map<string, int>();
	ref map<string, int> m_mCarried = new map<string, int>();
	ref map<string, int> m_mXp = new map<string, int>();
	ref array<string> m_aLicences = {};
	string m_sFaction = "CIVILIAN";
	string m_sJob = "UNEMPLOYED";
	bool m_bOnDuty;
	bool m_bCarOut;
	bool m_bCarStored;

	override int Level(string job)
	{
		if (!m_mLevels.Contains(job))
			return 1;

		return m_mLevels.Get(job);
	}

	override bool HasLicence(string licence)
	{
		return m_aLicences.Find(licence) != -1;
	}

	override string Faction()
	{
		return m_sFaction;
	}

	override bool OnDuty()
	{
		return m_bOnDuty;
	}

	override string HeldJob()
	{
		return m_sJob;
	}

	override int Carried(string prefab)
	{
		return m_mCarried.Get(prefab);
	}

	override bool HasVehicleOut()
	{
		return m_bCarOut;
	}

	override bool HasStoredVehicle()
	{
		return m_bCarStored;
	}

	override int Xp(string job)
	{
		return m_mXp.Get(job);
	}

	override LF_EQuestStatus Status(string questId)
	{
		if (!m_mStatus.Contains(questId))
			return LF_EQuestStatus.AVAILABLE;

		return m_mStatus.Get(questId);
	}
}

//------------------------------------------------------------------------------------------------
//! What to do next, with no world: the list shows only what the player can do now, each activity is
//! available, running or done, "Start" begins an activity once, the steps become a chain of quests in
//! order, each check reads the right fact, and the list survives the wire.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_NextActivities : LF_Test
{
	static const string PICK = "{23341E2840F5ACB4}Prefabs/Tools/Pickaxe.et";

	override string GetName()
	{
		return "activities/next-activities";
	}

	//------------------------------------------------------------------------------------------------
	static LF_ActivityStep Step(string id, LF_EActivityCheck check, string param = "", int count = 1)
	{
		LF_ActivityStep step = new LF_ActivityStep();
		step.m_sStepId = id;
		step.m_sTitleKey = "#T_" + id;
		step.m_sNoteKey = "#N_" + id;
		step.m_eCheck = check;
		step.m_sCheckParam = param;
		step.m_iCheckCount = count;
		step.m_fArriveRadius = 30;
		step.m_aObjectives = {};
		if (check == LF_EActivityCheck.ARRIVE)
			step.m_sTargetKind = "job";

		return step;
	}

	//------------------------------------------------------------------------------------------------
	static array<ref LF_ActivityDef> Table()
	{
		array<ref LF_ActivityDef> all = {};
		LF_ActivityDef mining = LF_ActivityDef.Make("mining");
		mining.m_sNameKey = "#Mining";
		mining.m_sPitchKey = "#MiningPitch";
		mining.m_iPayPerHour = 470;
		mining.m_sJob = "MINER";
		mining.m_aSteps.Insert(Step("job", LF_EActivityCheck.JOB, "MINER"));
		mining.m_aSteps.Insert(Step("pickaxe", LF_EActivityCheck.ITEM, PICK));
		mining.m_aSteps.Insert(Step("car", LF_EActivityCheck.VEHICLE));
		mining.m_aSteps.Insert(Step("drive", LF_EActivityCheck.ARRIVE));
		LF_ActivityStep smelt = Step("smelt", LF_EActivityCheck.NONE);
		LF_QuestObjective bar = new LF_QuestObjective();
		bar.m_eType = LF_EQuestEventType.PROCESS;
		bar.m_iTarget = 1;
		bar.m_sFilter = "{BCD1E8CD7F975074}Prefabs/Resources/Mining/IronBar.et";
		smelt.m_aObjectives.Insert(bar);
		mining.m_aSteps.Insert(smelt);
		all.Insert(mining);

		LF_ActivityDef copper = LF_ActivityDef.Make("copper");
		copper.m_sJob = "MINER";
		copper.m_iMinLevel = 3;
		copper.m_aSteps.Insert(Step("go", LF_EActivityCheck.ARRIVE));
		all.Insert(copper);

		LF_ActivityDef police = LF_ActivityDef.Make("police");
		police.m_sFaction = "POLICE";
		police.m_aSteps.Insert(Step("clockin", LF_EActivityCheck.DUTY, "POLICE"));
		all.Insert(police);

		LF_ActivityDef hunting = LF_ActivityDef.Make("hunting");
		hunting.m_sLicence = "HUNTING";
		hunting.m_aSteps.Insert(Step("go", LF_EActivityCheck.ARRIVE));
		all.Insert(hunting);
		return all;
	}

	//------------------------------------------------------------------------------------------------
	static string Ids(notnull array<LF_ActivityDef> defs)
	{
		string ids = string.Empty;
		foreach (LF_ActivityDef def : defs)
		{
			if (!ids.IsEmpty())
				ids += ",";

			ids += def.m_sId;
		}

		return ids;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_ActivityDef> all = Table();
		foreach (LF_ActivityDef checkedDef : all)
		{
			ctx.EqualStr(string.Empty, LF_ActivityConfig.Problem(checkedDef), "the table's activity stands: " + checkedDef.m_sId);
		}

		LF_ActivityDef broken = LF_ActivityDef.Make("broken");
		broken.m_aSteps.Insert(Step("x", LF_EActivityCheck.ITEM));
		ctx.False(LF_ActivityConfig.Problem(broken).IsEmpty(), "an item check with no prefab is refused");
		broken.m_aSteps.Clear();
		broken.m_aSteps.Insert(Step("x", LF_EActivityCheck.NONE));
		ctx.False(LF_ActivityConfig.Problem(broken).IsEmpty(), "a step with no check and no objective is refused");

		// Filtering: a new civilian sees only mining.
		LF_TestActivityFacts facts = new LF_TestActivityFacts();
		array<LF_ActivityDef> listed = {};
		if (!RedProof())
			LF_ActivityRules.Listed(all, facts, listed);

		ctx.EqualStr("mining", Ids(listed), "a new civilian sees only what needs no level, licence or badge");

		facts.m_mLevels.Set("MINER", 3);
		facts.m_aLicences.Insert("HUNTING");
		facts.m_sFaction = "POLICE";
		LF_ActivityRules.Listed(all, facts, listed);
		ctx.EqualStr("mining,copper,police,hunting", Ids(listed), "the level, the licence and the badge open the rest");
		ctx.EqualStr("faction", LF_ActivityRules.Gate(all[2], new LF_TestActivityFacts()), "police duty is shut to a civilian");

		// States and start once.
		LF_ActivityDef mining = all[0];
		string first = LF_ActivityTable.QuestIdOf("mining", "job");
		string second = LF_ActivityTable.QuestIdOf("mining", "pickaxe");
		string last = LF_ActivityTable.QuestIdOf("mining", "smelt");
		ctx.Equal(LF_EActivityState.AVAILABLE, LF_ActivityRules.StateOf(mining, facts), "an untouched activity is available");
		ctx.EqualStr(string.Empty, LF_ActivityRules.StartRefusal(mining, facts), "Start may begin it");
		facts.m_mStatus.Set(first, LF_EQuestStatus.ACTIVE);
		ctx.Equal(LF_EActivityState.ACTIVE, LF_ActivityRules.StateOf(mining, facts), "with its first step running it is active");
		ctx.EqualStr("running", LF_ActivityRules.StartRefusal(mining, facts), "Start does not begin it twice");
		ctx.Equal(0, LF_ActivityRules.RunningStep(mining, facts), "the running step is the first");
		facts.m_mStatus.Set(first, LF_EQuestStatus.TURNED_IN);
		ctx.Equal(LF_EActivityState.ACTIVE, LF_ActivityRules.StateOf(mining, facts), "between steps (a restart before the next settles) it stays active");
		facts.m_mStatus.Set(second, LF_EQuestStatus.ACTIVE);
		ctx.Equal(1, LF_ActivityRules.RunningStep(mining, facts), "the second step runs after the first");
		facts.m_mStatus.Set(last, LF_EQuestStatus.TURNED_IN);
		ctx.Equal(LF_EActivityState.DONE, LF_ActivityRules.StateOf(mining, facts), "the last step done makes it done");
		ctx.EqualStr("done", LF_ActivityRules.StartRefusal(mining, facts), "a done activity is not started again");
		ctx.EqualStr("unknown", LF_ActivityRules.StartRefusal(null, facts), "an unknown id is refused");
		LF_TestActivityFacts civilian = new LF_TestActivityFacts();
		ctx.EqualStr("locked", LF_ActivityRules.StartRefusal(all[2], civilian), "a civilian cannot start police duty");

		// The chain of quests, in order.
		LF_QuestDefinition q0 = LF_ActivityTable.BuildQuest(mining, 0);
		LF_QuestDefinition q1 = LF_ActivityTable.BuildQuest(mining, 1);
		LF_QuestDefinition q4 = LF_ActivityTable.BuildQuest(mining, 4);
		ctx.EqualStr(first, q0.m_sQuestId, "step one's quest id");
		ctx.False(q0.m_bAutoAccept, "step one starts only from Start");
		ctx.True(q0.m_sRequiredPreviousQuestId.IsEmpty(), "step one needs nothing before it");
		ctx.True(q0.m_bAutoTurnIn, "step one turns itself in");
		ctx.True(q1.m_bAutoAccept, "step two starts on its own");
		ctx.EqualStr(first, q1.m_sRequiredPreviousQuestId, "step two needs step one turned in");
		ctx.EqualStr("MINER", q1.m_sJob, "the steps pay XP in the activity's job");
		ctx.Equal(1, q0.GetObjectiveCount(), "a checked step waits on one objective");
		ctx.Equal(LF_EQuestEventType.SIGNAL, q0.GetObjective(0).m_eType, "the checked step's objective is its signal");
		ctx.EqualStr(LF_ActivityTable.SignalOf(first), q0.GetObjective(0).m_sFilter, "the signal names the step's quest");
		ctx.Equal(LF_EQuestEventType.PROCESS, q4.GetObjective(0).m_eType, "an objective step keeps its own objective");
		ctx.True(q0.m_sTargetKind.IsEmpty(), "an activity step is no arrival-chain step (the arrival guide ignores it)");

		// Each check reads its fact.
		LF_TestActivityFacts c = new LF_TestActivityFacts();
		ctx.False(LF_ActivityRules.CheckHolds(mining.m_aSteps[0], c, -1, 0), "no job: the job step waits");
		c.m_sJob = "MINER";
		ctx.True(LF_ActivityRules.CheckHolds(mining.m_aSteps[0], c, -1, 0), "the miner job held passes it");
		ctx.False(LF_ActivityRules.CheckHolds(mining.m_aSteps[1], c, -1, 0), "no pickaxe: the tool step waits");
		c.m_mCarried.Set(PICK, 1);
		ctx.True(LF_ActivityRules.CheckHolds(mining.m_aSteps[1], c, -1, 0), "a pickaxe carried passes it");
		ctx.True(LF_ActivityRules.CheckHolds(mining.m_aSteps[2], c, -1, 0), "no car owned: the car step is passed");
		c.m_bCarStored = true;
		ctx.False(LF_ActivityRules.CheckHolds(mining.m_aSteps[2], c, -1, 0), "a car in the garage: the car step waits for it to come out");
		c.m_bCarOut = true;
		ctx.True(LF_ActivityRules.CheckHolds(mining.m_aSteps[2], c, -1, 0), "a car out passes it");
		ctx.False(LF_ActivityRules.CheckHolds(mining.m_aSteps[3], c, -1, 0), "a target the world does not place is never reached");
		ctx.False(LF_ActivityRules.CheckHolds(mining.m_aSteps[3], c, 45, 0), "45 m off is not there");
		ctx.True(LF_ActivityRules.CheckHolds(mining.m_aSteps[3], c, 12, 0), "12 m off is there");
		LF_ActivityStep duty = all[2].m_aSteps[0];
		c.m_sFaction = "POLICE";
		ctx.False(LF_ActivityRules.CheckHolds(duty, c, -1, 0), "off duty: the clock-in step waits");
		c.m_bOnDuty = true;
		ctx.True(LF_ActivityRules.CheckHolds(duty, c, -1, 0), "on duty passes it");
		LF_ActivityStep xp = Step("fell", LF_EActivityCheck.XP, "LUMBERJACK", 5);
		c.m_mXp.Set("LUMBERJACK", 100);
		ctx.False(LF_ActivityRules.CheckHolds(xp, c, -1, 100), "no XP earned since the step started");
		c.m_mXp.Set("LUMBERJACK", 105);
		ctx.True(LF_ActivityRules.CheckHolds(xp, c, -1, 100), "5 XP earned passes it");

		// The list on the wire.
		LF_ActivityData data = new LF_ActivityData();
		LF_ActivityRow row = new LF_ActivityRow();
		row.m_sId = "mining";
		row.m_sNameKey = "#Mining";
		row.m_sPitchKey = "#MiningPitch";
		row.m_iPayPerHour = 470;
		row.m_iDistance = 350;
		row.m_eState = LF_EActivityState.ACTIVE;
		row.m_iStep = 1;
		row.m_iSteps = 5;
		row.m_sStepTitleKey = "#T_pickaxe";
		data.m_aRows.Insert(row);
		LF_ActivityRow done = new LF_ActivityRow();
		done.m_sId = "parcel";
		done.m_eState = LF_EActivityState.DONE;
		data.m_aRows.Insert(done);
		LF_ActivityData back = LF_ActivityData.Decode(data.Encode());
		ctx.NotNull(back, "the wire decodes");
		if (!back)
			return;

		ctx.Equal(2, back.m_aRows.Count(), "both rows cross the wire");
		ctx.EqualStr("#MiningPitch", back.m_aRows[0].m_sPitchKey, "the pitch crosses");
		ctx.Equal(350, back.m_aRows[0].m_iDistance, "the distance crosses");
		ctx.Equal(LF_EActivityState.ACTIVE, back.m_aRows[0].m_eState, "the state crosses");
		ctx.EqualStr("#T_pickaxe", back.m_aRows[0].m_sStepTitleKey, "the running step crosses");
		ctx.Equal(LF_EActivityState.DONE, back.m_aRows[1].m_eState, "a done row crosses");
		ctx.True(LF_ActivityData.Decode("9;x") == null, "a wire of another version is refused");
	}
}
