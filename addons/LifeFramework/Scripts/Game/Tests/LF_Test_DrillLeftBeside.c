// red-proof: 2026-11-11, ToolRestSpot returned the robber's origin (the old LeaveTool placement),
//   cli test --no-build --only LF_Test_DrillLeftBeside:
//   [LFTEST]   - the drill rests clear of the robber (0 m away, want 0.9 m or more)
//   [LFTEST]   - it lies on the robber's right-hand side (expected 253.4, got 252.4)
// Fixed (1 m to the right, ground height); green.
// tier: LOGIC
//! A drill left at a vault or the cash van box rests beside the robber, not under their feet: the bench drill
//! spawned at the robber's origin and the robber stood inside it for the whole work animation (#571 trailer take).
class LF_Test_DrillLeftBeside : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "crime/drill-left-beside";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		vector robber = "198.6 0 252.4";
		vector spot = LF_Bank.ToolRestSpot(robber, "0 0 1");
		float flat = vector.Distance(Vector(spot[0], 0, spot[2]), Vector(robber[0], 0, robber[2]));
		ctx.True(flat >= 0.9, string.Format("the drill rests clear of the robber (%1 m away, want 0.9 m or more)", flat));
		ctx.EqualFloat(robber[1], spot[1], 0.001, "the rest spot keeps the robber's ground height");
		ctx.EqualFloat(robber[2] + LF_Bank.TOOL_REST_SIDE, spot[2], 0.001, "it lies on the robber's right-hand side");

		// A tilted or zero right vector still gives a level spot to the side.
		spot = LF_Bank.ToolRestSpot(robber, "0 0.5 0");
		flat = vector.Distance(Vector(spot[0], 0, spot[2]), Vector(robber[0], 0, robber[2]));
		ctx.True(flat >= 0.9, "a degenerate right vector still moves the drill clear");
		ctx.EqualFloat(robber[1], spot[1], 0.001, "and keeps it on the ground height");
	}
}
