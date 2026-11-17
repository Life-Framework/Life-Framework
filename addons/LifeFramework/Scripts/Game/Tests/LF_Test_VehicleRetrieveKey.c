// red-proof: #2411, 2026-11-17, `tools/cli test --no-build --only LF_Test_VehicleLockedCargo,LF_Test_VehicleRetrieveKey` on the unfixed code
// (SpawnOwned handed a key only when the session key was gone): FAIL - the owner leaves with a key that opens the car.
// tier: WORLD
//! The owner leaves the garage with a working key (#2411): when the last key handed out is in
//! someone else's pocket, Retrieve cuts the owner a new one instead of sending them off keyless.
class LF_Test_VehicleRetrieveKey : LF_Test
{
	protected static const string OWNER = "retrieve-key-owner";
	protected static const string STREET = "retrieve-key-street";
	protected static const string PLATE = "RK EY 2411";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "vehicles/retrieve-gives-owner-a-key";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_VehicleRecord> before = LF_VehicleOwnership.ExportAll();
		IEntity owner = LF_TestCarriedInventoryKit.SpawnLoaded(ctx);
		IEntity thief = LF_TestCarriedInventoryKit.SpawnLoaded(ctx);
		if (!owner || !thief)
		{
			LF_TestCharacterFixture.Delete(owner);
			LF_TestCharacterFixture.Delete(thief);
			return;
		}

		LF_VehicleOwnership.Register(STREET, OWNER, STREET, LF_VehicleCatalog.S105, LF_VehicleCatalog.S105_LOCKED, "grid-lot", PLATE, "#EL-Vehicle_S105", true);
		int fee = LF_GarageConfig.ImpoundFee();
		LF_MoneyUtils.AddCash(owner, fee);
		string reasonKey;
		ctx.True(LF_VehicleOwnership.CollectFromImpound(STREET, OWNER, owner, fee, LF_TestAnnex.IMPOUND_PAD_ID, reasonKey), "the owner collects the car");
		IEntity firstKey = LF_VehicleOwnership.GetKey(STREET);
		InventoryStorageManagerComponent thiefManager = LF_InventoryUtils.GetResponsibleStorageManager(thief);
		ctx.True(firstKey && thiefManager && LF_Test_VehicleLockedCargo.MoveInto(thiefManager, firstKey), "the thief pockets the owner's key");
		LF_VehicleLockComponent liveLock = LF_Component<LF_VehicleLockComponent>.Find(LF_VehicleOwnership.GetVehicle(STREET));
		ctx.True(liveLock && !liveLock.UserHasValidKey(owner), "the owner no longer holds a key");

		ctx.True(LF_VehicleOwnership.Store(STREET, OWNER), "the owner garages the car");
		ctx.True(LF_VehicleOwnership.Retrieve(STREET, OWNER, LF_TestAnnex.GARAGE_PAD_ID, owner), "the owner retrieves it");
		IEntity car = LF_VehicleOwnership.GetVehicle(STREET);
		LF_VehicleLockComponent vehicleLock;
		if (car)
			vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(car);

		ctx.True(vehicleLock && vehicleLock.UserHasValidKey(owner), "the owner leaves with a key that opens the car");
		ctx.True(vehicleLock && !vehicleLock.IsValidKey(firstKey), "the key left with the thief no longer opens the car");

		IEntity secondKey = LF_VehicleOwnership.GetKey(STREET);
		LF_VehicleOwnership.Store(STREET, OWNER);
		if (secondKey != firstKey)
			LF_TestCharacterFixture.Delete(secondKey);

		LF_TestCharacterFixture.Delete(firstKey);
		LF_TestCharacterFixture.Delete(owner);
		LF_TestCharacterFixture.Delete(thief);
		LF_VehicleOwnership.ApplyAll(before);
		LF_LicensePlateRegistry.Release(PLATE);
	}
}
