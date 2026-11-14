// red-proof: observed 2026-11-14, tools\cli test --no-build --only LF_Test_Credits with the key-count assertion
// changed to expect 1 missing key instead of 0: "SUMMARY tier=only passed=0 failed=1 total=1"; restored, passed=1.

//------------------------------------------------------------------------------------------------
//! The credits screen (#481) opens as a kit sheet and lists every generated row of docs/credits.md;
//! every row's keys are in the string table.
// tier: WORLD
class LF_Test_Credits : LF_Test
{
	override string GetName()
	{
		return "credits/screen";
	}

	override void Run(LF_TestContext ctx)
	{
		array<string> rows = LF_CreditsData.GetRows();
		ctx.True(rows.Count() > 10, "the credits source gave rows: " + rows.Count().ToString());

		int keys = 0;
		int missing = 0;
		foreach (string row : rows)
		{
			array<string> cells = {};
			row.Split("|", cells, false);
			ctx.True(cells.Count() >= 2, "row has a label key: " + row);
			if (cells.Count() < 2)
				continue;

			keys++;
			if (!LF_CreditsSheet.CheckKey(cells[1]))
				missing++;

			if (cells.Count() > 2 && !cells[2].IsEmpty())
			{
				keys++;
				if (!LF_CreditsSheet.CheckKey(cells[2]))
					missing++;
			}
		}

		ctx.Equal(0, missing, string.Format("every one of %1 credits keys is in the string table", keys));
		ctx.False(LF_CreditsSheet.CheckKey("#Credits-NoSuchKey"), "a missing key is reported");

		LF_UIScreen screen = LF_CreditsSheet.Open();
		ctx.NotNull(screen, "the credits screen opens");
		if (!screen)
			return;

		ctx.True(screen.IsOpen(), "the credits screen is open");
		ctx.Equal(rows.Count(), LF_CreditsSheet.RowCount(), "the sheet read every credits row");
		ctx.True(LF_CreditsSheet.Pages() > 3, "the credits are paged by section: " + LF_CreditsSheet.Pages().ToString());
		LF_CreditsSheet.Hide();
		ctx.False(screen.IsOpen(), "Close removes the credits screen");
	}
}
