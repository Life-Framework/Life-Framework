// red-proof: drop the empty-identifier guard in LF_VehicleLockComponent.IdentifiersMatch
// (return a == b) and run `tools\cli test --tier fast`; the two empty-identifier
// assertions fail because "" == "" matches, and an unbound key opens everything.

// tier: LOGIC
class LF_Test_VehicleLockIdMatch : LF_Test
{
	override string GetName()
	{
		return "vehiclelock/id-match";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.True(LF_VehicleLockComponent.IdentifiersMatch("abc", "abc"), "matching non-empty identifiers match");
		ctx.False(LF_VehicleLockComponent.IdentifiersMatch("abc", "abd"), "different identifiers do not match");
		ctx.False(LF_VehicleLockComponent.IdentifiersMatch("", "abc"), "empty key identifier matches nothing");
		ctx.False(LF_VehicleLockComponent.IdentifiersMatch("abc", ""), "empty vehicle identifier matches nothing");
		ctx.False(LF_VehicleLockComponent.IdentifiersMatch("", ""), "two empty identifiers do not match");
	}
};