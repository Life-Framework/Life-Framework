// red-proof: drop the empty-identifier guard in EL_VehicleLockComponent.IdentifiersMatch
// (return a == b) and run `tools\cli test --tier fast`; the empty-identifier assertions fail
// because "" == "" matches, so an unbound house key opens every house.

// tier: LOGIC
class EL_Test_HouseKeyMatch : EL_Test
{
	override string GetName()
	{
		return "houses/key-match";
	}

	override void Run(EL_TestContext ctx)
	{
		ctx.True(EL_VehicleLockComponent.IdentifiersMatch("house-a", "house-a"), "matching house identifiers match");
		ctx.False(EL_VehicleLockComponent.IdentifiersMatch("house-a", "house-b"), "different house identifiers do not match");
		ctx.False(EL_VehicleLockComponent.IdentifiersMatch("", "house-a"), "empty key identifier matches nothing");
		ctx.False(EL_VehicleLockComponent.IdentifiersMatch("house-a", ""), "empty house identifier matches nothing");
		ctx.False(EL_VehicleLockComponent.IdentifiersMatch("", ""), "two empty identifiers do not match");
	}
};