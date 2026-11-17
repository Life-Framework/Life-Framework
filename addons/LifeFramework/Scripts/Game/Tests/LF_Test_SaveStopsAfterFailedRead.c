// red-proof: observed 2026-11-17 with `tools\cli test --no-build --only LF_Test_SaveStopsAfterFailedRead` on the
// unfixed serializer: `bank records failed to read` then `applied 1 survival record(s)`, FAIL `a payload after the
// failed read does not change live state (hunger stays 77)`. Green with the stop-at-first-failure fix.
// Also `tools\cli test --no-build --red LF_Test_SaveNewerVersion,LF_Test_SaveStopsAfterFailedRead,LF_Test_SaveRefusedOnDamage`:
// RED PROOF OK (all 3 listed test(s) fail with the hook on).
// tier: PERSISTENCE
//! #2489 (review P1): the store is read positionally, so after one payload fails to read every later
//! read can be misaligned, and a later read that happens to succeed would replace live state with
//! whatever it found. A save whose bank records cannot be read is followed by a survival payload
//! that reads fine; the load stops at the failure, so the live survival stats stay as they were.
class LF_Test_SaveStopsAfterFailedRead : LF_Test
{
	protected static const string CHARACTER = "lf-test-stop-after-failed-read";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "persist/stop-after-failed-read";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// A full load can re-apply every payload: keep what the tests after this one found.
		LF_TestLegacySnapshot keptOrgs = LF_TestLegacySnapshot.Take();
		array<ref LF_StorageSnapshotRecord> liveSnapshots = LF_StorageSnapshot.ExportAll();
		LF_PersistenceComponent persistence = Persistence();
		ctx.NotNull(persistence, "the game mode carries LF_PersistenceComponent");
		if (persistence)
		{
			map<string, ref LF_SurvivalStats> liveSurvival = new map<string, ref LF_SurvivalStats>();
			foreach (string id, LF_SurvivalStats stats : persistence.GetSurvivalStatsMap())
			{
				liveSurvival.Set(id, stats);
			}

			RunKept(ctx, persistence);

			persistence.ClearSurvivalStats();
			foreach (string keptId, LF_SurvivalStats keptStats : liveSurvival)
			{
				persistence.SetSurvivalStats(keptId, keptStats);
			}
		}

		LF_PersistenceComponentSerializer.ClearLoadDamaged();
		LF_SaveStamp.ResetForLoad();
		LF_StorageSnapshot.ApplyAll(liveSnapshots);
		keptOrgs.Restore();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunKept(LF_TestContext ctx, notnull LF_PersistenceComponent persistence)
	{
		LF_SurvivalStats live = LF_SurvivalStats.Create(CHARACTER);
		live.SetHunger(77);
		live.SetThirst(66);
		persistence.SetSurvivalStats(CHARACTER, live);

		// The save: accounts read, the bank records do not, then a survival payload that reads
		// cleanly and would set the character's hunger to 11, then the rest as the live store has it.
		JsonSaveContext saveContext = new JsonSaveContext(false);
		saveContext.WriteValue("version", LF_TestSaveVersion.CURRENT);
		// SaveContext.Write keys each value by the variable's name in JSON (positional in binary),
		// so these carry the serializer's own names.
		array<ref LF_PlayerAccountRecord> accountRecords = LF_PlayerAccountManager.ExportAll();
		if (!accountRecords)
			accountRecords = new array<ref LF_PlayerAccountRecord>();

		saveContext.Write(accountRecords);
		if (!RedProof())
			WriteUnreadableBank(saveContext);
		else
			WriteBank(saveContext);

		array<ref LF_SurvivalStatsRecord> survivalRecords = new array<ref LF_SurvivalStatsRecord>();
		LF_SurvivalStatsRecord misread = new LF_SurvivalStatsRecord();
		misread.m_sPersistentId = CHARACTER;
		misread.m_fHunger = 11;
		misread.m_fThirst = 22;
		survivalRecords.Insert(misread);
		saveContext.Write(survivalRecords);
		array<string> order = new array<string>();
		LF_PersistenceComponentSerializer.GetPayloadOrder(order);
		for (int index = 1; index < order.Count(); index++)
		{
			LF_PersistenceComponentSerializer.WritePersistencePayload(persistence, saveContext, order.Get(index));
		}

		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saveContext.SaveToString()), "the save parses as JSON");
		ctx.True(LF_PersistenceComponentSerializer.DeserializeStore(persistence, loadContext), "the store read returns");
		ctx.True(LF_PersistenceComponentSerializer.GetReadFailures() > 0, "the unreadable bank records count as a read failure");
		ctx.True(LF_PersistenceComponentSerializer.IsLoadDamaged(), "the load is damaged");
		ctx.True(LF_PersistenceComponentSerializer.GetSkippedAfterFailure() > 0, "the payloads after the failed read are counted as not applied");

		LF_SurvivalStats after = persistence.GetSurvivalStats(CHARACTER);
		ctx.NotNull(after, "the live survival stats are still there");
		if (after)
		{
			ctx.True(after.GetHunger() > 76 && after.GetHunger() < 78, "a payload after the failed read does not change live state (hunger stays 77)");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Bank records the serializer cannot read: text where the records belong. (An int array there
	//! crashes the engine's JSON read, so it is not used.)
	protected static void WriteUnreadableBank(notnull SaveContext saveContext)
	{
		string bankRecords = "these bytes are not bank records";
		saveContext.Write(bankRecords);
	}

	//------------------------------------------------------------------------------------------------
	protected static void WriteBank(notnull SaveContext saveContext)
	{
		array<ref LF_BankAccountRecord> bankRecords = LF_ATMManager.ExportAll();
		if (!bankRecords)
			bankRecords = new array<ref LF_BankAccountRecord>();

		saveContext.Write(bankRecords);
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_PersistenceComponent Persistence()
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		if (!gameMode)
			return null;

		return LF_PersistenceComponent.Cast(gameMode.FindComponent(LF_PersistenceComponent));
	}
}
