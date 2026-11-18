// red-proof: observed 2026-11-18 `tools/cli test --no-build --red LF_Test_ResidentMemoryPersist`: RED PROOF OK, first `the cooldown still holds after the load`. Green with `--only`. Restart: `tools/cli wt e2e slice-2861`.
//------------------------------------------------------------------------------------------------
//! Resident memory rides the real save path (#2861, the "residentmemory" save section): scores and
//! last-award times come back after a save and load, a resident no longer in config keeps its rows
//! (logged once), a neutral row is not written, a save without the section loads empty, and the
//! admin character wipe forgets the character. Across a restart (`tools\cli wt e2e`): every
//! non-reload run leaves one e2e row; the reload boot finds it.
[LF_TestInfo(LF_TestTier.PERSISTENCE)]
class LF_Test_ResidentMemoryPersist : LF_Test
{
	static const string KNOWN = "rm-test-baker";
	static const string GONE = "rm-test-gone";
	static const string UID_THIEF = "rm-test-thief";
	static const string UID_REGULAR = "rm-test-regular";
	static const string UID_NEUTRAL = "rm-test-neutral";
	static const string UID_COOLING = "rm-test-cooling";
	static const string UID_OTHER = "rm-test-other";
	//! The e2e row: every non-reload run seeds it, the reload boot reads it after the restart.
	static const string E2E_RESIDENT = "rm-e2e-baker";
	static const string E2E_UID = "rm-e2e-thief";
	static const int E2E_SCORE = -70;	// lf-fixed: refuses on the shipped thresholds

	override string GetName()
	{
		return "residents/memory-persist";
	}

	//------------------------------------------------------------------------------------------------
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
		// The reload boot: the e2e row came back from the real save, when it carried the section
		// (a --pre-move or --legacy-save ref build wrote none; LF_E2E logs the skip).
		if (LF_E2E.RequiresSection(LF_ResidentMemorySavePayload.NAME))
			VerifyE2E(ctx);

		return;
#endif

		ctx.NotNull(LF_SaveSections.Get(LF_ResidentMemorySavePayload.NAME), "residentmemory is a registered save section");
		ctx.Equal(0, LF_PersistenceComponentSerializer.PayloadSinceVersion(LF_ResidentMemorySavePayload.NAME), "residentmemory is not a legacy positional payload");

		UseTestConfigs();
		LF_ResidentMemory.ResetStatics();
		int now = System.GetUnixTime();
		int longAgo = now - 100000;

