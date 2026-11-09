// red-proof: observed 2026-11-08 (tools/cli test --no-build --tier all): with the vehicle
// distance dropped from LF_VehicleGarageComponent.SharesGarage (return true), the run printed
// "[LFTEST] FAIL security/garage-store-range" / "- a vehicle in an impound across the map
// cannot be stored". Restored, it passes (3 assertions).

// tier: LOGIC
//! Storing a vehicle requires the vehicle, not only the player, at the garage.
class LF_Test_GarageStoreRange : LF_Test
{
	override string GetName()
	{
		return "security/garage-store-range";
	}

	override void Run(LF_TestContext ctx)
	{
		vector garage = "500 0 500";
		vector player = "505 0 500";
		vector parked = "490 0 505";
		vector impound = "3000 0 3000";
		ctx.True(LF_VehicleGarageComponent.SharesGarage(garage, player, parked), "a vehicle parked at the garage can be stored");
		ctx.False(LF_VehicleGarageComponent.SharesGarage(garage, player, impound), "a vehicle in an impound across the map cannot be stored");
		ctx.False(LF_VehicleGarageComponent.SharesGarage(garage, impound, player), "a player away from the garage cannot store");
	}
}
