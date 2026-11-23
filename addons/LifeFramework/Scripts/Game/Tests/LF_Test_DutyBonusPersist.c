// red-proof: 2026-11-23 `tools/cli test --no-build --red LF_Test_DutyBonusWiring,LF_Test_DutyBonusPersist,LF_Test_PoliceCallPay`: RED PROOF OK; "the unpaid bonus is back after the load" (the section read skipped). Green with --only.
//! The duty bonus section (#4319): an officer's unpaid bonus and the hour's count are written in the
//! "dutybonus" save section and read back, loading twice ends in the same state, a row with no account is
//! skipped, and a save from before the section carries none. Restart: the e2e officer's bonus survives
//! `wt e2e`.
[LF_TestInfo(LF_TestTier.PERSISTENCE)]
class LF_Test_DutyBonusPersist : LF_Test
{
	static const string KEY = "test-duty-bonus-persist";
	static const string E2E_KEY = "e2e-duty-bonus";
	static const int PENDING = 35;
	static const int WINDOW_START = 1800000000;	// lf-fixed: a unix second above 2^24, so float rounding would show
	static const int WINDOW_EARNED = 45;

	override string GetName()
	{
		return "jobs/duty-bonus-persist";
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
		if (LF_E2E.RequiresSection(LF_DutyBonusSavePayload.NAME))
		{
			LF_DutyBonusRow restored = RowOf(E2E_KEY);
			ctx.NotNull(restored, "the e2e officer's bonus survives the restart");
			if (restored)
			{
				ctx.Equal(PENDING, restored.m_iPending, "the unpaid bonus survives the restart");
				ctx.Equal(WINDOW_START, restored.m_iWindowStart, "the hour window's start survives the restart");
				ctx.Equal(WINDOW_EARNED, restored.m_iWindowEarned, "the hour's count survives the restart");
			}
		}

		return;
#endif

		ctx.NotNull(LF_SaveSections.Get(LF_DutyBonusSavePayload.NAME), "dutybonus is a registered save section");

		array<ref LF_DutyBonusRow> seed = {};
		seed.Insert(LF_DutyBonusRow.Create(KEY, PENDING, WINDOW_START, WINDOW_EARNED));
		LF_DutyBonus.ApplyRows(seed);

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(LF_SaveSections.OBJECT_PREFIX + LF_DutyBonusSavePayload.NAME), "the save carries the dutybonus section");
		ctx.Equal(0, LF_PersistenceComponentSerializer.PayloadSinceVersion(LF_DutyBonusSavePayload.NAME), "dutybonus is not a legacy positional payload");

		// Restart: the session holds no bonus.
		LF_DutyBonus.ApplyRows(null);
		ctx.True(RowOf(KEY) == null, "the cleared session holds no bonus");
		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the save parses");
		// RedProof(): the load is skipped, so every read-back assertion fails (`--red`).
		if (!RedProof())
			ReadSection(loadContext, ctx);

		LF_DutyBonusRow row = RowOf(KEY);
		ctx.NotNull(row, "the unpaid bonus is back after the load");
		if (row)
		{
			ctx.Equal(PENDING, row.m_iPending, "the pending amount is back");
			ctx.Equal(WINDOW_START, row.m_iWindowStart, "the hour window's start is back");
			ctx.Equal(WINDOW_EARNED, row.m_iWindowEarned, "the hour's count is back");
		}

		JsonLoadContext again = new JsonLoadContext(false);
		again.LoadFromString(saved);
		ReadSection(again, ctx);
		LF_DutyBonusRow twice = RowOf(KEY);
		ctx.True(twice && twice.m_iPending == PENDING, "loading twice keeps the same bonus, not double");

		array<ref LF_DutyBonusRow> rows = {};
		rows.Insert(LF_DutyBonusRow.Create("", PENDING, WINDOW_START, WINDOW_EARNED));
		ctx.Equal(0, LF_DutyBonus.ApplyRows(rows), "a row with no account is skipped");

		// A save from before the section: no bonus section.
		JsonSaveContext oldContext = new JsonSaveContext(false);
		array<string> omit = {LF_DutyBonusSavePayload.NAME};
		LF_PersistenceComponentSerializer.SerializeStoreOmitting(persistence, oldContext, omit);
		ctx.False(oldContext.SaveToString().Contains(LF_SaveSections.OBJECT_PREFIX + LF_DutyBonusSavePayload.NAME), "the older save has no dutybonus section");

		// Every non-reload run (the e2e save boot among them) leaves the e2e officer's bonus in the store.
		array<ref LF_DutyBonusRow> e2e = {};
		e2e.Insert(LF_DutyBonusRow.Create(E2E_KEY, PENDING, WINDOW_START, WINDOW_EARNED));
		LF_DutyBonus.ApplyRows(e2e);
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_DutyBonusRow RowOf(string accountId)
	{
		array<ref LF_DutyBonusRow> rows = LF_DutyBonus.ExportRows();
		foreach (LF_DutyBonusRow row : rows)
		{
			// A copy, so the row outlives the exported array.
			if (row && row.m_sAccountId == accountId)
				return LF_DutyBonusRow.Create(row.m_sAccountId, row.m_iPending, row.m_iWindowStart, row.m_iWindowEarned);
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected static void ReadSection(notnull JsonLoadContext context, LF_TestContext ctx)
	{
		LF_SavePayload section = LF_SaveSections.Get(LF_DutyBonusSavePayload.NAME);
		if (!section)
			return;

		ctx.True(context.StartObject(LF_SaveSections.OBJECT_PREFIX + LF_DutyBonusSavePayload.NAME), "the dutybonus section opens");
		int savedVersion;
		context.ReadValue(LF_SaveSections.VERSION_KEY, savedVersion);
		ctx.Equal(section.Version(), savedVersion, "the section carries its own version");
		ctx.True(section.ReadSection(context, savedVersion), "the dutybonus section reads");
		context.EndObject();
	}
}
