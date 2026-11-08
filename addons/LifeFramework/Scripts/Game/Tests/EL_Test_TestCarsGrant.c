// red-proof: observed 2026-11-08 with `tools\cli test --no-build --only EL_Test_TestCarsGrant` and the
// first car not marked stored in EL_VehicleOwnership.GrantTestCars:
// [ELTEST]   - one test car is stored in the garage
// Restored; green.

//! Playtest grant (maintainer, 2026-11-08): a new character owns one car in the garage and one at
//! the impound lot, as records with keys, so both flows can be tried at once. Granted only to an
//! owner with no vehicle, so a second call (a respawn) grants nothing.
// tier: WORLD
class EL_Test_TestCarsGrant : EL_Test
{
	override string GetName()
	{
		return "vehicles/test-cars";
	}

	override void Run(EL_TestContext ctx)
	{
		IEntity lotEntity = GetGame().GetWorld().FindEntityByName("Roadmap_Vehicle_Lot");
		EL_VehicleLotComponent lot;
		if (lotEntity)
			lot = EL_Component<EL_VehicleLotComponent>.Find(lotEntity);

		ctx.NotNull(lot, "DebugWorld has the car lot Roadmap_Vehicle_Lot");
		if (!lot)
			return;

		array<EL_VehicleOffer> offers = {};
		for (int i = 0; i < lot.GetOfferCount(); i++)
		{
			offers.Insert(lot.GetOfferAt(i));
		}

		string uid = "test-cars-" + System.GetTickCount().ToString();
		ctx.Equal(2, EL_VehicleOwnership.GrantTestCars(uid, lotEntity, offers, lot.GetShopId()), "a new owner gets two test cars");

		string stored = EL_VehicleOwnership.LatestStored(uid);
		string impounded = EL_VehicleOwnership.LatestImpounded(uid);
		ctx.False(stored.IsEmpty(), "one test car is stored in the garage");
		ctx.False(impounded.IsEmpty(), "one test car is at the impound lot");
		ctx.True(stored != impounded, "the garaged and the impounded car are two cars");
		if (!stored.IsEmpty())
			ctx.True(EL_VehicleOwnership.IsStored(stored), "the garaged car reads as stored");
		if (!impounded.IsEmpty())
			ctx.True(EL_VehicleOwnership.IsImpounded(impounded), "the other car reads as impounded");

		ctx.Equal(0, EL_VehicleOwnership.GrantTestCars(uid, lotEntity, offers, lot.GetShopId()), "an owner with cars is not granted again");

		EL_SpawnLogic spawnLogic = EL_SpawnLogic.GetInstance();
		if (spawnLogic)
			ctx.True(spawnLogic.GrantsTestCars(), "the DebugWorld (playtest) game mode grants test cars");
	}
}
