// red-proof: observed 2026-11-07 with `tools\cli test --no-build`, then each mutation
// restored and the suite rerun green.
// Run A, tier all, five mutations at once (one per test): `new LF_ATMManager();` back in
// LF_GameModeRoleplay.ResolveConnectManagers plus `s_Instance = this;` back in the
// LF_ATMManager constructor; order.Insert("jail") dropped and the version back to 4;
// SpawnOwned(record, origin, true) in RestoreVehicle; the Remember call removed from
// LF_VehicleOwnership.ApplyAll; the s_bLoadFailed guard in LF_WhitelistManager.Save
// replaced by `if (false)`. Observed:
//   SUMMARY tier=all passed=128 failed=5 total=133
//   [LFTEST] FAIL restart/bank-survives-connect
//   [LFTEST]   - the loaded bank account survives the first connect
//   [LFTEST] FAIL restart/jail-serializer-roundtrip
//   [LFTEST]   - the save is written at version 5 (expected 5, got 4)
//   [LFTEST]   - the sentence is served again after the load
//   [LFTEST] FAIL restart/vehicle-restore-no-ground-key (now restart/vehicle-street-impounded)
//   [LFTEST]   - restore drops no key beside the pad
// restart/vehicle-street-impounded, 2026-11-08 (M1-16): LF_VehicleOwnership.IsImpounded returning
// false failed it with `- the street car is at the impound lot`, `- a second pass keeps it impounded`.
//   [LFTEST] FAIL restart/vehicle-plates-reserved
//   [LFTEST]   - a purchase after the load cannot take the saved plate (expected '', got 'RS PL 7001')
//   [LFTEST] FAIL restart/whitelist-unreadable-file-kept
//   [LFTEST]   - the unreadable file keeps its first line (expected '99', got '1')
// Run B, tier fast, only the unconditional `s_Instance = null;` in ~LF_ATMManager:
//   SUMMARY tier=fast passed=81 failed=1 total=82
//   [LFTEST] FAIL restart/bank-survives-connect
//   [LFTEST]   - a released stray manager does not drop the loaded balances

//------------------------------------------------------------------------------------------------
// tier: LOGIC
class LF_Test_RestartBankSurvivesConnect : LF_Test
{
	protected static const string ACCOUNT_UID = "restart-bank-uid";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "restart/bank-survives-connect";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// The save load runs before any player connects and fills the bank cache.
		LF_ATMManager.Reset();
		array<ref LF_BankAccountRecord> records = new array<ref LF_BankAccountRecord>();
		records.Insert(LF_BankAccountRecord.Create(ACCOUNT_UID, 4200));
		LF_ATMManager.ApplyAll(records);
		LF_ATMManager loaded = LF_ATMManager.GetInstance();

		// First player connects: the game mode's manager path runs.
		LF_CharacterCreationManager creation = LF_GameModeRoleplay.ResolveConnectManagers();
		ctx.NotNull(creation, "the connect path resolves the character creation manager");

		LF_BankAccount account = LF_ATMManager.GetInstance().GetAccount(ACCOUNT_UID);
		ctx.NotNull(account, "the loaded bank account survives the first connect");
		if (account)
			ctx.Equal(4200, account.GetBalance(), "the loaded balance survives the first connect");
		ctx.True(LF_ATMManager.GetInstance() == loaded, "the connect path keeps the instance the load filled");

		// A stray construction and release must neither replace nor drop the loaded cache.
		LF_ATMManager stray = new LF_ATMManager();
		ctx.True(LF_ATMManager.GetInstance() == loaded, "constructing a manager does not replace the singleton");
		stray = null;
		account = LF_ATMManager.GetInstance().GetAccount(ACCOUNT_UID);
		ctx.NotNull(account, "a released stray manager does not drop the loaded balances");
		if (account)
			ctx.Equal(4200, account.GetBalance(), "the balance is intact after a stray manager is released");

		// Second connect resolves the same singletons.
		LF_JobManager jobs = LF_JobManager.GetInstance();
		LF_GameModeRoleplay.ResolveConnectManagers();
		ctx.True(LF_ATMManager.GetInstance() == loaded, "a second connect keeps the bank singleton");
		ctx.True(LF_JobManager.GetInstance() == jobs, "a second connect keeps the job singleton");
		ctx.True(LF_CharacterCreationManager.GetInstance() == creation, "a second connect keeps the character creation singleton");

