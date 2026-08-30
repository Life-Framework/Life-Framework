// red-proof: first run the collect regression scenario went red for a real
// reason - the tracker skipped non-ACTIVE quests, so a lower held-count report
// on a COMPLETED quest was ignored and the quest never reverted to ACTIVE. To
// re-observe failure, drop the FIA kill target (2 -> 3) or comment out the
// COMPLETED guard in LF_QuestTracker.ApplyEvent, then run the fast tier.

// tier: LOGIC
class LF_Test_QuestTracker : EL_Test
{
	override string GetName()
	{
		return "quests/tracker-lifecycle";
	}

	override void Run(EL_TestContext ctx)
	{
		TestLifecycle(ctx);
		TestPrerequisites(ctx);
		TestCollectSet(ctx);
		TestTimedExpiry(ctx);
		TestCooldown(ctx);
		TestZeroObjective(ctx);
	}

	//------------------------------------------------------------------------------------------------
	//! Accept -> progress -> complete -> turn in; wrong-filter kills are ignored.
	void TestLifecycle(EL_TestContext ctx)
	{
		LF_QuestDefinition def = MakeKillQuest("q_kill", 2, "FIA", false, 0);
		LF_QuestTracker tracker = new LF_QuestTracker();
		map<string, ref LF_QuestDefinition> defs = MakeDefs(def);
		array<string> completed = {};

		ctx.True(tracker.Accept(def, 1, EL_EJobType.UNEMPLOYED, "", 0, completed), "unrestricted quest accepts");
		ctx.True(tracker.GetStatus("q_kill") == LF_EQuestStatus.ACTIVE, "accepted quest is ACTIVE");

		tracker.ApplyEvent(MakeEvent(LF_EQuestEventType.KILL, "USSR", 1), defs);
		ctx.Equal(0, tracker.GetState("q_kill").GetObjectiveProgress(0), "wrong-faction kill is ignored");

		tracker.ApplyEvent(MakeEvent(LF_EQuestEventType.KILL, "FIA", 1), defs);
		ctx.Equal(1, tracker.GetState("q_kill").GetObjectiveProgress(0), "first matching kill advances to 1");

		tracker.ApplyEvent(MakeEvent(LF_EQuestEventType.KILL, "FIA", 1), defs);
		ctx.True(tracker.GetStatus("q_kill") == LF_EQuestStatus.COMPLETED, "target reached completes the quest");

		ctx.True(tracker.TurnIn("q_kill", 100), "completed quest turns in");
		ctx.True(tracker.GetStatus("q_kill") == LF_EQuestStatus.TURNED_IN, "turn-in moves quest to TURNED_IN");
		ctx.False(tracker.TurnIn("q_kill", 101), "second turn-in is refused");
		ctx.False(tracker.CanAccept(def, 1, EL_EJobType.UNEMPLOYED, "", 101, completed), "non-repeatable quest cannot re-accept");
	}

	//------------------------------------------------------------------------------------------------
	//! Level, job, faction and previous-quest prerequisites gate acceptance.
	void TestPrerequisites(EL_TestContext ctx)
	{
		LF_QuestTracker tracker = new LF_QuestTracker();
		array<string> completed = {};

		LF_QuestDefinition lvl = MakeKillQuest("q_lvl", 1, "", false, 0);
		lvl.m_iMinLevel = 5;
		ctx.False(tracker.Accept(lvl, 3, EL_EJobType.UNEMPLOYED, "", 0, completed), "level below minimum rejects");
		ctx.True(tracker.Accept(lvl, 5, EL_EJobType.UNEMPLOYED, "", 0, completed), "level at minimum accepts");

		LF_QuestTracker jobTracker = new LF_QuestTracker();
		LF_QuestDefinition job = MakeKillQuest("q_job", 1, "", false, 0);
		job.m_eRequiredJob = EL_EJobType.MINER;
		ctx.False(jobTracker.Accept(job, 1, EL_EJobType.FARMER, "", 0, completed), "wrong job rejects");
		ctx.True(jobTracker.Accept(job, 1, EL_EJobType.MINER, "", 0, completed), "matching job accepts");

		LF_QuestTracker factionTracker = new LF_QuestTracker();
		LF_QuestDefinition faction = MakeKillQuest("q_faction", 1, "", false, 0);
		faction.m_sRequiredFaction = "US";
		ctx.False(factionTracker.Accept(faction, 1, EL_EJobType.UNEMPLOYED, "FIA", 0, completed), "wrong faction rejects");
		ctx.True(factionTracker.Accept(faction, 1, EL_EJobType.UNEMPLOYED, "US", 0, completed), "matching faction accepts");

		LF_QuestTracker chainTracker = new LF_QuestTracker();
		LF_QuestDefinition intro = MakeKillQuest("q_intro", 1, "", false, 0);
		LF_QuestDefinition chained = MakeKillQuest("q_chained", 1, "", false, 0);
		chained.m_sRequiredPreviousQuestId = "q_intro";
		map<string, ref LF_QuestDefinition> defs = MakeDefs(intro);
		defs.Set("q_chained", chained);

		ctx.False(chainTracker.Accept(chained, 1, EL_EJobType.UNEMPLOYED, "", 0, completed), "quest gated on unturned-in previous quest rejects");

		chainTracker.Accept(intro, 1, EL_EJobType.UNEMPLOYED, "", 0, completed);
		chainTracker.ApplyEvent(MakeEvent(LF_EQuestEventType.KILL, "", 1), defs);
		chainTracker.TurnIn("q_intro", 10);
		completed = chainTracker.GetCompletedQuestIds();
		ctx.True(chainTracker.Accept(chained, 1, EL_EJobType.UNEMPLOYED, "", 11, completed), "chained quest accepts after previous is turned in");
	}

