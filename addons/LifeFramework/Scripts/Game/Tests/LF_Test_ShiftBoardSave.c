// red-proof: observed 2026-11-24 `tools/cli test --no-build --red LF_Test_ShiftBoardWeek,LF_Test_ShiftBoardSave`: RED-OK shiftboard/save (22 assertions), with the load skipped it fails `the week is back after the load`, `both workers are back after the load`. Green with `--only`.
//! The shift board section (WK-W22, #4712): the week and its rows are written in the "shiftboard"
//! save section and read back, loading twice ends in the same state, a bad row is skipped, and a
//! save from before the section loads through OnAbsent with empty boards.
//! Restart: the e2e worker's row survives `wt e2e`.
[LF_TestInfo(LF_TestTier.PERSISTENCE)]
class LF_Test_ShiftBoardSave : LF_Test
{
	static const string SITE = "test-shift-save";
	static const string E2E_SITE = "e2e-shift-site";
	static const string E2E_UID = "e2e-shift-worker";
	static const string MINER = "MINER";
	static const int WEEK = 2970;

	override string GetName()
	{
		return "shiftboard/save";
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
		if (LF_E2E.RequiresSection(LF_ShiftBoardSavePayload.NAME))
		{
			// The save boot recorded on the real clock; the load boot reads in the same week.
			array<ref LF_ShiftBoardRow> kept = LF_ShiftBoard.Top(E2E_SITE, LF_ShiftBoard.Now());
			ctx.Equal(1, kept.Count(), "the e2e worker's shift board row survives the restart");
			if (!kept.IsEmpty())
				ctx.Equal(321, kept[0].m_iXp, "with its XP");
		}

		return;
#endif

		ctx.NotNull(LF_SaveSections.Get(LF_ShiftBoardSavePayload.NAME), "shiftboard is a registered save section");
		int now = LF_ShiftBoard.WEEK_ANCHOR + WEEK * LF_ShiftBoard.WEEK_SECONDS + 3600;
		LF_ShiftBoard.ApplyAll(null, -1);
		LF_ShiftBoard.Record(SITE, "uid-a", "Ada Newcomer", MINER, 1240, now);
		LF_ShiftBoard.Record(SITE, "uid-b", "Ben Hauler", MINER, 300, now);
		ctx.Equal(WEEK, LF_ShiftBoard.CurrentWeek(), "the store is in the test week");

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(LF_SaveSections.OBJECT_PREFIX + LF_ShiftBoardSavePayload.NAME), "the save carries the shiftboard section");
		ctx.Equal(0, LF_PersistenceComponentSerializer.PayloadSinceVersion(LF_ShiftBoardSavePayload.NAME), "shiftboard is not a legacy positional payload");

		// Restart: the session holds no board.
		LF_ShiftBoard.ApplyAll(null, -1);
		ctx.Equal(0, LF_ShiftBoard.Count(), "a fresh boot holds no row");
		// RedProof(): the load is skipped, so every read-back assertion fails (`--red`).
		if (!RedProof())
			ReadSection(saved, ctx);

		ctx.Equal(WEEK, LF_ShiftBoard.CurrentWeek(), "the week is back after the load");
		array<ref LF_ShiftBoardRow> top = LF_ShiftBoard.Top(SITE, now);
		ctx.Equal(2, top.Count(), "both workers are back after the load");
		if (!top.IsEmpty())
		{
			ctx.True(top[0].m_sName == "Ada Newcomer", "the leader's name is back");
			ctx.Equal(1240, top[0].m_iXp, "the leader's XP is back");
		}

		ReadSection(saved, ctx);
		ctx.Equal(2, LF_ShiftBoard.Count(), "loading twice keeps the same two rows");

		array<ref LF_ShiftBoardRow> bad = {};
		bad.Insert(LF_ShiftBoardRow.Create("", "uid-a", "A", MINER, 10));
		bad.Insert(LF_ShiftBoardRow.Create(SITE, "", "A", MINER, 10));
		bad.Insert(LF_ShiftBoardRow.Create(SITE, "uid-a", "A", "", 10));
		bad.Insert(LF_ShiftBoardRow.Create(SITE, "uid-a", "A", MINER, 0));
		ctx.Equal(0, LF_ShiftBoard.ApplyAll(bad, WEEK), "rows with a missing field are skipped");

		// A save from before the section (a fresh boot, empty boards): OnAbsent keeps them empty.
		LF_ShiftBoard.ApplyAll(null, -1);
		JsonSaveContext oldContext = new JsonSaveContext(false);
		array<string> omit = {LF_ShiftBoardSavePayload.NAME};
		LF_PersistenceComponentSerializer.SerializeStoreOmitting(persistence, oldContext, omit);
		string oldSave = oldContext.SaveToString();
		ctx.False(oldSave.Contains(LF_SaveSections.OBJECT_PREFIX + LF_ShiftBoardSavePayload.NAME), "the older save has no shiftboard section");
		JsonLoadContext oldLoad = new JsonLoadContext(false);
		ctx.True(oldLoad.LoadFromString(oldSave), "the old save parses");
		ctx.True(LF_PersistenceComponentSerializer.DeserializeStore(persistence, oldLoad), "the pre-section save loads");
		ctx.Equal(0, LF_ShiftBoard.Count(), "a pre-section save loads with empty boards (OnAbsent)");
		ctx.Equal(-1, LF_ShiftBoard.CurrentWeek(), "and no week until the first award");

		// Every non-reload run (the e2e save boot among them) leaves the e2e row in this week's store.
		LF_ShiftBoard.ApplyAll(null, -1);
		LF_ShiftBoard.Record(E2E_SITE, E2E_UID, "E2E Worker", MINER, 321, LF_ShiftBoard.Now());
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the shiftboard section of a whole-store JSON save, as LF_SaveSections.ReadAll does.
	protected static void ReadSection(string saved, LF_TestContext ctx)
	{
		LF_SavePayload section = LF_SaveSections.Get(LF_ShiftBoardSavePayload.NAME);
		if (!section)
			return;

		JsonLoadContext context = new JsonLoadContext(false);
		ctx.True(context.LoadFromString(saved), "the save parses");
		ctx.True(context.StartObject(LF_SaveSections.OBJECT_PREFIX + LF_ShiftBoardSavePayload.NAME), "the shiftboard section opens");
		int savedVersion;
		context.ReadValue(LF_SaveSections.VERSION_KEY, savedVersion);
		ctx.Equal(section.Version(), savedVersion, "the section carries its own version");
		ctx.True(section.ReadSection(context, savedVersion), "the shiftboard section reads");
		context.EndObject();
	}
}
