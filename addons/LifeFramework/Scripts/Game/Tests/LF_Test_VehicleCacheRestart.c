// red-proof: observed 2026-11-19 `tools/cli test --no-build --red LF_Test_VehicleCacheRestart`: RED PROOF OK (RedProof() skips the section read: "the two bottles are back in the compartment after the restart (expected 2, got 0)", "the collected car reads a valid sticker"). Restart: `tools/cli wt e2e slice-3435`.
//! A parked car keeps its inspection sticker and its hidden compartment's goods across a restart (#3435,
//! the "vehiclecache" save section). An owned car on the street is inspected (expiry day 107) and holds two
//! bottles in a fitted compartment; the save point captures the compartment and writes both under the car's
//! garage id. After a restart (records back, ledger and sticker empty) the section brings both back; the car
//! collected from the impound is fitted again with the two bottles, once, and reads a valid sticker. A row
//! for a vehicle that no longer exists is dropped and logged. Across a real restart (`tools\cli wt e2e`):
//! the save boot leaves the car on the street, the reload boot collects it with its sticker and goods.
[LF_TestInfo(LF_TestTier.PERSISTENCE)]
class LF_Test_VehicleCacheRestart : LF_Test
{
	protected static const string OWNER = "vehicle-cache-owner";
	protected static const string VEHICLE = "vehicle-cache-car";
	protected static const string PLATE = "VC CH 3435";
	protected static const string UNKNOWN = "vehicle-cache-no-such-car";
	protected static const int TODAY = 100;
	protected static const int EXPIRY = 107;
	//! The e2e car (the save boot seeds it on the street, the reload boot collects it).
	static const string E2E_ID = "e2e-vehicle-cache-car";
	static const string E2E_OWNER = "e2e-vehicle-cache-owner";
	static const string E2E_PLATE = "EE VC 3435";

	override string GetName()
	{
		return "vehicles/cache-restart";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		LF_PersistenceComponent persistence;
		if (gameMode)
			persistence = LF_PersistenceComponent.Cast(gameMode.FindComponent(LF_PersistenceComponent));

		ctx.NotNull(persistence, "the game mode carries LF_PersistenceComponent");
		if (!persistence)
			return;

#ifdef LF_E2E_LOAD_BOOT
		if (LF_E2E.RequiresSection(LF_VehicleCacheSavePayload.NAME))
			VerifyE2E(ctx);

		return;
#endif

		ctx.NotNull(LF_SaveSections.Get(LF_VehicleCacheSavePayload.NAME), "vehiclecache is a registered save section");
		LF_VehicleCache.ResetStatics();

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
		ctx.NotNull(car, "the car is on the street");
		if (!car)
		{
			Cleanup(owner, before, snapshotsBefore);
			return;
		}

		ctx.True(LF_Inspection.IdOf(car) == VEHICLE, "the sticker id of an owned car is its garage id");
		ctx.Equal(EXPIRY, LF_Inspection.Inspect(VEHICLE, TODAY, EXPIRY - TODAY), "the car is inspected");
		ctx.Equal(2, LoadGoods(ctx, car), "two bottles go in the fitted compartment");

		// The save point captures the street car's compartment, then writes the section.
		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(LF_SaveSections.OBJECT_PREFIX + LF_VehicleCacheSavePayload.NAME), "the save carries the vehiclecache section");
		ctx.Equal(0, LF_PersistenceComponentSerializer.PayloadSinceVersion(LF_VehicleCacheSavePayload.NAME), "vehiclecache is not a legacy positional payload");

