// red-proof: #4589, 2026-11-23, `tools/cli test --no-build --red LF_Test_LiveryBayDevPark` (the park skipped): RED PROOF OK (beside the bay the owner's car is parked (expected 'bay-park-car', got '')).
//! The dev park at the respray bay (#4589): the owner's car, set down anywhere, goes onto the bay's pad when
//! they stand beside the bay; a player at no bay moves nothing.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_LiveryBayDevPark : LF_Test
{
	protected static const string OWNER = "bay-park-owner";
	protected static const string CAR = "bay-park-car";
	protected static const ResourceName BAY_PREFAB = "{837750C2B0CBF9D9}Prefabs/Props/LF_LiveryBay.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "vehicles/livery-bay-dev-park";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_VehicleRecord> before = LF_VehicleOwnership.ExportAll();
		IEntity owner = LF_TestCarriedInventoryKit.SpawnLoaded(ctx);
		if (!owner)
			return;

		LF_Utils.SetUidForTests(owner, OWNER);
		LF_VehicleOwnership.Register(CAR, OWNER, CAR, LF_VehicleCatalog.S105, LF_VehicleCatalog.S105_LOCKED, "grid-lot", "BY PK 4589", "#EL-Vehicle_S105", true);
		int fee = LF_GarageConfig.ImpoundFee();
		LF_MoneyUtils.AddCash(owner, fee);
		string reasonKey;
		LF_VehicleOwnership.CollectFromImpound(CAR, OWNER, owner, fee, LF_TestAnnex.IMPOUND_PAD_ID, reasonKey);
		IEntity car = LF_VehicleOwnership.GetVehicle(CAR);
		ctx.NotNull(car, "the car stands in the world");
		if (!car)
		{
			Cleanup(owner, null, before);
			return;
		}

		vector bayAt = car.GetOrigin() + "20 0 0";
		IEntity bay = LF_Utils.SpawnEntityPrefab(BAY_PREFAB, bayAt);
		ctx.NotNull(bay, "the bay prefab spawns");
		if (!bay)
		{
			Cleanup(owner, null, before);
			return;
		}

		// Away from the bay nothing moves.
		owner.SetOrigin(car.GetOrigin() + "0 0 -12");
		ctx.EqualStr("", LF_LiveryBayDevPark.ParkLatest(owner), "a player at no bay parks nothing");
		vector parkedAt = car.GetOrigin();

		owner.SetOrigin(bayAt + "0 0 -1.5");
		string parked;
		if (!RedProof())
			parked = LF_LiveryBayDevPark.ParkLatest(owner);

		ctx.EqualStr(CAR, parked, "beside the bay the owner's car is parked");
		ctx.True(vector.Distance(car.GetOrigin(), bayAt + "0 0 4") < 1.5, "on the bay's pad");
		ctx.True(vector.Distance(car.GetOrigin(), parkedAt) > 5, "away from where it stood");

		Cleanup(owner, bay, before);
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup(IEntity owner, IEntity bay, array<ref LF_VehicleRecord> before)
	{
		IEntity key = LF_VehicleOwnership.GetKey(CAR);
		LF_VehicleOwnership.Store(CAR, OWNER);
		LF_Utils.SetUidForTests(null, "");
		LF_TestCharacterFixture.Delete(key);
		LF_TestCharacterFixture.Delete(owner);
		if (bay)
			SCR_EntityHelper.DeleteEntityAndChildren(bay);

		LF_VehicleOwnership.ApplyAll(before);
		LF_LicensePlateRegistry.Release("BY PK 4589");
	}
}
