// red-proof: 2026-11-23, `--only LF_Test_BusArrivalFacing` before the change: LF_BusArrival.FacingYaw did not exist, so the test could not compile.
//! bus/arrival-facing: a passenger set down at a stop looks the way the sign faces, out to the street,
//! not at the wall the sign stands against (first-2-hours re-review N6).
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_BusArrivalFacing : LF_Test
{
	override string GetName()
	{
		return "bus/arrival-facing";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.EqualFloat(0, LF_BusArrival.FacingYaw("0 0 1"), 0.01, "a sign facing +Z: the passenger looks along +Z");
		ctx.EqualFloat(90, LF_BusArrival.FacingYaw("1 0 0"), 0.01, "a sign facing +X: the passenger looks along +X");
		ctx.EqualFloat(-90, LF_BusArrival.FacingYaw("-1 0 0"), 0.01, "a sign facing -X: the passenger looks along -X");
		ctx.EqualFloat(180, Math.AbsFloat(LF_BusArrival.FacingYaw("0 0 -1")), 0.01, "a sign facing -Z: the passenger looks along -Z");
		ctx.EqualFloat(90, LF_BusArrival.FacingYaw("1 0.6 0"), 0.01, "a tilted sign still gives a flat heading");
		ctx.EqualFloat(0, LF_BusArrival.FacingYaw("0 1 0"), 0.01, "a sign facing straight up gives a heading of 0, not NaN");
	}
}
