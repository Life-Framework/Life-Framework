// red-proof: observed 2026-11-19 (#3768): `tools\cli test --no-build --red LF_Test_QuestChoice,LF_Test_BuyerSale` (with the pack's shipment tests in the same run) printed `RED PROOF OK (all 2 listed test(s) fail with the hook on)` in both legs; the acts that RedProof() skips fail the choice, status, standing and sale assertions. `--only` then passed.
//! #3768: a choice between two endings. Two quests that name each other in `m_aExclusiveQuestIds` are one
//! choice: taking one fails the other if it was running, a done one (completed or turned in, also one
//! restored after a restart) shuts the other out, and a step that is shut out is not offered to
//! auto-accept again. A collect objective flagged `m_bKeepOnTurnIn` is a show-it step. Pure tracker rules.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_QuestChoice : LF_Test
{
	override string GetName()
	{
		return "quests/choice";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_QuestDefinition Quest(string id, string other)
	{
		LF_QuestObjective objective = new LF_QuestObjective();
		objective.m_eType = LF_EQuestEventType.SIGNAL;
		objective.m_sFilter = "choice.done:" + id;
		objective.m_iTarget = 1;

		LF_QuestDefinition def = new LF_QuestDefinition();
		def.m_sQuestId = id;
		def.m_aObjectives = {};
		def.m_aObjectives.Insert(objective);
		def.m_aExclusiveQuestIds = {};
		def.m_aExclusiveQuestIds.Insert(other);
		return def;
	}

	//------------------------------------------------------------------------------------------------
	protected bool Take(LF_QuestTracker tracker, LF_QuestDefinition def)
	{
		return tracker.Accept(def, 0, LF_EJobType.UNEMPLOYED, "", 0, tracker.GetCompletedQuestIds());
	}

	//------------------------------------------------------------------------------------------------
	protected void Signal(LF_QuestTracker tracker, map<string, ref LF_QuestDefinition> defs, string key)
	{
		LF_QuestEvent ev = new LF_QuestEvent();
		ev.m_eType = LF_EQuestEventType.SIGNAL;
		ev.m_sFilter = key;
		ev.m_iValue = 1;
		tracker.ApplyEvent(ev, defs);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_QuestDefinition sold = Quest("c_sold", "c_report");
		LF_QuestDefinition report = Quest("c_report", "c_sold");
		map<string, ref LF_QuestDefinition> defs = new map<string, ref LF_QuestDefinition>();
		defs.Set("c_sold", sold);
		defs.Set("c_report", report);

		// Taking one side fails the other that was running.
		LF_QuestTracker running = new LF_QuestTracker();
		ctx.True(Take(running, sold), "the sale is taken");
		if (!RedProof())
			ctx.True(Take(running, report), "the report is taken while the sale runs");

		ctx.True(running.GetStatus("c_report") == LF_EQuestStatus.ACTIVE, "the report runs");
		ctx.True(running.GetStatus("c_sold") == LF_EQuestStatus.FAILED, "taking the report fails the running sale");
		ctx.False(Take(running, sold), "a failed sale cannot be taken again");

		// A failed side hears nothing: its signal changes nothing.
		Signal(running, defs, "choice.done:c_sold");
		ctx.True(running.GetStatus("c_sold") == LF_EQuestStatus.FAILED, "the failed sale stays failed on its signal");

		// A done side shuts the other out, taken or not.
		LF_QuestTracker done = new LF_QuestTracker();
		ctx.True(Take(done, sold), "a second player takes the sale");
		Signal(done, defs, "choice.done:c_sold");
		ctx.True(done.GetStatus("c_sold") == LF_EQuestStatus.COMPLETED, "the sale completes on its signal");
		if (!RedProof())
			ctx.False(Take(done, report), "a done sale shuts the report out");

		ctx.True(done.GetStatus("c_report") == LF_EQuestStatus.AVAILABLE, "the shut-out report was never taken");
		ctx.True(done.TurnIn("c_sold", 1), "the sale is turned in");
		ctx.False(done.CanAccept(report, 0, LF_EJobType.UNEMPLOYED, "", 2, done.GetCompletedQuestIds()), "a turned-in sale still shuts the report out");

		// A turned-in side restored after a restart shuts the other out and the chain does not offer it.
		LF_QuestTracker restored = new LF_QuestTracker();
		restored.RestoreTurnedIn("c_report", 0);
		report.m_bAutoAccept = true;
		sold.m_bAutoAccept = true;
		sold.m_sRequiredPreviousQuestId = "c_report";
		if (!RedProof())
			ctx.True(restored.ChoiceMade(sold), "a turned-in report shuts a restored sale out");

		array<string> offers = {};
		restored.CollectAutoAccepts(defs, offers);
		ctx.Equal(0, offers.Count(), "a shut-out step is not offered to auto-accept");

		// With nothing done the same step is offered.
		LF_QuestTracker fresh = new LF_QuestTracker();
		fresh.RestoreTurnedIn("c_report", 0);
		sold.m_aExclusiveQuestIds.Clear();
		report.m_aExclusiveQuestIds.Clear();
		fresh.CollectAutoAccepts(defs, offers);
		ctx.Equal(1, offers.Count(), "without the choice the step is offered");

		// The state: only a running quest fails; a turned-in one stays.
		LF_QuestState state = new LF_QuestState();
		ctx.False(state.Fail(), "a quest not taken cannot fail");
		state.Accept("c_x", 0, 0);
		ctx.True(state.Fail(), "a running quest fails");
		ctx.True(state.GetStatus() == LF_EQuestStatus.FAILED, "and reads failed");
		ctx.False(state.Fail(), "a failed quest does not fail twice");
		LF_QuestState turned = new LF_QuestState();
		turned.RestoreTurnedIn("c_y", 0);
		ctx.False(turned.Fail(), "a turned-in quest stays turned in");

		// A show-it collect objective is a plain flag on the objective; a normal one takes the item.
		LF_QuestObjective show = new LF_QuestObjective();
		ctx.False(show.m_bKeepOnTurnIn, "an objective takes its item on turn-in unless flagged");
	}
}
