// red-proof: observed 2026-11-22 `tools/cli test --no-build --red LF_Test_PlayerMenuLayout`: RED-OK (13 assertions fail with the hook on: the share falls back to the floor width). Green with `--only`.
//! The wide personal file's sizing, no widgets: the sheet takes 80% of the screen (never under its floor,
//! never past the edge margins), columns share the width by weight, every job skill shows without
//! scrolling up to ten, and the island day counts from day 1 on arrival.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_PlayerMenuLayout : LF_Test
{
	override string GetName()
	{
		return "playermenu/wide-layout";
	}

	override void Run(LF_TestContext ctx)
	{
		// RedProof(): the share falls back to the old narrow sheet, so the width assertions fail (`--red`).
		float share = LF_PlayerMenuSheet.SHEET_SHARE;
		if (RedProof())
			share = 0;

		ctx.EqualFloat(1536, LF_UISheetFit.SheetWidth(1920, share, LF_PlayerMenuSheet.SHEET_MIN_WIDTH), 0.5, "the file is 80% of a 1920-wide reference screen (1080p and 720p alike)");
		ctx.EqualFloat(LF_PlayerMenuSheet.SHEET_MIN_WIDTH, LF_UISheetFit.SheetWidth(1200, 0.8, LF_PlayerMenuSheet.SHEET_MIN_WIDTH), 0.5, "a narrow screen keeps the floor width");
		ctx.EqualFloat(1000 - 2 * LF_UIScreen.SHEET_EDGE_MARGIN, LF_UISheetFit.SheetWidth(1000, 0.8, 1100), 0.5, "never wider than the screen less its margins");

		array<float> weights = {1, 1, 1.3};
		float content = 1496;
		float total = LF_UISheetFit.ColumnWidth(content, weights, 0) + LF_UISheetFit.ColumnWidth(content, weights, 1) + LF_UISheetFit.ColumnWidth(content, weights, 2);
		ctx.EqualFloat(content, total, 0.5, "the columns share the content width exactly");
		ctx.True(LF_UISheetFit.ColumnWidth(content, weights, 2) > LF_UISheetFit.ColumnWidth(content, weights, 0), "the skills column is the widest");
		ctx.EqualFloat(0, LF_UISheetFit.ColumnWidth(content, weights, 3), 0.001, "a column that is not there has no width");

		LF_PlayerMenuData data = new LF_PlayerMenuData();
		for (int i = 0; i < 8; i++)
		{
			data.m_aSkills.Insert(new LF_PlayerMenuSkill());
		}

		float pitch = LF_UIScreen.COMPACT_ROW_PITCH + LF_UIScreen.COMPACT_BAR_PITCH;
		ctx.True(LF_PlayerMenuSheet.ListHeight(data, LF_PlayerMenuSheet.TAB_PERSON) >= 8 * pitch, "eight skills all show: the list is as tall as eight rows and their bars");
		for (int j = 0; j < 6; j++)
		{
			data.m_aSkills.Insert(new LF_PlayerMenuSkill());
		}

		ctx.True(LF_PlayerMenuSheet.ListHeight(data, LF_PlayerMenuSheet.TAB_PERSON) < 11 * pitch, "past ten skills the list stops growing and scrolls");

		ctx.Equal(1, LF_PlayerMenuSheet.DaysOnIsland(0, 120), "a new arrival is on day 1");
		ctx.Equal(3, LF_PlayerMenuSheet.DaysOnIsland(5 * 3600, 120), "five hours on a two-hour day is day 3");
		ctx.Equal(1, LF_PlayerMenuSheet.DaysOnIsland(5 * 3600, 0), "an unset day length counts real days");
		ctx.True(LF_PlayerMenuNextTab.HasContent(data), "the What to do next tab shows: the activities feature fills it");

		LF_NotificationHold.Hold(true);
		ctx.True(LF_NotificationHold.IsHeld(), "an open file holds the notification toasts");
		LF_NotificationHold.Hold(false);
		ctx.False(LF_NotificationHold.IsHeld(), "closing it lets them show again");
	}
}
