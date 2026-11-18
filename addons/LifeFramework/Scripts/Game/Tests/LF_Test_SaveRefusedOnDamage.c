// red-proof: observed 2026-11-17 with `tools\cli test --no-build --only LF_Test_SaveStopsAfterFailedRead,LF_Test_SaveRefusedOnDamage`
// on the unfixed gates (PassesSaveGates without the damage check, SyncSavingWithLoadDamage empty): FAIL
// `SaveGame() is refused because the load is damaged, got ''`, `no save point is requested` (it asked for
// a MANUAL save point), `a direct AutoSave() is refused ...`, `a damaged re-apply turns engine saving off`. Green with the fix.
// Also `tools\cli test --no-build --red LF_Test_SaveNewerVersion,LF_Test_SaveStopsAfterFailedRead,LF_Test_SaveRefusedOnDamage`:
// RED PROOF OK (all 3 listed test(s) fail with the hook on).
//! #2489 (review P2): a damaged load used to pause only the autosave timer, and the engine's saving
//! was turned off only for a damaged boot load. A damaged load later in the session (a re-apply) let
//! SaveGame(), a direct AutoSave(), an admin save and the engine's shutdown save overwrite the save on
//! disk. Every save request through the manager is now refused with the reason logged, and the
//! re-apply turns engine saving off until a clean load clears the damage.
[LF_TestInfo(LF_TestTier.PERSISTENCE)]
class LF_Test_SaveRefusedOnDamage : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "persist/save-refused-on-damage";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		ctx.NotNull(gameMode, "the DebugWorld game mode resolves");
		if (!gameMode)
			return;

		LF_PersistenceComponent persistence = LF_PersistenceComponent.Cast(gameMode.FindComponent(LF_PersistenceComponent));
		LF_PersistenceManagerComponent manager = LF_PersistenceManagerComponent.Cast(gameMode.FindComponent(LF_PersistenceManagerComponent));
		SaveGameManager saves = GetGame().GetSaveGameManager();
		ctx.NotNull(persistence, "the game mode carries LF_PersistenceComponent");
		ctx.NotNull(manager, "the game mode carries LF_PersistenceManagerComponent");
		ctx.NotNull(saves, "the game has a SaveGameManager");
		if (!persistence || !manager || !saves)
			return;

		// A load can re-apply payloads: keep what the tests after this one found.
		LF_TestLegacySnapshot keptOrgs = LF_TestLegacySnapshot.Take();
		array<ref LF_StorageSnapshotRecord> liveSnapshots = LF_StorageSnapshot.ExportAll();
		bool savingWasAllowed = saves.IsSavingAllowed();

		RunKept(ctx, persistence, manager, saves, savingWasAllowed);

		LF_PersistenceComponentSerializer.ClearLoadDamaged();
		manager.SyncSavingWithLoadDamage("the test cleanup");
		saves.SetSavingAllowed(savingWasAllowed);
		LF_SaveStamp.ResetForLoad();
		LF_StorageSnapshot.ApplyAll(liveSnapshots);
		keptOrgs.Restore();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunKept(LF_TestContext ctx, LF_PersistenceComponent persistence, LF_PersistenceManagerComponent manager, SaveGameManager saves, bool savingWasAllowed)
	{
		// The re-applied load reads a save that holds nothing past its version.
		if (!RedProof())
		{
			JsonSaveContext broken = new JsonSaveContext(false);
			broken.WriteValue("version", LF_TestSaveVersion.CURRENT);
			JsonLoadContext loadContext = new JsonLoadContext(false);
			ctx.True(loadContext.LoadFromString(broken.SaveToString()), "the broken save parses as JSON");
			LF_PersistenceComponentSerializer.DeserializeStore(persistence, loadContext);
		}

		ctx.True(LF_PersistenceComponentSerializer.IsLoadDamaged(), "the re-applied load is damaged");

		int completed = manager.GetCompletedSaveCount();
		manager.SaveGame();
		ctx.True(manager.GetLastSaveRejection().Contains("damaged"), "SaveGame() is refused because the load is damaged, got '" + manager.GetLastSaveRejection() + "'");
		ctx.False(manager.IsSaveInProgress(), "no save point is requested");

		manager.AutoSave();
		ctx.True(manager.GetLastSaveRejection().Contains("damaged"), "a direct AutoSave() is refused because the load is damaged");
		ctx.False(manager.IsSaveInProgress(), "no autosave point is requested");
		ctx.Equal(completed, manager.GetCompletedSaveCount(), "no save point landed");

		// Admin saves (the Game Master toolbar) and the engine's shutdown save go straight to the
		// SaveGameManager: only turning engine saving off stops them.
		saves.SetSavingAllowed(true);
		manager.SyncSavingWithLoadDamage("the re-applied load");
		ctx.False(saves.IsSavingAllowed(), "a damaged re-apply turns engine saving off (admin and shutdown saves)");

		LF_PersistenceComponentSerializer.ClearLoadDamaged();
		manager.SyncSavingWithLoadDamage("the next re-applied load");
		ctx.True(saves.IsSavingAllowed(), "a clean load turns engine saving back on");
	}
}
