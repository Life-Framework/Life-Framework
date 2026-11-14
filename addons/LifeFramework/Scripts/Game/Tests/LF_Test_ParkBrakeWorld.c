// red-proof: observed 2026-11-14 with `tools\cli test --only LF_Test_ParkBrakeWorld`: expected the brake OFF on a fresh S105 instead of on,
// `- spawns with the handbrake on: {D8C109C681E7AAF1}Prefabs/Vehicles/Wheeled/M997/LF_M997_Ambulance.et` (one such line per prefab, seven in all). Restored; green.
// tier: WORLD
//! Parked and spawned cars hold on slopes (#979). Measured on a 19 degree ramp (`park-brake-proof`):
//! a car the garage, lot or street spawns comes up with the vanilla persistent handbrake on, and that
//! is what holds an empty car. Since 1.9 an empty car also self-steers, so an engine update that
//! flips the prefab default would let every spawned car roll. This test names that default for
//! every vehicle prefab we spawn, so an update turns it red before a player sees a car roll.
class LF_Test_ParkBrakeWorld : LF_Test
{
	override string GetName()
	{
		return "vehicles/park-brake-world";
	}

	override void Run(LF_TestContext ctx)
	{
		array<ResourceName> prefabs = {
			LF_VehicleCatalog.S105_LOCKED, LF_VehicleCatalog.UAZ469_LOCKED, LF_VehicleCatalog.S1203_LOCKED,
			LF_VehicleCatalog.UAZ452_LOCKED, LF_VehicleCatalog.M923_LOCKED,
			"{EFEB161F9BC2031D}Prefabs/Vehicles/Wheeled/UAZ469/LF_UAZ469_Police.et",
			"{D8C109C681E7AAF1}Prefabs/Vehicles/Wheeled/M997/LF_M997_Ambulance.et"
		};

		foreach (ResourceName prefab : prefabs)
		{
			IEntity car = LF_Utils.SpawnEntityPrefab(prefab, "300 0 100");
			ctx.NotNull(car, "spawns: " + prefab);
			if (!car)
				continue;

			CarControllerComponent controller = CarControllerComponent.Cast(car.FindComponent(CarControllerComponent));
			ctx.NotNull(controller, "has a car controller: " + prefab);
			if (controller)
				ctx.True(controller.GetPersistentHandBrake(), "spawns with the handbrake on: " + prefab);

			SCR_EntityHelper.DeleteEntityAndChildren(car);
		}
	}
}
