// red-proof: `tools\cli test --no-build --red LF_Test_SignFacePlan,LF_Test_SignStyles` (2026-11-18): RED-OK signs/face-plan ("and the cut rows are counted (expected 3, got 0)").

//! A styled sign face's plan (docs/architecture/sign-system.md): the title and price rows fit the shown
//! area, rows sit under the title without overlapping, one font lines the prices up, a long label
//! shrinks the font, and a list longer than a board holds is cut at MAX_ROWS.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_SignFacePlan : LF_Test
{
	override string GetName()
	{
		return "signs/face-plan";
	}

	//------------------------------------------------------------------------------------------------
	static LF_SignStyleDef ChalkLike()
	{
		LF_SignStyleDef style = new LF_SignStyleDef();
		style.m_sId = "test_chalk";
		style.m_fMargin = 0.08;
		style.m_fTitleShare = 0.3;
		style.m_iTitleFontMax = 200;
		style.m_iRowFontMax = 110;
		style.m_bUpperTitle = true;
		style.m_sLeader = ".";
		style.m_fGhostAlpha = 0.1;
		style.m_vGhostShift = "14 9 0";
		return style;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		vector shown = "1920 1080 0";
		LF_SignStyleDef style = ChalkLike();

		// Title only: the title takes the whole inner area.
		array<string> none = {};
		LF_SignFacePlan plan = new LF_SignFacePlan();
		if (!RedProof())
			plan = LF_SignFacePlan.Compose(shown, style, "Back room", none, none);
		ctx.EqualStr("BACK ROOM", plan.m_sTitle, "the chalk title is in capitals");
		ctx.EqualFloat(1920 - 2 * 86.4, plan.m_vTitleSize[0], 0.5, "a title-only face spans the width less the margins");
		ctx.EqualFloat(1080 - 2 * 86.4, plan.m_vTitleSize[1], 0.5, "and the height less the margins");
		ctx.True(plan.m_iTitleFont > 24 && plan.m_iTitleFont <= 200, "the title font is inside the style's range");
		ctx.True(plan.m_bGhost, "a style with ghost alpha draws a ghost");

		// Four price rows under a title.
		array<string> labels = {"Coffee", "Bread", "Fishing bait", "Jerry can of fuel"};
		array<string> values = {"2", "3", "5", "25"};
		if (!RedProof())
			plan = LF_SignFacePlan.Compose(shown, style, "Today's prices", labels, values);
		ctx.Equal(4, plan.m_aRows.Count(), "four rows are planned");
		float bottom = plan.m_vTitlePos[1] + plan.m_vTitleSize[1];
		int font = -1;
		foreach (int i, LF_SignFaceRowPlan row : plan.m_aRows)
		{
			ctx.True(row.m_vPos[1] >= bottom - 0.5, string.Format("row %1 starts below the title or the row above", i));
			bottom = row.m_vPos[1] + row.m_vSize[1];
			ctx.True(row.m_vPos[0] + row.m_vSize[0] <= shown[0] + 0.5, string.Format("row %1 fits the width", i));
			if (font < 0)
				font = row.m_iFont;
			ctx.Equal(font, row.m_iFont, string.Format("row %1 shares the font, so the prices line up", i));
			ctx.True(row.m_sLeader.Length() >= 2, string.Format("row %1 has a dot leader", i));
		}
		ctx.True(bottom <= shown[1] + 0.5, "the last row ends on the face");
		ctx.True(font <= style.m_iRowFontMax && font >= LF_SignFacePlan.ROW_FONT_MIN, "the row font is inside its range");

		// A long label shrinks the font so label, leader and value still fit one line.
		array<string> longLabels = {"Coffee", "A very long description of a hardware item for sale"};
		array<string> longValues = {"2", "125"};
		if (!RedProof())
			plan = LF_SignFacePlan.Compose(shown, style, "Prices", longLabels, longValues);
		ctx.Equal(2, plan.m_aRows.Count(), "two rows are planned");
		if (plan.m_aRows.Count() == 2)
		{
			LF_SignFaceRowPlan wide = plan.m_aRows[1];
			float ems = (wide.m_sLabel.Length() + wide.m_sValue.Length()) * LF_SignFacePlan.ROW_EM_PER_CHAR;
			ctx.True(ems * wide.m_iFont <= wide.m_vSize[0], "the long row's text fits its width");
			ctx.True(wide.m_iFont < style.m_iRowFontMax, "the long label shrank the font below the style's cap");
		}

		// More rows than a board holds: cut, and counted.
		array<string> many = {};
		array<string> manyValues = {};
		for (int n = 0; n < LF_SignFacePlan.MAX_ROWS + 3; n++)
		{
			many.Insert("Item " + n);
			manyValues.Insert(n.ToString());
		}
		if (!RedProof())
			plan = LF_SignFacePlan.Compose(shown, style, "Prices", many, manyValues);
		ctx.Equal(LF_SignFacePlan.MAX_ROWS, plan.m_aRows.Count(), "a long list is cut at MAX_ROWS");
		ctx.Equal(3, plan.m_iDropped, "and the cut rows are counted");

		// A row with no value has no leader.
		LF_SignFaceRowPlan bare = new LF_SignFaceRowPlan();
		bare.m_iFont = 60;
		bare.m_sLabel = "Closed today";
		LF_SignFacePlan.FillLeader(bare, ".", 1700);
		ctx.EqualStr("", bare.m_sLeader, "a row with no value draws no leader");
	}
}