		// Restart: the car is gone, the records, the ledger and the sticker with it.
		LF_TestCharacterFixture.Delete(car);
		LF_VehicleOwnership.ApplyAll(before);
		LF_StorageSnapshot.ApplyAll(snapshotsBefore);
		LF_VehicleCache.ResetStatics();
		LF_Inspection.Restore(VEHICLE, -1);
		ctx.True(LF_VehicleOwnership.GetVehicle(VEHICLE) == null, "the restarted session has no car in the world");
		ctx.Equal(-1, LF_Inspection.ExpiryOf(VEHICLE), "the restarted session holds no sticker");
		ctx.False(LF_VehicleCache.Has(VEHICLE), "the restarted session holds no compartment");

		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the save parses");
		LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, loadContext, "vehicles");
		// RedProof(): the section is not read, so neither the sticker nor the goods come back (`--red`).
		if (!RedProof())
			ReadSection(saved, ctx);

		ctx.Equal(EXPIRY, LF_Inspection.ExpiryOf(VEHICLE), "the sticker's expiry is back after the restart");
		ctx.True(LF_VehicleCache.Has(VEHICLE), "the compartment row is back after the restart");
		LF_VehicleOwnership.ImpoundStreetVehicles();
		ctx.True(LF_VehicleOwnership.IsImpounded(VEHICLE), "the street car is at the impound after the load");
		LF_MoneyUtils.AddCash(owner, fee);
		ctx.True(LF_VehicleOwnership.CollectFromImpound(VEHICLE, OWNER, owner, fee, LF_TestAnnex.IMPOUND_PAD_ID, reasonKey), "the owner collects the car after the restart");
		car = LF_VehicleOwnership.GetVehicle(VEHICLE);
		ctx.NotNull(car, "the car is back on the street");
		if (car)
		{
			LF_VehicleCache.Apply(VEHICLE, car);
			ctx.Equal(2, CountGoods(car), "the two bottles are back in the compartment after the restart");
			ctx.Equal(0, LF_VehicleCache.Apply(VEHICLE, car), "restoring again spawns nothing");
			ctx.Equal(2, CountGoods(car), "the goods are back once, not doubled");
			ctx.True(LF_Inspection.Check(LF_Inspection.IdOf(car), TODAY) == LF_EInspectionStatus.VALID, "the collected car reads a valid sticker");

			// Reading the section again ends in the same state.
			ReadSection(saved, ctx);
			ctx.Equal(2, CountGoods(car), "a second read leaves the same goods");
			ctx.Equal(EXPIRY, LF_Inspection.ExpiryOf(VEHICLE), "a second read leaves the same sticker");
		}

		// A row for a vehicle that no longer exists is dropped (and logged).
		array<ref LF_VehicleCacheRow> rows = {};
		rows.Insert(LF_VehicleCacheRow.Create(UNKNOWN, EXPIRY, LF_StorageSnapshotRecord.Create(UNKNOWN, LF_VehicleCache.SLOT_COMPARTMENT)));
		ctx.Equal(0, LF_VehicleCache.ApplyAll(rows), "a row for an unknown vehicle is not kept");
		ctx.Equal(-1, LF_Inspection.ExpiryOf(UNKNOWN), "no sticker for the unknown vehicle");
		ctx.False(LF_VehicleCache.Has(UNKNOWN), "no compartment for the unknown vehicle");
		ctx.Equal(-1, LF_Inspection.ExpiryOf(VEHICLE), "a read without the car's row clears its sticker");

		// A save without the section (an older save) holds neither.
		JsonSaveContext oldContext = new JsonSaveContext(false);
		array<string> omit = {LF_VehicleCacheSavePayload.NAME};
		LF_PersistenceComponentSerializer.SerializeStoreOmitting(persistence, oldContext, omit);
		ctx.False(oldContext.SaveToString().Contains(LF_SaveSections.OBJECT_PREFIX + LF_VehicleCacheSavePayload.NAME), "the older save has no vehiclecache section");

		Cleanup(owner, before, snapshotsBefore);

#ifdef LF_E2E_HOST_STOP
		SeedE2E(ctx);
