// red-proof: with `RedProof()` on the line builders are not asked, so every text stays "?" and the label, pay, deadline and
// hauler-name checks fail.

//! jobs/export-board-line: one row of the export board (LF_ExportBoard). The left text names the count, the goods and the
//! destination (a #key goods or place is translated), the right text of an open order shows the pay and the deadline only, a
//! taken order adds the hauler's name when the reader knows it and the word "taken" when not, and the two taken forms never
//! show each other's text. Expected texts are built from the same terms, so a re-worded string passes and a dropped value fails.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_ExportBoardLine : LF_Test
{
	override string GetName()
	{
		return "jobs/export-board-line";
	}

	override void Run(LF_TestContext ctx)
	{
		string label = "?";
		string keyLabel = "?";
		string open = "?";
		string openIgnoresName = "?";
		string takenNamed = "?";
		string takenUnknown = "?";
		if (!RedProof())
		{
			label = LF_ExportBoard.LineLabel(12, "Apples", "Port");
			keyLabel = LF_ExportBoard.LineLabel(1, "#LF-Wanted_OnFoot", "#LF-Wanted_NoDescription");
			open = LF_ExportBoard.LineValue(920, "13:00", false, "");
			openIgnoresName = LF_ExportBoard.LineValue(920, "13:00", false, "Sam Hauler");
			takenNamed = LF_ExportBoard.LineValue(920, "13:00", true, "Sam Hauler");
			takenUnknown = LF_ExportBoard.LineValue(920, "13:00", true, "");
		}

		ctx.EqualStr(WidgetManager.Translate("#LF-ExportBoard_Label", "12", "Apples", "Port"), label, "the label carries the count, the goods and the destination");
		ctx.True(label.Contains("12") && label.Contains("Apples") && label.Contains("Port"), "the label shows all three values");
		ctx.EqualStr(WidgetManager.Translate("#LF-ExportBoard_Label", "1", WidgetManager.Translate("#LF-Wanted_OnFoot"), WidgetManager.Translate("#LF-Wanted_NoDescription")), keyLabel, "goods and destination given as #keys are translated");

		string takenWord = WidgetManager.Translate("#LF-ExportBoard_Taken");
		ctx.EqualStr(LF_Terms.Text("#LF-ExportBoard_Value", "920", "13:00"), open, "an open order shows the pay and the deadline");
		ctx.True(open.Contains("920") && open.Contains("13:00"), "the open line shows both values");
		ctx.False(open.Contains(takenWord), "an open order is not marked taken");
		ctx.EqualStr(open, openIgnoresName, "an open order never shows a name, even one the reader knows");
		ctx.EqualStr(LF_Terms.Text("#LF-ExportBoard_ValueTaken", "920", "13:00", "Sam Hauler"), takenNamed, "a taken order shows the hauler the reader knows");
		ctx.True(takenNamed.Contains("Sam Hauler") && takenNamed.Contains("920") && takenNamed.Contains("13:00"), "the named line shows pay, deadline and name");
		ctx.False(takenNamed.Contains(takenWord), "a named hauler replaces the word 'taken'");
		ctx.EqualStr(LF_Terms.Text("#LF-ExportBoard_ValueTaken", "920", "13:00", takenWord), takenUnknown, "a hauler the reader does not know reads as 'taken'");
		ctx.True(open != takenUnknown, "a taken order reads differently from an open one");
	}
}
