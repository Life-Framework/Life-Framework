// red-proof: #4714, 2026-11-24, `tools\cli test --no-build --red LF_Test_LiverySaveRoundTrip` (the respray skipped): RED-OK
// persist/livery-save (fails with the hook on, 26 assertions); - the record loads in the new paint (load 1, 2), - the loaded car is
// retrieved in the new paint. Green with the respray (--only).
//! A resprayed car keeps its paint across a restart (WO-L10, #4714). The respray (LF_LiveryShop, #4473) is a
//! spawn prefab swap: the car's `vehicles` record names the new paint, so the save needs no new field. In any
//! run: a car resprayed at the bay and garaged writes its paint into the store; with the live record put back
//! to the old paint, reading the store (twice) brings the new paint back, and a retrieve spawns it.
//! Across a restart (`tools\cli wt e2e`): the save boot leaves a resprayed car in the garage; the reload boot
//! finds its record in the new paint and retrieves the car in it.
[LF_TestInfo(LF_TestTier.PERSISTENCE)]
class LF_Test_LiverySaveRoundTrip : LF_Test
{
	protected static const string OWNER = "livery-save-owner";
	protected static const string CAR = "livery-save-car";
	protected static const string PLATE = "LV SV 4714";
	static const string E2E_ID = "e2e-livery-save-car";
	static const string E2E_OWNER = "e2e-livery-save-owner";
	static const string E2E_PLATE = "EE LV 4714";
	protected static const int PRICE = 250;
	//! The stand-in livery for the S105: any locked prefab of another paint (as LF_Test_LiveryShop uses).
	static const ResourceName PAINT = LF_VehicleCatalog.S1203_LOCKED;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "persist/livery-save";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
#ifdef LF_E2E_LOAD_BOOT
		VerifyE2E(ctx);
		return;
#endif

		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		LF_PersistenceComponent persistence;
		if (gameMode)
			persistence = LF_PersistenceComponent.Cast(gameMode.FindComponent(LF_PersistenceComponent));

		ctx.NotNull(persistence, "the game mode carries LF_PersistenceComponent");
		if (!persistence)
			return;

		array<ref LF_VehicleRecord> before = LF_VehicleOwnership.ExportAll();
		UseShop();
		IEntity owner = LF_TestCarriedInventoryKit.SpawnLoaded(ctx);
		if (!owner)
		{
			Cleanup(owner, before);
			return;
		}

		bool painted = ResprayAndGarage(ctx, CAR, OWNER, PLATE, owner, !RedProof());
		ctx.True(painted, "the car is resprayed and garaged");

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();

		// Put the old paint back on the live record, so only the load can bring the new one.
		LF_VehicleRecord live = LF_VehicleOwnership.Export(CAR);
		if (live)
		{
			live.m_sSpawnPrefab = LF_VehicleCatalog.S105_LOCKED;
			LF_VehicleOwnership.Apply(live);
		}

		for (int pass = 1; pass <= 2; pass++)
		{
			string tag = string.Format(" (load %1)", pass);
			JsonLoadContext loadContext = new JsonLoadContext(false);
			ctx.True(loadContext.LoadFromString(saved), "the save parses" + tag);
			LF_PersistenceComponentSerializer.DeserializeStore(persistence, loadContext);
			LF_VehicleRecord loaded = LF_VehicleOwnership.Export(CAR);
			ctx.True(loaded && LF_VehiclePriceModel.SameResource(loaded.m_sSpawnPrefab, PAINT), "the record loads in the new paint" + tag);
			ctx.True(loaded && LF_VehiclePriceModel.SameResource(loaded.m_sPrefab, LF_VehicleCatalog.S105), "the garage record is still the S105" + tag);
			ctx.True(loaded && loaded.m_bStored && loaded.m_sOwnerUid == OWNER && loaded.m_sPlate == PLATE, "owner, plate and the garaged flag are kept" + tag);
		}

