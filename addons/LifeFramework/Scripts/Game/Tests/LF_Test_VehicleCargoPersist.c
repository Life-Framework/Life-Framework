// red-proof: #2809, 2026-11-18 `tools/cli test --no-build --red LF_Test_VehicleStoreCargo,LF_Test_VehicleCargoPersist`:
// RED PROOF OK (RedProof() skips the carried payload read): the impounded car came back without its apple.
// Restart: `tools/cli wt e2e slice-2809` (SeedE2E / VerifyE2E).
//! A car left on the street keeps its cargo through the real save path (#2809): the save point
//! captures it into the "carried" payload, a restarted session loads the car at the impound, and
//! the collect puts the apple back once. A prefab that no longer loads is held in the snapshot, not
//! dropped. A car that left the world without a capture loses its cargo at the next save point.
[LF_TestInfo(LF_TestTier.PERSISTENCE)]
class LF_Test_VehicleCargoPersist : LF_Test
{
	protected static const string OWNER = "cargo-persist-owner";
	protected static const string VEHICLE = "cargo-persist-car";
	protected static const string PLATE = "CP GO 2809";
	//! A prefab that never existed, joined at run time so no resource check takes it for a real reference.
	protected static const string MISSING_GUID = "{0000000000002809}";
	//! The e2e car (LF_Test_E2E_SaveState pass 1 seeds it on the street, pass 2 collects it).
	static const string E2E_ID = "e2e-cargo-car";
	static const string E2E_OWNER = "e2e-cargo-owner";
	static const string E2E_PLATE = "EE CG 2809";

