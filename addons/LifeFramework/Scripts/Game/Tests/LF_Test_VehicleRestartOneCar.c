// red-proof: 2026-11-19 `tools/cli test --no-build --only LF_Test_VehicleRestartOneCar` on the unfixed code: FAIL `a car collected at
// the impound is not in the engine's world save` (and the retrieve and e2e lines). Restart: `tools/cli wt e2e slice-3614` with Hold and IsSaved
// stubbed out failed pass 2 `e2e no unowned copy of the truck stands by the impound after the restart (expected 0, got 1)` and
// `exactly one truck stands by the impound after the collect (expected 1, got 2)`; with the fix both legs pass.
//! An owned car left on the street comes back once after a restart (#3614). The engine's own world save
//! wrote every owned car and put it back where it was parked, unowned, beside the record the restart
//! impound holds, so collecting it made a second car. Owned cars leave the engine save when they spawn
//! (LF_VehicleWorldSave): a collected or retrieved car is not tracked. Across a restart (`tools\cli wt e2e`):
//! the save boot leaves an owned truck with an apple on the street by the impound; the reload boot finds no
//! truck and no loose apple there, collects it, and then exactly one truck stands there, holding one apple.
[LF_TestInfo(LF_TestTier.PERSISTENCE)]
class LF_Test_VehicleRestartOneCar : LF_Test
{
	protected static const string OWNER = "restart-one-car-owner";
	protected static const string VEHICLE = "restart-one-car";
	protected static const string PLATE = "RO NE 3614";
	static const string E2E_ID = "e2e-restart-one-car";
	static const string E2E_OWNER = "e2e-restart-one-car-owner";
	static const string E2E_PLATE = "EE RO 3614";
	//! Around the impound desk: its three pad slots, and nothing a layer places of the e2e truck's prefab.
	protected static const float PAD_RADIUS = 20;

	protected static int s_iFound;
	protected static ResourceName s_sFindPrefab;

