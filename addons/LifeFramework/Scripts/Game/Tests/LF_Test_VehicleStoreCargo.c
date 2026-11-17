// red-proof: #2411, 2026-11-17, `tools/cli test --no-build --only LF_Test_VehicleStoreCargo` on the unfixed code
// (Store deleted the car with its storage): FAIL - a car with cargo is not stored, - the apple is still in the car.
// Green with LF_Garage.StoreRefusalKey; the emptied car (stock field dressings only) stores.
// tier: WORLD
//! Storing a car never deletes its cargo (#2411): the vehicle record keeps no inventory, so a car
//! that still holds items stays on the street with them. Emptied, the same car stores.
class LF_Test_VehicleStoreCargo : LF_Test
{
	protected static const string OWNER = "store-cargo-owner";
	protected static const string VEHICLE = "store-cargo-car";
	protected static const string PLATE = "SC GO 2411";
	protected static const ResourceName APPLE = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "vehicles/store-keeps-cargo";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_VehicleRecord> before = LF_VehicleOwnership.ExportAll();
		IEntity owner = LF_TestCarriedInventoryKit.SpawnLoaded(ctx);
		if (!owner)
			return;

		LF_VehicleOwnership.Register(VEHICLE, OWNER, VEHICLE, LF_VehicleCatalog.S105, LF_VehicleCatalog.S105_LOCKED, "grid-lot", PLATE, "#EL-Vehicle_S105", true);
		int fee = LF_GarageConfig.ImpoundFee();
		LF_MoneyUtils.AddCash(owner, fee);
		string reasonKey;
		ctx.True(LF_VehicleOwnership.CollectFromImpound(VEHICLE, OWNER, owner, fee, LF_TestAnnex.IMPOUND_PAD_ID, reasonKey), "the owner collects the car");
		IEntity car = LF_VehicleOwnership.GetVehicle(VEHICLE);
		LF_VehicleLockComponent vehicleLock;
		SCR_UniversalInventoryStorageComponent cargo;
		InventoryStorageManagerComponent carManager;
		if (car)
		{
			vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(car);
			cargo = SCR_UniversalInventoryStorageComponent.Cast(car.FindComponent(SCR_UniversalInventoryStorageComponent));
			carManager = InventoryStorageManagerComponent.Cast(car.FindComponent(InventoryStorageManagerComponent));
		}

		IEntity apple;
		if (car)
			apple = LF_Utils.SpawnEntityPrefab(APPLE, car.GetOrigin() + "0 2 0");

		if (!car || !cargo || !carManager || !apple)
		{
			ctx.Fail("fixture: car, cargo storage or apple did not resolve");
			Cleanup(owner, apple, before);
			return;
		}

		if (vehicleLock)
			vehicleLock.SetLocked(false);

		ctx.True(carManager.TryInsertItemInStorage(apple, cargo), "the apple goes in the car's storage");
		ctx.False(LF_VehicleOwnership.Store(VEHICLE, OWNER), "a car with cargo is not stored");
		ctx.True(LF_VehicleOwnership.GetVehicle(VEHICLE) == car, "the car stays on the street");
		ctx.False(LF_VehicleOwnership.IsStored(VEHICLE), "the record is not marked stored");
		ctx.True(cargo.Contains(apple), "the apple is still in the car");

		ctx.True(carManager.TryRemoveItemFromStorage(apple, cargo), "the owner takes the apple out");
		ctx.True(LF_VehicleOwnership.Store(VEHICLE, OWNER), "the emptied car is stored");
		ctx.True(LF_VehicleOwnership.IsStored(VEHICLE), "the record is marked stored");

		Cleanup(owner, apple, before);
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup(IEntity owner, IEntity apple, array<ref LF_VehicleRecord> before)
	{
		LF_TestCharacterFixture.Delete(apple);
		if (LF_VehicleOwnership.GetVehicle(VEHICLE))
			LF_VehicleOwnership.Store(VEHICLE, OWNER);

		LF_TestCharacterFixture.Delete(LF_VehicleOwnership.GetKey(VEHICLE));
		LF_TestCharacterFixture.Delete(LF_VehicleOwnership.GetVehicle(VEHICLE));
		LF_TestCharacterFixture.Delete(owner);
		LF_VehicleOwnership.ApplyAll(before);
		LF_LicensePlateRegistry.Release(PLATE);
	}
}
