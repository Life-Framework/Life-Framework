// red-proof: observed 2026-11-08 with `tools\cli test --no-build --only ...`.
// LF_VehicleOwnership.IsImpounded returning false and the fee line in LF_GarageLedger.BuildParts
// skipped: `SUMMARY tier=only passed=0 failed=4 total=4`,
// `- title, two rows, the fee line, and close (expected 5, got 4)`,
// `- the ledger lists only the reader's two cars (expected 2, got 1)`,
// `- the street car is at the impound lot after the load`, `- the owner pays and collects`.
// The impound-state check in CollectFromImpound skipped: `SUMMARY tier=only passed=1 failed=1 total=2`,
// `- the car is collected only once`, `- the fee is charged once (expected 350, got 100)`.
// Restored; green.

// tier: LOGIC
//! The ledger card lists the reader's vehicles: model, plate, and where each one is.
class LF_Test_GarageLedger : LF_Test
{
	protected static const string OWNER = "garage-ledger-owner";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "garage/ledger-card";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<string> nameKeys = {"#EL-Vehicle_S105", "#EL-Vehicle_UAZ469"};
		array<string> plates = {"LG AA 1001", "LG BB 2002"};
		array<int> impounded = {0, 1};
		array<ref LF_UIPart> parts = LF_GarageLedger.BuildParts(nameKeys, plates, impounded, 250);
		ctx.Equal(5, parts.Count(), "title, two rows, the fee line, and close");
		if (parts.Count() == 5)
		{
			ctx.True(parts.Get(0).GetKind() == LF_EUIPartKind.TITLE, "the card opens with its title");
			ctx.True(parts.Get(1).GetKind() == LF_EUIPartKind.LINE, "a stored car is a wrapped line, not a clipped row");
			ctx.True(parts.Get(1).GetText().Contains("LG AA 1001"), "the row prints the plate");
			ctx.True(parts.Get(2).GetText().Contains("LG BB 2002"), "the impounded car is listed too");
			ctx.True(parts.Get(3).GetText().Contains("250"), "the fee line prints the configured fee");
			ctx.True(parts.Get(4).GetKind() == LF_EUIPartKind.CLOSE, "the card closes");
		}

		array<int> allStored = {0, 2};
		ctx.Equal(4, LF_GarageLedger.BuildParts(nameKeys, plates, allStored, 250).Count(), "no fee line when nothing is impounded (stored, on the street)");

		array<string> none = {};
		array<int> noFlags = {};
		array<ref LF_UIPart> empty = LF_GarageLedger.BuildParts(none, none, noFlags, 250);
		ctx.Equal(3, empty.Count(), "an empty ledger is title, the empty line, and close");
		if (empty.Count() == 3)
			ctx.EqualStr("#LF-Garage_LedgerEmpty", empty.Get(1).GetText(), "an empty ledger says so");

		array<int> shortFlags = {0};
		ctx.Equal(3, LF_GarageLedger.BuildParts(nameKeys, plates, shortFlags, 250).Count(), "mismatched lists print no rows");

		// Rows come from the owner's records: stored first, then impounded. A record with no live
		// entity that is not stored is at the impound lot.
		array<ref LF_VehicleRecord> before = LF_VehicleOwnership.ExportAll();
		LF_VehicleOwnership.Register("ledger-street", OWNER, "ledger-street", LF_VehicleCatalog.S105, LF_VehicleCatalog.S105_LOCKED, "roadmap-lot", "LG CC 3003", "#EL-Vehicle_S105", true);
		LF_VehicleOwnership.Register("ledger-kept", OWNER, "ledger-kept", LF_VehicleCatalog.UAZ469, LF_VehicleCatalog.UAZ469_LOCKED, "roadmap-lot", "LG DD 4004", "#EL-Vehicle_UAZ469", true);
		LF_VehicleOwnership.Store("ledger-kept", OWNER);
		LF_VehicleOwnership.Register("ledger-other", "garage-ledger-stranger", "ledger-other", LF_VehicleCatalog.S105, LF_VehicleCatalog.S105_LOCKED, "roadmap-lot", "LG EE 5005", "#EL-Vehicle_S105", true);

