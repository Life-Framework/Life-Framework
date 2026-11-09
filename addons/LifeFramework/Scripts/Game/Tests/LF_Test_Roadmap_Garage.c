// red-proof: observed 2026-11-08 with `tools\cli test --no-build --only ...`: the seated-player
// check in LF_Garage.CanPark skipped failed it with `- park refuses a seated player`
// (`SUMMARY tier=only passed=0 failed=4 total=4`, roadmap/garage among them). Restored; green.

// tier: LOGIC
//! Park rules at the garage: the owner, on foot, at a garage.
class LF_Test_Roadmap_Garage : LF_Test
{
	override string GetName()
	{
		return "roadmap/garage";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.False(LF_Garage.CanPark(false, false, true), "a stranger cannot park the vehicle");
		ctx.False(LF_Garage.CanPark(true, true, true), "park refuses a seated player");
		ctx.False(LF_Garage.CanPark(true, false, false), "park refuses when no garage is nearby");
		ctx.True(LF_Garage.CanPark(true, false, true), "the owner on foot can park");
		ctx.False(LF_Garage.ParkAllowed(true, true, true), "the store action is greyed out for a seated player");
		ctx.True(LF_Garage.ParkAllowed(true, false, true), "the store action is offered on foot at a garage");
		ctx.False(LF_Garage.WithinReach(null, null), "reach needs a player and a desk");
	}
}
