// order-proof (2026-11-19): the seeded e2e truck stayed in every plain run; `--only LF_Test_CargoBedPersist,LF_Test_DebugWorldCollision` failed debugworld/entity-collision (6 SupplyStack_Large_01_Vehicle props at <106,1.4,62> not traceable). Seeded only under LF_E2E_HOST_STOP now: green.
// red-proof: observed 2026-11-19 `tools/cli test --no-build --red LF_Test_CargoBedPersist`: RED PROOF OK (RedProof() skips the section read, so the row and the bed load never come back after the restart). Restart: `tools/cli wt e2e slice-3266`.
//! A parked truck keeps its logs across a restart (#3266, the "cargobed" save section). An owned truck on
//! the street holds a mixed load; the save point writes it under the truck's garage key; after a restart
//! (records back, ledger empty) the section brings the key's row back and the truck spawning again at the
//! impound takes its load. Reading the section twice ends in the same state and replaces a live bed's
//! stack, a row for an unknown vehicle or with no readable load is dropped, an empty bed clears its row,
//! and a save without the section holds no load. Across a restart (`tools\cli wt e2e`): the save boot
//! leaves a loaded truck on the street; the reload boot finds it at the impound and collects it loaded.
[LF_TestInfo(LF_TestTier.PERSISTENCE)]
class LF_Test_CargoBedPersist : LF_Test
{
	protected static const string OWNER = "cargo-bed-owner";
	protected static const string VEHICLE = "cargo-bed-truck";
	protected static const string PLATE = "CB ED 3266";
	protected static const string UNKNOWN = "cargo-bed-no-such-truck";
	//! The e2e truck (this test's save boot seeds it on the street, the reload boot collects it).
	static const string E2E_ID = "e2e-cargo-bed-truck";
	static const string E2E_OWNER = "e2e-cargo-bed-owner";
	static const string E2E_PLATE = "EE CB 3266";
	//! 2 softwood logs and 1 hardwood log: the stack "0,0,1,1" (4 units).
	static const string LOADED = "0,0,1,1";

	override string GetName()
	{
		return "carry/cargo-bed-persist";
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
		// The reload boot: the e2e truck comes back from the real save with its load, when it held the section.
		if (LF_E2E.RequiresSection(LF_CargoBedSavePayload.NAME))
			VerifyE2E(ctx);

		return;
#endif

		ctx.NotNull(LF_SaveSections.Get(LF_CargoBedSavePayload.NAME), "cargobed is a registered save section");
		LF_CargoBedPersist.ResetStatics();

		array<ref LF_VehicleRecord> before = LF_VehicleOwnership.ExportAll();
		array<ref LF_StorageSnapshotRecord> snapshotsBefore = LF_StorageSnapshot.ExportAll();
		IEntity owner = LF_TestCarriedInventoryKit.SpawnLoaded(ctx);
		if (!owner)
			return;

		LF_VehicleOwnership.Register(VEHICLE, OWNER, VEHICLE, LF_VehicleCatalog.M923, LF_VehicleCatalog.M923_LOCKED, "grid-lot", PLATE, "#EL-Vehicle_M923", true);
		int fee = LF_GarageConfig.ImpoundFee();
		LF_MoneyUtils.AddCash(owner, fee);
		string reasonKey;
		ctx.True(LF_VehicleOwnership.CollectFromImpound(VEHICLE, OWNER, owner, fee, LF_TestAnnex.IMPOUND_PAD_ID, reasonKey), "the owner collects the truck");
		IEntity truck = LF_VehicleOwnership.GetVehicle(VEHICLE);
		LF_CargoBedComponent bed;
		if (truck)
			bed = LF_Component<LF_CargoBedComponent>.Find(truck);

		ctx.NotNull(bed, "the truck carries a cargo bed");
		if (!bed)
		{
			Cleanup(owner, before, snapshotsBefore);
			return;
		}

		ctx.True(bed.IsEmpty(), "a truck with no saved load starts with an empty bed");
		ctx.True(bed.ServerLoadKind("log") == LF_ECargoRefusal.NONE, "a softwood log goes on the bed");
		ctx.True(bed.ServerLoadKind("log") == LF_ECargoRefusal.NONE, "a second softwood log goes on the bed");
		ctx.True(bed.ServerLoadKind("hardwood") == LF_ECargoRefusal.NONE, "a hardwood log goes on the bed");
		ctx.True(bed.SavedText() == LOADED, "the bed holds two softwood logs and a hardwood log");

		// The save point captures the street truck's bed through the carried payload, then writes the section.
		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(LF_SaveSections.OBJECT_PREFIX + LF_CargoBedSavePayload.NAME), "the save carries the cargobed section");
		ctx.Equal(0, LF_PersistenceComponentSerializer.PayloadSinceVersion(LF_CargoBedSavePayload.NAME), "cargobed is not a legacy positional payload");
		ctx.Equal(1, LF_CargoBedPersist.Count(), "the ledger holds the one loaded truck");