		array<ref LF_VehicleGarageRow> rows = LF_VehicleGarageDialog.LedgerRows(OWNER);
		ctx.Equal(2, rows.Count(), "the ledger lists only the reader's two cars");
		if (rows.Count() == 2)
		{
			ctx.EqualStr("LG DD 4004", rows.Get(0).m_sPlate, "the stored car comes first");
			ctx.Equal(LF_GarageLedger.PLACE_STORED, rows.Get(0).m_iPlace, "the stored car is in the garage");
			ctx.EqualStr("LG CC 3003", rows.Get(1).m_sPlate, "the street car follows");
			ctx.Equal(LF_GarageLedger.PLACE_IMPOUNDED, rows.Get(1).m_iPlace, "the unspawned street car is at the impound lot");
		}

		LF_VehicleOwnership.ApplyAll(before);
		LF_LicensePlateRegistry.Release("LG CC 3003");
		LF_LicensePlateRegistry.Release("LG DD 4004");
		LF_LicensePlateRegistry.Release("LG EE 5005");
	}
}

//------------------------------------------------------------------------------------------------
// tier: PERSISTENCE
//! Garage it to keep it, through the real serializer: the garaged car comes back stored, the
//! street car is at the impound lot, and the impound desk releases it once for the fee.
class LF_Test_VehicleImpound : LF_Test
{
	protected static const string OWNER = "impound-owner";
	protected static const string KEPT = "impound-kept";
	protected static const string STREET = "impound-street";
	protected static const string KEPT_PLATE = "IM KP 6006";
	protected static const string STREET_PLATE = "IM ST 7007";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "vehicles/impound";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		ctx.NotNull(gameMode, "the DebugWorld game mode resolves");
		if (!gameMode)
			return;

		LF_PersistenceComponent persistence = LF_PersistenceComponent.Cast(gameMode.FindComponent(LF_PersistenceComponent));
		ctx.NotNull(persistence, "the game mode carries LF_PersistenceComponent");
		if (!persistence)
			return;

		int fee = LF_GarageConfig.ImpoundFee();
		ctx.Equal(250, fee, "the impound fee comes from LF_Garage.conf");
		ctx.True(LF_VehicleSpawnPointComponent.CountForShop("roadmap-garage") > 0, "the garage has its own pad");
		ctx.True(LF_VehicleSpawnPointComponent.CountForShop("roadmap-impound") > 0, "the impound yard has a pad");

		array<ref LF_VehicleRecord> before = LF_VehicleOwnership.ExportAll();
		LF_VehicleOwnership.Register(KEPT, OWNER, KEPT, LF_VehicleCatalog.S105, LF_VehicleCatalog.S105_LOCKED, "roadmap-lot", KEPT_PLATE, "#EL-Vehicle_S105", true);
		LF_VehicleOwnership.Register(STREET, OWNER, STREET, LF_VehicleCatalog.S105, LF_VehicleCatalog.S105_LOCKED, "roadmap-lot", STREET_PLATE, "#EL-Vehicle_S105", true);
		ctx.True(LF_VehicleOwnership.Store(KEPT, OWNER), "the owner garages one car before the save");

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();

