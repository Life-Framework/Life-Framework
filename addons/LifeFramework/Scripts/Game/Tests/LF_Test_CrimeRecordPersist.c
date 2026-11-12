// red-proof: Observed 2026-11-09 with the same `+= 2` flip: `crime/record-persist` failed `- the subscriber counted one robbery (expected 1, got 2)`. Restored; green. (Restart path stays with wt e2e.)
// tier: PERSISTENCE
//! The crime record rides the real save path (the "crimerecord" payload, now save version 16): a crime
//! published on the bus is counted by the subscriber, comes back after the store is wiped, and
//! loading twice leaves the same counts.
class LF_Test_CrimeRecordPersist : LF_Test
{
	protected static const string UID = "crime-save-uid";

	override string GetName()
	{
		return "crime/record-persist";
	}

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

		array<ref LF_CrimeRecordRow> before = LF_CrimeRecord.ExportAll();
		LF_CrimeRecord.ApplyAll(null);
		ctx.True(LF_CrimeRecord.EnsureSubscribed(), "the record subscribes to LF_OnCrimeCommitted");

		// One robbery published on the bus counts once.
		LF_EventBus.Publish(LF_OnCrimeCommitted.Create(UID, LF_WantedReports.OFFENCE_ROBBERY));
		ctx.Equal(1, LF_CrimeRecord.Count(UID, LF_WantedReports.OFFENCE_ROBBERY), "the subscriber counted one robbery");

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(UID), "the save carries the crime record payload");

		// Restart: the session has no record.
		LF_CrimeRecord.ApplyAll(null);
		ctx.Equal(0, LF_CrimeRecord.Total(UID), "the restarted session starts empty");

		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the save parses");
		int version;
		loadContext.ReadValue("version", version);
		ctx.Equal(27, version, "the save is written at version 27 (crimerecord is payload 15)");
		LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, loadContext, "crimerecord");
		ctx.Equal(1, LF_CrimeRecord.Count(UID, LF_WantedReports.OFFENCE_ROBBERY), "the robbery count is back");
		ctx.Equal(1, LF_CrimeRecord.Total(UID), "no extra rows after the load");

		LF_CrimeRecord.ApplyAll(before);
	}
}
