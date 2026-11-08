// red-proof: observed 2026-11-07 with `tools\cli test --no-build`, then each mutation
// restored and the suite rerun green.
// Run A, tier all, five mutations at once (one per test): `new EL_ATMManager();` back in
// EL_GameModeRoleplay.ResolveConnectManagers plus `s_Instance = this;` back in the
// EL_ATMManager constructor; order.Insert("jail") dropped and the version back to 4;
// SpawnOwned(record, origin, true) in RestoreVehicle; the Remember call removed from
// EL_VehicleOwnership.ApplyAll; the s_bLoadFailed guard in EL_WhitelistManager.Save
// replaced by `if (false)`. Observed:
//   SUMMARY tier=all passed=128 failed=5 total=133
//   [ELTEST] FAIL restart/bank-survives-connect
//   [ELTEST]   - the loaded bank account survives the first connect
//   [ELTEST] FAIL restart/jail-serializer-roundtrip
//   [ELTEST]   - the save is written at version 5 (expected 5, got 4)
//   [ELTEST]   - the sentence is served again after the load
//   [ELTEST] FAIL restart/vehicle-restore-no-ground-key
//   [ELTEST]   - restore drops no key beside the pad
//   [ELTEST] FAIL restart/vehicle-plates-reserved
//   [ELTEST]   - a purchase after the load cannot take the saved plate (expected '', got 'RS PL 7001')
//   [ELTEST] FAIL restart/whitelist-unreadable-file-kept
//   [ELTEST]   - the unreadable file keeps its first line (expected '99', got '1')
// Run B, tier fast, only the unconditional `s_Instance = null;` in ~EL_ATMManager:
//   SUMMARY tier=fast passed=81 failed=1 total=82
//   [ELTEST] FAIL restart/bank-survives-connect
//   [ELTEST]   - a released stray manager does not drop the loaded balances

//------------------------------------------------------------------------------------------------
// tier: LOGIC
class EL_Test_RestartBankSurvivesConnect : EL_Test
{
	protected static const string ACCOUNT_UID = "restart-bank-uid";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "restart/bank-survives-connect";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(EL_TestContext ctx)
	{
		// The save load runs before any player connects and fills the bank cache.
		EL_ATMManager.Reset();
		array<ref EL_BankAccountRecord> records = new array<ref EL_BankAccountRecord>();
		records.Insert(EL_BankAccountRecord.Create(ACCOUNT_UID, 4200));
		EL_ATMManager.ApplyAll(records);
		EL_ATMManager loaded = EL_ATMManager.GetInstance();

		// First player connects: the game mode's manager path runs.
		EL_CharacterCreationManager creation = EL_GameModeRoleplay.ResolveConnectManagers();
		ctx.NotNull(creation, "the connect path resolves the character creation manager");

		EL_BankAccount account = EL_ATMManager.GetInstance().GetAccount(ACCOUNT_UID);
		ctx.NotNull(account, "the loaded bank account survives the first connect");
		if (account)
			ctx.Equal(4200, account.GetBalance(), "the loaded balance survives the first connect");
		ctx.True(EL_ATMManager.GetInstance() == loaded, "the connect path keeps the instance the load filled");

		// A stray construction and release must neither replace nor drop the loaded cache.
		EL_ATMManager stray = new EL_ATMManager();
		ctx.True(EL_ATMManager.GetInstance() == loaded, "constructing a manager does not replace the singleton");
		stray = null;
		account = EL_ATMManager.GetInstance().GetAccount(ACCOUNT_UID);
		ctx.NotNull(account, "a released stray manager does not drop the loaded balances");
		if (account)
			ctx.Equal(4200, account.GetBalance(), "the balance is intact after a stray manager is released");

		// Second connect resolves the same singletons.
		EL_JobManager jobs = EL_JobManager.GetInstance();
		EL_GameModeRoleplay.ResolveConnectManagers();
		ctx.True(EL_ATMManager.GetInstance() == loaded, "a second connect keeps the bank singleton");
		ctx.True(EL_JobManager.GetInstance() == jobs, "a second connect keeps the job singleton");
		ctx.True(EL_CharacterCreationManager.GetInstance() == creation, "a second connect keeps the character creation singleton");

		EL_JobManager strayJobs = new EL_JobManager();
		strayJobs = null;
		ctx.True(EL_JobManager.GetInstance() == jobs, "a released stray job manager does not drop the singleton");

		EL_ATMManager.Reset();
	}
}

//------------------------------------------------------------------------------------------------
// tier: WORLD
class EL_Test_RestartJailSerializerRoundtrip : EL_Test
{
	protected static const string PRISONER_UID = "restart-jail-uid";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "restart/jail-serializer-roundtrip";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(EL_TestContext ctx)
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		ctx.NotNull(gameMode, "the DebugWorld game mode resolves");
		if (!gameMode)
			return;

		EL_PersistenceComponent persistence = EL_PersistenceComponent.Cast(gameMode.FindComponent(EL_PersistenceComponent));
		ctx.NotNull(persistence, "the game mode carries EL_PersistenceComponent");
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
		ctx.True(EL_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
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
		ctx.Equal(7, version, "the save is written at version 7 (jail is payload 5)");
		EL_PersistenceComponentSerializer.ReadPersistencePayload(persistence, loadContext, "jail");
		ctx.True(LF_Jail.IsServing(PRISONER_UID), "the sentence is served again after the load");
		ctx.EqualFloat(240, LF_Jail.GetRemaining(PRISONER_UID), 0.01, "the remaining time survives the load");

		// Re-applying the same save converges on the same state.
		JsonLoadContext again = new JsonLoadContext(false);
		again.LoadFromString(saved);
		EL_PersistenceComponentSerializer.ReadPersistencePayload(persistence, again, "jail");
		ctx.True(LF_Jail.IsServing(PRISONER_UID), "a second load keeps the sentence");
		ctx.EqualFloat(240, LF_Jail.GetRemaining(PRISONER_UID), 0.01, "a second load does not change the remaining time");

		LF_Jail.ApplyAll(before);
	}
}

