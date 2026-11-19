// red-proof: 2026-11-19 `tools\cli test --no-build --red LF_Test_LicenceBansRestart`: RED PROOF OK, with the load skipped `A's ban ends at the same in-game minute after the load (expected 10014400, got -1)` and `A's saved licences hold no permit after the load`. Green: `--only` passed=2.
//! Licence bans ride the real save path (#3430, the "licencebans" save section): a ban takes the
//! licence from the holder and refuses it back; after a save and load the licence is still gone, the
//! ban ends at the same in-game minute, and a ban that ended while the server was down is lifted.
//! Loading twice gives the same bans; a save without the section has none. Across a restart
//! (`tools\cli wt e2e`): every non-reload run leaves a 30-day ban for E2E_UID; the reload boot finds it.
[LF_TestInfo(LF_TestTier.PERSISTENCE)]
class LF_Test_LicenceBansRestart : LF_Test
{
	protected static const string UID_A = "test-licban-a";
	protected static const string UID_B = "test-licban-b";
	static const string E2E_UID = "test-licban-e2e";
	protected static const int T0 = 10000000;	// in-game minute the test pins the clock at
	protected static const int DAY = 1440;

	override string GetName()
	{
		return "licence/bans-restart";
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
		if (LF_E2E.RequiresSection(LF_LicenceBansSavePayload.NAME))
			VerifyE2E(ctx);

		return;
#endif

		ctx.NotNull(LF_SaveSections.Get(LF_LicenceBansSavePayload.NAME), "licencebans is a registered save section");
		ctx.Equal(0, LF_PersistenceComponentSerializer.PayloadSinceVersion(LF_LicenceBansSavePayload.NAME), "licencebans is not a legacy positional payload");

		IEntity holder = LF_TestCharacterFixture.Spawn(ctx);
		LF_LicenseManagerComponent manager;
		if (holder)
			manager = LF_LicenseManagerComponent.Cast(holder.FindComponent(LF_LicenseManagerComponent));

		ctx.NotNull(manager, "the fixture body carries LF_LicenseManagerComponent");
		if (manager)
			RunRestart(ctx, persistence, holder, manager);

		LF_Utils.SetUidForTests(null, "");
		LF_TestCharacterFixture.Delete(holder);
		LF_LicenceBans.ResetStatics();
		// Every non-reload run (the e2e save boot among them) leaves the e2e ban for the shutdown save.
		SeedE2E(ctx);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunRestart(LF_TestContext ctx, LF_PersistenceComponent persistence, IEntity holder, LF_LicenseManagerComponent manager)
	{
		LF_LicenceBans.ResetStatics();
		LF_LicenceBans.SetNowForTest(T0);
		LF_Utils.SetUidForTests(holder, UID_A);
		array<LF_ELicenseType> held = {LF_ELicenseType.UNEMPLOYED, LF_ELicenseType.FIREARM_PERMIT};
		manager.SetUnlockedLicenses(held);
		LF_Progression.NoteLicenses(UID_A, held);

		// The ban takes the licence and refuses it back.
		int end = LF_LicenceBans.Ban(UID_A, LF_ELicenseType.FIREARM_PERMIT, 10, holder);
		ctx.Equal(T0 + 10 * DAY, end, "a 10-day ban ends 10 in-game days from now");
		ctx.False(manager.HasLicense(LF_ELicenseType.FIREARM_PERMIT), "the ban takes the permit from the holder");
		ctx.True(manager.HasLicense(LF_ELicenseType.UNEMPLOYED), "the holder keeps the licences the ban does not name");
		ctx.False(SavedHas(UID_A, LF_ELicenseType.FIREARM_PERMIT), "the ban takes the permit from the saved licences");
		ctx.False(manager.CanUnlockLicense(LF_ELicenseType.FIREARM_PERMIT), "the banned permit cannot be bought back");
		LF_LicenceBans.Ban(UID_B, LF_ELicenseType.DRIVING_LICENCE, 3);
		ctx.True(LF_LicenceBans.IsBanned(UID_B, LF_ELicenseType.DRIVING_LICENCE), "B's 3-day ban is in force");

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(LF_SaveSections.OBJECT_PREFIX + LF_LicenceBansSavePayload.NAME), "the save carries the licencebans section");

		// Restart five in-game days later: no bans in memory, and a stale record that still lists the permit.
		LF_LicenceBans.ResetStatics();
		LF_LicenceBans.SetNowForTest(T0 + 5 * DAY);
		LF_Progression.NoteLicenses(UID_A, held);
		// RedProof(): the load is skipped, so every read-back assertion fails (`--red`).
		if (!RedProof())
			ReadSection(saved, ctx);

		CheckAfterLoad(ctx, end, "after the load");
		ReadSection(saved, ctx);
		ReadSection(saved, ctx);
		CheckAfterLoad(ctx, end, "after a second load");

		// The ban ends: A may get the permit again.
		LF_LicenceBans.SetNowForTest(end);
		ctx.False(LF_LicenceBans.IsBanned(UID_A, LF_ELicenseType.FIREARM_PERMIT), "the ban is lifted at its end minute");
		ctx.Equal(-1, LF_LicenceBans.EndOf(UID_A, LF_ELicenseType.FIREARM_PERMIT), "a lifted ban has no end");

		// A save without the section (an older save) holds no bans.
		LF_LicenceBans.SetNowForTest(T0);
		LF_LicenceBans.Ban(UID_A, LF_ELicenseType.FIREARM_PERMIT, 10);
		JsonSaveContext oldContext = new JsonSaveContext(false);
		array<string> omit = {LF_LicenceBansSavePayload.NAME};
		LF_PersistenceComponentSerializer.SerializeStoreOmitting(persistence, oldContext, omit);
		string oldSave = oldContext.SaveToString();
		ctx.False(oldSave.Contains(LF_SaveSections.OBJECT_PREFIX + LF_LicenceBansSavePayload.NAME), "the older save has no licencebans section");
		LF_LicenceBans.ResetStatics();
		LF_LicenceBans.SetNowForTest(T0);
		JsonLoadContext oldLoad = new JsonLoadContext(false);
		ctx.True(oldLoad.LoadFromString(oldSave), "the old save parses");
		LF_PersistenceComponentSerializer.DeserializeStore(persistence, oldLoad);
		ctx.False(LF_LicenceBans.IsBanned(UID_A, LF_ELicenseType.FIREARM_PERMIT), "a pre-section save loads no ban");
	}

