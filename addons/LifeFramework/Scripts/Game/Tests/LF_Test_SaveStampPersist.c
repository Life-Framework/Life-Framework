// red-proof: Observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_SaveStampPersist`: replacing
// `LF_SaveStamp.Apply(stampRows)` in the "buildstamp" read with a no-op failed `- the stamp names the build that
// wrote the save (load 1) (expected '1.8.0.13', got 'unknown')` and `- the save is from another build than the
// running one (load 1)`; dropping `s_bLoadDamaged = true` in ReportLoad failed `- the damaged load pauses
// autosave`. Restored; green. The restart path is `tools\cli wt e2e <slug> --old-build`.
// tier: PERSISTENCE
//! #980 through the real save path ("buildstamp" payload, save version 32). A save written by
//! another game build (a faked older stamp) loads whole: the bank balance in the same save comes
//! back, the stamp names the old build, the load is not damaged, and loading twice changes nothing.
//! A save from this build reads as this build. A version 31 save has no stamp (unknown build).
//! A save whose payloads cannot be read leaves the live state alone, logs the damage and pauses
//! autosave (IsLoadDamaged) instead of wiping.
class LF_Test_SaveStampPersist : LF_Test
{
	protected static const string UID = "stamp-bank-uid";
	protected static const string RUNNING_BUILD = "1.9.0.37";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "persist/save-build-stamp";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// A full load re-applies every payload: keep what the tests after this one found.
		LF_TestLegacySnapshot keptOrgs = LF_TestLegacySnapshot.Take();
		array<ref LF_StorageSnapshotRecord> liveSnapshots = LF_StorageSnapshot.ExportAll();
		RunKept(ctx);
		LF_SaveStamp.SetBuildOverride(string.Empty);
		LF_PersistenceComponentSerializer.ClearLoadDamaged();
		LF_SaveStamp.ResetForLoad();
		LF_StorageSnapshot.ApplyAll(liveSnapshots);
		keptOrgs.Restore();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunKept(LF_TestContext ctx)
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		LF_PersistenceComponent persistence;
		if (gameMode)
			persistence = LF_PersistenceComponent.Cast(gameMode.FindComponent(LF_PersistenceComponent));

		ctx.NotNull(persistence, "the game mode carries LF_PersistenceComponent");
		if (!persistence)
			return;

