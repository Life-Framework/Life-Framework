// red-proof: made LF_WantedReports.ApplyLooks keep the record's own look and ran
// `tools\cli test --no-build --only LF_Test_WantedLooksPersist` (2026-11-13). Observed:
//   [LFTEST]   - the robber's look is back on the report (load 1) (expected '9B5BB216CC7FF18E|1=9F546CCA2582D16F', got '')
//   [LFTEST]   - the look applied by sequence (expected '9B5BB216CC7FF18E|1=9F546CCA2582D16F', got '')
// Restored it and the test passed (21 assertions).

// tier: PERSISTENCE
//! The look on a crime report (#758) rides the real save path: the "wantedlooks" payload, save
//! version 29, keyed by report sequence. After the store is wiped the reports come back from
//! "wanted" with their looks from "wantedlooks", twice the same (idempotent); a version 28 save
//! loads the reports with no look; a row for a report the save does not hold is skipped. The test
//! puts back what it found.
class LF_Test_WantedLooksPersist : LF_Test
{
	protected static const string ROBBER = "test-wantedlooks-robber";
	protected static const string THIEF = "test-wantedlooks-thief";
	protected static const string ROBBER_LOOK = "9B5BB216CC7FF18E|1=9F546CCA2582D16F";

	override string GetName()
	{
		return "persist/wanted-looks";
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

		array<ref LF_WantedReportRecord> before = LF_WantedReports.ExportAll();
		array<ref LF_WantedLookRow> beforeLooks = LF_WantedReports.ExportLooks();
		LF_TestLegacySnapshot kept = LF_TestLegacySnapshot.Take();
		LF_WantedReports.ResetStatics();

		LF_WantedReportRecord robbery = LF_WantedReports.Add(ROBBER, LF_WantedReports.OFFENCE_ROBBERY, "#AR-Jacket_M88", "", "", false);
		robbery.m_sAppearance = ROBBER_LOOK;
		LF_WantedReports.Add(THIEF, LF_WantedReports.OFFENCE_VEHICLE_THEFT, "", "", "", false);
		int robberySequence = robbery.m_iSequence;

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(ROBBER_LOOK), "the save carries the look");

		for (int pass = 1; pass <= 2; pass++)
		{
			LF_WantedReports.ResetStatics();
			JsonLoadContext loadContext = new JsonLoadContext(false);
			ctx.True(loadContext.LoadFromString(saved), "the save parses");
			int version;
			loadContext.ReadValue("version", version);
			ctx.Equal(30, version, "the save is written at version 30 (wantedlooks is payload 29)");
			ctx.True(LF_PersistenceComponentSerializer.DeserializeStore(persistence, loadContext), "the save loads");
			string tag = string.Format(" (load %1)", pass);
			ctx.EqualStr(ROBBER_LOOK, LookOf(ROBBER), "the robber's look is back on the report" + tag);
			ctx.EqualStr("", LookOf(THIEF), "the thief's report had no look and has none" + tag);
		}

		// A version 28 save has no "wantedlooks": the reports load, without a look.
		JsonSaveContext oldContext = new JsonSaveContext(false);
		LF_PersistenceComponentSerializer.SerializeStoreAsVersion(persistence, oldContext, 28);
		string oldSave = oldContext.SaveToString();
		ctx.False(oldSave.Contains(ROBBER_LOOK), "a version 28 save carries no look");
		LF_WantedReports.ResetStatics();
		JsonLoadContext oldLoad = new JsonLoadContext(false);
		ctx.True(oldLoad.LoadFromString(oldSave) && LF_PersistenceComponentSerializer.DeserializeStore(persistence, oldLoad), "the version 28 save loads");
		ctx.Equal(1, LF_WantedReports.CountFor(ROBBER), "the version 28 save keeps the report");
		ctx.EqualStr("", LookOf(ROBBER), "the version 28 report has no look (an empty photo frame)");

		// Rows match by sequence; one for a report the store does not hold is skipped.
		array<ref LF_WantedLookRow> rows = {};
		rows.Insert(LF_WantedLookRow.Create(robberySequence, ROBBER_LOOK));
		rows.Insert(LF_WantedLookRow.Create(99999, "0000000000000000"));
		ctx.Equal(1, LF_WantedReports.ApplyLooks(rows), "only the row whose report exists applies");
		ctx.EqualStr(ROBBER_LOOK, LookOf(ROBBER), "the look applied by sequence");
		ctx.Equal(0, LF_WantedReports.ApplyLooks(null), "no rows apply nothing");

		LF_WantedReports.ApplyAll(before);
		LF_WantedReports.ApplyLooks(beforeLooks);
		kept.Restore();
	}

	//------------------------------------------------------------------------------------------------
	protected static string LookOf(string uid)
	{
		array<ref LF_WantedReportRecord> list = LF_WantedReports.GetFor(uid);
		if (!list || list.IsEmpty())
			return "<no report>";

		return list[0].m_sAppearance;
	}
}
