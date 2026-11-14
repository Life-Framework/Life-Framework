// red-proof: Observed 2026-11-13 with `tools/cli test --no-build --only LF_Test_CaseBoard`: loosening the per-case cap in LF_CaseBoard.PinCard (`>` for `>=`) failed `- a case holds at most its four cards (expected 4, got 5)` and `- the card pinned last comes down first (expected 104, got 105)`. Restored; green.
// tier: LOGIC
//! Case board (#148), with no world: a report gets its time; a logged bag becomes an unpinned card;
//! Pin puts the next card of an open case up, four at most a case; Unpin takes the last one down; the
//! cases with cards come first; the look and the name only go out once identified; a field never
//! carries the separator; burning the case file clears the cards; a closed case is not saved; and
//! the face's string runs pin to pin.
class LF_Test_CaseBoard : LF_Test
{
	protected static const string ROBBER = "caseboard-test-robber";
	protected static const string THIEF = "caseboard-test-thief";

	override string GetName()
	{
		return "investigation/case-board";
	}

	override void Run(LF_TestContext ctx)
	{
		array<ref LF_WantedReportRecord> reportsBefore = LF_WantedReports.ExportAll();
		array<ref LF_CaseBoardRow> boardBefore = LF_CaseBoard.ExportAll();
		LF_WantedReports.ApplyAll(null);
		LF_CaseBoard.ResetStatics();

		LF_WantedReportRecord robbery = LF_WantedReports.Add(ROBBER, LF_WantedReports.OFFENCE_ROBBERY, "", "", "Fuel station", false);
		robbery.m_sAppearance = "9B5BB216CC7FF18E";
		LF_CaseBoard.OnReport(robbery.m_iSequence);
		ctx.Equal(5, LF_CaseBoard.TimeOf(robbery.m_iSequence).Length(), "the case keeps the time of the crime as HH:MM");

		array<string> entries = {};
		ctx.Equal(1, LF_CaseBoard.BuildEntries(entries), "an open report is a case on the board");
		array<ref LF_CaseBoardCase> cases = {};
		LF_CaseBoard.Decode(entries, cases);
		ctx.Equal(1, cases.Count(), "the client decodes the case");
		if (cases.Count() == 1)
		{
			ctx.Equal(0, cases[0].m_aKinds.Count(), "nothing is pinned yet");
			ctx.EqualStr("", cases[0].m_sAppearance, "an unidentified suspect's look stays off the board");
			ctx.EqualStr("", cases[0].m_sName, "an unidentified suspect has no name on the board");
		}

		ctx.Equal(0, LF_CaseBoard.Pin(null), "with nothing logged there is nothing to pin");
		for (int i = 1; i <= 6; i++)
			LF_CaseBoard.OnLogged(LockerRow(100 + i, robbery.m_iSequence, LF_Evidence.KIND_FOOTPRINT), "#AR-Boots;x");

		ctx.Equal(0, LF_CaseBoard.PinnedCount(robbery.m_iSequence), "a logged card waits unpinned");
		ctx.Equal(1, LF_CaseBoard.Pin(null), "Pin puts the next logged card up");
		for (int n = 0; n < 6; n++)
			LF_CaseBoard.Pin(null);

		ctx.Equal(LF_CaseBoard.CARDS_PER_CASE, LF_CaseBoard.PinnedCount(robbery.m_iSequence), "a case holds at most its four cards");
		LF_CaseBoard.BuildEntries(entries);
		LF_CaseBoard.Decode(entries, cases);
		if (cases.Count() == 1)
		{
			ctx.Equal(LF_CaseBoard.CARDS_PER_CASE, cases[0].m_aKinds.Count(), "the face gets the pinned cards");
			ctx.EqualStr("#AR-Boots,x", cases[0].m_aNotes[0], "a separator in a note cannot shift the fields");
		}

		LF_CaseBoardRow down = LF_CaseBoard.Unpin();
		ctx.NotNull(down, "Unpin takes a card down");
		if (down)
			ctx.Equal(104, down.m_iNumber, "the card pinned last comes down first");

		ctx.Equal(3, LF_CaseBoard.PinnedCount(robbery.m_iSequence), "three cards stay up");

		// A newer case with no card sorts below the case with cards; once identified it shows the look.
		LF_WantedReportRecord theft = LF_WantedReports.Add(THIEF, LF_WantedReports.OFFENCE_VEHICLE_THEFT, "", "", "Car lot", true);
		theft.m_sAppearance = "AAAABBBBCCCCDDDD";
		LF_CaseBoard.OnReport(theft.m_iSequence);
		LF_CaseBoard.BuildEntries(entries);
		LF_CaseBoard.Decode(entries, cases);
		ctx.Equal(2, cases.Count(), "both open reports are cases");
		if (cases.Count() == 2)
		{
			ctx.Equal(robbery.m_iSequence, cases[0].m_iReport, "the case with pinned cards comes first");
			ctx.True(cases[1].m_bIdentified, "the identified case says so");
			ctx.EqualStr("AAAABBBBCCCCDDDD", cases[1].m_sAppearance, "an identified suspect's look reaches the board");
		}

		// A closed case (the wanted level cleared) is not saved; the open one is, with its pins.
		LF_WantedReports.ClearFor(THIEF);
		array<ref LF_CaseBoardRow> saved = LF_CaseBoard.ExportAll();
		int savedForTheft;
		foreach (LF_CaseBoardRow row : saved)
		{
			if (row.m_iReport == theft.m_iSequence)
				savedForTheft++;
		}

		ctx.Equal(0, savedForTheft, "a closed case's rows are not saved");
		ctx.Equal(7, saved.Count(), "the open case saves its time row and its six cards");

		LF_CaseBoard.OnCaseFileBurned();
		ctx.Equal(0, LF_CaseBoard.PinnedCount(robbery.m_iSequence), "burning the case file takes every card down");
		ctx.False(LF_CaseBoard.MayRead(null), "nobody without a body reads the board");

		RunStringGeometry(ctx);

		LF_WantedReports.ApplyAll(reportsBefore);
		LF_CaseBoard.ResetStatics();
		LF_CaseBoard.ApplyAll(boardBefore);
	}

	//------------------------------------------------------------------------------------------------
	protected LF_EvidenceLockerRow LockerRow(int number, int report, string kind)
	{
		LF_EvidenceLockerRow row = new LF_EvidenceLockerRow();
		row.m_iNumber = number;
		row.m_iReport = report;
		row.m_sKind = kind;
		return row;
	}

	//------------------------------------------------------------------------------------------------
	//! The face's string: a line of the right length, turned to run from one pin to the other.
	protected void RunStringGeometry(LF_TestContext ctx)
	{
		vector centre;
		float length;
		float angle;
		LF_CaseBoardFace.StringBetween("100 500 0", "130 460 0", centre, length, angle);
		ctx.True(Math.AbsFloat(length - 50) < 0.01, "the string is as long as the gap between the pins");
		ctx.True(Math.AbsFloat(centre[0] - 115) < 0.01 && Math.AbsFloat(centre[1] - 480) < 0.01, "the string turns about the midpoint");
		ctx.True(Math.AbsFloat(angle + 53.13) < 0.05, "the string is turned up toward the suspect's pin");
		ctx.True(Math.AbsFloat(LF_CaseBoardFace.ColumnLeft(0, 1) - 640) < 0.01, "one case sits in the middle column");
		vector second = LF_CaseBoardFace.CardPosition(0, 1);
		vector third = LF_CaseBoardFace.CardPosition(0, 2);
		ctx.True(second[0] > third[0] && third[1] > second[1], "cards fill two rows of two");
	}
}
