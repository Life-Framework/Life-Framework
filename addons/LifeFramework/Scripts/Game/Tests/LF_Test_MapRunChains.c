// red-proof: 2026-11-22, cli test --no-build --red LF_Test_MapRunChains,LF_Test_EveronMapRuns: RED-OK (RedProof() skips MarkShown, so no stop shows and no badge matches a run).
//------------------------------------------------------------------------------------------------
//! map/run-chains: a badge belongs to the run whose stop stands at it, at that step; a stop two runs share
//! belongs to both; a stop no known place is near stays off the map (an undiscovered mine never leaks
//! through a chain); the card reads the next stop, or "last stop"; a connector is dashed.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_MapRunChains : LF_Test
{
	override string GetName()
	{
		return "map/run-chains";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_MapRunPoint Point(int run, int step, float x, float z)
	{
		LF_MapRunPoint p = new LF_MapRunPoint();
		p.m_iRun = run;
		p.m_iStep = step;
		p.m_vPos = Vector(x, 0, z);
		p.m_bFound = true;
		return p;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// Run 0, mining: desk, hardware, rocks, furnace, buyer. Run 1, fishing: pond, smokehouse, the same buyer.
		array<ref LF_MapRunPoint> points = {};
		points.Insert(Point(0, 0, 0, 0));
		points.Insert(Point(0, 1, 1000, 0));
		points.Insert(Point(0, 2, 2000, 0));
		points.Insert(Point(0, 3, 2010, 30));
		points.Insert(Point(0, 4, 3000, 500));
		points.Insert(Point(1, 0, 5000, 5000));
		points.Insert(Point(1, 1, 3200, 600));
		points.Insert(Point(1, 2, 3000, 500));

		// The player knows every place except the pond.
		array<vector> known = {"0 0 5", "1000 0 10", "2000 0 20", "3000 0 505", "3200 0 600"};
		if (!RedProof())
			LF_MapRunRules.MarkShown(points, known);

		ctx.False(points[5].m_bShown, "the unknown pond stays off the map");
		array<ref LF_MapRunMatch> matches = {};
		ctx.Equal(1, LF_MapRunRules.Match("2010 0 32", points, matches), "the furnace badge belongs to one run");
		if (matches.Count() == 1)
		{
			ctx.Equal(0, matches[0].m_iRun, "the mining run");
			ctx.Equal(3, matches[0].m_iStep, "at its nearest stop, the furnace (step 4)");
		}

		ctx.Equal(2, LF_MapRunRules.Match("3000 0 505", points, matches), "the dock buyer is the end of both runs");
		ctx.Equal(0, LF_MapRunRules.Match("5000 0 5000", points, matches), "hovering the unknown pond draws nothing");
		ctx.Equal(0, LF_MapRunRules.Match("8000 0 8000", points, matches), "a badge on no run draws nothing");

		string stepText;
		string countText;
		ctx.EqualStr("#LF-MapRun_Card", LF_MapRunRules.CardKey(2, 5, stepText, countText), "step 3 names the next stop");
		ctx.EqualStr("3", stepText, "step 3");
		ctx.EqualStr("5", countText, "of 5");
		ctx.EqualStr("#LF-MapRun_CardLast", LF_MapRunRules.CardKey(4, 5, stepText, countText), "the last step says so");

		array<float> dashes = {};
		int n = LF_MapRunRules.Dashes(0, 0, 150, 0, dashes);
		ctx.Equal(10, n, "a 150 px connector is ten dashes of 9 px with 6 px gaps");
		ctx.Equal(n * 4, dashes.Count(), "four numbers per dash");
		float end = dashes[dashes.Count() - 2];
		ctx.True(end <= 150 && end > 150 - LF_MapRunRules.DASH - LF_MapRunRules.GAP, "the last dash ends at the stop, never past it");
	}
}