	//------------------------------------------------------------------------------------------------
	//! Collect objectives SET progress from a report and regress when the player
	//! drops items before turning in.
	void TestCollectSet(EL_TestContext ctx)
	{
		LF_QuestDefinition def = MakeKillQuest("q_collect", 1, "", false, 0);
		def.m_aObjectives.Clear();
		LF_QuestObjective collect = new LF_QuestObjective();
		collect.m_eType = LF_EQuestEventType.COLLECT_REPORT;
		collect.m_eMode = LF_EQuestProgressMode.SET;
		collect.m_iTarget = 3;
		collect.m_sFilter = "";
		def.m_aObjectives.Insert(collect);

		LF_QuestTracker tracker = new LF_QuestTracker();
		map<string, ref LF_QuestDefinition> defs = MakeDefs(def);
		array<string> completed = {};
		tracker.Accept(def, 1, EL_EJobType.UNEMPLOYED, "", 0, completed);

		tracker.ApplyEvent(MakeEvent(LF_EQuestEventType.COLLECT_REPORT, "", 2), defs);
		ctx.Equal(2, tracker.GetState("q_collect").GetObjectiveProgress(0), "report sets progress to held count");
		ctx.True(tracker.GetStatus("q_collect") == LF_EQuestStatus.ACTIVE, "below target stays ACTIVE");

		tracker.ApplyEvent(MakeEvent(LF_EQuestEventType.COLLECT_REPORT, "", 3), defs);
		ctx.True(tracker.GetStatus("q_collect") == LF_EQuestStatus.COMPLETED, "held count at target completes the quest");

		tracker.ApplyEvent(MakeEvent(LF_EQuestEventType.COLLECT_REPORT, "", 1), defs);
		ctx.Equal(1, tracker.GetState("q_collect").GetObjectiveProgress(0), "dropping items regresses progress");
		ctx.True(tracker.GetStatus("q_collect") == LF_EQuestStatus.ACTIVE, "regressed collect quest reverts to ACTIVE");

		tracker.ApplyEvent(MakeEvent(LF_EQuestEventType.COLLECT_REPORT, "", 3), defs);
		ctx.True(tracker.GetStatus("q_collect") == LF_EQuestStatus.COMPLETED, "re-acquired count completes again");
	}

	//------------------------------------------------------------------------------------------------
	//! A quest time limit expires the quest to FAILED; repeatable quests can then
	//! be accepted again, but a FAILED quest cannot be turned in.
	void TestTimedExpiry(EL_TestContext ctx)
	{
		LF_QuestDefinition def = MakeKillQuest("q_timed", 1, "", true, 0);
		def.m_fTimeLimitSeconds = 100;

		LF_QuestTracker tracker = new LF_QuestTracker();
		map<string, ref LF_QuestDefinition> defs = MakeDefs(def);
		array<string> completed = {};
		ctx.True(tracker.Accept(def, 1, EL_EJobType.UNEMPLOYED, "", 0, completed), "timed quest accepts");

		tracker.Tick(50);
		ctx.True(tracker.GetStatus("q_timed") == LF_EQuestStatus.ACTIVE, "before the deadline the quest stays ACTIVE");

		tracker.Tick(100);
		ctx.True(tracker.GetStatus("q_timed") == LF_EQuestStatus.FAILED, "at the deadline the quest expires to FAILED");
		ctx.False(tracker.TurnIn("q_timed", 101), "expired quest cannot turn in");

		ctx.True(tracker.CanAccept(def, 1, EL_EJobType.UNEMPLOYED, "", 101, completed), "repeatable expired quest can re-accept");
		ctx.True(tracker.Accept(def, 1, EL_EJobType.UNEMPLOYED, "", 101, completed), "repeatable expired quest re-accepts");
		ctx.True(tracker.GetStatus("q_timed") == LF_EQuestStatus.ACTIVE, "re-accepted quest restarts ACTIVE");
	}

