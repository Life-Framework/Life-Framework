// red-proof: 2026-11-22 `tools/cli.mjs test --no-build --red LF_Test_FirstHoursGuide`: RED PROOF OK (the hook skips the objective Set and the list build); the first run also failed for real on the sort sitting in Decode instead of Build, then passed once moved.
//! The facts of LF_Test_NextActivities, with papers in hand or not.
class LF_TestPapersFacts : LF_TestActivityFacts
{
	bool m_bRegistered = true;

	override bool Registered()
	{
		return m_bRegistered;
	}
}

//------------------------------------------------------------------------------------------------
//! The first-hours review fixes with no world (C:\LF\lead\review-first2h, rows A14 B1 B2 B6 B12 B13):
//! the objective line keeps its own source and flashes only on a change; the What to do next list puts
//! the run in progress first and done ones last; a newcomer without papers gets no run and a line that
//! sends them to the clerk, while a run already going stays; the moment between two steps keeps the
//! waypoint (no fallback to the arrival mark); a registered character's pending welcome guide is dropped
//! at a rejoin; the licences row names one or two licences; the HUD has an objective part.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_FirstHoursGuide : LF_Test
{
	override string GetName()
	{
		return "quests/first-hours-guide";
	}

	//------------------------------------------------------------------------------------------------
	static LF_ActivityStep Step(string id)
	{
		LF_ActivityStep step = new LF_ActivityStep();
		step.m_sStepId = id;
		step.m_sTitleKey = "#LF-Activity_Start";
		step.m_eCheck = LF_EActivityCheck.ITEM;
		step.m_sCheckParam = "x";
		step.m_aObjectives = {};
		return step;
	}

	//------------------------------------------------------------------------------------------------
	static LF_ActivityDef Def(string id)
	{
		LF_ActivityDef def = LF_ActivityDef.Make(id);
		def.m_sNameKey = "#LF-Activity_Tab";
		def.m_sPitchKey = "#LF-Activity_Summary";
		def.m_aSteps.Insert(Step("one"));
		def.m_aSteps.Insert(Step("two"));
		return def;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		Objective(ctx);
		Ordering(ctx);
		Papers(ctx);
		BetweenSteps(ctx);
		WelcomeGuide(ctx);
		Licences(ctx);
		ctx.True(LF_HudPrefs.Parts().Contains(LF_HudPrefs.PART_OBJECTIVE), "the HUD has an objective part /hud can turn off");
	}

	//------------------------------------------------------------------------------------------------
	protected void Objective(LF_TestContext ctx)
	{
		LF_Objective.ResetStatics();
		int start = LF_Objective.Changes();
		if (!RedProof())
			LF_Objective.Set("activity", "#LF-Activity_Start");

		ctx.EqualStr("#LF-Activity_Start", LF_Objective.TitleKey(), "the step title is the objective line");
		ctx.Equal(start + 1, LF_Objective.Changes(), "a new line counts as a change (the HUD flashes it)");
		ctx.False(LF_Objective.Set("activity", "#LF-Activity_Start"), "the same line again is no change");
		ctx.False(LF_Objective.Clear("arrival"), "another guide cannot clear the line");
		ctx.EqualStr("#LF-Activity_Start", LF_Objective.TitleKey(), "the line stays");
		ctx.True(LF_Objective.Set("activity", string.Empty), "its own guide clears it with an empty title");
		ctx.EqualStr(string.Empty, LF_Objective.TitleKey(), "the line is gone");
		LF_Objective.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	protected void Ordering(LF_TestContext ctx)
	{
		array<ref LF_ActivityDef> all = {Def("first"), Def("second"), Def("third")};
		LF_TestPapersFacts facts = new LF_TestPapersFacts();
		// "first" is done, "third" runs.
		facts.m_mStatus.Set(LF_ActivityTable.QuestIdOf("first", "one"), LF_EQuestStatus.TURNED_IN);
		facts.m_mStatus.Set(LF_ActivityTable.QuestIdOf("first", "two"), LF_EQuestStatus.TURNED_IN);
		facts.m_mStatus.Set(LF_ActivityTable.QuestIdOf("third", "one"), LF_EQuestStatus.ACTIVE);
		LF_ActivityData data = LF_ActivityData.Build(all, facts, vector.Zero, false);
		if (RedProof())
			data = new LF_ActivityData();

		ctx.Equal(3, data.m_aRows.Count(), "all three are listed");
		if (data.m_aRows.Count() == 3)
		{
			ctx.EqualStr("third", data.m_aRows[0].m_sId, "the run in progress comes first");
			ctx.EqualStr("second", data.m_aRows[1].m_sId, "what can start comes next");
			ctx.EqualStr("first", data.m_aRows[2].m_sId, "a done run sinks to the end");
			ctx.EqualStr("#LF-Activity_Guide", LF_ActivityPage.ActionKey(data.m_aRows[0]), "the running row's button is the Resume key");
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void Papers(LF_TestContext ctx)
	{
		LF_ActivityDef def = Def("papers");
		LF_TestPapersFacts facts = new LF_TestPapersFacts();
		facts.m_bRegistered = false;
		ctx.EqualStr("papers", LF_ActivityRules.Gate(def, facts), "no papers, no run");
		ctx.EqualStr("locked", LF_ActivityRules.StartRefusal(def, facts), "Start is refused before the clerk");

		array<ref LF_ActivityDef> all = {def};
		LF_ActivityData data = LF_ActivityData.Build(all, facts, vector.Zero, false);
		ctx.Equal(0, data.m_aRows.Count(), "nothing is listed");
		ctx.True(data.m_bNeedsPapers, "the list says the clerk comes first");
		LF_ActivityData back = LF_ActivityData.Decode(data.Encode());
		ctx.True(back && back.m_bNeedsPapers, "the papers line survives the wire");

		facts.m_mStatus.Set(LF_ActivityTable.QuestIdOf("papers", "one"), LF_EQuestStatus.ACTIVE);
		ctx.Equal(LF_EActivityState.ACTIVE, LF_ActivityRules.StateOf(def, facts), "a run already going stays listed");

		facts.m_bRegistered = true;
		facts.m_mStatus.Clear();
		ctx.EqualStr(string.Empty, LF_ActivityRules.StartRefusal(def, facts), "with papers it starts");
		LF_ActivityData registered = LF_ActivityData.Build(all, facts, vector.Zero, false);
		LF_ActivityData registeredBack = LF_ActivityData.Decode(registered.Encode());
		ctx.True(registeredBack && !registeredBack.m_bNeedsPapers, "a registered player gets the list");
	}

	//------------------------------------------------------------------------------------------------
	protected void BetweenSteps(LF_TestContext ctx)
	{
		LF_ActivityConfig config = new LF_ActivityConfig();
		config.m_aActivities = {Def("chain")};
		LF_ActivityTable.SetForTests(config);
		string one = LF_ActivityTable.QuestIdOf("chain", "one");
		string two = LF_ActivityTable.QuestIdOf("chain", "two");
		LF_TestPapersFacts facts = new LF_TestPapersFacts();
		facts.m_mStatus.Set(one, LF_EQuestStatus.TURNED_IN);
		ctx.True(LF_ActivityGuide.BetweenSteps(one, facts), "step one turned in, step two not yet taken: between steps");
		facts.m_mStatus.Set(two, LF_EQuestStatus.ACTIVE);
		ctx.False(LF_ActivityGuide.BetweenSteps(one, facts), "once step two runs, it is not between steps");
		facts.m_mStatus.Set(two, LF_EQuestStatus.TURNED_IN);
		ctx.False(LF_ActivityGuide.BetweenSteps(two, facts), "the last step turned in ends the run");
		ctx.False(LF_ActivityGuide.BetweenSteps("not-an-activity", facts), "a quest of no activity is never between steps");
		LF_ActivityTable.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	protected void WelcomeGuide(LF_TestContext ctx)
	{
		array<ref LF_WelcomeGuideRow> saved = LF_WelcomeGuide.ExportAll();
		LF_WelcomeGuide.ResetStatics();
		LF_WelcomeGuide.MarkPending("guide-new");
		LF_WelcomeGuide.MarkPending("guide-registered");
		ctx.True(LF_WelcomeGuide.ResolveAutoShow("guide-new", false), "a newcomer without papers sees the guide at a rejoin");
		ctx.False(LF_WelcomeGuide.ResolveAutoShow("guide-registered", true), "a registered character does not");
		ctx.False(LF_WelcomeGuide.IsPending("guide-registered"), "and the mark is dropped for good");
		ctx.True(LF_WelcomeGuide.IsPending("guide-new"), "the newcomer's mark stays");
		LF_WelcomeGuide.ResetStatics();
		LF_WelcomeGuide.ApplyAll(saved);
	}

	//------------------------------------------------------------------------------------------------
	protected void Licences(LF_TestContext ctx)
	{
		LF_PlayerMenuData data = new LF_PlayerMenuData();
		ctx.EqualStr(LF_Terms.Text("#LF-PlayerMenu_LicencesNoneShort"), LF_PlayerMenuSheet.LicencesValue(data), "none held says None");
		data.m_aLicenceKeys.Insert("Field hand");
		ctx.EqualStr("Field hand", LF_PlayerMenuSheet.LicencesValue(data), "one licence is named in the row");
		data.m_aLicenceKeys.Insert("Driving");
		data.m_aLicenceKeys.Insert("Fishing");
		ctx.EqualStr(LF_Terms.Text("#LF-PlayerMenu_LicencesHeld", "3"), LF_PlayerMenuSheet.LicencesValue(data), "three are counted, each on its own row");
	}
}