		OtherBuild(ctx, persistence);
		SameBuild(ctx, persistence);
		NoStamp(ctx, persistence);
		Damaged(ctx, persistence);
	}

	//------------------------------------------------------------------------------------------------
	//! The balance exactly (a bank account of an earlier boot can survive in the e2e save).
	protected static void SetBank(string key, int balance)
	{
		LF_ATMManager atm = LF_ATMManager.GetInstance();
		LF_BankAccount bank = atm.GetAccount(key);
		if (!bank)
			bank = atm.CreateAccount(key);

		if (bank && bank.GetBalance() > 0)
			atm.Withdraw(key, bank.GetBalance());

		atm.Deposit(key, balance);
	}

	//------------------------------------------------------------------------------------------------
	protected static int Bank(string key)
	{
		LF_BankAccount bank = LF_ATMManager.GetInstance().GetAccount(key);
		if (!bank)
			return -1;

		return bank.GetBalance();
	}

	//------------------------------------------------------------------------------------------------
	//! A save from the build before a Steam update loads on the new build.
	protected void OtherBuild(LF_TestContext ctx, LF_PersistenceComponent persistence)
	{
		SetBank(UID, 420);
		LF_SaveStamp.SetBuildOverride(LF_SaveStamp.FAKE_OLD_BUILD);
		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(LF_SaveStamp.FAKE_OLD_BUILD), "the save carries the old build in its stamp");

		// The update: the server now runs another build, and the balance moved after the save.
		LF_SaveStamp.SetBuildOverride(RUNNING_BUILD);
		SetBank(UID, 5);

		for (int pass = 1; pass <= 2; pass++)
		{
			string tag = string.Format(" (load %1)", pass);
			JsonLoadContext loadContext = new JsonLoadContext(false);
			ctx.True(loadContext.LoadFromString(saved), "the save parses" + tag);
			int version;
			loadContext.ReadValue("version", version);
			ctx.Equal(32, version, "the save is written at version 32 (buildstamp is payload 32)" + tag);

			JsonLoadContext fullLoad = new JsonLoadContext(false);
			fullLoad.LoadFromString(saved);
			ctx.True(LF_PersistenceComponentSerializer.DeserializeStore(persistence, fullLoad), "the old-build save loads" + tag);
			ctx.Equal(420, Bank(UID), "the bank balance from the old build is back" + tag);
			ctx.True(LF_SaveStamp.WasLoaded(), "the stamp was read" + tag);
			ctx.EqualStr(LF_SaveStamp.FAKE_OLD_BUILD, LF_SaveStamp.LoadedGameBuild(), "the stamp names the build that wrote the save" + tag);
			ctx.Equal(32, LF_SaveStamp.LoadedSaveVersion(), "the stamp names the save version" + tag);
			ctx.True(LF_SaveStamp.IsFromOtherBuild(), "the save is from another build than the running one" + tag);
			ctx.Equal(0, LF_PersistenceComponentSerializer.GetReadFailures(), "every payload read" + tag);
			ctx.False(LF_PersistenceComponentSerializer.IsLoadDamaged(), "the load is not damaged, autosave runs" + tag);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void SameBuild(LF_TestContext ctx, LF_PersistenceComponent persistence)
	{
		LF_SaveStamp.SetBuildOverride(RUNNING_BUILD);
		JsonSaveContext saveContext = new JsonSaveContext(false);
		LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext);
		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saveContext.SaveToString()), "the same-build save parses");
		ctx.True(LF_PersistenceComponentSerializer.DeserializeStore(persistence, loadContext), "the same-build save loads");
		ctx.EqualStr(RUNNING_BUILD, LF_SaveStamp.LoadedGameBuild(), "the stamp names the running build");
		ctx.False(LF_SaveStamp.IsFromOtherBuild(), "a save from this build is not from another build");
	}

	//------------------------------------------------------------------------------------------------
	//! A save written before #980 (version 31) has no stamp: the build is unknown, and it still loads.
	protected void NoStamp(LF_TestContext ctx, LF_PersistenceComponent persistence)
	{
		SetBank(UID, 77);
		JsonSaveContext saveContext = new JsonSaveContext(false);
		LF_PersistenceComponentSerializer.SerializeStoreAsVersion(persistence, saveContext, 31);
		SetBank(UID, 1);
		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saveContext.SaveToString()), "the version 31 save parses");
		ctx.True(LF_PersistenceComponentSerializer.DeserializeStore(persistence, loadContext), "the version 31 save loads");
		ctx.Equal(77, Bank(UID), "the version 31 save brings its bank balance back");
		ctx.False(LF_SaveStamp.WasLoaded(), "a version 31 save has no stamp");
		ctx.EqualStr(LF_SaveStamp.UNKNOWN_BUILD, LF_SaveStamp.LoadedGameBuild(), "its build is unknown");
		ctx.False(LF_SaveStamp.IsFromOtherBuild(), "an unknown build is not reported as another build");
		ctx.False(LF_PersistenceComponentSerializer.IsLoadDamaged(), "a version 31 save is not damaged");
	}

	//------------------------------------------------------------------------------------------------
	//! A save that claims version 32 and holds nothing readable: no payload is applied, the live
	//! state stays, and the load is marked damaged so autosave does not overwrite the save.
	protected void Damaged(LF_TestContext ctx, LF_PersistenceComponent persistence)
	{
		SetBank(UID, 640);
		JsonSaveContext saveContext = new JsonSaveContext(false);
		saveContext.WriteValue("version", 32);
		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saveContext.SaveToString()), "the broken save parses as JSON");
		LF_PersistenceComponentSerializer.DeserializeStore(persistence, loadContext);
		ctx.True(LF_PersistenceComponentSerializer.GetReadFailures() > 0, "the unreadable payloads are counted");
		ctx.True(LF_PersistenceComponentSerializer.IsLoadDamaged(), "the damaged load pauses autosave");
		ctx.Equal(640, Bank(UID), "the live bank balance is not wiped by the broken save");

		// A good save re-applied afterwards clears the flag (load is idempotent, not sticky).
		JsonSaveContext good = new JsonSaveContext(false);
		LF_PersistenceComponentSerializer.SerializeStore(persistence, good);
		JsonLoadContext goodLoad = new JsonLoadContext(false);
		goodLoad.LoadFromString(good.SaveToString());
		LF_PersistenceComponentSerializer.DeserializeStore(persistence, goodLoad);
		ctx.False(LF_PersistenceComponentSerializer.IsLoadDamaged(), "a clean load afterwards clears the damage");
		int left = Bank(UID);
		LF_ATMManager atm = LF_ATMManager.GetInstance();
		if (atm && left > 0)
			atm.Withdraw(UID, left);
	}
}
