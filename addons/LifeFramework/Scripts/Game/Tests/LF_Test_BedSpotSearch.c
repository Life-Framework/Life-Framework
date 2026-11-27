// red-proof: observed 2026-11-27 `tools/cli test --no-build --red LF_Test_BedSpotSearch`: RED-OK spawn/bed-spot-search (fails with the hook on, 3 assertions); the old two-sided SitCandidates had no foot, head or wider-ring point, so those assertions fail on it too.
//! #5153: the clinic bed has a desk or a second bed close on both long sides, so the old search (two
//! sides, 0.4 m out) found no room and the patient woke under the frame. The search now also tries the
//! foot and head ends and rings further out, and never offers a point whose body overlaps the bed.
//! Pure bed-space geometry: no world, no town coordinates.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_BedSpotSearch : LF_Test
{
	override string GetName()
	{
		return "spawn/bed-spot-search";
	}

	override void Run(LF_TestContext ctx)
	{
		vector mins = "-0.58 0 -1.25";
		vector maxs = "0.58 0.66 1.25";
		vector standOffset = "0.85 0 0";

		array<vector> candidates = {};
		if (!RedProof())
			LF_BedWakePose.SitCandidates(mins, maxs, standOffset, candidates);

		ctx.True(candidates.Count() > 0, "the search offers sit points");
		if (candidates.IsEmpty())
		{
			ctx.Fail("no candidates: the rest is not checked");
			return;
		}

		vector preferred = LF_BedWakePose.SitLocal(mins, maxs, standOffset);
		ctx.True(vector.Distance(candidates[0], preferred) < 0.001, "the stand offset's side is tried first");

		bool otherSide;
		bool foot;
		bool head;
		bool wide;
		bool overlaps;
		foreach (vector candidate : candidates)
		{
			if (candidate[0] < mins[0])
				otherSide = true;

			if (candidate[2] < mins[2] && Math.AbsFloat(candidate[0]) < 0.001)
				foot = true;

			if (candidate[2] > maxs[2] && Math.AbsFloat(candidate[0]) < 0.001)
				head = true;

			if (candidate[0] > maxs[0] + LF_BedWakePose.SIT_OUT + LF_BedWakePose.RING_STEP * 0.99)
				wide = true;

			if (LF_BedWakePose.InsideBed(mins, maxs, candidate))
				overlaps = true;
		}

		ctx.True(otherSide, "the other long side is tried");
		ctx.True(foot, "the foot end is tried");
		ctx.True(head, "the head end is tried");
		ctx.True(wide, "a wider ring beside the bed is tried");
		ctx.False(overlaps, "no point puts the body inside the bed");

		// An open room: the first point wins. A wall along both long sides (0.5 m thick, from the
		// mattress edge): the foot end is the next nearest spot.
		array<vector> blockMins = {};
		array<vector> blockMaxs = {};
		ctx.True(LF_BedWakePose.FirstClear(candidates, blockMins, blockMaxs) == 0, "no obstacle: the first point");

		blockMins.Insert("-1.1 0 -5");
		blockMaxs.Insert("-0.58 2 5");
		blockMins.Insert("0.58 0 -5");
		blockMaxs.Insert("1.1 2 5");
		int footIndex = LF_BedWakePose.FirstClear(candidates, blockMins, blockMaxs);
		ctx.True(footIndex > 0, "walls beside the bed move the spot");
		if (footIndex > 0)
		{
			vector footSpot = candidates[footIndex];
			ctx.True(Math.AbsFloat(footSpot[2]) > maxs[2] && Math.AbsFloat(footSpot[0]) < 0.001, string.Format("walls beside the bed: the foot or head end is used (%1)", footSpot));
			vector footFacing = LF_BedWakePose.OutwardLocal(mins, maxs, footSpot);
			ctx.True(footFacing[2] != 0 && footFacing[0] == 0, "a foot spot faces along the bed");
		}

		// Both ends boxed in too: a ring further out on a long side.
		blockMins.Insert("-3 0 -2");
		blockMaxs.Insert("3 2 -1.25");
		blockMins.Insert("-3 0 1.25");
		blockMaxs.Insert("3 2 2");
		int ringIndex = LF_BedWakePose.FirstClear(candidates, blockMins, blockMaxs);
		ctx.True(ringIndex > 0, "walls and furniture at both ends still leave a spot");
		if (ringIndex > 0)
		{
			vector ringSpot = candidates[ringIndex];
			ctx.True(Math.AbsFloat(ringSpot[0]) > maxs[0] + LF_BedWakePose.SIT_OUT + LF_BedWakePose.RING_STEP, string.Format("a wider ring is used (%1)", ringSpot));
		}

		// Boxed in on every side: no spot, so the caller leaves the patient on the mattress.
		blockMins.Insert("-9 0 -9");
		blockMaxs.Insert("9 2 9");
		ctx.True(LF_BedWakePose.FirstClear(candidates, blockMins, blockMaxs) == -1, "a bed boxed in on every side has no spot");

		// The lay point is the middle of the bed's bounds, just above the top: not the origin on the floor.
		vector lay = LF_BedWakePose.LayLocal(mins, maxs);
		ctx.True(Math.AbsFloat(lay[0]) < 0.001 && Math.AbsFloat(lay[2]) < 0.001, "a patient lies in the middle of the bed");
		ctx.True(lay[1] > maxs[1] && lay[1] < maxs[1] + 0.1, string.Format("a patient lies on the mattress top, not under the frame (y %1, top %2)", lay[1], maxs[1]));

		// Facing: a side point faces across the bed, an end point along it.
		vector right = LF_BedWakePose.OutwardLocal(mins, maxs, "1 0 0");
		vector left = LF_BedWakePose.OutwardLocal(mins, maxs, "-1 0 0");
		vector headEnd = LF_BedWakePose.OutwardLocal(mins, maxs, "0 0 1.7");
		vector footEnd = LF_BedWakePose.OutwardLocal(mins, maxs, "0 0 -1.7");
		ctx.True(right[0] == 1 && left[0] == -1, "side points face across the bed");
		ctx.True(headEnd[2] == 1 && footEnd[2] == -1, "end points face along the bed");
	}
}