#endif
	}

	//------------------------------------------------------------------------------------------------
	//! Fits the car's compartment and puts two bottles of moonshine in it.
	//! \return The bottles in the compartment afterwards.
	protected static int LoadGoods(LF_TestContext ctx, IEntity car)
	{
		IEntity panel = LF_VehicleCompartment.Fit(car);
		ctx.NotNull(panel, "fitting builds the compartment");
		if (!panel)
			return 0;

		InventoryStorageManagerComponent bag = LF_InventoryUtils.GetResponsibleStorageManager(panel);
		if (!bag)
			return 0;

		LF_InventoryUtils.AddAmount(bag, LF_Test_AlcoholDrinks.MOONSHINE, 2);
		return CountGoods(car);
	}

	//------------------------------------------------------------------------------------------------
	//! \return Bottles of moonshine in the car's compartment; 0 with no compartment.
	protected static int CountGoods(IEntity car)
	{
		IEntity panel = LF_VehicleCompartment.Find(car);
		if (!panel)
			return 0;

		InventoryStorageManagerComponent bag = LF_InventoryUtils.GetResponsibleStorageManager(panel);
		if (!bag)
			return 0;

		return LF_InventoryUtils.GetAmount(bag, LF_Test_AlcoholDrinks.MOONSHINE);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Cleanup(IEntity owner, array<ref LF_VehicleRecord> before, array<ref LF_StorageSnapshotRecord> snapshotsBefore)
	{
		LF_TestCharacterFixture.Delete(LF_VehicleOwnership.GetVehicle(VEHICLE));
		LF_TestCharacterFixture.Delete(LF_VehicleOwnership.GetKey(VEHICLE));
		LF_TestCharacterFixture.Delete(owner);
		LF_VehicleOwnership.ApplyAll(before);
		LF_StorageSnapshot.ApplyAll(snapshotsBefore);
		LF_VehicleCache.ResetStatics();
		LF_Inspection.Restore(VEHICLE, -1);
		LF_LicensePlateRegistry.Release(PLATE);
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the vehiclecache section of a whole-store JSON save, as LF_SaveSections.ReadAll does.
	protected static void ReadSection(string saved, LF_TestContext ctx)
	{
		LF_SavePayload section = LF_SaveSections.Get(LF_VehicleCacheSavePayload.NAME);
		if (!section)
			return;

		JsonLoadContext context = new JsonLoadContext(false);
		ctx.True(context.LoadFromString(saved), "the save parses");
		ctx.True(context.StartObject(LF_SaveSections.OBJECT_PREFIX + LF_VehicleCacheSavePayload.NAME), "the vehiclecache section opens");
		int savedVersion;
		context.ReadValue(LF_SaveSections.VERSION_KEY, savedVersion);
		ctx.Equal(section.Version(), savedVersion, "the section carries its own version");
		ctx.True(section.ReadSection(context, savedVersion), "the vehiclecache section reads");
		context.EndObject();
	}

	//------------------------------------------------------------------------------------------------
	//! The impound pad the e2e car uses: the annex one the grid leg has, else the town's, "" when none.
	protected static string E2EPad()
	{
		if (LF_VehicleSpawnPointComponent.CountForShop(LF_TestAnnex.IMPOUND_PAD_ID) > 0)
			return LF_TestAnnex.IMPOUND_PAD_ID;

		if (LF_VehicleSpawnPointComponent.CountForShop("roadmap-impound") > 0)
			return "roadmap-impound";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! wt e2e save boot: an owned car on the street with a sticker and two bottles in its compartment.
	static void SeedE2E(LF_TestContext ctx)
	{
		string pad = E2EPad();
		if (pad.IsEmpty())
		{
			LF_Debug.Info(LF_VehicleCache.FEATURE, "e2e cache car not seeded: this world has no grid or roadmap impound pad");
			return;
		}

		IEntity payer = LF_TestCarriedInventoryKit.SpawnLoaded(ctx);
		if (!payer)
			return;

		LF_VehicleOwnership.Register(E2E_ID, E2E_OWNER, E2E_ID, LF_VehicleCatalog.S105, LF_VehicleCatalog.S105_LOCKED, "grid-lot", E2E_PLATE, "#EL-Vehicle_S105", true);
		int fee = LF_GarageConfig.ImpoundFee();
		LF_MoneyUtils.AddCash(payer, fee);
		string reasonKey;
		ctx.True(LF_VehicleOwnership.CollectFromImpound(E2E_ID, E2E_OWNER, payer, fee, pad, reasonKey), "e2e cache car is put on the street");
		IEntity car = LF_VehicleOwnership.GetVehicle(E2E_ID);
		if (car)
		{
			LF_Inspection.Inspect(E2E_ID, TODAY, EXPIRY - TODAY);
			ctx.Equal(2, LoadGoods(ctx, car), "e2e cache car holds two bottles before the save");
		}

		LF_TestCharacterFixture.Delete(payer);
	}

	//------------------------------------------------------------------------------------------------
	//! wt e2e reload boot: the car is at the impound with its sticker; collecting it brings the goods back.
	static void VerifyE2E(LF_TestContext ctx)
	{
		// The save holds the section (RequiresSection), so this build's save boot seeded the car wherever it has a pad.
		if (E2EPad().IsEmpty())
			return;

		ctx.False(LF_VehicleOwnership.GetOwner(E2E_ID).IsEmpty(), "e2e the cache car's record is back after the restart");

		ctx.True(LF_VehicleOwnership.IsImpounded(E2E_ID), "e2e cache car is at the impound after the restart");
		ctx.Equal(EXPIRY, LF_Inspection.ExpiryOf(E2E_ID), "e2e the sticker is back after the restart");
		ctx.True(LF_VehicleCache.Has(E2E_ID), "e2e the compartment row is back after the restart");
		IEntity payer = LF_TestCarriedInventoryKit.SpawnLoaded(ctx);
		if (!payer)
			return;

		int fee = LF_GarageConfig.ImpoundFee();
		LF_MoneyUtils.AddCash(payer, fee);
		string reasonKey;
		ctx.True(LF_VehicleOwnership.CollectFromImpound(E2E_ID, E2E_OWNER, payer, fee, E2EPad(), reasonKey), "e2e cache car collected after the restart");
		IEntity car = LF_VehicleOwnership.GetVehicle(E2E_ID);
		if (car)
		{
			LF_VehicleCache.Apply(E2E_ID, car);
			int bottles = CountGoods(car);
			ctx.Equal(2, bottles, "e2e the two bottles are back in the compartment after the restart");
			LF_Debug.Info(LF_VehicleCache.FEATURE, string.Format("e2e restart: %1 collected with sticker to day %2 and %3 bottle(s) in its compartment", E2E_ID, LF_Inspection.ExpiryOf(E2E_ID), bottles));
		}

		LF_VehicleOwnership.Store(E2E_ID, E2E_OWNER);
		LF_TestCharacterFixture.Delete(payer);
	}
}