		// Three kept rows (one for a resident the config no longer lists, twice), one neutral row, and
		// a score-0 row whose cooldown is still running.
		array<ref LF_ResidentMemorySaveRow> rows = new array<ref LF_ResidentMemorySaveRow>();
		rows.Insert(LF_ResidentMemorySaveRow.Create(KNOWN, UID_THIEF, -70).With("crime_seen", longAgo));
		rows.Insert(LF_ResidentMemorySaveRow.Create(KNOWN, UID_REGULAR, 65).With("helped", now - 10));
		rows.Insert(LF_ResidentMemorySaveRow.Create(GONE, UID_REGULAR, 25).With("helped", longAgo));
		rows.Insert(LF_ResidentMemorySaveRow.Create(GONE, UID_OTHER, 500));
		rows.Insert(LF_ResidentMemorySaveRow.Create(KNOWN, UID_NEUTRAL, 0).With("helped", longAgo));
		rows.Insert(LF_ResidentMemorySaveRow.Create(KNOWN, UID_COOLING, 0).With("helped", now - 5));
		int unknown;
		ctx.Equal(6, LF_ResidentMemory.ApplyRows(rows, unknown), "every keyed row applies");
		ctx.Equal(1, unknown, "a resident missing from config is logged once, however many rows it has");
		ctx.Equal(100, LF_ResidentMemory.Score(GONE, UID_OTHER), "a score off the scale clamps to 100");
		ctx.EqualStr("refuses", LF_ResidentMemory.Tier(KNOWN, UID_THIEF), "the thief is refused before the save");

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(LF_SaveSections.OBJECT_PREFIX + LF_ResidentMemorySavePayload.NAME), "the save carries the residentmemory section");
		ctx.True(saved.Contains(UID_THIEF) && saved.Contains(UID_REGULAR) && saved.Contains(UID_OTHER), "the non-neutral rows are written");
		ctx.True(saved.Contains(GONE), "a row for a resident not in config is written");
		ctx.False(saved.Contains(UID_NEUTRAL), "a neutral row (score 0, no cooldown running) is not written");
		ctx.True(saved.Contains(UID_COOLING), "a score-0 row with a running cooldown is written");

		// Restart: the session holds no memory.
		LF_ResidentMemory.ResetStatics();

		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the save parses");
		// RedProof(): the load is skipped, so every read-back assertion fails (`--red`).
		if (!RedProof())
			ReadSection(loadContext, ctx);

		ctx.Equal(-70, LF_ResidentMemory.Score(KNOWN, UID_THIEF), "the thief's score is back");
		ctx.EqualStr("refuses", LF_ResidentMemory.Tier(KNOWN, UID_THIEF), "the baker still refuses the thief");
		ctx.Equal(65, LF_ResidentMemory.Score(KNOWN, UID_REGULAR), "the regular's score is back");
		ctx.EqualStr("regular", LF_ResidentMemory.Tier(KNOWN, UID_REGULAR), "the baker still greets the regular");
		ctx.Equal(25, LF_ResidentMemory.Score(GONE, UID_REGULAR), "a resident not in config keeps its row");
		ctx.Equal(longAgo, LastSeconds(KNOWN, UID_THIEF, "crime_seen", now), "the last award time is back");
		ctx.Equal(now - 10, LastSeconds(KNOWN, UID_REGULAR, "helped", now), "a running cooldown's time is back");
		ctx.False(LF_ResidentMemory.Apply(KNOWN, UID_REGULAR, "helped", now), "the cooldown still holds after the load");
		ctx.Equal(65, LF_ResidentMemory.Score(KNOWN, UID_REGULAR), "an award inside the restored cooldown changes nothing");
		ctx.Equal(5, LF_ResidentMemory.ExportRows(now).Count(), "five rows come back, the neutral one does not");

		JsonLoadContext again = new JsonLoadContext(false);
		again.LoadFromString(saved);
		ReadSection(again, ctx);
		ctx.Equal(5, LF_ResidentMemory.ExportRows(now).Count(), "loading twice gives the same rows");
		ctx.Equal(-70, LF_ResidentMemory.Score(KNOWN, UID_THIEF), "loading twice keeps the same score");

		// The admin character wipe (#2771): every resident forgets that character.
		LF_Progression.Forget(UID_REGULAR);
		ctx.Equal(0, LF_ResidentMemory.Score(KNOWN, UID_REGULAR), "the wipe clears the regular at the baker");
		ctx.Equal(0, LF_ResidentMemory.Score(GONE, UID_REGULAR), "the wipe clears the regular everywhere");
		ctx.Equal(-70, LF_ResidentMemory.Score(KNOWN, UID_THIEF), "the wipe leaves other characters alone");

		// A save without this section (an older save) loads empty.
		JsonSaveContext oldContext = new JsonSaveContext(false);
		array<string> omit = {LF_ResidentMemorySavePayload.NAME};
		LF_PersistenceComponentSerializer.SerializeStoreOmitting(persistence, oldContext, omit);
		string oldSave = oldContext.SaveToString();
		ctx.False(oldSave.Contains(LF_SaveSections.OBJECT_PREFIX + LF_ResidentMemorySavePayload.NAME), "the older save has no residentmemory section");
		LF_ResidentMemory.ResetStatics();
		JsonLoadContext oldLoad = new JsonLoadContext(false);
		ctx.True(oldLoad.LoadFromString(oldSave), "the old save parses");
		LF_PersistenceComponentSerializer.DeserializeStore(persistence, oldLoad);
		ctx.Equal(0, LF_ResidentMemory.ExportRows(now).Count(), "a pre-section save loads empty memory");

		LF_ResidentMemory.ResetStatics();
		LF_ResidentMemoryConfig.ResetStatics();
		LF_ResidentsConfig.Reload();

		// Every non-reload run (the e2e save boot among them) leaves the e2e row.
		SeedE2E(ctx);
	}

	//------------------------------------------------------------------------------------------------
	//! A memory config with the shipped shape, and a residents config that lists only KNOWN.
	protected static void UseTestConfigs()
	{
		LF_ResidentMemoryConfig config = new LF_ResidentMemoryConfig();
		config.m_iCooldownSeconds = 600;
		config.m_iGossipPercent = 50;
		config.m_iRefusesBelow = -60;
		config.m_iWaryBelow = -20;
		config.m_iFriendlyFrom = 20;
		config.m_iRegularFrom = 60;
		config.m_aReasons = {};
		config.m_aReasons.Insert(LF_ResidentMemoryReason.Make("helped", 10, false));
		config.m_aReasons.Insert(LF_ResidentMemoryReason.Make("crime_seen", -30, true));
		config.m_aKnows = {};
		LF_ResidentMemoryConfig.UseForTest(config);

		LF_ResidentsConfig residents = new LF_ResidentsConfig();
		residents.m_aResidents = {};
		residents.m_aResidents.Insert(LF_ResidentDef.Make(KNOWN, "home", "work"));
		LF_ResidentsConfig.UseForTest(residents);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The saved last-award second of `reason` for the pair, or -1.
	protected static int LastSeconds(string resident, string uid, string reason, int now)
	{
		foreach (LF_ResidentMemorySaveRow row : LF_ResidentMemory.ExportRows(now))
		{
			if (row.m_sResident != resident || row.m_sUid != uid)
				continue;

			int at = row.m_aReasons.Find(reason);
			if (at >= 0)
				return row.m_aLastSeconds[at];
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the residentmemory section of a whole-store JSON save, as LF_SaveSections.ReadAll does.
	protected static void ReadSection(notnull JsonLoadContext context, LF_TestContext ctx)
	{
		LF_SavePayload section = LF_SaveSections.Get(LF_ResidentMemorySavePayload.NAME);
		if (!section)
			return;

		ctx.True(context.StartObject(LF_SaveSections.OBJECT_PREFIX + LF_ResidentMemorySavePayload.NAME), "the residentmemory section opens");
		int savedVersion;
		context.ReadValue(LF_SaveSections.VERSION_KEY, savedVersion);
		ctx.Equal(section.Version(), savedVersion, "the section carries its own version");
		ctx.True(section.ReadSection(context, savedVersion), "the residentmemory section reads");
		context.EndObject();
	}

	//------------------------------------------------------------------------------------------------
	//! wt e2e save boot: one resident refuses one character, with a crime award time.
	static void SeedE2E(LF_TestContext ctx)
	{
		array<ref LF_ResidentMemorySaveRow> rows = new array<ref LF_ResidentMemorySaveRow>();
		rows.Insert(LF_ResidentMemorySaveRow.Create(E2E_RESIDENT, E2E_UID, E2E_SCORE).With("crime_seen", 1700000000));
		int unknown;
		LF_ResidentMemory.ApplyRows(rows, unknown);
		ctx.Equal(E2E_SCORE, LF_ResidentMemory.Score(E2E_RESIDENT, E2E_UID), "e2e row is set before the save");
	}

	//------------------------------------------------------------------------------------------------
	//! wt e2e reload boot: the same row after the restart.
	static void VerifyE2E(LF_TestContext ctx)
	{
		ctx.Equal(E2E_SCORE, LF_ResidentMemory.Score(E2E_RESIDENT, E2E_UID), "e2e resident memory score survives the restart");
		ctx.Equal(1700000000, LastSeconds(E2E_RESIDENT, E2E_UID, "crime_seen", System.GetUnixTime()), "e2e last award time survives the restart");
		LF_Debug.Info(LF_ResidentMemory.FEATURE, string.Format("e2e restart: %1 about %2 is %3", E2E_RESIDENT, E2E_UID, LF_ResidentMemory.Score(E2E_RESIDENT, E2E_UID)));
	}
}
