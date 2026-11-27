// red-proof: 2026-11-27 `tools/cli.mjs test --no-build --red LF_Test_WelcomeGuideDone`: with the hook on the finished chain is read as unfinished, so the finished-state assertions fail (RED-OK line in the PR). Green with `--only`.
//! The welcome guide after the arrival errands are done (#5119 playtest row 3): page 1 stops sending the
//! player to the clerk and says what to do next, with no "Set waypoint" button. A character with errands
//! left, or no chain at all, keeps the clerk page. No world; the data is built by hand.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_WelcomeGuideDone : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "quests/welcome-guide-done";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_PlayerMenuData Chain(LF_EPlayerMenuQuestState first, LF_EPlayerMenuQuestState second)
	{
		LF_PlayerMenuData data = new LF_PlayerMenuData();
		data.m_aQuests.Insert(Quest("arrive_a", first));
		data.m_aQuests.Insert(Quest("arrive_b", second));
		return data;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_PlayerMenuQuest Quest(string id, LF_EPlayerMenuQuestState state)
	{
		LF_PlayerMenuQuest quest = new LF_PlayerMenuQuest();
		quest.m_sId = id;
		quest.m_eGroup = LF_EPlayerMenuQuestGroup.ARRIVAL;
		quest.m_eState = state;
		return quest;
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasPart(array<ref LF_UIPart> parts, string name)
	{
		foreach (LF_UIPart part : parts)
		{
			if (part.GetName() == name)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_PlayerMenuData done = Chain(LF_EPlayerMenuQuestState.DONE, LF_EPlayerMenuQuestState.DONE);
		LF_PlayerMenuData midway = Chain(LF_EPlayerMenuQuestState.DONE, LF_EPlayerMenuQuestState.CURRENT);
		LF_PlayerMenuData noChain = new LF_PlayerMenuData();
		bool finished = done.ArrivalFinished();
		if (RedProof())
			finished = false;

		ctx.True(finished, "every arrival step turned in is a finished chain");
		ctx.False(midway.ArrivalFinished(), "a step left is not finished");
		ctx.False(noChain.ArrivalFinished(), "a character with no chain is not finished (nothing was ever asked of them)");

		array<string> clerk = LF_WelcomeGuideSheet.PageKeys(0, false);
		array<string> next = LF_WelcomeGuideSheet.PageKeys(0, finished);
		ctx.EqualStr("#LF-WelcomeGuide_Head1", clerk[0], "an unfinished chain keeps the first-steps page");
		ctx.EqualStr("#LF-WelcomeGuide_DoneHead1", next[0], "a finished chain gets the what-next page");
		ctx.Equal(clerk.Count(), next.Count(), "the same number of paragraphs");
		ctx.EqualStr(LF_WelcomeGuideSheet.PageKeys(1, false)[0], LF_WelcomeGuideSheet.PageKeys(1, true)[0], "only page 1 changes");

		string body = LF_Terms.Text(next[1]) + " " + LF_Terms.Text(next[2]);
		ctx.False(body.Contains("clerk"), "the finished page never mentions the clerk");
		ctx.True(body.Contains("F2") && body.Contains("What to do next"), "the finished page points at F2 What to do next");
		ctx.False(body.Contains("#LF-"), "the finished page's keys are in the string table");

		ctx.True(HasPart(LF_WelcomeGuideSheet.BuildParts(0, false), LF_WelcomeGuideSheet.PART_WAYPOINT), "the first-steps page has Set waypoint");
		ctx.False(HasPart(LF_WelcomeGuideSheet.BuildParts(0, finished), LF_WelcomeGuideSheet.PART_WAYPOINT), "the finished page has no Set waypoint: there is no errand to mark");
		ctx.False(HasPart(LF_WelcomeGuideSheet.BuildParts(1, finished), LF_WelcomeGuideSheet.PART_WAYPOINT), "later pages never had the button");
	}
}
