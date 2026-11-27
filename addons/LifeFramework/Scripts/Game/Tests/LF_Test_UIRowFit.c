// red-proof: `toolscli test --no-build --red LF_Test_UIRowFit` skips the fit (the old fixed 1.5 : 1 : 1.5 shares) so the empty-status label keeps 1.5 and the
// assertions fail.
//! 2026-11-27 UI sweep part 2: a status row with no status gives that column's share to its label. The handbook title "How to read this handbook" and the
//! map result "Bank, Saint-Philippe" were cut at 37.5% of the row because the empty status held a quarter of it for nothing. A row with a status keeps the
//! default shares so a list stays aligned.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_UIRowFit : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "ui/row-fit";
	}

	//------------------------------------------------------------------------------------------------
	protected void Fit(bool hasStatus, out float wl, out float ws, out float wa)
	{
		if (RedProof())
		{
			wl = LF_UIRowFit.WEIGHT_LABEL;
			ws = LF_UIRowFit.WEIGHT_STATUS;
			wa = LF_UIRowFit.WEIGHT_ACTION;
			return;
		}

		LF_UIRowFit.Weights(hasStatus, wl, ws, wa);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		float wl, ws, wa;

		// A row with a status keeps the default shares, so rows in a list line up.
		Fit(true, wl, ws, wa);
		ctx.EqualFloat(1.5, wl, 0.001, "the label keeps 1.5 beside a status");
		ctx.EqualFloat(1, ws, 0.001, "the status keeps 1");
		ctx.EqualFloat(1.5, wa, 0.001, "the action keeps 1.5");

		// An empty status gives its share to the label; the action column stays where it was.
		Fit(false, wl, ws, wa);
		ctx.EqualFloat(2.5, wl, 0.001, "the label takes the empty status's share");
		ctx.EqualFloat(0, ws, 0.001, "an empty status holds no share");
		ctx.EqualFloat(1.5, wa, 0.001, "the action column keeps its share");
		float before = 1.5 / (1.5 + 1 + 1.5);
		float after = wa / (wl + wa);
		ctx.EqualFloat(before, after, 0.001, "the action column keeps its place in the row");

		// On a real list a row with no status and a long title is added, and a later change refits it.
		array<ref LF_UIPart> parts = {};
		parts.Insert(LF_UIPart.CreateList("Pages"));
		LF_UIScreen screen = LF_UIScreen.Open(parts);
		ctx.NotNull(screen, "a list screen assembles");
		if (!screen)
			return;

		ctx.True(screen.AddListRow("Pages", "PageRow0", "How to read this handbook", "", "#LF-Handbook_Read"), "a title row with no status is added");
		ctx.True(screen.AddListRow("Pages", "PageRow1", "Alley yard lease", "Yours", "#LF-Org_GiveUpLease"), "a lease row is added");
		ctx.True(screen.SetStatusRow("PageRow1", "Alley yard lease", "", "#LF-Org_TakeLease"), "a row's status can be emptied and refits");
		screen.Close();
	}
}
