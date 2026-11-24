// red-proof: observed 2026-11-24 `tools/cli test --no-build --red LF_Test_ShiftBoardWeek,LF_Test_ShiftBoardSave`: RED-OK shiftboard/week (19 assertions), failing `six workers, the board keeps five`, `the list holds until Monday 06:00`. Green with `--only`.
//! The weekly shift board (WK-W22, #4712): six fixture workers earn XP at one site; the board keeps
//! the top five in order (a worker's trades summed, the larger trade named), XP at another site stays
//! off it, the row prints "1. Ada Newcomer, ... 1,240 XP", and Monday 06:00 empties it.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_ShiftBoardWeek : LF_Test
{
	static const string SITE = "test-shift-site";
	static const string OTHER_SITE = "test-shift-other";
	static const string MINER = "MINER";
	static const string FARMER = "FARMER";
	//! 2026-11-24 12:00 UTC, a Tuesday.
	static const int TUESDAY = 1795521600;

	override string GetName()
	{
		return "shiftboard/week";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// The store as it was (the e2e row among it) goes back at the end.
		array<ref LF_ShiftBoardRow> before = LF_ShiftBoard.ExportAll();
		int weekBefore = LF_ShiftBoard.CurrentWeek();
		LF_ShiftBoard.ApplyAll(null, -1);
		int week = LF_ShiftBoard.WeekOf(TUESDAY);
		int monday = LF_ShiftBoard.WEEK_ANCHOR + (week + 1) * LF_ShiftBoard.WEEK_SECONDS;
		ctx.Equal(week, LF_ShiftBoard.WeekOf(monday - 1), "Monday 05:59:59 is still this week");
		ctx.Equal(week + 1, LF_ShiftBoard.WeekOf(monday), "Monday 06:00 starts the next week");
		ctx.False(LF_ShiftBoard.Record(SITE, "", "Nobody", MINER, 10, TUESDAY), "XP with no worker is refused");
		ctx.False(LF_ShiftBoard.Record(SITE, "uid-a", "Ada Newcomer", MINER, 0, TUESDAY), "no XP is refused");

		// RedProof(): nothing is recorded, so the list is empty and every order check fails (`--red`).
		if (!RedProof())
		{
			LF_ShiftBoard.Record(SITE, "uid-a", "Ada Newcomer", MINER, 1000, TUESDAY);
			LF_ShiftBoard.Record(SITE, "uid-a", "Ada Newcomer", MINER, 240, TUESDAY);
			LF_ShiftBoard.Record(SITE, "uid-b", "Ben Hauler", MINER, 300, TUESDAY);
			LF_ShiftBoard.Record(SITE, "uid-b", "Ben Hauler", FARMER, 600, TUESDAY);
			LF_ShiftBoard.Record(SITE, "uid-c", "Cleo Digger", MINER, 700, TUESDAY);
			LF_ShiftBoard.Record(SITE, "uid-d", "Dan Picker", FARMER, 500, TUESDAY);
			LF_ShiftBoard.Record(SITE, "uid-e", "Eve Stoker", MINER, 400, TUESDAY);
			LF_ShiftBoard.Record(SITE, "uid-f", "Finn Late", MINER, 100, TUESDAY);
			LF_ShiftBoard.Record(OTHER_SITE, "uid-f", "Finn Late", MINER, 5000, TUESDAY);
		}

		array<ref LF_ShiftBoardRow> top = LF_ShiftBoard.Top(SITE, TUESDAY);
		ctx.Equal(5, top.Count(), "six workers, the board keeps five");
		array<string> order = {"uid-a", "uid-b", "uid-c", "uid-d", "uid-e"};
		array<int> xp = {1240, 900, 700, 500, 400};
		for (int i = 0; i < order.Count() && i < top.Count(); i++)
		{
			ctx.True(top[i].m_sPlayerUid == order[i], string.Format("place %1 is %2 (got %3)", i + 1, order[i], top[i].m_sPlayerUid));
			ctx.Equal(xp[i], top[i].m_iXp, string.Format("place %1 holds the week's XP at the site", i + 1));
		}

		if (top.Count() > 1)
			ctx.True(top[1].m_sJob == FARMER, "a worker with two trades shows the larger one");

		if (!top.IsEmpty())
		{
			ctx.True(top[0].m_sName == "Ada Newcomer", "the first row carries the character name");
			string text = LF_ShiftBoard.RowText(1, top[0].m_sName, top[0].m_sJob, 7, top[0].m_iXp);
			ctx.True(text.Contains("Ada Newcomer") && text.Contains("1,240") && text.Contains("7"), "the row reads '1. Ada Newcomer, Mining 7, 1,240 XP': " + text);
		}

		ctx.Equal(1, LF_ShiftBoard.Top(OTHER_SITE, TUESDAY).Count(), "XP at another site sits on that site's board only");

		LF_BoardAnswer answer = LF_ShiftBoard.BuildAnswerFor(SITE, TUESDAY);
		ctx.Equal(10, answer.m_aNumbers.Count(), "the board answer carries level and XP for five workers");
		array<ref LF_UIPart> parts = LF_ShiftBoard.BuildParts(answer);
		ctx.Equal(7, parts.Count(), "the card is a title, five rows and close");
		ctx.NotNull(LF_BoardProviders.Find(LF_ShiftBoard.KIND), "the shiftboard kind is registered");

		// The week turns: every board is empty and a new award starts the new list.
		ctx.Equal(5, LF_ShiftBoard.Top(SITE, monday - 1).Count(), "the list holds until Monday 06:00");
		ctx.Equal(0, LF_ShiftBoard.Top(SITE, monday).Count(), "Monday 06:00 empties the board");
		ctx.Equal(0, LF_ShiftBoard.Top(OTHER_SITE, monday).Count(), "and every other board");
		ctx.Equal(week + 1, LF_ShiftBoard.CurrentWeek(), "the store moved to the new week");
		LF_ShiftBoard.Record(SITE, "uid-f", "Finn Late", MINER, 50, monday + 60);
		ctx.Equal(1, LF_ShiftBoard.Top(SITE, monday + 60).Count(), "the new week starts a new list");

		LF_ShiftBoard.ApplyAll(before, weekBefore);
	}
}