//------------------------------------------------------------------------------------------------
// tier: WORLD
class EL_Test_RestartVehicleNoGroundKey : EL_Test
{
	protected static const string VEHICLE_ID = "restart-vehicle-id";
	protected static const string OWNER_UID = "restart-vehicle-owner";
	protected static const string PLATE = "RS VK 4242";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "restart/vehicle-restore-no-ground-key";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(EL_TestContext ctx)
	{
		// A loaded save holds an owned vehicle that was out in the world. A restart has no
		// session for it: no live vehicle, no live key.
		array<ref EL_VehicleRecord> before = EL_VehicleOwnership.ExportAll();
		array<ref EL_VehicleRecord> loaded = EL_VehicleOwnership.ExportAll();
		loaded.Insert(EL_VehicleRecord.Create(VEHICLE_ID, OWNER_UID, VEHICLE_ID, EL_VehicleCatalog.S105, EL_VehicleCatalog.S105_LOCKED, "roadmap-lot", PLATE, "#EL-Vehicle_S105", true, false));
		EL_VehicleOwnership.ApplyAll(loaded);

		ctx.True(EL_VehicleOwnership.RestoreVehicle(VEHICLE_ID), "restore puts the out-in-world vehicle back");
		ctx.True(EL_VehicleOwnership.GetVehicle(VEHICLE_ID) != null, "the restored vehicle is live");
		ctx.True(EL_VehicleOwnership.GetKey(VEHICLE_ID) == null, "restore drops no key beside the pad");
		ctx.True(EL_VehicleOwnership.IsLocked(VEHICLE_ID), "the restored vehicle keeps its saved lock");

		ctx.True(EL_VehicleOwnership.RestoreVehicle(VEHICLE_ID), "a second restore finds the live vehicle");
		ctx.True(EL_VehicleOwnership.GetKey(VEHICLE_ID) == null, "a second restore still drops no key");

		// Cleanup: the owner store removes the entity, then the registry returns to its prior state.
		EL_VehicleOwnership.Store(VEHICLE_ID, OWNER_UID);
		EL_VehicleOwnership.ApplyAll(before);
		EL_LicensePlateRegistry.Release(PLATE);
	}
}

//------------------------------------------------------------------------------------------------
// tier: LOGIC
class EL_Test_RestartVehiclePlatesReserved : EL_Test
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
	override void Run(EL_TestContext ctx)
	{
		// A restart starts with an empty plate registry.
		EL_LicensePlateRegistry.Release(PLATE);
		ctx.False(EL_LicensePlateRegistry.IsLive(PLATE), "the plate is free before the load");

		array<ref EL_VehicleRecord> before = EL_VehicleOwnership.ExportAll();
		array<ref EL_VehicleRecord> loaded = EL_VehicleOwnership.ExportAll();
		loaded.Insert(EL_VehicleRecord.Create(VEHICLE_ID, OWNER_UID, VEHICLE_ID, EL_VehicleCatalog.S105, EL_VehicleCatalog.S105_LOCKED, "roadmap-lot", PLATE, "#EL-Vehicle_S105", true, true));
		EL_VehicleOwnership.ApplyAll(loaded);

		ctx.True(EL_LicensePlateRegistry.IsLive(PLATE), "a loaded plate is reserved again");
		ctx.EqualStr("", EL_LicensePlateRegistry.TakeUnique(PLATE, "restart-plate-other"), "a purchase after the load cannot take the saved plate");

		EL_VehicleOwnership.ApplyAll(loaded);
		ctx.True(EL_LicensePlateRegistry.IsLive(PLATE), "a second load keeps the plate reserved");

		EL_VehicleOwnership.ApplyAll(before);
		EL_LicensePlateRegistry.Release(PLATE);
	}
}

//------------------------------------------------------------------------------------------------
// tier: LOGIC
class EL_Test_RestartWhitelistUnreadableFileKept : EL_Test
{
	protected static const string EXISTING_UID = "restart-whitelist-existing";
	protected static const string NEW_UID = "restart-whitelist-new";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "restart/whitelist-unreadable-file-kept";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(EL_TestContext ctx)
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

		EL_WhitelistManager.SetSavePath(path);
		EL_WhitelistManager.Reset();
		EL_WhitelistManager.Load();
		ctx.True(EL_WhitelistManager.IsLoadFailed(), "an unreadable file marks the load failed");
		ctx.False(EL_WhitelistManager.IsWhitelistedForFaction(EXISTING_UID, EL_EFactionType.POLICE), "an unreadable file leaves police closed");

		// An admin adds someone while the file is unreadable.
		EL_WhitelistManager.AddFactionWhitelist(NEW_UID, EL_EFactionType.POLICE);
		ctx.False(EL_WhitelistManager.Save(), "an admin edit is not saved over the unreadable file");

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
		EL_WhitelistManager.Reset();
		EL_WhitelistManager.Load();
		ctx.False(EL_WhitelistManager.IsLoadFailed(), "a missing file is a first run, not a failed load");
		ctx.True(EL_WhitelistManager.Save(), "saving works again once the load did not fail");

		if (FileIO.FileExists(path))
			FileIO.DeleteFile(path);
		EL_WhitelistManager.SetSavePath("");
		EL_WhitelistManager.Reset();
	}
}