		// Restart: the truck is gone, the records and the ledger with it.
		LF_TestCharacterFixture.Delete(truck);
		LF_VehicleOwnership.ApplyAll(before);
		LF_StorageSnapshot.ApplyAll(snapshotsBefore);
		LF_CargoBedPersist.ResetStatics();
		ctx.True(LF_VehicleOwnership.GetVehicle(VEHICLE) == null, "the restarted session has no truck in the world");
		ctx.True(LF_CargoBedPersist.StackOf(VEHICLE) == "", "the restarted session holds no load");

		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the save parses");
		LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, loadContext, "vehicles");
		// RedProof(): the section is not read, so no load comes back (`--red`).
		if (!RedProof())
			ReadSection(saved, ctx);

		ctx.True(LF_CargoBedPersist.StackOf(VEHICLE) == LOADED, "the section brings the truck's row back");
		LF_VehicleOwnership.ImpoundStreetVehicles();
		ctx.True(LF_VehicleOwnership.IsImpounded(VEHICLE), "the street truck is at the impound after the load");
		LF_MoneyUtils.AddCash(owner, fee);
		ctx.True(LF_VehicleOwnership.CollectFromImpound(VEHICLE, OWNER, owner, fee, LF_TestAnnex.IMPOUND_PAD_ID, reasonKey), "the owner collects the truck after the restart");
		truck = LF_VehicleOwnership.GetVehicle(VEHICLE);
		bed = null;
		if (truck)
			bed = LF_Component<LF_CargoBedComponent>.Find(truck);

		ctx.NotNull(bed, "the truck is back with its bed");
		if (!bed)
		{
			Cleanup(owner, before, snapshotsBefore);
			return;
		}

		ctx.True(bed.SavedText() == LOADED, "the truck's load is back on the bed after the restart");
		ctx.Equal(4, bed.GetUnits(), "four units are on the bed");

		// Reading the section again, twice, ends in the same state; it replaces a live bed's stack.
		ReadSection(saved, ctx);
		ReadSection(saved, ctx);
		ctx.True(bed.SavedText() == LOADED, "two reads leave the same load");
		ctx.Equal(1, LF_CargoBedPersist.Count(), "two reads leave one row");
		bed.ServerLoadKind("log");
		ctx.Equal(5, bed.GetUnits(), "a log was added after the save");
		ReadSection(saved, ctx);
		ctx.True(bed.SavedText() == LOADED, "the read puts the live bed back to the saved stack");
		bed.ServerRestore("");
		ReadSection(saved, ctx);
		ctx.True(bed.SavedText() == LOADED, "the read refills an emptied bed");

		// A row for a vehicle nobody owns, and a row with no readable load, are dropped.
		array<ref LF_CargoBedRow> rows = {};
		rows.Insert(LF_CargoBedRow.Create(UNKNOWN, "0,0"));
		rows.Insert(LF_CargoBedRow.Create(VEHICLE, "x,99"));
		ctx.Equal(0, LF_CargoBedPersist.ApplyAll(rows), "an unknown vehicle and an unreadable load keep no row");
		ctx.True(LF_CargoBedPersist.StackOf(UNKNOWN) == "", "no row for the unknown vehicle");
		ctx.Equal(0, bed.GetUnits(), "a live bed with no saved load is emptied by the read");
		ctx.False(LF_CargoBedPersist.Apply(UNKNOWN, null), "applying to a missing vehicle does nothing");

		// An empty bed clears its row at the next capture.
		rows.Clear();
		rows.Insert(LF_CargoBedRow.Create(VEHICLE, LOADED));
		ctx.Equal(1, LF_CargoBedPersist.ApplyAll(rows), "a good row is kept");
		bed.ServerRestore("");
		LF_CargoBedPersist.Capture(VEHICLE, truck);
		ctx.True(LF_CargoBedPersist.StackOf(VEHICLE) == "", "capturing an empty bed clears the row");

		// A save without the section (an older save) holds no load.
		JsonSaveContext oldContext = new JsonSaveContext(false);
		array<string> omit = {LF_CargoBedSavePayload.NAME};
		LF_PersistenceComponentSerializer.SerializeStoreOmitting(persistence, oldContext, omit);
		ctx.False(oldContext.SaveToString().Contains(LF_SaveSections.OBJECT_PREFIX + LF_CargoBedSavePayload.NAME), "the older save has no cargobed section");

		Cleanup(owner, before, snapshotsBefore);

		// Only the e2e save boot leaves one loaded truck on the street for the shutdown save. A plain suite
		// run leaving it put six non-traceable SupplyStack props of the M923 bed in front of
		// debugworld/entity-collision (red on main 13228af58, 2026-11-19).
