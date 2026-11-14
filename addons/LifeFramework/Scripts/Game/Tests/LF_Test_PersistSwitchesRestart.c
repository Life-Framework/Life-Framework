// red-proof: DeserializeStore without the ApplyAfterLoad call failed "the active character's wanted level is 0 (switches off, load 1) (expected 0, got 4)" and every other switched-off field
// tier: PERSISTENCE
//! Persistence switches (#689) on the real save path. A character with two slots (wanted 2 and 4),
//! survival stats, a licence, carried cash and a jail sentence is saved. Loaded with every switch
//! off, each field is back at its default, twice over (idempotent). The same save loaded with the
//! switches on again brings every field back: the save kept its shape and its version. A version 23
//! save (before character slots) loads under the switches with no read failure.
//!
//! Across a restart (`tools\cli wt e2e`): the save boot writes the e2e owner config.json (shared
//! with systems/keep-data) with keepLicences off and gives E2E_UID a licence, so the shutdown save
//! carries it. The reload boot finds it cleared by the boot load, then turns the switch on (owner
//! layer without the key), and a licence noted again survives a save and load. Every other field
//! the other e2e tests check (cash, jail, wanted, survival) kept its switch on and survived.
class LF_Test_PersistSwitchesRestart : LF_Test
{
	//! The one owner config.json the e2e save boot writes; systems/keep-data writes the same text.
	static const string E2E_OWNER = "{\"schemaVersion\":1,\"systems\":{\"switches\":{\"Farming\":{\"enabled\":false}}},\"persistence\":{\"keepLicences\":false}}";
	static const string E2E_UID = "e2e-persist-switch-licence";

	protected static const string UID = "test-persist-switches";
	protected static const string PREFAB = "{9B5BB216CC7FF18E}Prefabs/Characters/Core/Character_Roleplay.et";
	protected static const string ALL_OFF = "{\"schemaVersion\":1,\"persistence\":{\"keepCash\":false,\"keepSurvival\":false,\"keepWanted\":false,\"keepJail\":false,\"keepLicences\":false}}";

	override string GetName()
	{
		return "persistence/switches";
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
		RunReloadBoot(ctx, persistence);
#else
		LF_TestLegacySnapshot keptOrgs = LF_TestLegacySnapshot.Take();
		array<ref LF_StorageSnapshotRecord> snapshots = LF_StorageSnapshot.ExportAll();
		LF_CharacterConfig.SetMaxPerAccountOverride(2);
		RunRoundTrip(ctx, persistence);
		LF_CharacterConfig.SetMaxPerAccountOverride(0);
		LF_PlayerAccountManager.GetInstance().RemoveFromCache(UID);
		LF_Progression.Forget(UID);
		LF_StorageSnapshot.ApplyAll(snapshots);
		keptOrgs.Restore();
		LF_ConfigService.Reset();
	#ifdef LF_E2E_HOST_STOP
		SeedSaveBoot(ctx);
	#endif
#endif
	}

