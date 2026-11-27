// red-proof: (#5147) `--red LF_Test_UIRowFit` also falls back to the fixed weights for a 26-character label, so "a long label ... takes more than the default share" fails. Earlier: `toolscli test --no-build --red LF_Test_UIRowFit` skips the fit (the old fixed 1.5 : 1 : 1.5 shares) so the empty-status label keeps 1.5 and the
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

		// #5147: a long label beside a status ("Carrying without a license", 26 characters) takes width from the action
		// column; the status keeps its share, and a short label keeps the defaults.
		LF_UIRowFit.WeightsFor(true, 26, wl, ws, wa);
		if (RedProof())
			LF_UIRowFit.Weights(true, wl, ws, wa);

		ctx.True(wl > LF_UIRowFit.WEIGHT_LABEL, "a long label beside a status takes more than the default share");
		ctx.EqualFloat(1, ws, 0.001, "the status keeps its share beside a long label");
		ctx.EqualFloat(4, wl + ws + wa, 0.001, "the row total stays the same");
		ctx.True(wa >= 1, "the action column keeps at least one share");
		LF_UIRowFit.WeightsFor(true, 60, wl, ws, wa);
		ctx.EqualFloat(2, wl, 0.001, "the label share stops at the cap");
		LF_UIRowFit.WeightsFor(true, 12, wl, ws, wa);
		ctx.EqualFloat(1.5, wl, 0.001, "a short label keeps the default share");
		ctx.EqualFloat(1.5, wa, 0.001, "a short label leaves the action column alone");

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