#ifdef LF_E2E_HOST_STOP
		SeedE2E(ctx);
#endif
	}

	//------------------------------------------------------------------------------------------------
	protected static void Cleanup(IEntity owner, array<ref LF_VehicleRecord> before, array<ref LF_StorageSnapshotRecord> snapshotsBefore)
	{
		LF_TestCharacterFixture.Delete(LF_VehicleOwnership.GetVehicle(VEHICLE));
		LF_TestCharacterFixture.Delete(LF_VehicleOwnership.GetKey(VEHICLE));
		LF_TestCharacterFixture.Delete(owner);
		LF_VehicleOwnership.ApplyAll(before);
		LF_StorageSnapshot.ApplyAll(snapshotsBefore);
		LF_CargoBedPersist.ResetStatics();
		LF_LicensePlateRegistry.Release(PLATE);
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the cargobed section of a whole-store JSON save, as LF_SaveSections.ReadAll does.
	protected static void ReadSection(string saved, LF_TestContext ctx)
	{
		LF_SavePayload section = LF_SaveSections.Get(LF_CargoBedSavePayload.NAME);
		if (!section)
			return;

		JsonLoadContext context = new JsonLoadContext(false);
		ctx.True(context.LoadFromString(saved), "the save parses");
		ctx.True(context.StartObject(LF_SaveSections.OBJECT_PREFIX + LF_CargoBedSavePayload.NAME), "the cargobed section opens");
		int savedVersion;
		context.ReadValue(LF_SaveSections.VERSION_KEY, savedVersion);
		ctx.Equal(section.Version(), savedVersion, "the section carries its own version");
		ctx.True(section.ReadSection(context, savedVersion), "the cargobed section reads");
		context.EndObject();
	}

	//------------------------------------------------------------------------------------------------
	//! The impound pad the e2e truck uses: the annex one the grid leg has, else the town's, "" when none.
	protected static string E2EPad()
	{
		// Three slots: the engine's own persistence puts back the street truck as an unowned copy on slot 0 at the
		// reload, so a one-slot pad (grid-impound) would be taken before the owner collects.
		if (LF_VehicleSpawnPointComponent.CountForShop(LF_TestAnnex.IMPOUND_PAD_ID) > 0)
			return LF_TestAnnex.IMPOUND_PAD_ID;

		if (LF_VehicleSpawnPointComponent.CountForShop("roadmap-impound") > 0)
			return "roadmap-impound";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! wt e2e save boot: an owned truck on the street holds a mixed load; the shutdown save captures it.
	static void SeedE2E(LF_TestContext ctx)
	{
		string pad = E2EPad();
		if (pad.IsEmpty())
		{
			LF_Debug.Info(LF_CargoBed.FEATURE, "e2e truck not seeded: this world has no grid or roadmap impound pad");
			return;
		}

		IEntity payer = LF_TestCarriedInventoryKit.SpawnLoaded(ctx);
		if (!payer)
			return;

		LF_VehicleOwnership.Register(E2E_ID, E2E_OWNER, E2E_ID, LF_VehicleCatalog.M923, LF_VehicleCatalog.M923_LOCKED, "grid-lot", E2E_PLATE, "#EL-Vehicle_M923", true);
		int fee = LF_GarageConfig.ImpoundFee();
		LF_MoneyUtils.AddCash(payer, fee);
		string reasonKey;
		ctx.True(LF_VehicleOwnership.CollectFromImpound(E2E_ID, E2E_OWNER, payer, fee, pad, reasonKey), "e2e cargo bed truck is put on the street");
		IEntity truck = LF_VehicleOwnership.GetVehicle(E2E_ID);
		LF_CargoBedComponent bed;
		if (truck)
			bed = LF_Component<LF_CargoBedComponent>.Find(truck);

		ctx.NotNull(bed, "the e2e truck carries a cargo bed");
		if (bed)
		{
			bed.ServerLoadKind("log");
			bed.ServerLoadKind("log");
			bed.ServerLoadKind("hardwood");
			ctx.True(bed.SavedText() == LOADED, "e2e truck holds its load before the save");
		}

		LF_TestCharacterFixture.Delete(payer);
	}

	//------------------------------------------------------------------------------------------------
	//! wt e2e reload boot: the truck is at the impound; collecting it brings the load back on the bed.
	static void VerifyE2E(LF_TestContext ctx)
	{
		if (!LF_E2E.RequiresSeed("cargo bed truck (#3266)", !LF_VehicleOwnership.GetOwner(E2E_ID).IsEmpty()))
			return;

		ctx.True(LF_VehicleOwnership.IsImpounded(E2E_ID), "e2e cargo bed truck is at the impound after the restart");
		ctx.True(LF_CargoBedPersist.StackOf(E2E_ID) == LOADED, "e2e the truck's row is back after the restart");
		IEntity payer = LF_TestCarriedInventoryKit.SpawnLoaded(ctx);
		if (!payer)
			return;

		int fee = LF_GarageConfig.ImpoundFee();
		LF_MoneyUtils.AddCash(payer, fee);
		string reasonKey;
		ctx.True(LF_VehicleOwnership.CollectFromImpound(E2E_ID, E2E_OWNER, payer, fee, E2EPad(), reasonKey), "e2e cargo bed truck collected after the restart");
		IEntity truck = LF_VehicleOwnership.GetVehicle(E2E_ID);
		LF_CargoBedComponent bed;
		if (truck)
			bed = LF_Component<LF_CargoBedComponent>.Find(truck);

		ctx.NotNull(bed, "the collected e2e truck has a bed");
		if (bed)
		{
			ctx.True(bed.SavedText() == LOADED, "e2e the load is back on the bed after the restart");
			LF_Debug.Info(LF_CargoBed.FEATURE, string.Format("e2e restart: %1 collected with %2 unit(s) on its bed", E2E_ID, bed.GetUnits()));
		}

		LF_VehicleOwnership.Store(E2E_ID, E2E_OWNER);
		LF_TestCharacterFixture.Delete(payer);
	}
}
