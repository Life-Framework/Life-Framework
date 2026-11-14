// red-proof: observed 2026-11-13 with `tools\cli test --no-build --only LF_Test_SpikeStripGeometry`: SweepCrosses testing only the end point (`from = to`) failed `a path across the middle crosses`, `a fast path that jumps the strip between ticks still crosses` and `a turned strip catches a path along z`; restored, green.
// tier: LOGIC
//! The spike strip's pure rules (#646): a wheel path that crosses the footprint is caught even when
//! it jumps clean over the 30 cm strip between two ticks, a path beside it or short of it is not;
//! the strip runs across a nearby road on the officer's side, else where the officer faces; the
//! ground rule is the placement tool's 0.25 m; the laid transform follows a slope.
class LF_Test_SpikeStripGeometry : LF_Test
{
	override string GetName()
	{
		return "police/spike-strip-geometry";
	}

	override void Run(LF_TestContext ctx)
	{
		vector centre = "0 0 0";
		vector alongZ = "0 0 1";
		float hl = 1.75;
		float hw = 0.15;

		// A strip running along z, cars driving along x.
		ctx.True(LF_SpikeStripGeometry.SweepCrosses(centre, alongZ, hl, hw, "-2 0 0", "2 0 0"), "a path across the middle crosses");
		ctx.True(LF_SpikeStripGeometry.SweepCrosses(centre, alongZ, hl, hw, "-5 0 1", "5 0 1"), "a fast path that jumps the strip between ticks still crosses");
		ctx.True(LF_SpikeStripGeometry.SweepCrosses(centre, alongZ, hl, hw, "-0.1 0 1.7", "0.1 0 1.7"), "a path near the end crosses");
		ctx.False(LF_SpikeStripGeometry.SweepCrosses(centre, alongZ, hl, hw, "-2 0 2", "2 0 2"), "a path past the end of the strip does not cross");
		ctx.False(LF_SpikeStripGeometry.SweepCrosses(centre, alongZ, hl, hw, "-2 0 0", "-0.5 0 0"), "a path that stops short does not cross");
		ctx.False(LF_SpikeStripGeometry.SweepCrosses(centre, alongZ, hl, hw, "0.3 0 -1", "0.3 0 1"), "a path beside the strip, along it, does not cross");
		ctx.True(LF_SpikeStripGeometry.SweepCrosses(centre, alongZ, hl, hw, "0.1 0 -1", "0.1 0 1"), "a path along the strip on top of it crosses");

		// The same strip turned to run along x.
		vector alongX = "1 0 0";
		ctx.True(LF_SpikeStripGeometry.SweepCrosses("10 0 10", alongX, hl, hw, "11 0 8", "11 0 12"), "a turned strip catches a path along z");
		ctx.False(LF_SpikeStripGeometry.SweepCrosses("10 0 10", alongX, hl, hw, "12 0 8", "12 0 12"), "a turned strip misses a path past its end");

		// Which way it runs.
		vector noRoad = LF_PlacementGeometry.Axis("0 -0.5 0.8", false, vector.Zero);
		ctx.EqualFloat(1, noRoad[2], 0.001, "with no road the strip runs where the officer faces, flattened");
		vector acrossRoad = LF_PlacementGeometry.Axis("0.6 0 0.8", true, "1 0 0");
		ctx.EqualFloat(0, acrossRoad[0], 0.001, "with a road along x the strip runs straight across it (x)");
		ctx.EqualFloat(1, acrossRoad[2], 0.001, "with a road along x the strip runs straight across it (z)");
		vector otherSide = LF_PlacementGeometry.Axis("0 0 -1", true, "1 0 0");
		ctx.EqualFloat(-1, otherSide[2], 0.001, "an officer on the other verge lays it the other way");

		// The road segment nearest the officer.
		array<vector> road = {"0 0 0", "10 0 0", "10 0 10"};
		float distance;
		vector first = LF_PlacementGeometry.NearestSegmentDirection(road, "3 0 1", distance);
		ctx.EqualFloat(1, first[0], 0.001, "the first segment runs along x");
		ctx.EqualFloat(1, distance, 0.001, "one metre from the first segment");
		vector second = LF_PlacementGeometry.NearestSegmentDirection(road, "9 0 6", distance);
		ctx.EqualFloat(1, second[2], 0.001, "the second segment runs along z");
		array<vector> onePoint = {"0 0 0"};
		ctx.True(LF_PlacementGeometry.NearestSegmentDirection(onePoint, "1 0 1", distance) == vector.Zero, "one point is no road");

		// The ground rule.
		ctx.True(LF_SpikeStripGeometry.GroundAcceptable(0, 0.2, 0.25), "a 20 cm rise along the strip is fine");
		ctx.False(LF_SpikeStripGeometry.GroundAcceptable(0, 0.3, 0.25), "a 30 cm kerb is refused");

		// The laid transform.
		vector flat[4];
		LF_PlacementGeometry.Transform("0 0 0", "0 0 3.5", flat);
		ctx.EqualFloat(1, flat[2][2], 0.001, "forward runs from start to end");
		ctx.EqualFloat(1, flat[0][0], 0.001, "right is x");
		ctx.EqualFloat(1, flat[1][1], 0.001, "up is up on flat ground");
		ctx.EqualFloat(1.75, flat[3][2], 0.001, "the origin is the middle");
		vector slope[4];
		LF_PlacementGeometry.Transform("0 0 0", "0 0.35 3.5", slope);
		ctx.InRange(slope[2][1], 0.09, 0.11, "on a slope the strip tilts with it");
		ctx.InRange(slope[1][1], 0.99, 1.0, "and its up stays nearly up");
	}
}