		ctx.True(RetrievesInPaint(ctx, CAR, OWNER, owner, ""), "the loaded car is retrieved in the new paint");
		Cleanup(owner, before);

#ifdef LF_E2E_HOST_STOP
		SeedE2E(ctx);
#endif
	}

	//------------------------------------------------------------------------------------------------
	//! One civilian livery for the S105 and no lot colours, so the bay offers exactly PAINT.
	protected static void UseShop()
	{
		LF_VehicleColoursConfig noColours = new LF_VehicleColoursConfig();
		noColours.m_aModels = {};
		LF_VehicleColoursConfig.Use(noColours);
		LF_LiveryShopConfig shop = LF_LiveryShopConfig.Make(150);
		shop.m_aLiveries.Insert(LF_LiveryShopRow.Make(LF_VehicleCatalog.S105, PAINT, "#EL-Vehicle_S105", PRICE, LF_Faction.CIVILIAN));
		LF_LiveryShopConfig.Use(shop);
	}

	//------------------------------------------------------------------------------------------------
	protected static void ResetShop()
	{
		LF_LiveryShopConfig.Reload();
		LF_VehicleColoursConfig.Reload();
	}

	//------------------------------------------------------------------------------------------------
	//! Registers an S105, collects it onto the impound pad, resprays it in PAINT at the bay (when `respray`)
	//! and garages it. \return True when the garaged record names PAINT.
	protected static bool ResprayAndGarage(LF_TestContext ctx, string vehicleId, string ownerUid, string plate, IEntity owner, bool respray)
	{
		LF_VehicleOwnership.Register(vehicleId, ownerUid, vehicleId, LF_VehicleCatalog.S105, LF_VehicleCatalog.S105_LOCKED, "grid-lot", plate, "#EL-Vehicle_S105", true);
		int fee = LF_GarageConfig.ImpoundFee();
		LF_MoneyUtils.AddCash(owner, fee + PRICE);
		string reasonKey;
		ctx.True(LF_VehicleOwnership.CollectFromImpound(vehicleId, ownerUid, owner, fee, LF_TestAnnex.IMPOUND_PAD_ID, reasonKey), "the owner collects the car");
		IEntity car = LF_VehicleOwnership.GetVehicle(vehicleId);
		if (!car)
			return false;

		int charged;
		if (respray)
			ctx.True(LF_LiveryShop.Respray(owner, ownerUid, LF_Faction.CIVILIAN, vehicleId, car.GetOrigin(), LF_TestAnnex.GARAGE_PAD_ID, PAINT, charged, reasonKey), "the owner pays for the respray");

		IEntity painted = LF_VehicleOwnership.GetVehicle(vehicleId);
		ctx.True(LF_VehicleOwnership.Store(vehicleId, ownerUid), "the resprayed car is garaged");
		// Store deletes the car at the end of the frame; move it off the pad so a retrieve this frame finds it free.
		if (painted)
			painted.SetOrigin(painted.GetOrigin() + LF_LiveryShop.PARK_OFFSET);

		LF_VehicleRecord record = LF_VehicleOwnership.Export(vehicleId);
		return record && record.m_bStored && LF_VehiclePriceModel.SameResource(record.m_sSpawnPrefab, PAINT);
	}

	//------------------------------------------------------------------------------------------------
	//! Retrieves the garaged car on the garage pad; \return True when the live car is the PAINT variant
	//! with its own id. The car goes back to the garage after.
	protected static bool RetrievesInPaint(LF_TestContext ctx, string vehicleId, string ownerUid, IEntity keyHolder, string tag)
	{
		ctx.True(LF_VehicleOwnership.Retrieve(vehicleId, ownerUid, LF_TestAnnex.GARAGE_PAD_ID, keyHolder), "the owner retrieves the car" + tag);
		IEntity car = LF_VehicleOwnership.GetVehicle(vehicleId);
		bool inPaint = car && LF_VehiclePriceModel.SameResource(car.GetPrefabData().GetPrefabName(), PAINT);
		LF_VehicleLockComponent vehicleLock;
		if (car)
			vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(car);

		ctx.True(vehicleLock && vehicleLock.GetVehicleIdentifier() == vehicleId, "the retrieved car keeps its id" + tag);
		if (car)
			LF_Debug.Info("Vehicle", string.Format("livery save: %1 retrieved as %2", vehicleId, car.GetPrefabData().GetPrefabName()));

		IEntity key = LF_VehicleOwnership.GetKey(vehicleId);
		LF_VehicleOwnership.Store(vehicleId, ownerUid);
		if (car)
			car.SetOrigin(car.GetOrigin() + LF_LiveryShop.PARK_OFFSET);

		LF_TestCharacterFixture.Delete(key);
		return inPaint;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Cleanup(IEntity owner, array<ref LF_VehicleRecord> before)
	{
		IEntity key = LF_VehicleOwnership.GetKey(CAR);
		LF_VehicleOwnership.Store(CAR, OWNER);
		LF_TestCharacterFixture.Delete(key);
		LF_TestCharacterFixture.Delete(owner);
		LF_VehicleOwnership.ApplyAll(before);
		LF_LicensePlateRegistry.Release(PLATE);
		ResetShop();
	}

	//------------------------------------------------------------------------------------------------
	//! wt e2e save boot: a resprayed S105 waits in the garage when the shutdown save runs.
	static void SeedE2E(LF_TestContext ctx)
	{
		IEntity payer = LF_TestCarriedInventoryKit.SpawnLoaded(ctx);
		if (!payer)
			return;

		UseShop();
		bool painted = ResprayAndGarage(ctx, E2E_ID, E2E_OWNER, E2E_PLATE, payer, true);
		ctx.True(painted, "e2e livery save: the resprayed car is garaged before the save");
		IEntity key = LF_VehicleOwnership.GetKey(E2E_ID);
		LF_TestCharacterFixture.Delete(key);
		LF_TestCharacterFixture.Delete(payer);
		ResetShop();
		LF_Debug.Info("Vehicle", string.Format("e2e livery save: %1 garaged in %2", E2E_ID, PAINT));
	}

	//------------------------------------------------------------------------------------------------
	//! wt e2e reload boot: the garaged car's record names the new paint and it is retrieved in it.
	static void VerifyE2E(LF_TestContext ctx)
	{
		if (!LF_E2E.RequiresSeed("livery save car (#4714)", !LF_VehicleOwnership.GetOwner(E2E_ID).IsEmpty()))
			return;

		LF_VehicleRecord record = LF_VehicleOwnership.Export(E2E_ID);
		ctx.True(record && record.m_bStored, "e2e livery save: the resprayed car is in the garage after the restart");
		ctx.True(record && LF_VehiclePriceModel.SameResource(record.m_sSpawnPrefab, PAINT), "e2e livery save: the record keeps the new paint after the restart");
		IEntity payer = LF_TestCarriedInventoryKit.SpawnLoaded(ctx);
		if (!payer)
			return;

		bool inPaint = RetrievesInPaint(ctx, E2E_ID, E2E_OWNER, payer, " (e2e)");
		ctx.True(inPaint, "e2e livery save: the car is retrieved in the new paint after the restart");
		LF_TestCharacterFixture.Delete(payer);
		if (inPaint)
			LF_Debug.Info("Vehicle", string.Format("e2e livery save kept: %1 in %2", E2E_ID, PAINT));
	}
}
