// red-proof: #2809, 2026-11-18 `tools/cli test --no-build --red LF_Test_VehicleStoreCargo,LF_Test_VehicleCargoPersist`:
// RED PROOF OK (RedProof() drops the snapshot the store and the tow took): `the retrieved car holds the apple (expected 1, got 0)`.
// Before #2809 the car with an apple was refused at the garage (#2411); the first assertion failed on that code.
// #2830 review, 2026-11-18: a car deleted outside Store/tow, then collected at the impound, came back with an apple already taken out (`--only LF_Test_VehicleStoreCargo` on the unfixed code: `expected 0, got 1`); passes with the SpawnOwned guard.
// tier: WORLD
//! The garage, the tow and the impound keep a car's cargo (#2809, replacing the #2411 refusal): a
//! car holding an apple is stored, comes back from the garage with the apple and its stock fill
//! (no dupe, no extra field dressings), goes to the impound with it on a tow, and comes back from
//! the impound with it. A scrapped car drops its cargo.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_VehicleStoreCargo : LF_Test
{
	protected static const string OWNER = "store-cargo-owner";
	protected static const string VEHICLE = "store-cargo-car";
	protected static const string PLATE = "SC GO 2411";
	static const ResourceName APPLE_PREFAB = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "vehicles/store-keeps-cargo";
	}

	//------------------------------------------------------------------------------------------------
	//! \return How many of the prefab the live car holds, -1 when it is not in the world.
	static int CountInCar(string vehicleId, ResourceName prefab)
	{
		IEntity car = LF_VehicleOwnership.GetVehicle(vehicleId);
		if (!car)
			return -1;

		LF_StorageSnapshotRecord now = LF_StorageSnapshot.Capture(vehicleId, "count", car);
		if (!now)
			return -1;

		return now.CountPrefab(prefab);
	}

	//------------------------------------------------------------------------------------------------
	//! \return Items in the live car, stock fill included; -1 when it is not in the world.
	static int ItemsInCar(string vehicleId)
	{
		IEntity car = LF_VehicleOwnership.GetVehicle(vehicleId);
		if (!car)
			return -1;

		LF_StorageSnapshotRecord now = LF_StorageSnapshot.Capture(vehicleId, "count", car);
		if (!now)
			return -1;

		return now.m_aItems.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Puts one apple in the car's cargo storage (unlocks it first).
	static bool LoadApple(LF_TestContext ctx, string vehicleId)
	{
		IEntity car = LF_VehicleOwnership.GetVehicle(vehicleId);
		if (!car)
			return false;

		LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(car);
		if (vehicleLock)
			vehicleLock.SetLocked(false);

		SCR_UniversalInventoryStorageComponent cargo = SCR_UniversalInventoryStorageComponent.Cast(car.FindComponent(SCR_UniversalInventoryStorageComponent));
		InventoryStorageManagerComponent carManager = InventoryStorageManagerComponent.Cast(car.FindComponent(InventoryStorageManagerComponent));
		IEntity apple = LF_Utils.SpawnEntityPrefab(APPLE_PREFAB, car.GetOrigin() + "0 2 0");
		if (!cargo || !carManager || !apple)
		{
			ctx.Fail("fixture: cargo storage, storage manager or apple did not resolve");
			LF_TestCharacterFixture.Delete(apple);
			return false;
		}

		bool inserted = carManager.TryInsertItemInStorage(apple, cargo);
		if (!inserted)
			LF_TestCharacterFixture.Delete(apple);

		// Locked again: the garage and the impound must put the cargo back into a locked car.
		if (vehicleLock)
			vehicleLock.SetLocked(true);

		return inserted;
	}

	//------------------------------------------------------------------------------------------------
	//! Takes the apple out of the car's cargo storage, as a player emptying the trunk would.
	static bool TakeApple(string vehicleId)
	{
		IEntity car = LF_VehicleOwnership.GetVehicle(vehicleId);
		if (!car)
			return false;

		SCR_UniversalInventoryStorageComponent cargo = SCR_UniversalInventoryStorageComponent.Cast(car.FindComponent(SCR_UniversalInventoryStorageComponent));
		if (!cargo)
			return false;

		for (int slot = 0; slot < cargo.GetSlotsCount(); slot++)
		{
			IEntity item = cargo.Get(slot);
			if (item && LF_Utils.SamePrefab(LF_Utils.GetPrefabName(item), APPLE_PREFAB))
			{
				LF_TestCharacterFixture.Delete(item);
				return true;
			}
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_VehicleRecord> before = LF_VehicleOwnership.ExportAll();
		array<ref LF_StorageSnapshotRecord> snapshotsBefore = LF_StorageSnapshot.ExportAll();
		IEntity owner = LF_TestCarriedInventoryKit.SpawnLoaded(ctx);
		if (!owner)
			return;

		LF_VehicleOwnership.Register(VEHICLE, OWNER, VEHICLE, LF_VehicleCatalog.S105, LF_VehicleCatalog.S105_LOCKED, "grid-lot", PLATE, "#EL-Vehicle_S105", true);
		int fee = LF_GarageConfig.ImpoundFee();
		LF_MoneyUtils.AddCash(owner, fee);
		string reasonKey;
		ctx.True(LF_VehicleOwnership.CollectFromImpound(VEHICLE, OWNER, owner, fee, LF_TestAnnex.IMPOUND_PAD_ID, reasonKey), "the owner collects the car");
		int stock = ItemsInCar(VEHICLE);
		ctx.True(stock >= 0, "the collected car is in the world");
		if (!LoadApple(ctx, VEHICLE))
		{
			ctx.Fail("fixture: the apple did not go in the car");
			Cleanup(owner, before, snapshotsBefore);
			return;
		}

		ctx.Equal(1, CountInCar(VEHICLE, APPLE_PREFAB), "the apple is in the car");

		// Garage: the car with cargo is stored, and the cargo comes back with it.
		ctx.EqualStr("", LF_Garage.StoreRefusalKey(LF_VehicleOwnership.GetVehicle(VEHICLE), VEHICLE, OWNER), "a car with cargo is not refused");
		// RedProof(): the store drops the snapshot it just took, as the code before #2809 kept none.
		ctx.True(LF_VehicleOwnership.Store(VEHICLE, OWNER), "a car with cargo is stored");
		if (RedProof())
			LF_StorageSnapshot.Clear(VEHICLE, LF_VehicleOwnership.SLOT_CARGO);

		ctx.True(LF_VehicleOwnership.IsStored(VEHICLE), "the record is marked stored");
		LF_StorageSnapshotRecord saved = LF_StorageSnapshot.Get(VEHICLE, LF_VehicleOwnership.SLOT_CARGO);
		ctx.NotNull(saved, "the garage keeps a cargo snapshot");
		if (saved)
			ctx.Equal(1, saved.CountPrefab(APPLE_PREFAB), "the snapshot holds the apple");

		ctx.True(LF_VehicleOwnership.Retrieve(VEHICLE, OWNER, LF_TestAnnex.GARAGE_PAD_ID, owner), "the owner retrieves the car");
		LF_VehicleOwnership.ApplyCargo(VEHICLE);
		ctx.Equal(1, CountInCar(VEHICLE, APPLE_PREFAB), "the retrieved car holds the apple");
		ctx.Equal(stock + 1, ItemsInCar(VEHICLE), "the retrieved car holds its stock fill and the apple, nothing doubled");
		ctx.Equal(0, LF_VehicleOwnership.ApplyCargo(VEHICLE), "applying the cargo again spawns nothing");

		// Tow: the cargo goes to the impound with the car and comes back on collect.
		ctx.True(LF_VehicleOwnership.TakeOffStreet(VEHICLE, LF_VehicleOwnership.GetVehicle(VEHICLE)), "the tow takes the car off the street");
		if (RedProof())
			LF_StorageSnapshot.Clear(VEHICLE, LF_VehicleOwnership.SLOT_CARGO);

		ctx.True(LF_VehicleOwnership.IsImpounded(VEHICLE), "the towed car is at the impound");
		LF_MoneyUtils.AddCash(owner, fee);
		ctx.True(LF_VehicleOwnership.CollectFromImpound(VEHICLE, OWNER, owner, fee, LF_TestAnnex.IMPOUND_PAD_ID, reasonKey), "the owner collects the towed car");
		LF_VehicleOwnership.ApplyCargo(VEHICLE);
		ctx.Equal(1, CountInCar(VEHICLE, APPLE_PREFAB), "the collected car holds the apple");
		ctx.Equal(stock + 1, ItemsInCar(VEHICLE), "the collected car holds its stock fill and the apple, nothing doubled");

		// Untracked exit (#2830 review): the apple is taken out, then the car leaves the world without a
		// capture (an admin delete, a wreck sweep). The impound gives the car back, never the old snapshot's apple.
		ctx.True(TakeApple(VEHICLE), "fixture: the apple is taken out of the car");
		ctx.Equal(0, CountInCar(VEHICLE, APPLE_PREFAB), "fixture: the car is empty of apples");
		LF_TestCharacterFixture.Delete(LF_VehicleOwnership.GetVehicle(VEHICLE));
		ctx.True(LF_VehicleOwnership.IsImpounded(VEHICLE), "the deleted car is at the impound");
		LF_MoneyUtils.AddCash(owner, fee);
		ctx.True(LF_VehicleOwnership.CollectFromImpound(VEHICLE, OWNER, owner, fee, LF_TestAnnex.IMPOUND_PAD_ID, reasonKey), "the owner collects the deleted car");
		LF_VehicleOwnership.ApplyCargo(VEHICLE);
		ctx.Equal(0, CountInCar(VEHICLE, APPLE_PREFAB), "a car that left without a capture does not bring back the apple taken out of it");
		ctx.Equal(stock, ItemsInCar(VEHICLE), "the car comes back with its stock fill only");

		// Scrap: the record and its cargo are gone.
		IEntity car = LF_VehicleOwnership.GetVehicle(VEHICLE);
		ctx.True(LF_VehicleOwnership.Scrap(VEHICLE), "the car is scrapped");
		ctx.False(LF_StorageSnapshot.Has(VEHICLE, LF_VehicleOwnership.SLOT_CARGO), "a scrapped car drops its cargo");
		LF_TestCharacterFixture.Delete(car);

		Cleanup(owner, before, snapshotsBefore);
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup(IEntity owner, array<ref LF_VehicleRecord> before, array<ref LF_StorageSnapshotRecord> snapshotsBefore)
	{
		if (LF_VehicleOwnership.GetVehicle(VEHICLE))
			LF_VehicleOwnership.Store(VEHICLE, OWNER);

		LF_TestCharacterFixture.Delete(LF_VehicleOwnership.GetKey(VEHICLE));
		LF_TestCharacterFixture.Delete(LF_VehicleOwnership.GetVehicle(VEHICLE));
		LF_TestCharacterFixture.Delete(owner);
		LF_VehicleOwnership.ApplyAll(before);
		LF_StorageSnapshot.ApplyAll(snapshotsBefore);
		LF_LicensePlateRegistry.Release(PLATE);
	}
}