		LF_JobManager strayJobs = new LF_JobManager();
		strayJobs = null;
		ctx.True(LF_JobManager.GetInstance() == jobs, "a released stray job manager does not drop the singleton");

		LF_ATMManager.Reset();
	}
}

//------------------------------------------------------------------------------------------------
// tier: WORLD
class LF_Test_RestartJailSerializerRoundtrip : LF_Test
{
	protected static const string PRISONER_UID = "restart-jail-uid";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "restart/jail-serializer-roundtrip";
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

		// Seed one serving sentence, keeping every sentence other tests own.
		array<ref LF_JailRecord> before = LF_Jail.ExportAll();
		array<ref LF_JailRecord> seeded = LF_Jail.ExportAll();
		seeded.Insert(LF_JailRecord.Create(PRISONER_UID, 240, true));
		LF_Jail.ApplyAll(seeded);
		ctx.True(LF_Jail.IsServing(PRISONER_UID), "the prisoner is serving before the save");

		// Save through the real serializer write path.
		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(PRISONER_UID), "the save carries the jail payload");

		// Restart: the session cache no longer has the sentence.
		LF_Jail.ApplyAll(before);
		ctx.False(LF_Jail.IsServing(PRISONER_UID), "the restarted session starts without the sentence");

		// Load through the real serializer read path.
		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the save parses");
		int version;
		loadContext.ReadValue("version", version);
		ctx.Equal(13, version, "the save is written at version 13 (jail is payload 5)");
		LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, loadContext, "jail");
		ctx.True(LF_Jail.IsServing(PRISONER_UID), "the sentence is served again after the load");
		ctx.EqualFloat(240, LF_Jail.GetRemaining(PRISONER_UID), 0.01, "the remaining time survives the load");

		// Re-applying the same save converges on the same state.
		JsonLoadContext again = new JsonLoadContext(false);
		again.LoadFromString(saved);
		LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, again, "jail");
		ctx.True(LF_Jail.IsServing(PRISONER_UID), "a second load keeps the sentence");
		ctx.EqualFloat(240, LF_Jail.GetRemaining(PRISONER_UID), 0.01, "a second load does not change the remaining time");

		LF_Jail.ApplyAll(before);
	}
}

//------------------------------------------------------------------------------------------------
// tier: WORLD
class LF_Test_RestartVehicleNoGroundKey : LF_Test
{
	protected static const string VEHICLE_ID = "restart-vehicle-id";
	protected static const string OWNER_UID = "restart-vehicle-owner";
	protected static const string PLATE = "RS VK 4242";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "restart/vehicle-street-impounded";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// A loaded save holds an owned vehicle that was out in the world. A restart has no
		// session for it: no live vehicle, no live key.
		array<ref LF_VehicleRecord> before = LF_VehicleOwnership.ExportAll();
		array<ref LF_VehicleRecord> loaded = LF_VehicleOwnership.ExportAll();
		loaded.Insert(LF_VehicleRecord.Create(VEHICLE_ID, OWNER_UID, VEHICLE_ID, LF_VehicleCatalog.S105, LF_VehicleCatalog.S105_LOCKED, "roadmap-lot", PLATE, "#EL-Vehicle_S105", true, false));
		LF_VehicleOwnership.ApplyAll(loaded);

		// Garage it to keep it: a car left on the street is at the impound lot, not on a pad.
		ctx.True(LF_VehicleOwnership.ImpoundStreetVehicles() >= 1, "the load counts the street car as impounded");
		ctx.True(LF_VehicleOwnership.IsImpounded(VEHICLE_ID), "the street car is at the impound lot");
		ctx.True(LF_VehicleOwnership.GetVehicle(VEHICLE_ID) == null, "the impounded car is not spawned");
		ctx.True(LF_VehicleOwnership.GetKey(VEHICLE_ID) == null, "the load drops no key anywhere");
		ctx.True(LF_VehicleOwnership.IsLocked(VEHICLE_ID), "the impounded car keeps its saved lock");

		LF_VehicleOwnership.ImpoundStreetVehicles();
		ctx.True(LF_VehicleOwnership.IsImpounded(VEHICLE_ID), "a second pass keeps it impounded");
		ctx.True(LF_VehicleOwnership.GetVehicle(VEHICLE_ID) == null, "a second pass spawns nothing");

		LF_VehicleOwnership.ApplyAll(before);
		LF_LicensePlateRegistry.Release(PLATE);
	}
}