	override string GetName()
	{
		return "vehicles/restart-one-car";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
#ifdef LF_E2E_LOAD_BOOT
		VerifyE2E(ctx);
		return;
#endif

		ctx.NotNull(PersistenceSystem.GetInstance(), "the world has the engine's persistence system");
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
		IEntity car = LF_VehicleOwnership.GetVehicle(VEHICLE);
		ctx.NotNull(car, "the collected car is in the world");
		ctx.False(LF_VehicleWorldSave.IsSaved(car), "a car collected at the impound is not in the engine's world save");

		ctx.True(LF_VehicleOwnership.Store(VEHICLE, OWNER), "the car is stored");
		ctx.True(LF_VehicleOwnership.Retrieve(VEHICLE, OWNER, LF_TestAnnex.GARAGE_PAD_ID, owner), "the owner retrieves the car");
		car = LF_VehicleOwnership.GetVehicle(VEHICLE);
		ctx.NotNull(car, "the retrieved car is in the world");
		ctx.False(LF_VehicleWorldSave.IsSaved(car), "a car retrieved from the garage is not in the engine's world save");

		Cleanup(owner, before, snapshotsBefore);

		// Every non-reload run (the e2e save boot among them) leaves one owned truck with an apple on the street.
		SeedE2E(ctx);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Cleanup(IEntity owner, array<ref LF_VehicleRecord> before, array<ref LF_StorageSnapshotRecord> snapshotsBefore)
	{
		LF_TestCharacterFixture.Delete(LF_VehicleOwnership.GetVehicle(VEHICLE));
		LF_TestCharacterFixture.Delete(LF_VehicleOwnership.GetKey(VEHICLE));
		LF_TestCharacterFixture.Delete(owner);
		LF_VehicleOwnership.ApplyAll(before);
		LF_StorageSnapshot.ApplyAll(snapshotsBefore);
		LF_LicensePlateRegistry.Release(PLATE);
	}

	//------------------------------------------------------------------------------------------------
	//! The impound desk the e2e truck is parked by, or null when this world has no annex impound.
	protected static IEntity ImpoundDesk()
	{
		if (!GetGame() || !GetGame().GetWorld())
			return null;

		return GetGame().GetWorld().FindEntityByName(LF_TestAnnex.IMPOUND_DESK);
	}

	//------------------------------------------------------------------------------------------------
	//! A root entity (not inside a vehicle or a pocket) of the counted prefab.
	protected static bool CountFound(IEntity entity)
	{
		if (entity && !entity.GetParent() && LF_Utils.SamePrefab(LF_Utils.GetPrefabName(entity), s_sFindPrefab))
			s_iFound++;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return How many root entities of the prefab stand within PAD_RADIUS of the impound desk, -1 with no desk.
	protected static int CountAtImpound(ResourceName prefab)
	{
		IEntity desk = ImpoundDesk();
		if (!desk)
			return -1;

		s_iFound = 0;
		s_sFindPrefab = prefab;
		GetGame().GetWorld().QueryEntitiesBySphere(desk.GetOrigin(), PAD_RADIUS, CountFound, null, EQueryEntitiesFlags.ALL);
		return s_iFound;
	}

	//------------------------------------------------------------------------------------------------
	//! wt e2e save boot: an owned truck holding an apple stands on the impound pad when the shutdown save runs.
	static void SeedE2E(LF_TestContext ctx)
	{
		if (!ImpoundDesk())
		{
			LF_Debug.Info("Vehicle", "e2e restart-one-car truck not seeded: this world has no annex impound");
			return;
		}

		IEntity payer = LF_TestCarriedInventoryKit.SpawnLoaded(ctx);
		if (!payer)
			return;

		LF_VehicleOwnership.Register(E2E_ID, E2E_OWNER, E2E_ID, LF_VehicleCatalog.M923, LF_VehicleCatalog.M923_LOCKED, "grid-lot", E2E_PLATE, "#EL-Vehicle_M923", true);
		int fee = LF_GarageConfig.ImpoundFee();
		LF_MoneyUtils.AddCash(payer, fee);
		string reasonKey;
		ctx.True(LF_VehicleOwnership.CollectFromImpound(E2E_ID, E2E_OWNER, payer, fee, LF_TestAnnex.IMPOUND_PAD_ID, reasonKey), "e2e restart-one-car truck is put on the street");
		ctx.True(LF_Test_VehicleStoreCargo.LoadApple(ctx, E2E_ID), "e2e restart-one-car truck holds an apple before the save");
		IEntity truck = LF_VehicleOwnership.GetVehicle(E2E_ID);
		ctx.False(LF_VehicleWorldSave.IsSaved(truck), "e2e the owned truck is not in the engine's world save");
		if (truck)
			LF_Debug.Info("Vehicle", string.Format("e2e restart-one-car: %1 parked at %2 with %3 apple(s)", E2E_ID, truck.GetOrigin(), LF_Test_VehicleStoreCargo.CountInCar(E2E_ID, LF_Test_VehicleStoreCargo.APPLE_PREFAB)));

		LF_TestCharacterFixture.Delete(payer);
	}

	//------------------------------------------------------------------------------------------------
	//! wt e2e reload boot: no copy of the truck and no loose apple by the impound; one truck after the collect.
	static void VerifyE2E(LF_TestContext ctx)
	{
		if (!LF_E2E.RequiresSeed("restart-one-car truck (#3614)", !LF_VehicleOwnership.GetOwner(E2E_ID).IsEmpty()))
			return;

		ctx.True(LF_VehicleOwnership.IsImpounded(E2E_ID), "e2e restart-one-car truck is at the impound after the restart");
		ctx.Equal(0, CountAtImpound(LF_VehicleCatalog.M923_LOCKED), "e2e no unowned copy of the truck stands by the impound after the restart");
		ctx.Equal(0, CountAtImpound(LF_Test_VehicleStoreCargo.APPLE_PREFAB), "e2e no loose apple from the truck's cargo lies by the impound");
		IEntity payer = LF_TestCarriedInventoryKit.SpawnLoaded(ctx);
		if (!payer)
			return;

		int fee = LF_GarageConfig.ImpoundFee();
		LF_MoneyUtils.AddCash(payer, fee);
		string reasonKey;
		ctx.True(LF_VehicleOwnership.CollectFromImpound(E2E_ID, E2E_OWNER, payer, fee, LF_TestAnnex.IMPOUND_PAD_ID, reasonKey), "e2e restart-one-car truck collected after the restart");
		LF_VehicleOwnership.ApplyCargo(E2E_ID);
		int trucks = CountAtImpound(LF_VehicleCatalog.M923_LOCKED);
		int apples = LF_Test_VehicleStoreCargo.CountInCar(E2E_ID, LF_Test_VehicleStoreCargo.APPLE_PREFAB);
		ctx.Equal(1, trucks, "e2e exactly one truck stands by the impound after the collect");
		ctx.Equal(1, apples, "e2e the collected truck holds its one apple");
		ctx.Equal(0, CountAtImpound(LF_Test_VehicleStoreCargo.APPLE_PREFAB), "e2e still no loose apple by the impound after the collect");
		LF_Debug.Info("Vehicle", string.Format("e2e restart: %1 collected, %2 truck(s) by the impound, %3 apple(s) in it", E2E_ID, trucks, apples));
		LF_VehicleOwnership.Store(E2E_ID, E2E_OWNER);
		LF_TestCharacterFixture.Delete(payer);
	}
}
