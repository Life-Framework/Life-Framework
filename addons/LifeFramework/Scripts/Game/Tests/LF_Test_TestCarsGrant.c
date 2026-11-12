// red-proof: observed 2026-11-08 with `tools\cli test --no-build --only LF_Test_TestCarsGrant` and the
// first car not marked stored in LF_VehicleOwnership.GrantTestCars:
// [LFTEST]   - one test car is stored in the garage
// Restored; green.

//! Playtest grant (maintainer, 2026-11-08): a new character owns one car in the garage and one at
//! the impound lot, as records with keys, so both flows can be tried at once. Granted only to an
//! owner with no vehicle, so a second call (a respawn) grants nothing.
// tier: WORLD
class LF_Test_TestCarsGrant : LF_Test
{
	override string GetName()
	{
		return "vehicles/test-cars";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity lotEntity = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.VEHICLE_LOT);
		LF_VehicleLotComponent lot;
		if (lotEntity)
			lot = LF_Component<LF_VehicleLotComponent>.Find(lotEntity);

		ctx.NotNull(lot, "DebugWorld has the car lot Annex_Vehicle_Lot");
		if (!lot)
			return;

		array<LF_VehicleOffer> offers = {};
		for (int i = 0; i < lot.GetOfferCount(); i++)
		{
			offers.Insert(lot.GetOfferAt(i));
		}

		LF_VehicleLotComponent testLot = LF_VehicleLotComponent.FindTestCarLot();
		ctx.NotNull(testLot, "a placed lot is flagged m_bTestCarLot");
		if (testLot && !offers.IsEmpty())
			ctx.True(testLot.GetOffer(offers[0].GetPrefab()) != null, "the flagged lot sells the test cars");

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
			ctx.True(spawnLogic.GrantsTestCars(), "the DebugWorld (playtest) game mode grants test cars");
	}
}
