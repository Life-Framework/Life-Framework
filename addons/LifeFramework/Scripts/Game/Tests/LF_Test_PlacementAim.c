// red-proof: observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_PlacementAim`: STEP_DEGREES = 10 failed `a 15 degree turn right from 350 wraps to 5 (0 not in [4.99, 5.01])`; restored, green.
// tier: LOGIC
//! Aimed placement (#1028): the geometry the ghost follows, with no world. The aim point is where the
//! ray strikes (or its far end), kept within the aim reach of the player; turning moves in 15 degree
//! steps (1 fine) and wraps; a thing placed across a road snaps to the road's axis, either way
//! across, with a little turn either side the server still accepts; a wall face gives the heading out
//! of it and refuses a floor or ceiling.
class LF_Test_PlacementAim : LF_Test
{
	override string GetName()
	{
		return "placement/aim";
	}

	override void Run(LF_TestContext ctx)
	{
		// --- the aim ray to a spot, and the reach clamp
		bool clamped;
		vector player = "100 5 100";
		vector eye = "100 6.7 100";
		vector hit = "102 5 101";
		vector near = LF_PlacementGeometry.AimPoint(eye, "0.89 -0.45 0.45", true, hit, player, 4, 16, clamped);
		ctx.False(clamped, "a strike 2.2 m away is inside a 4 m reach");
		ctx.InRange(near[0], 101.99, 102.01, "the aim point is where the ray struck (x)");
		ctx.InRange(near[2], 100.99, 101.01, "the aim point is where the ray struck (z)");

		vector far = LF_PlacementGeometry.AimPoint(eye, "1 0 0", true, "110 5 100", player, 4, 16, clamped);
		ctx.True(clamped, "a strike 10 m away is cut by the reach");
		ctx.InRange(far[0], 103.99, 104.01, "the clamped point lies 4 m out along the same bearing (x)");
		ctx.InRange(far[2], 99.99, 100.01, "the clamped point lies 4 m out along the same bearing (z)");

		vector level = LF_PlacementGeometry.AimPoint(eye, "0 0 1", false, vector.Zero, player, 4, 16, clamped);
		ctx.True(clamped, "a level look with no strike points at its far end, which the reach cuts");
		ctx.InRange(level[2], 103.99, 104.01, "the level look lands 4 m ahead");
		ctx.InRange(level[1], 4.99, 5.01, "and at the player's own height, for the ground probe to settle");

		// --- rotation in steps and fine, wrapping both ways
		float yaw = LF_PlacementGeometry.TurnYaw(350, 1, LF_PlacementGeometry.STEP_DEGREES);
		ctx.InRange(yaw, 4.99, 5.01, "a 15 degree turn right from 350 wraps to 5");
		yaw = LF_PlacementGeometry.TurnYaw(5, -1, LF_PlacementGeometry.STEP_DEGREES);
		ctx.InRange(yaw, 349.99, 350.01, "a 15 degree turn left from 5 wraps to 350");
		yaw = LF_PlacementGeometry.TurnYaw(90, 3, LF_PlacementGeometry.FINE_DEGREES);
		ctx.InRange(yaw, 92.99, 93.01, "three fine steps are 3 degrees");
		vector east = LF_PlacementGeometry.HeadingOf(90);
		ctx.InRange(east[0], 0.99, 1.01, "yaw 90 heads along +x");
		ctx.InRange(LF_PlacementGeometry.YawOf("0 0 -1"), 179.99, 180.01, "heading -z is yaw 180");

		// --- snap to a road axis: the road runs along +x, across is +z (yaw 90 off its along-axis is 0 or 180)
		vector roadAlongX = "1 0 0";
		float snapped = LF_PlacementGeometry.SnapYaw(LF_EPlacementSnap.ACROSS_ROAD, 4, true, roadAlongX);
		ctx.InRange(snapped, 3.99, 4.01, "4 degrees off across is kept (within the slack)");
		snapped = LF_PlacementGeometry.SnapYaw(LF_EPlacementSnap.ACROSS_ROAD, 60, true, roadAlongX);
		ctx.InRange(snapped, 9.99, 10.01, "60 degrees off across is held to the slack (10) on the nearer side");
		snapped = LF_PlacementGeometry.SnapYaw(LF_EPlacementSnap.ACROSS_ROAD, 200, true, roadAlongX);
		ctx.InRange(snapped, 189.99, 190.01, "facing the other way across keeps that way, held to the slack");
		ctx.True(LF_PlacementGeometry.AcrossAngle(LF_PlacementGeometry.HeadingOf(snapped), roadAlongX) <= LF_PlacementService.TOLERANCE_DEGREES, "the snapped heading is one the server accepts across the road");
		ctx.True(LF_PlacementGeometry.ROAD_SLACK < LF_PlacementService.TOLERANCE_DEGREES, "the ghost's slack is under the server's tolerance");
		ctx.InRange(LF_PlacementGeometry.SnapYaw(LF_EPlacementSnap.ACROSS_ROAD, 60, false, roadAlongX), 59.99, 60.01, "with no road near the turn is free");
		ctx.InRange(LF_PlacementGeometry.SnapYaw(LF_EPlacementSnap.GROUND, 60, true, roadAlongX), 59.99, 60.01, "a ground placeable ignores a road");

		// --- snap to a wall normal
		vector facing = LF_PlacementGeometry.WallForward("0 0.1 -1");
		ctx.InRange(facing[2], -1.01, -0.99, "a wall facing -z gives the heading -z (its normal flattened)");
		ctx.True(LF_PlacementGeometry.WallForward("0 1 0") == vector.Zero, "a floor is not a wall");
		ctx.True(LF_PlacementGeometry.WallForward("0 -1 0") == vector.Zero, "a ceiling is not a wall");
		vector against = LF_PlacementGeometry.WallOrigin("10 2 5", "0 0 -1", 0.05);
		ctx.InRange(against[2], 4.94, 4.96, "the thing lies 5 cm out of the face");
		ctx.InRange(against[1], 1.99, 2.01, "at the hit's height");
	}
}
