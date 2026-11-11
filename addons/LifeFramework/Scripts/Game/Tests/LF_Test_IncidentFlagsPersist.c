// red-proof: Observed 2026-11-09 with `--only`: commenting out `LF_IncidentFlags.ApplyAll(flagRows)` in the load failed `the flag is back` and `no extra rows after the load (expected 1, got 0)`; restored; green. Restart path stays with wt e2e.
// tier: PERSISTENCE
//! The incident flags ride the real save path (the "incidentflags" payload, save version 17): a
//! flagged incident is still flagged after the store is wiped, and loading twice adds no rows.
class LF_Test_IncidentFlagsPersist : LF_Test
{
	override string GetName()
	{
		return "persist/incident-flags-roundtrip";
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

		array<ref LF_IncidentRecord> logBefore = LF_IncidentLog.ExportAll();
		array<ref LF_IncidentFlagRow> before = LF_IncidentFlags.ExportAll();
		LF_IncidentLog.ResetStatics();
		int now = System.GetUnixTime();
		LF_IncidentRecord record = LF_IncidentLog.Append(LF_IncidentConfig.KIND_DEATH, now, "flag-save-victim", "flag-save-killer", Vector(1, 0, 1), 0, "rifle", true);
		ctx.NotNull(record, "an incident is logged");
		if (!record)
			return;

		int flagged;
		ctx.Equal(LF_IncidentFlags.REPORT_OK, LF_IncidentFlags.Report("flag-save-victim", now, flagged), "the victim reports it");

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains("flag-save-victim"), "the save carries the flag");

		LF_IncidentFlags.ApplyAll(null);
		ctx.False(LF_IncidentFlags.IsFlagged(record.m_iNumber), "the restarted session has no flags");

		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the save parses");
		int version;
		loadContext.ReadValue("version", version);
		ctx.Equal(25, version, "the save is written at version 25 (incidentflags is payload 17)");
		LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, loadContext, "incidentflags");
		ctx.True(LF_IncidentFlags.IsFlagged(record.m_iNumber), "the flag is back");
		ctx.Equal(1, LF_IncidentFlags.Count(), "no extra rows after the load");

		LF_IncidentLog.ResetStatics();
		LF_IncidentFlags.ApplyAll(before);
		LF_IncidentLog.ApplyAll(logBefore);
	}
}
