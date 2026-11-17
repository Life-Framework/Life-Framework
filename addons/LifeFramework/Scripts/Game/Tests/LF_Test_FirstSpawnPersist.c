// red-proof: Observed 2026-11-16 with `tools/cli test --no-build --red LF_Test_FirstSpawn,LF_Test_FirstSpawnPersist` (RED PROOF OK): with the payload read skipped, failed "the waiting character is new again after the load (expected 1, got 0)"; green on the real code.
// tier: PERSISTENCE
//! #1964: the first-spawn mark survives the "firstspawns" payload and a restart. A character marked
//! and not yet spawned is still new after a reload; one that spawned before the save is not.
//! Across a restart (`tools\cli wt e2e`): the save boot marks E2E_PENDING and marks then spawns
//! E2E_SPAWNED, so the shutdown save carries only the first; the reload boot finds E2E_PENDING first
//! and E2E_SPAWNED not.
class LF_Test_FirstSpawnPersist : LF_Test
{
	protected static const string PENDING_ID = "firstspawn-persist-pending";
	protected static const string SPAWNED_ID = "firstspawn-persist-spawned";
	protected static const string E2E_PENDING = "firstspawn-e2e-pending";
	protected static const string E2E_SPAWNED = "firstspawn-e2e-spawned";

	override string GetName()
	{
		return "character/first-spawn-persist";
	}

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
		// The reload boot: the marks the save boot left came back from the real save.
		ctx.True(LF_FirstSpawn.IsPending(E2E_PENDING), "the character created and not yet spawned is still new after the restart");
		ctx.False(LF_FirstSpawn.IsPending(E2E_SPAWNED), "the character that spawned before the save is not new after the restart");
		ctx.True(LF_FirstSpawn.Consume(E2E_PENDING), "its first spawn after the restart is first");
		ctx.False(LF_FirstSpawn.Consume(E2E_PENDING), "and only once");
		return;
#endif

		array<ref LF_FirstSpawnRow> before = LF_FirstSpawn.ExportAll();
		LF_FirstSpawn.ApplyAll(null);

		LF_FirstSpawn.Mark(PENDING_ID);
		LF_FirstSpawn.Mark(SPAWNED_ID);
		LF_FirstSpawn.Consume(SPAWNED_ID);

		JsonSaveContext saveContext = new JsonSaveContext(false);
		LF_PersistenceComponentSerializer.WritePersistencePayload(persistence, saveContext, "firstspawns");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(PENDING_ID), "the save carries the character still waiting");
		ctx.False(saved.Contains(SPAWNED_ID), "the save leaves out the character that already spawned");

		// A fresh session, then the payload read back.
		LF_FirstSpawn.ApplyAll(null);
		ctx.False(LF_FirstSpawn.IsPending(PENDING_ID), "the restarted session starts with no marks");

		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the save parses");
		if (!RedProof())
			LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, loadContext, "firstspawns");
		ctx.True(LF_FirstSpawn.IsPending(PENDING_ID), "the waiting character is new again after the load");
		ctx.False(LF_FirstSpawn.IsPending(SPAWNED_ID), "the spawned character stays not new");
		ctx.Equal(1, LF_FirstSpawn.Count(), "no extra marks");

		// Loading the same rows twice is idempotent.
		LF_FirstSpawn.ApplyAll(LF_FirstSpawn.ExportAll());
		ctx.Equal(1, LF_FirstSpawn.Count(), "re-applying the marks changes nothing");

		LF_FirstSpawn.ApplyAll(before);
#ifdef LF_E2E_HOST_STOP
		// The save boot: one character waiting, one that already spawned, carried by the shutdown save.
		LF_FirstSpawn.Mark(E2E_PENDING);
		LF_FirstSpawn.Mark(E2E_SPAWNED);
		LF_FirstSpawn.Consume(E2E_SPAWNED);
		ctx.True(LF_FirstSpawn.IsPending(E2E_PENDING), "the save boot marked the e2e character");
#endif
	}
}