		// Restart: the session knows neither car.
		LF_VehicleOwnership.ApplyAll(before);
		ctx.EqualStr("", LF_VehicleOwnership.GetOwner(STREET), "the restarted session starts without the cars");

		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the save parses");
		int version;
		loadContext.ReadValue("version", version);
		LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, loadContext, "vehicles");

		ctx.True(LF_VehicleOwnership.IsStored(KEPT), "the garaged car is still in the garage after the load");
		ctx.False(LF_VehicleOwnership.IsImpounded(KEPT), "the garaged car is not impounded");
		ctx.True(LF_VehicleOwnership.IsImpounded(STREET), "the street car is at the impound lot after the load");
		ctx.True(LF_VehicleOwnership.GetVehicle(STREET) == null, "the impounded car is not spawned on the street");

		IEntity owner = LF_TestCharacterFixture.Spawn(ctx);
		if (!owner)
		{
			LF_VehicleOwnership.ApplyAll(before);
			return;
		}

		LF_MoneyUtils.AddCash(owner, fee - 1);
		string reasonKey;
		ctx.False(LF_VehicleOwnership.CollectFromImpound(STREET, OWNER, owner, fee, "roadmap-impound", reasonKey), "short cash does not release the car");
		ctx.EqualStr("#LF-Impound_CannotAfford", reasonKey, "the owner is told the fee is short");
		ctx.Equal(fee - 1, LF_MoneyUtils.GetCash(owner), "a refused collect takes no cash");
		ctx.True(LF_VehicleOwnership.IsImpounded(STREET), "the car stays impounded");

		LF_MoneyUtils.AddCash(owner, 101);
		int cashBefore = LF_MoneyUtils.GetCash(owner);
		ctx.False(LF_VehicleOwnership.CollectFromImpound(STREET, "impound-stranger", owner, fee, "roadmap-impound", reasonKey), "a stranger cannot collect the car");
		ctx.True(LF_VehicleOwnership.CollectFromImpound(STREET, OWNER, owner, fee, "roadmap-impound", reasonKey), "the owner pays and collects");
		ctx.Equal(cashBefore - fee, LF_MoneyUtils.GetCash(owner), "the fee is taken from carried cash");
		ctx.False(LF_VehicleOwnership.IsImpounded(STREET), "the collected car left the impound lot");
		IEntity released = LF_VehicleOwnership.GetVehicle(STREET);
		ctx.True(released != null, "the collected car is on the impound pad");
		IEntity desk = GetGame().GetWorld().FindEntityByName("Roadmap_Impound_Desk");
		ctx.True(desk != null, "Roadmap_Impound_Desk is placed");
		if (released && desk)
			ctx.True(vector.Distance(released.GetOrigin(), desk.GetOrigin()) < 20, "the car is released inside the impound yard");

		IEntity key = LF_VehicleOwnership.GetKey(STREET);
		ctx.True(key != null, "the owner is handed a key bound to the car");
		if (key && released)
		{
			LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(released);
			ctx.True(vehicleLock && vehicleLock.IsValidKey(key), "the handed key opens the collected car");
		}

		// The owner can afford a second fee, so only the impound state stops a second release.
		LF_MoneyUtils.AddCash(owner, fee);
		ctx.False(LF_VehicleOwnership.CollectFromImpound(STREET, OWNER, owner, fee, "roadmap-impound", reasonKey), "the car is collected only once");
		ctx.Equal(cashBefore, LF_MoneyUtils.GetCash(owner), "the fee is charged once");

		// The garaged car comes out on the garage pad, in front of the shell.
		ctx.True(LF_VehicleOwnership.Retrieve(KEPT, OWNER, "roadmap-garage", owner), "the owner retrieves the garaged car");
		IEntity retrieved = LF_VehicleOwnership.GetVehicle(KEPT);
		IEntity garage = GetGame().GetWorld().FindEntityByName("Roadmap_Garage");
		ctx.True(retrieved != null && garage != null, "the retrieved car and the garage are in the world");
		if (retrieved && garage)
			ctx.True(vector.Distance(retrieved.GetOrigin(), garage.GetOrigin()) < 10, "the retrieved car stands at the garage");

		// Cleanup: store both (removes the entities), drop the keys, restore the registry.
		LF_VehicleOwnership.Store(STREET, OWNER);
		LF_VehicleOwnership.Store(KEPT, OWNER);
		IEntity streetKey = LF_VehicleOwnership.GetKey(STREET);
		if (streetKey)
			SCR_EntityHelper.DeleteEntityAndChildren(streetKey);

		IEntity keptKey = LF_VehicleOwnership.GetKey(KEPT);
		if (keptKey)
			SCR_EntityHelper.DeleteEntityAndChildren(keptKey);

		LF_TestCharacterFixture.Delete(owner);
		LF_VehicleOwnership.ApplyAll(before);
		LF_LicensePlateRegistry.Release(KEPT_PLATE);
		LF_LicensePlateRegistry.Release(STREET_PLATE);
	}
}
