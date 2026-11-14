// red-proof: 2026-11-14 `tools/cli test --no-build --only LF_Test_ChopShopPlacedCar` with PayoutFor reading `vehicle.GetPrefabData().GetPrefabName()` (the old code) failed `- a placed S105 pays 30 percent of 1000, not the 800 default (expected 300, got 240)`. Restored to LF_Utils.GetPrefabName; green.
// tier: WORLD
//! A lock-up car placed in the world (a layer entity that overrides the lock component, the shape of
//! Roadmap_Lockpick_Car) pays what its prefab is worth, $300 for the S105 (#611, #415, #601). It paid
//! the unlisted default (800 x 30 = 240) because the yard read the prefab name off the entity data
//! raw, which a layer override does not carry.
class LF_Test_ChopShopPlacedCar : LF_Test
{
	override string GetName()
	{
		return "chopshop/placed-car-payout";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity car = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.LOCKPICK_CAR);
		ctx.NotNull(car, "the placed lock-up car is there");
		if (!car)
			return;

		ctx.True(LF_Utils.SamePrefab(LF_Utils.GetPrefabName(car), LF_VehicleCatalog.S105_LOCKED), "the placed car resolves to the S105 prefab");
		ctx.Equal(300, LF_ChopShopRules.PayoutFor(car), "a placed S105 pays 30 percent of 1000, not the 800 default");

		// A car spawned from the prefab pays the same.
		IEntity spawned = LF_Utils.SpawnEntityPrefab(LF_VehicleCatalog.S105_LOCKED, car.GetOrigin() + "0 20 0");
		ctx.NotNull(spawned, "an S105 spawns");
		if (spawned)
		{
			ctx.Equal(300, LF_ChopShopRules.PayoutFor(spawned), "a spawned S105 pays the same");
			SCR_EntityHelper.DeleteEntityAndChildren(spawned);
		}
	}
}
