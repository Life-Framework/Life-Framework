// red-proof: change GetCornerPositions to use `size[0] / 2` (halving the extents)
// or mix up the sign of one axis, then run `tools\cli test --tier fast` on the
// main serial thread - the corner assertions fail because the four corners are
// no longer the full ±size box around the origin.

// tier: LOGIC
class EL_Test_DebugAreaBoundary : EL_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "debugworld/area-boundary-corners";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(EL_TestContext ctx)
	{
		vector corners[4];
		vector origin = Vector(100, 0, 200);
		vector size = Vector(5, 1, 8);

		EL_DebugAreaBoundary.GetCornerPositions(origin, size, corners);

		ctx.True(corners[0] == origin + Vector(-5, 0, -8), "corner 0 is origin + (-x,-z)");
		ctx.True(corners[1] == origin + Vector(-5, 0, 8), "corner 1 is origin + (-x,+z)");
		ctx.True(corners[2] == origin + Vector(5, 0, -8), "corner 2 is origin + (+x,-z)");
		ctx.True(corners[3] == origin + Vector(5, 0, 8), "corner 3 is origin + (+x,+z)");

		// Each corner keeps the origin's Y (poles snap to terrain at spawn).
		for (int i = 0; i < 4; i++)
			ctx.True(corners[i][1] == 0, string.Format("corner %1 keeps the origin height", i));

		// Zero size collapses all corners onto the origin; negatives must mirror.
		EL_DebugAreaBoundary.GetCornerPositions(origin, Vector(0, 1, 0), corners);
		for (int i = 0; i < 4; i++)
			ctx.True(corners[i] == origin, string.Format("zero size collapses corner %1 onto origin", i));
	}
}