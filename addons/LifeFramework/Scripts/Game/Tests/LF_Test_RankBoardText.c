// red-proof: 2026-11-23 `tools\cli test --no-build --red LF_Test_RankBoardText` RED-OK (RedProof() skips the two awards; ledger count and both plaque lines fail). Green with --only.
//! The station's printed rank board and commendations plaque (#4206, chapter #4170): the board
//! provider lists on-roster officers by rank (highest first) with their titles, a rank with nobody
//! on it is simply absent, an empty roster says so; the plaque provider lists commendations newest
//! first and says so when there are none. Both are board kinds, so a reader gets an answer built
//! fresh per read and the flat RPC shape survives into the same lines.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_RankBoardText : LF_Test
{
	override string GetName()
	{
		return "police/rank-board-text";
	}

	//------------------------------------------------------------------------------------------------
	protected static string LineText(array<ref LF_UIPart> parts, string name)
	{
		foreach (LF_UIPart part : parts)
		{
			if (part.GetName() == name)
				return part.GetText();
		}

		return "<no part " + name + ">";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_BoardProviders.ResetStatics();
		LF_Commendations.ResetStatics();
		LF_Commendations.SetMemoryOnly(true);

		// The board: a patrol officer, the senior, another patrol officer, in roster order.
		array<string> names = {"Zoe Patrol", "Sam Senior", "Abe Patrol"};
		array<int> ranks = {LF_PoliceRanks.PATROL, LF_PoliceRanks.SENIOR, LF_PoliceRanks.PATROL};
		LF_BoardAnswer answer = LF_RankBoard.BuildAnswerFrom(names, ranks);
		ctx.True(answer != null, "the rank board builds an answer");
		if (!answer)
			return;

		array<ref LF_UIPart> parts = LF_RankBoard.BuildParts(answer);
		string seniorTitle = WidgetManager.Translate("#" + LF_PoliceRanks.TitleKey(LF_PoliceRanks.SENIOR));
		string patrolTitle = WidgetManager.Translate("#" + LF_PoliceRanks.TitleKey(LF_PoliceRanks.PATROL));
		ctx.EqualStr(seniorTitle + ": Sam Senior", LineText(parts, "Officer0"), "the highest rank is the first line, with its title");
		ctx.EqualStr(patrolTitle + ": Abe Patrol", LineText(parts, "Officer1"), "within a rank the names run alphabetically");
		ctx.EqualStr(patrolTitle + ": Zoe Patrol", LineText(parts, "Officer2"), "every officer on the roster is listed");

		// An empty roster says so instead of printing a bare title.
		array<string> noNames = {};
		array<int> noRanks = {};
		LF_BoardAnswer emptyAnswer = LF_RankBoard.BuildAnswerFrom(noNames, noRanks);
		ctx.EqualStr(WidgetManager.Translate("#LF-RankBoard_None"), LineText(LF_RankBoard.BuildParts(emptyAnswer), "None"), "an empty roster prints the none line");

		// The board is a registered kind: the registry finds it and paints the same lines.
		LF_BoardProvider provider = LF_BoardProviders.Find(LF_RankBoard.KIND);
		ctx.True(provider != null, "the rank board is registered as a board kind");
		if (provider)
		{
			ctx.EqualStr(LF_RankBoard.READ_ACTION, provider.GetReadAction(), "the rank board names its read action");
			ctx.EqualStr(seniorTitle + ": Sam Senior", LineText(provider.Paint(answer), "Officer0"), "the provider paints the same first line");
		}

		// The plaque: nothing yet, then two commendations, newest first.
		ctx.EqualStr(WidgetManager.Translate("#LF-Commendations_None"), LineText(LF_CommendationPlaque.BuildParts(LF_CommendationPlaque.BuildAnswer()), "None"), "an empty plaque prints the none line");
		if (!RedProof())
		{
			LF_Commendations.Award("Sam Senior", "Pulled a child from the river", 1000);
			LF_Commendations.Award("Abe Patrol", "Ten years of service", 2000);
		}

		ctx.Equal(2, LF_Commendations.Count(), "the ledger holds both commendations");
		array<ref LF_UIPart> plaque = LF_CommendationPlaque.BuildParts(LF_CommendationPlaque.BuildAnswer());
		ctx.EqualStr("Abe Patrol: Ten years of service", LineText(plaque, "Commendation0"), "the newest commendation is first");
		ctx.EqualStr("Sam Senior: Pulled a child from the river", LineText(plaque, "Commendation1"), "the older commendation follows");

		LF_BoardProvider plaqueProvider = LF_BoardProviders.Find(LF_CommendationPlaque.KIND);
		ctx.True(plaqueProvider != null, "the plaque is registered as a board kind");
		if (plaqueProvider)
			ctx.EqualStr(LF_CommendationPlaque.READ_ACTION, plaqueProvider.GetReadAction(), "the plaque names its read action");

		// A name with the file separator in it cannot corrupt the ledger.
		LF_Commendations.Award("Bad|Name", "Reason|Odd", 3000);
		ctx.EqualStr("Bad Name: Reason Odd", LineText(LF_CommendationPlaque.BuildParts(LF_CommendationPlaque.BuildAnswer()), "Commendation0"), "the separator is scrubbed from a name and a reason");

		LF_Commendations.ResetStatics();
	}
}
