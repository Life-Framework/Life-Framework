// red-proof: observed 2026-11-08 with `tools\cli test --no-build --only LF_Test_TestCarsGrant` and the
// first car not marked stored in LF_VehicleOwnership.GrantTestCars:
// [LFTEST]   - one test car is stored in the garage
// Restored; green.

//! Playtest grant (maintainer, 2026-11-08): a new character owns one car in the garage and one at
//! the impound lot, as records with keys, so both flows can be tried at once. Granted only to an
//! owner with no vehicle, so a second call (a respawn) grants nothing.
// red-proof (#1892, 2026-11-16): `tools\cli test --no-build --only <the 7 slice-1892 tests> --world Worlds/GridWorld/GridWorld.ent` before Grid_orgs_vehicles.layer failed all 7 (no org hideout door, no flagged lot, no store, no impound pad); green with it.
// tier: WORLD
class LF_Test_TestCarsGrant : LF_Test
{
	override string GetName()
	{
		return "vehicles/test-cars";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_VehicleLotComponent lot = LF_VehicleLotComponent.FindTestCarLot();
		ctx.NotNull(lot, "the world places a car lot flagged m_bTestCarLot (GridWorld: Grid_orgs_vehicles_Lot)");
		if (!lot)
			return;

		IEntity lotEntity = lot.GetOwner();

		array<LF_VehicleOffer> offers = {};
		for (int i = 0; i < lot.GetOfferCount(); i++)
		{
			offers.Insert(lot.GetOfferAt(i));
		}

		if (!offers.IsEmpty())
			ctx.True(lot.GetOffer(offers[0].GetPrefab()) != null, "the flagged lot sells the test cars");

		string uid = "test-cars-" + System.GetTickCount().ToString();
		ctx.Equal(2, LF_VehicleOwnership.GrantTestCars(uid, lotEntity, offers, lot.GetShopId()), "a new owner gets two test cars");

		string stored = LF_VehicleOwnership.LatestStored(uid);
		string impounded = LF_VehicleOwnership.LatestImpounded(uid);
		ctx.False(stored.IsEmpty(), "one test car is stored in the garage");
		ctx.False(impounded.IsEmpty(), "one test car is at the impound lot");
		ctx.True(stored != impounded, "the garaged and the impounded car are two cars");
		if (!stored.IsEmpty())
			ctx.True(LF_VehicleOwnership.IsStored(stored), "the garaged car reads as stored");
		if (!impounded.IsEmpty())
			ctx.True(LF_VehicleOwnership.IsImpounded(impounded), "the other car reads as impounded");

		ctx.Equal(0, LF_VehicleOwnership.GrantTestCars(uid, lotEntity, offers, lot.GetShopId()), "an owner with cars is not granted again");

		LF_SpawnLogic spawnLogic = LF_SpawnLogic.GetInstance();
		if (spawnLogic)
			ctx.True(spawnLogic.GrantsTestCars(), "the playtest game mode grants test cars");
	}
}
