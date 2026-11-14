// red-proof: 2026-11-14 `tools/cli test --no-build --only LF_Test_StolenVehicleLookPlacedCar` with PrefabGuidOf reading `vehicle.GetPrefabData().GetPrefabName()` (the old code) failed `- a placed S105 is filed under the S105 GUID (expected 'E1A105BE1A000001', got '')`. Restored to LF_Utils.GetPrefabName; green.
// tier: WORLD
//! A lock-up car placed in the world (a layer entity that overrides the lock component) is filed
//! on the wanted board with its prefab's GUID, the same as a spawned car. The report read the
//! prefab name raw, which a layer override does not carry, so the poster had no car to draw
//! (the bug class #1307 found in the chop shop payout).
class LF_Test_StolenVehicleLookPlacedCar : LF_Test
{
	override string GetName()
	{
		return "stolen/placed-car-look";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity car = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.LOCKPICK_CAR);
		ctx.NotNull(car, "the placed lock-up car is there");
		if (!car)
			return;

		string expected = LF_WantedLook.GuidOf(LF_VehicleCatalog.S105_LOCKED);
		ctx.False(expected.IsEmpty(), "the S105 prefab has a GUID to compare with");
		ctx.EqualStr(expected, LF_StolenVehicles.PrefabGuidOf(car), "a placed S105 is filed under the S105 GUID");

		// A car spawned from the prefab is filed the same.
		IEntity spawned = LF_Utils.SpawnEntityPrefab(LF_VehicleCatalog.S105_LOCKED, car.GetOrigin() + "0 20 0");
		ctx.NotNull(spawned, "an S105 spawns");
		if (spawned)
		{
			ctx.EqualStr(expected, LF_StolenVehicles.PrefabGuidOf(spawned), "a spawned S105 is filed under the same GUID");
			SCR_EntityHelper.DeleteEntityAndChildren(spawned);
		}
	}
}