	//------------------------------------------------------------------------------------------------
	//! Cooldown blocks re-accept until it elapses; non-repeatable stays closed.
	void TestCooldown(EL_TestContext ctx)
	{
		LF_QuestDefinition def = MakeKillQuest("q_cd", 1, "", true, 60);
		LF_QuestTracker tracker = new LF_QuestTracker();
		map<string, ref LF_QuestDefinition> defs = MakeDefs(def);
		array<string> completed = {};

		tracker.Accept(def, 1, EL_EJobType.UNEMPLOYED, "", 0, completed);
		tracker.ApplyEvent(MakeEvent(LF_EQuestEventType.KILL, "", 1), defs);
		tracker.TurnIn("q_cd", 100);

		ctx.False(tracker.CanAccept(def, 1, EL_EJobType.UNEMPLOYED, "", 120, completed), "cooldown still running rejects re-accept");
		ctx.True(tracker.CanAccept(def, 1, EL_EJobType.UNEMPLOYED, "", 160, completed), "cooldown elapsed allows re-accept");
		ctx.True(tracker.Accept(def, 1, EL_EJobType.UNEMPLOYED, "", 160, completed), "re-accept after cooldown succeeds");
		ctx.True(tracker.GetStatus("q_cd") == LF_EQuestStatus.ACTIVE, "re-accepted quest restarts ACTIVE");
	}

	//------------------------------------------------------------------------------------------------
	//! A quest with no objectives must never auto-complete and never turn in:
	//! misconfiguration degrades to an uncompletable quest, not a free reward.
	void TestZeroObjective(EL_TestContext ctx)
	{
		LF_QuestDefinition def = new LF_QuestDefinition();
		def.m_sQuestId = "q_empty";
		LF_QuestTracker tracker = new LF_QuestTracker();
		map<string, ref LF_QuestDefinition> defs = MakeDefs(def);
		array<string> completed = {};

		ctx.True(tracker.Accept(def, 1, EL_EJobType.UNEMPLOYED, "", 0, completed), "empty quest accepts (world layer logs the misconfig)");
		tracker.ApplyEvent(MakeEvent(LF_EQuestEventType.KILL, "", 1), defs);
		ctx.True(tracker.GetStatus("q_empty") == LF_EQuestStatus.ACTIVE, "empty quest never auto-completes");
		ctx.False(tracker.TurnIn("q_empty", 10), "empty quest cannot turn in");
	}

	//------------------------------------------------------------------------------------------------
	LF_QuestDefinition MakeKillQuest(string id, int target, string filter, bool repeatable, float cooldown)
	{
		LF_QuestDefinition def = new LF_QuestDefinition();
		def.m_sQuestId = id;
		def.m_bRepeatable = repeatable;
		def.m_fCooldownSeconds = cooldown;

		LF_QuestObjective obj = new LF_QuestObjective();
		obj.m_eType = LF_EQuestEventType.KILL;
		obj.m_eMode = LF_EQuestProgressMode.ADD;
		obj.m_iTarget = target;
		obj.m_sFilter = filter;

		def.m_aObjectives = new array<ref LF_QuestObjective>();
		def.m_aObjectives.Insert(obj);
		return def;
	}

	//------------------------------------------------------------------------------------------------
	map<string, ref LF_QuestDefinition> MakeDefs(LF_QuestDefinition def)
	{
		map<string, ref LF_QuestDefinition> defs = new map<string, ref LF_QuestDefinition>();
		defs.Set(def.m_sQuestId, def);
		return defs;
	}

	//------------------------------------------------------------------------------------------------
	LF_QuestEvent MakeEvent(LF_EQuestEventType type, string filter, int value)
	{
		LF_QuestEvent ev = new LF_QuestEvent();
		ev.m_eType = type;
		ev.m_sFilter = filter;
		ev.m_iValue = value;
		return ev;
	}
};