	override string GetName()
	{
		return "vehicles/cargo-persist";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		string missing = MISSING_GUID + "Prefabs/Items/LF_Gone2809.et";
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		LF_PersistenceComponent persistence;
		if (gameMode)
			persistence = LF_PersistenceComponent.Cast(gameMode.FindComponent(LF_PersistenceComponent));

		ctx.NotNull(persistence, "the game mode carries LF_PersistenceComponent");
		if (!persistence)
			return;

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
		int stock = LF_Test_VehicleStoreCargo.ItemsInCar(VEHICLE);
		ctx.True(LF_Test_VehicleStoreCargo.LoadApple(ctx, VEHICLE), "an apple goes in the car on the street");

		// The save point captures the street car into the carried payload.
		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();

		// Restart: the car and its snapshot are gone; the load brings the record and the cargo back.
		LF_VehicleOwnership.ApplyAll(before);
		LF_StorageSnapshot.ApplyAll(snapshotsBefore);
		ctx.True(LF_VehicleOwnership.GetVehicle(VEHICLE) == null, "the restarted session has no car in the world");
		ctx.False(LF_StorageSnapshot.Has(VEHICLE, LF_VehicleOwnership.SLOT_CARGO), "the restarted session has no cargo before the load");

		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the save parses");
		LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, loadContext, "vehicles");
		// RedProof(): the carried payload is not read, so no cargo comes back (`--red`).
		if (!RedProof())
			LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, loadContext, "carried");
		LF_VehicleOwnership.ImpoundStreetVehicles();
		ctx.True(LF_VehicleOwnership.IsImpounded(VEHICLE), "the street car is at the impound after the load");
		LF_StorageSnapshotRecord cargo = LF_StorageSnapshot.Get(VEHICLE, LF_VehicleOwnership.SLOT_CARGO);
		ctx.NotNull(cargo, "the cargo snapshot is back after the load");

		// A prefab that no longer loads rides along and is held, not dropped.
		if (cargo)
			cargo.m_aItems.Insert(LF_StorageItemRecord.Create(missing, 1, -1, 0, -1));

		LF_MoneyUtils.AddCash(owner, fee);
		ctx.True(LF_VehicleOwnership.CollectFromImpound(VEHICLE, OWNER, owner, fee, LF_TestAnnex.IMPOUND_PAD_ID, reasonKey), "the owner collects the car after the restart");
		LF_VehicleOwnership.ApplyCargo(VEHICLE);
		ctx.Equal(1, LF_Test_VehicleStoreCargo.CountInCar(VEHICLE, LF_Test_VehicleStoreCargo.APPLE_PREFAB), "the apple is back in the car after the restart");
		ctx.Equal(stock + 1, LF_Test_VehicleStoreCargo.ItemsInCar(VEHICLE), "the car holds its stock fill and the apple, nothing doubled");
		ctx.Equal(0, LF_VehicleOwnership.ApplyCargo(VEHICLE), "applying the cargo again spawns nothing");

		LF_VehicleOwnership.CaptureAllCargo();
		cargo = LF_StorageSnapshot.Get(VEHICLE, LF_VehicleOwnership.SLOT_CARGO);
		ctx.True(cargo && cargo.CountPrefab(missing) == 1, "the next save point still holds the item whose prefab is gone");

		// A car that leaves the world without a capture loses its cargo at the save point.
		LF_TestCharacterFixture.Delete(LF_VehicleOwnership.GetVehicle(VEHICLE));
		LF_VehicleOwnership.CaptureAllCargo();
		ctx.False(LF_StorageSnapshot.Has(VEHICLE, LF_VehicleOwnership.SLOT_CARGO), "a car gone without a capture drops its cargo");

		LF_TestCharacterFixture.Delete(LF_VehicleOwnership.GetKey(VEHICLE));
		LF_TestCharacterFixture.Delete(owner);
		LF_VehicleOwnership.ApplyAll(before);
		LF_StorageSnapshot.ApplyAll(snapshotsBefore);
		LF_LicensePlateRegistry.Release(PLATE);
	}

	//------------------------------------------------------------------------------------------------
	//! wt e2e save boot: an owned car on the street holds one apple; the shutdown save captures it.
	static void SeedE2E(LF_TestContext ctx, IEntity payer)
	{
		LF_VehicleOwnership.Register(E2E_ID, E2E_OWNER, E2E_ID, LF_VehicleCatalog.S105, LF_VehicleCatalog.S105_LOCKED, "roadmap-lot", E2E_PLATE, "#EL-Vehicle_S105", true);
		int fee = LF_GarageConfig.ImpoundFee();
		LF_MoneyUtils.AddCash(payer, fee);
		string reasonKey;
		ctx.True(LF_VehicleOwnership.CollectFromImpound(E2E_ID, E2E_OWNER, payer, fee, "roadmap-impound", reasonKey), "e2e cargo car is put on the street");
		ctx.True(LF_Test_VehicleStoreCargo.LoadApple(ctx, E2E_ID), "e2e cargo car holds an apple before the save");
	}

	//------------------------------------------------------------------------------------------------
	//! wt e2e reload boot: the car is at the impound, and collecting it brings the apple back.
	static void VerifyE2E(LF_TestContext ctx, IEntity payer)
	{
		// --pre-move: a ref build from before #2809 seeded no cargo car, so there is nothing to check.
		if (!LF_E2E.RequiresSeed("cargo car (#2809)", !LF_VehicleOwnership.GetOwner(E2E_ID).IsEmpty()))
			return;

		ctx.True(LF_VehicleOwnership.IsImpounded(E2E_ID), "e2e cargo car is at the impound after the restart");
		ctx.True(LF_StorageSnapshot.Has(E2E_ID, LF_VehicleOwnership.SLOT_CARGO), "e2e cargo car's snapshot is back after the restart");
		int fee = LF_GarageConfig.ImpoundFee();
		LF_MoneyUtils.AddCash(payer, fee);
		string reasonKey;
		ctx.True(LF_VehicleOwnership.CollectFromImpound(E2E_ID, E2E_OWNER, payer, fee, "roadmap-impound", reasonKey), "e2e cargo car collected after the restart");
		LF_VehicleOwnership.ApplyCargo(E2E_ID);
		int apples = LF_Test_VehicleStoreCargo.CountInCar(E2E_ID, LF_Test_VehicleStoreCargo.APPLE_PREFAB);
		ctx.Equal(1, apples, "e2e the apple is back in the car after the restart");
		LF_Debug.Info("Vehicle", string.Format("e2e restart: %1 collected with %2 apple(s) in its cargo", E2E_ID, apples));
		LF_VehicleOwnership.Store(E2E_ID, E2E_OWNER);
	}
}