	//------------------------------------------------------------------------------------------------
	//! Loads a save through the real read path under an owner config ("" = no owner file).
	protected static bool Load(LF_PersistenceComponent persistence, string saved, string owner)
	{
		LF_ConfigService.Reset();
		if (!owner.IsEmpty())
			LF_ConfigService.LoadOwnerString(owner, "test");

		JsonLoadContext loadContext = new JsonLoadContext(false);
		if (!loadContext.LoadFromString(saved))
			return false;

		return LF_PersistenceComponentSerializer.DeserializeStore(persistence, loadContext);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunRoundTrip(LF_TestContext ctx, LF_PersistenceComponent persistence)
	{
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		LF_PlayerAccount account = LF_PlayerAccount.Create(UID);
		accounts.AddToCache(account);
		LF_PlayerCharacter first = LF_PlayerCharacter.Create(PREFAB, "Ada", "Switch", 30);
		account.AddCharacter(first, true);
		account.SetWantedLevel(2);
		LF_PlayerCharacter second = LF_PlayerCharacter.Create(PREFAB, "Bob", "Switch", 40);
		account.AddCharacter(second, true);
		account.SetWantedLevel(4);
		string firstKey = first.GetRecordKey();
		string secondId = second.GetId();

		LF_SurvivalStats stats = new LF_SurvivalStats();
		stats.SetPersistentId(secondId);
		stats.SetHunger(17);
		persistence.SetSurvivalStats(secondId, stats);

		LF_Progression.NoteLicenses(UID, {LF_ELicenseType.MINER_IRON});
		ResourceName cash = LF_MoneyUtils.CashPrefab();
		LF_StorageSnapshotRecord carried = LF_StorageSnapshotRecord.Create(secondId, LF_StorageSnapshot.SLOT_CARRIED);
		carried.m_aItems.Insert(LF_StorageItemRecord.Create(cash, 40, -1, 0, 0));
		LF_StorageSnapshot.Set(carried);
		array<ref LF_JailRecord> sentences = LF_Jail.ExportAll();
		sentences.Insert(LF_JailRecord.Create(UID, 300, true));
		LF_Jail.ApplyAll(sentences);

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();

		for (int pass = 1; pass <= 2; pass++)
		{
			string tag = string.Format(" (switches off, load %1)", pass);
			ctx.True(Load(persistence, saved, ALL_OFF), "the save loads" + tag);
			ctx.Equal(0, LF_PersistenceComponentSerializer.GetReadFailures(), "no payload fails to read" + tag);
			LF_PlayerAccount loaded = accounts.GetAccount(UID);
			ctx.NotNull(loaded, "the account loads" + tag);
			if (loaded)
			{
				ctx.Equal(0, loaded.GetCharacterById(secondId).GetStanding().m_iWantedLevel, "the active character's wanted level is 0" + tag);
				ctx.Equal(0, loaded.GetCharacterByKey(firstKey).GetStanding().m_iWantedLevel, "the other character's wanted level is 0" + tag);
			}
			ctx.True(persistence.GetSurvivalStats(secondId) == null, "survival starts full (no saved stats)" + tag);
			array<LF_ELicenseType> licences = {};
			ctx.Equal(0, LF_Progression.GetLicenses(UID, licences), "no earned licence" + tag);
			LF_StorageSnapshotRecord back = LF_StorageSnapshot.Get(secondId, LF_StorageSnapshot.SLOT_CARRIED);
			ctx.True(back != null && back.CountPrefab(cash) == 0, "the carried snapshot has no cash" + tag);
			ctx.False(LF_Jail.IsServing(UID), "no sentence" + tag);
		}

		// The owner turns the switches on again: the same save, same version, every field back.
		ctx.True(Load(persistence, saved, string.Empty), "the save loads with the switches on");
		LF_PlayerAccount again = accounts.GetAccount(UID);
		if (again)
		{
			ctx.Equal(4, again.GetCharacterById(secondId).GetStanding().m_iWantedLevel, "the active character is wanted 4 again");
			ctx.Equal(2, again.GetCharacterByKey(firstKey).GetStanding().m_iWantedLevel, "the other character is wanted 2 again");
		}
		LF_SurvivalStats hunger = persistence.GetSurvivalStats(secondId);
		ctx.True(hunger != null && Math.AbsFloat(hunger.GetHunger() - 17) < 0.01, "hunger 17 is back");
		array<LF_ELicenseType> kept = {};
		ctx.Equal(1, LF_Progression.GetLicenses(UID, kept), "the licence is back");
		ctx.Equal(40, LF_StorageSnapshot.Get(secondId, LF_StorageSnapshot.SLOT_CARRIED).CountPrefab(cash), "the carried cash is back");
		ctx.True(LF_Jail.IsServing(UID), "the sentence is back");

		// An old save (version 23, before character slots) loads under the switches.
		JsonSaveContext oldContext = new JsonSaveContext(false);
		LF_PersistenceComponentSerializer.SerializeStoreAsVersion(persistence, oldContext, 23);
		string old = oldContext.SaveToString();
		ctx.True(Load(persistence, old, ALL_OFF), "a version 23 save loads with the switches off");
		ctx.Equal(0, LF_PersistenceComponentSerializer.GetReadFailures(), "with no read failure");
		ctx.False(LF_Jail.IsServing(UID), "and its sentence is reset");
		ctx.Equal(0, LF_Progression.GetLicenses(UID, kept), "and its licence");

		// Leave the session as the current save had it, the test's own sentence released.
		Load(persistence, saved, string.Empty);
		LF_Jail.ReleaseEarly(UID);
		LF_PersistenceComponentSerializer.ClearLoadDamaged();
	}

	//------------------------------------------------------------------------------------------------
	//! The e2e save boot: the owner file with keepLicences off, and a licence for the shutdown save.
	protected void SeedSaveBoot(LF_TestContext ctx)
	{
		ctx.True(LF_Test_PersistSwitchesRestart.WriteE2EOwner(), "the save boot writes the e2e owner config.json");
		LF_ConfigService.Reset();
		ctx.False(LF_PersistenceConfig.ResolveKeep().m_bKeepLicences, "the owner file turns keepLicences off");
		LF_Progression.NoteLicenses(E2E_UID, {LF_ELicenseType.MINER_IRON});
		array<LF_ELicenseType> licences = {};
		ctx.Equal(1, LF_Progression.GetLicenses(E2E_UID, licences), "e2e a licence waits for the shutdown save");
	}

	//------------------------------------------------------------------------------------------------
	//! Writes the shared e2e owner file (systems/keep-data writes it too; same text either way).
	static bool WriteE2EOwner()
	{
		FileHandle handle = FileIO.OpenFile(LF_ConfigService.OWNER_PATH, FileMode.WRITE);
		if (!handle)
			return false;

		handle.WriteLine(E2E_OWNER);
		handle.Close();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The e2e reload boot: the boot load reset the licence; on again, a licence survives.
	protected void RunReloadBoot(LF_TestContext ctx, LF_PersistenceComponent persistence)
	{
		LF_ConfigService.Reset();
		ctx.False(LF_PersistenceConfig.ResolveKeep().m_bKeepLicences, "e2e keepLicences is off after the restart, from the owner file");
		array<LF_ELicenseType> licences = {};
		ctx.NotNull(LF_Progression.Get(E2E_UID), "e2e the progression record saved before the restart is loaded");
		ctx.Equal(0, LF_Progression.GetLicenses(E2E_UID, licences), "e2e the licence saved with keepLicences off is reset by the restart");

		// The owner turns the switch on again (an owner layer without the key; the file stays for
		// systems/keep-data, which runs later in this boot).
		LF_Progression.NoteLicenses(E2E_UID, {LF_ELicenseType.MINER_IRON});
		JsonSaveContext saveContext = new JsonSaveContext(false);
		LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext);
		ctx.True(Load(persistence, saveContext.SaveToString(), "{\"schemaVersion\":1}"), "e2e the save loads with keepLicences on");
		ctx.Equal(1, LF_Progression.GetLicenses(E2E_UID, licences), "e2e with the switch on again the licence survives the load");

		LF_Progression.Forget(E2E_UID);
		LF_ConfigService.Reset();
	}
}
