// red-proof: Observed 2026-11-10 with `tools/cli test --no-build --only LF_Test_EvidenceLockerPersist`: commenting out `LF_EvidenceLocker.ApplyAll(lockerRows)` in the load failed `- the logged bag is back (expected 1, got 0)` and `- it is still linked to its report`. Restored; green. Restart path stays with wt e2e.
// tier: PERSISTENCE
//! The evidence locker rides the real save path (the "evidencelocker" payload, save version 18,
//! payload 18): a bag logged in the locker is still on file, against its report, after the store is
//! wiped and read back, and loading twice adds no rows. The bag logging itself calls
//! LF_FenceSerials.IdentifyItem: a serial'd tool in the bag identifies its buyer's wanted report.
class LF_Test_EvidenceLockerPersist : LF_Test
{
	protected static const string BUYER = "locker-save-buyer";
	protected static const string OFFICER = "locker-save-officer";
	protected static const string DRILL = "{4FF102B4CE9B0ED3}Prefabs/Items/Fence/LF_Drill.et";

	override string GetName()
	{
		return "investigation/locker";
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

		array<ref LF_EvidenceLockerRow> before = LF_EvidenceLocker.ExportAll();
		array<ref LF_FenceSerialRow> serialsBefore = LF_FenceSerials.ExportAll();
		array<ref LF_WantedReportRecord> reportsBefore = LF_WantedReports.ExportAll();
		LF_EvidenceLocker.ApplyAll(null);
		LF_FenceSerials.ApplyAll(null);
		LF_WantedReports.ApplyAll(null);

		// The buyer owns a serial'd drill and has an unidentified report; an officer bags the drill.
		string serial = LF_FenceSerials.Issue(BUYER, DRILL);
		LF_WantedReportRecord report = LF_WantedReports.Add(BUYER, LF_WantedReports.OFFENCE_ROBBERY, "", "", "Test Square", false);
		IEntity drill = LF_Utils.SpawnEntityPrefab(DRILL, "1000 0 1000");
		ctx.NotNull(drill, "the drill spawns");
		LF_SerialComponent drillSerial;
		if (drill)
			drillSerial = LF_SerialComponent.Cast(drill.FindComponent(LF_SerialComponent));

		ctx.NotNull(drillSerial, "the drill carries a serial component");
		if (!drillSerial)
			return;

		drillSerial.SetSerial(serial);
		IEntity bag = LF_Utils.SpawnEntityPrefab(LF_Evidence.BAG_PREFAB, "1000 0 1000");
		LF_EvidenceBagComponent bagComponent;
		if (bag)
			bagComponent = LF_EvidenceBagComponent.Cast(bag.FindComponent(LF_EvidenceBagComponent));

		ctx.NotNull(bagComponent, "the bag spawns with its component");
		if (!bagComponent)
			return;

		bagComponent.Stamp(report.m_iSequence, LF_WantedReports.OFFENCE_ROBBERY, "Test Square", LF_Evidence.KIND_DROPPED, 4);
		bagComponent.Seal(drill);
		LF_EvidenceLockerRow row = LF_EvidenceLocker.LogBag(bag, OFFICER);
		ctx.NotNull(row, "the stamped bag is logged");
		ctx.True(LF_WantedReports.IsIdentified(BUYER), "logging the bag identified the buyer through IdentifyItem");
		ctx.Equal(1, LF_EvidenceLocker.CountFor(report.m_iSequence), "the bag is linked to its report");

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(serial), "the save carries the logged bag");

		LF_EvidenceLocker.ApplyAll(null);
		ctx.Equal(0, LF_EvidenceLocker.Count(), "the restarted session has an empty locker");

		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the save parses");
		int version;
		loadContext.ReadValue("version", version);
		ctx.Equal(22, version, "the save is written at version 22 (evidencelocker is payload 18)");
		LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, loadContext, "evidencelocker");
		ctx.Equal(1, LF_EvidenceLocker.Count(), "the logged bag is back");
		ctx.Equal(1, LF_EvidenceLocker.CountFor(report.m_iSequence), "it is still linked to its report");
		LF_EvidenceLockerRow back = LF_EvidenceLocker.Find(row.m_iNumber);
		ctx.NotNull(back, "the row is found by its number");
		if (back)
		{
			ctx.EqualStr(serial, back.m_sSerial, "the serial came back");
			ctx.True(back.m_bIdentified, "the identified flag came back");
		}

		LF_EvidenceLocker.ApplyAll(before);
		LF_FenceSerials.ApplyAll(serialsBefore);
		LF_WantedReports.ApplyAll(reportsBefore);
	}
}