	//------------------------------------------------------------------------------------------------
	protected static void CheckAfterLoad(LF_TestContext ctx, int end, string when)
	{
		ctx.True(LF_LicenceBans.IsBanned(UID_A, LF_ELicenseType.FIREARM_PERMIT), "A's permit ban is in force " + when);
		ctx.Equal(end, LF_LicenceBans.EndOf(UID_A, LF_ELicenseType.FIREARM_PERMIT), "A's ban ends at the same in-game minute " + when);
		ctx.False(SavedHas(UID_A, LF_ELicenseType.FIREARM_PERMIT), "A's saved licences hold no permit " + when);
		ctx.True(SavedHas(UID_A, LF_ELicenseType.UNEMPLOYED), "A keeps the licences the ban does not name " + when);
		ctx.False(LF_LicenceBans.IsBanned(UID_B, LF_ELicenseType.DRIVING_LICENCE), "B's ban, which ended while the server was down, is lifted " + when);
		int ours;
		foreach (LF_LicenceBanRow row : LF_LicenceBans.ExportAll(LF_LicenceBans.Now()))
		{
			if (row.m_sPlayerUid == UID_A || row.m_sPlayerUid == UID_B)
				ours++;
		}

		ctx.Equal(1, ours, "one ban of the test's characters rides the next save " + when);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool SavedHas(string uid, LF_ELicenseType licence)
	{
		array<LF_ELicenseType> saved = {};
		LF_Progression.GetLicenses(uid, saved);
		return saved.Contains(licence);
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the licencebans section of a whole-store JSON save, as LF_SaveSections.ReadAll does.
	protected static void ReadSection(string saved, LF_TestContext ctx)
	{
		LF_SavePayload section = LF_SaveSections.Get(LF_LicenceBansSavePayload.NAME);
		if (!section)
			return;

		JsonLoadContext context = new JsonLoadContext(false);
		ctx.True(context.LoadFromString(saved), "the save parses");
		ctx.True(context.StartObject(LF_SaveSections.OBJECT_PREFIX + LF_LicenceBansSavePayload.NAME), "the licencebans section opens");
		int savedVersion;
		context.ReadValue(LF_SaveSections.VERSION_KEY, savedVersion);
		ctx.Equal(section.Version(), savedVersion, "the section carries its own version");
		ctx.True(section.ReadSection(context, savedVersion), "the licencebans section reads");
		context.EndObject();
	}

	//------------------------------------------------------------------------------------------------
	//! wt e2e save boot: a 30-day ban on the real game clock, for the shutdown save.
	static void SeedE2E(LF_TestContext ctx)
	{
		int end = LF_LicenceBans.Ban(E2E_UID, LF_ELicenseType.FIREARM_PERMIT, 30);
		ctx.True(end > 0, string.Format("e2e ban is in force before the save (ends at minute %1)", end));
	}

	//------------------------------------------------------------------------------------------------
	//! wt e2e reload boot: the e2e ban came back from the real save, still in force.
	static void VerifyE2E(LF_TestContext ctx)
	{
		int end = LF_LicenceBans.EndOf(E2E_UID, LF_ELicenseType.FIREARM_PERMIT);
		ctx.True(end > 0, "e2e licence ban is back after the restart");
		ctx.True(end > LF_LicenceBans.Now(), string.Format("e2e licence ban still in force after the restart (ends %1, now %2)", end, LF_LicenceBans.Now()));
		LF_Debug.Info(LF_LicenceBans.FEATURE, string.Format("e2e restart: %1's permit ban ends at minute %2", E2E_UID, end));
		LF_LicenceBans.ResetStatics();
	}
}
