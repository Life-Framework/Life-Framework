// red-proof: 2026-11-22, `--red LF_Test_TreeFallPlan,LF_Test_TreeFallLogs`: RED PROOF OK (all 2 listed test(s) fail with the hook on); the hook skips the log plan / the drop, so the yield-count, ground and sweep checks fail.
//! lumber/tree-fall-plan: where a felled tree's pieces go. One log spot per log the yield gives, all on the
//! far side from the feller along the fallen line, none overlapping; the fall angle rises to the landing
//! and bounces once; the branch count is capped; the loose-log book forgets a taken or moved log, deletes
//! one left lying past its time, and trims the oldest past the cap.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_TreeFallPlan : LF_Test
{
	override string GetName()
	{
		return "lumber/tree-fall-plan";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		CheckYaw(ctx);
		CheckLogs(ctx);
		CheckAngle(ctx);
		CheckBook(ctx);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckYaw(LF_TestContext ctx)
	{
		// The player stands south (-z) of the tree: it falls north (+z), yaw 0; standing east, it falls west.
		ctx.EqualFloat(0, LF_TreeFallPlan.FallYaw("0 0 -2", "0 0 0"), 0.01, "a player to the south fells it north");
		ctx.EqualFloat(-90, LF_TreeFallPlan.FallYaw("2 0 0", "0 0 0"), 0.01, "a player to the east fells it west");
		vector dir = LF_TreeFallPlan.Dir(-90);
		ctx.EqualFloat(-1, dir[0], 0.001, "yaw -90 points along -x");

		// Tipping the up axis 90 degrees about up x dir lays it along dir.
		vector axis = LF_TreeFallPlan.Cross(vector.Up, dir);
		vector laid = LF_TreeFallPlan.Rotate(vector.Up, axis, 90);
		ctx.EqualFloat(-1, laid[0], 0.001, "the trunk lands along the fall direction");
		ctx.EqualFloat(0, laid[1], 0.001, "the landed trunk is flat");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckLogs(LF_TestContext ctx)
	{
		vector foot = "100 10 200";
		array<int> counts = { 1, 2, 3, 6 };
		foreach (int count : counts)
		{
			array<ref LF_TreeFallSpot> spots = {};
			if (!RedProof())
				spots = LF_TreeFallPlan.LogSpots(foot, 90, 14, 2.4, count);

			ctx.Equal(count, spots.Count(), string.Format("%1 logs from the yield give %1 log spots", count));
			for (int i = 0; i < spots.Count(); i++)
			{
				vector at = spots[i].m_vPos;
				ctx.True(at[0] > foot[0], "a log lies on the side the tree fell (+x)");
				ctx.True(vector.DistanceXZ(at, foot) < 14, "a log lies within the tree's height");
				for (int j = 0; j < i; j++)
				{
					ctx.True(vector.DistanceXZ(at, spots[j].m_vPos) > 0.8, "no two logs share a spot");
				}
			}
		}

		// A short trunk with many logs: they stagger side by side, and two on the same side stay a log length apart.
		array<ref LF_TreeFallSpot> tight = LF_TreeFallPlan.LogSpots(foot, 90, 5, 2.4, 4);
		for (int k = 2; k < tight.Count(); k++)
		{
			ctx.True(vector.DistanceXZ(tight[k].m_vPos, tight[k - 2].m_vPos) >= 2.4, "two logs on one side of a short trunk do not overlap");
		}

		ctx.Equal(0, LF_TreeFallPlan.LogSpots(foot, 0, 10, 2, 0).Count(), "no logs, no spots");
		ctx.Equal(2, LF_TreeFallPlan.DebrisCount(0), "a small tree leaves two branch props");
		ctx.Equal(LF_TreeFallPlan.MAX_DEBRIS, LF_TreeFallPlan.DebrisCount(40), "the branch props are capped");
		array<ref LF_TreeFallSpot> a = LF_TreeFallPlan.DebrisSpots(foot, 0, 12, 4, 77);
		array<ref LF_TreeFallSpot> b = LF_TreeFallPlan.DebrisSpots(foot, 0, 12, 4, 77);
		ctx.Equal(4, a.Count(), "four branch spots");
		ctx.EqualFloat(a[2].m_vPos[2], b[2].m_vPos[2], 0.001, "the same seed gives the same branch spots on every machine");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckAngle(LF_TestContext ctx)
	{
		ctx.EqualFloat(0, LF_TreeFallPlan.AngleAt(0), 0.001, "the tree starts upright");
		float early = LF_TreeFallPlan.AngleAt(LF_TreeFallPlan.FALL_MS / 4);
		float late = LF_TreeFallPlan.AngleAt(LF_TreeFallPlan.FALL_MS * 3 / 4);
		ctx.True(early < late - early, "it falls slowly at first and fast at the end");
		ctx.EqualFloat(LF_TreeFallPlan.LAND_DEG, LF_TreeFallPlan.AngleAt(LF_TreeFallPlan.FALL_MS), 0.01, "it lands at the landing angle");
		ctx.True(LF_TreeFallPlan.AngleAt(LF_TreeFallPlan.FALL_MS + LF_TreeFallPlan.BOUNCE_MS / 2) < LF_TreeFallPlan.LAND_DEG, "it bounces once on landing");
		ctx.EqualFloat(LF_TreeFallPlan.LAND_DEG, LF_TreeFallPlan.AngleAt(LF_TreeFallPlan.FALL_MS * 3), 0.01, "then it lies still");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckBook(LF_TestContext ctx)
	{
		LF_LooseLogBook book = new LF_LooseLogBook();
		book.Add(EntityID.INVALID, "0 0 0", 100);
		LF_LooseLog log = book.m_aLogs[0];
		int life = 600;
		ctx.Equal(LF_LooseLogFate.KEEP, LF_LooseLogBook.Fate(log, true, false, "0.5 0 0", 200, life), "a fresh log lying where it fell is kept");
		ctx.Equal(LF_LooseLogFate.DELETE, LF_LooseLogBook.Fate(log, true, false, "0.5 0 0", 100 + life, life), "a log left lying past its time is deleted");
		ctx.Equal(LF_LooseLogFate.FORGET, LF_LooseLogBook.Fate(log, true, true, "0 0 0", 100 + life, life), "a log a player holds is never deleted");
		ctx.Equal(LF_LooseLogFate.FORGET, LF_LooseLogBook.Fate(log, false, false, "0 0 0", 200, life), "a log that is gone (loaded, sold) is forgotten");
		ctx.Equal(LF_LooseLogFate.FORGET, LF_LooseLogBook.Fate(log, true, false, "20 0 0", 100 + life, life), "a log a player carried away is theirs");
		book.Add(EntityID.INVALID, "0 0 0", 110);
		book.Add(EntityID.INVALID, "0 0 0", 120);
		ctx.Equal(1, book.OverCap(2), "three lying logs over a cap of two: the oldest goes");
		ctx.Equal(0, book.OverCap(5), "under the cap nothing goes");
	}
}
