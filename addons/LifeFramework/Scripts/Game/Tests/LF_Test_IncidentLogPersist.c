// red-proof: Observed 2026-11-09 with `tools/cli test --no-build --only`: the serializer read with LF_IncidentLog.ApplyAll commented out failed `both incidents are back (expected 2, got 0)`; restored; green.

// tier: PERSISTENCE
//! persist/incident-log-roundtrip: incidents ride the real save path (the "incidents" payload,
//! save version 13) and are queryable after the store is wiped; a second load converges.
class LF_Test_IncidentLogPersist : LF_Test
{
	override string GetName()
	{
		return "persist/incident-log-roundtrip";
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

		array<ref LF_IncidentRecord> before = LF_IncidentLog.ExportAll();
		LF_IncidentLog.ApplyAll(null);
		int now = System.GetUnixTime();
		LF_IncidentLog.Append(LF_IncidentConfig.KIND_DEATH, now - 60, "persist-victim", "persist-killer", Vector(1200, 5, 3400), 0, "persist-rifle", true);
		LF_IncidentLog.Append(LF_IncidentConfig.KIND_CASH, now - 30, "persist-payer", "", Vector(50, 0, 60), 900, "atm-deposit", false);

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains("persist-rifle"), "the save carries the incidents payload");

		// Restart: the session holds no incidents.
		LF_IncidentLog.ApplyAll(null);
		ctx.Equal(0, LF_IncidentLog.Count(), "the restarted session starts empty");

		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the save parses");
		int version;
		loadContext.ReadValue("version", version);
		ctx.Equal(27, version, "the save is written at version 27 (incidents is payload 13)");
		LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, loadContext, "incidents");

		ctx.Equal(2, LF_IncidentLog.Count(), "both incidents are back");
		array<ref LF_IncidentRecord> found = LF_IncidentLog.Query(Vector(1200, 0, 3400), 5, now - 120, -1, LF_IncidentConfig.KIND_DEATH);
		ctx.Equal(1, found.Count(), "the death is queryable by position and window after the load");
		if (found.Count() == 1)
		{
			ctx.EqualStr("persist-killer", found[0].m_sOtherUid, "the killer is back");
			ctx.EqualStr("persist-rifle", found[0].m_sHow, "the weapon is back");
			ctx.True(found[0].m_bMurder, "the murder flag is back");
		}

		// A second apply converges, and numbering carries on from the saved list.
		JsonLoadContext again = new JsonLoadContext(false);
		again.LoadFromString(saved);
		again.ReadValue("version", version);
		LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, again, "incidents");
		ctx.Equal(2, LF_IncidentLog.Count(), "a second load keeps two incidents, not four");
		LF_IncidentRecord next = LF_IncidentLog.Append(LF_IncidentConfig.KIND_CASH, now, "persist-payer", "", vector.Zero, 1, "atm-deposit", false);
		if (next)
			ctx.Equal(3, next.m_iNumber, "numbering continues after the load");

		LF_IncidentLog.ApplyAll(before);
	}
}
