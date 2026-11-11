// red-proof: Observed 2026-11-09 with `--only LF_Test_FenceSerialPersist`: commenting out `LF_FenceSerials.ApplyAll(serialRows)` in the load failed `- the serial is back` and `- no extra rows after the load (expected 1, got 0)`. Restored; green. Restart path stays with wt e2e.
// tier: PERSISTENCE
//! The serial book rides the real save path (the "fenceserials" payload, save version 16; fenceserials is payload 16, incidentflags 17): a
//! booked serial still names its buyer after the store is wiped, and loading twice adds no rows.
class LF_Test_FenceSerialPersist : LF_Test
{
	protected static const string UID = "serial-save-uid";

	override string GetName()
	{
		return "investigation/serial-persist";
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

		array<ref LF_FenceSerialRow> before = LF_FenceSerials.ExportAll();
		LF_FenceSerials.ApplyAll(null);
		string serial = LF_FenceSerials.Issue(UID, "Prefabs/Items/Fence/LF_Drill.et");
		ctx.True(!serial.IsEmpty(), "a serial is booked");

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(UID), "the save carries the serial book");

		LF_FenceSerials.ApplyAll(null);
		ctx.True(LF_FenceSerials.Find(serial) == null, "the restarted session has an empty book");

		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the save parses");
		int version;
		loadContext.ReadValue("version", version);
		ctx.Equal(26, version, "the save is written at version 26 (fenceserials is payload 16)");
		LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, loadContext, "fenceserials");
		LF_FenceSerialRow row = LF_FenceSerials.Find(serial);
		ctx.NotNull(row, "the serial is back");
		if (row)
			ctx.EqualStr(UID, row.m_sBuyerUid, "it still names its buyer");
		ctx.Equal(1, LF_FenceSerials.ExportAll().Count(), "no extra rows after the load");

		LF_FenceSerials.ApplyAll(before);
	}
}