//------------------------------------------------------------------------------------------------
// tier: LOGIC
class LF_Test_RestartVehiclePlatesReserved : LF_Test
{
	protected static const string VEHICLE_ID = "restart-plate-vehicle";
	protected static const string OWNER_UID = "restart-plate-owner";
	protected static const string PLATE = "RS PL 7001";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "restart/vehicle-plates-reserved";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// A restart starts with an empty plate registry.
		LF_LicensePlateRegistry.Release(PLATE);
		ctx.False(LF_LicensePlateRegistry.IsLive(PLATE), "the plate is free before the load");

		array<ref LF_VehicleRecord> before = LF_VehicleOwnership.ExportAll();
		array<ref LF_VehicleRecord> loaded = LF_VehicleOwnership.ExportAll();
		loaded.Insert(LF_VehicleRecord.Create(VEHICLE_ID, OWNER_UID, VEHICLE_ID, LF_VehicleCatalog.S105, LF_VehicleCatalog.S105_LOCKED, "roadmap-lot", PLATE, "#EL-Vehicle_S105", true, true));
		LF_VehicleOwnership.ApplyAll(loaded);

		ctx.True(LF_LicensePlateRegistry.IsLive(PLATE), "a loaded plate is reserved again");
		ctx.EqualStr("", LF_LicensePlateRegistry.TakeUnique(PLATE, "restart-plate-other"), "a purchase after the load cannot take the saved plate");

		LF_VehicleOwnership.ApplyAll(loaded);
		ctx.True(LF_LicensePlateRegistry.IsLive(PLATE), "a second load keeps the plate reserved");

		LF_VehicleOwnership.ApplyAll(before);
		LF_LicensePlateRegistry.Release(PLATE);
	}
}

//------------------------------------------------------------------------------------------------
// tier: LOGIC
class LF_Test_RestartWhitelistUnreadableFileKept : LF_Test
{
	protected static const string EXISTING_UID = "restart-whitelist-existing";
	protected static const string NEW_UID = "restart-whitelist-new";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "restart/whitelist-unreadable-file-kept";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		string folder = "$profile:TestResults";
		string path = folder + "/whitelist-unreadable.txt";
		FileIO.MakeDirectory(folder);

		// A whitelist this build cannot read (a future version) that still holds a grant.
		FileHandle writer = FileIO.OpenFile(path, FileMode.WRITE);
		ctx.True(writer != null, "the unreadable fixture file can be written");
		if (!writer)
			return;

		writer.WriteLine("99");
		writer.WriteLine("1");
		writer.WriteLine(EXISTING_UID);
		writer.WriteLine("POLICE");
		writer.WriteLine("0");
		writer.Close();

		LF_WhitelistManager.SetSavePath(path);
		LF_WhitelistManager.Reset();
		LF_WhitelistManager.Load();
		ctx.True(LF_WhitelistManager.IsLoadFailed(), "an unreadable file marks the load failed");
		ctx.False(LF_WhitelistManager.IsWhitelistedForFaction(EXISTING_UID, LF_EFactionType.POLICE), "an unreadable file leaves police closed");

		// An admin adds someone while the file is unreadable.
		LF_WhitelistManager.AddFactionWhitelist(NEW_UID, LF_EFactionType.POLICE);
		ctx.False(LF_WhitelistManager.Save(), "an admin edit is not saved over the unreadable file");

		string firstLine = "";
		bool existingKept = false;
		FileHandle reader = FileIO.OpenFile(path, FileMode.READ);
		if (reader)
		{
			reader.ReadLine(firstLine);
			string line;
			while (reader.ReadLine(line) > 0)
			{
				line.TrimInPlace();
				if (line == EXISTING_UID)
					existingKept = true;
			}
			reader.Close();
		}

		firstLine.TrimInPlace();
		ctx.EqualStr("99", firstLine, "the unreadable file keeps its first line");
		ctx.True(existingKept, "the unreadable file keeps the existing grant");

		// A readable file clears the flag again (the restart after the owner fixes the file).
		FileIO.DeleteFile(path);
		LF_WhitelistManager.Reset();
		LF_WhitelistManager.Load();
		ctx.False(LF_WhitelistManager.IsLoadFailed(), "a missing file is a first run, not a failed load");
		ctx.True(LF_WhitelistManager.Save(), "saving works again once the load did not fail");

		if (FileIO.FileExists(path))
			FileIO.DeleteFile(path);
		LF_WhitelistManager.SetSavePath("");
		LF_WhitelistManager.Reset();
	}
}
