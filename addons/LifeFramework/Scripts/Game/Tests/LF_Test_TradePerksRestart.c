// red-proof: observed 2026-11-23 `tools/cli test --no-build --red LF_Test_TradePerksRestart`: RED PROOF OK; with the load skipped it fails `the mining perk is back after the load`, `the farming perk is back after the load`, `Has finds the loaded mining perk`. Green with `--only`. Again 2026-11-23 (#4525): RED-OK (49 assertions), also failing `the week lock is back after the load`.
//! The trade perks section (WK-W8, #4186): two picked perks are written in the "tradeperks" save
//! section and read back, loading twice ends in the same state, a row with a missing field is
//! skipped, and a save from before the section (0.2.1) loads through OnAbsent with no perk.
//! Restart: the e2e character's perk survives `wt e2e`.
[LF_TestInfo(LF_TestTier.PERSISTENCE)]
class LF_Test_TradePerksRestart : LF_Test
{
	static const string KEY = "test-trade-perks";
	static const string E2E_KEY = "e2e-trade-perks";
	static const string TRADE_A = "MINER";
	static const string TRADE_B = "FARMER";
	static const string OLD_TRADE = "TEST_PERK_OLD_TRADE";
	static const string PERK_A = "double_yield";
	static const string PERK_B = "fast_swing";

	override string GetName()
	{
		return "tradeperks/restart";
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
		if (LF_E2E.RequiresSection(LF_TradePerksSavePayload.NAME))
		{
			ctx.True(LF_TradePerks.PerkAt(E2E_KEY, TRADE_A, 10) == PERK_A, "the e2e character's perk survives the restart");
			ctx.True(LF_PerkPick.DaysLocked(E2E_KEY, TRADE_A, 10) > 0, "the e2e character's week lock survives the restart");
		}

		return;
#endif

		ctx.NotNull(LF_SaveSections.Get(LF_TradePerksSavePayload.NAME), "tradeperks is a registered save section");
		ctx.False(LF_TradePerks.Pick("", TRADE_A, 10, PERK_A), "a pick with no character is refused");
		ctx.False(LF_TradePerks.Pick(KEY, TRADE_A, 0, PERK_A), "a pick at level 0 is refused");

		LF_TradePerks.ApplyAll(null);
		LF_PerkPick.ApplyChanges(null);
		LF_PerkPick.SetDayForTests(300);
		ctx.True(LF_TradePerks.Pick(KEY, TRADE_A, 10, PERK_A), "the level 10 mining perk is picked");
		ctx.True(LF_TradePerks.Pick(KEY, TRADE_B, 20, PERK_B), "the level 20 farming perk is picked");
		LF_PerkPick.MarkChanged(KEY, TRADE_A, 10);
		ctx.Equal(LF_PerkPick.WEEK_DAYS, LF_PerkPick.DaysLocked(KEY, TRADE_A, 10), "a change today locks the level for a week");

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(LF_SaveSections.OBJECT_PREFIX + LF_TradePerksSavePayload.NAME), "the save carries the tradeperks section");
		ctx.Equal(0, LF_PersistenceComponentSerializer.PayloadSinceVersion(LF_TradePerksSavePayload.NAME), "tradeperks is not a legacy positional payload");

		// Restart: the session holds no perk and no change clock.
		LF_TradePerks.ApplyAll(null);
		LF_PerkPick.ApplyChanges(null);
		ctx.Equal(0, LF_TradePerks.Count(), "a fresh boot holds no perk");
		ctx.Equal(0, LF_PerkPick.DaysLocked(KEY, TRADE_A, 10), "a fresh boot holds no week lock");
		// RedProof(): the load is skipped, so every read-back assertion fails (`--red`).
		if (!RedProof())
			ReadSection(saved, ctx);

		ctx.Equal(LF_PerkPick.WEEK_DAYS, LF_PerkPick.DaysLocked(KEY, TRADE_A, 10), "the week lock is back after the load");
		ctx.Equal(0, LF_PerkPick.DaysLocked(KEY, TRADE_B, 20), "a level with no change has no lock after the load");
		LF_PerkPick.SetDayForTests(303);
		ctx.Equal(4, LF_PerkPick.DaysLocked(KEY, TRADE_A, 10), "three days on, four days of the lock are left");
		LF_PerkPick.SetDayForTests(250);
		ctx.Equal(LF_PerkPick.WEEK_DAYS, LF_PerkPick.DaysLocked(KEY, TRADE_A, 10), "a clock that went back never locks past a week");
		LF_PerkPick.SetDayForTests(-1);
		ctx.True(LF_TradePerks.PerkAt(KEY, TRADE_A, 10) == PERK_A, "the mining perk is back after the load");
		ctx.True(LF_TradePerks.PerkAt(KEY, TRADE_B, 20) == PERK_B, "the farming perk is back after the load");
		ctx.True(LF_TradePerks.Has(KEY, TRADE_A, PERK_A), "Has finds the loaded mining perk");
		ctx.False(LF_TradePerks.Has(KEY, TRADE_B, PERK_A), "a perk of one trade is not held in another");

		ReadSection(saved, ctx);
		ctx.Equal(2, LF_TradePerks.Count(), "loading twice keeps the same two perks");

		// A version-1 section (before the change clock) loads its perks and no week lock.
		ReadSection(saved, ctx, 1);
		ctx.Equal(2, LF_TradePerks.Count(), "a version-1 section loads its perks");
		ctx.Equal(0, LF_PerkPick.DaysLocked(KEY, TRADE_A, 10), "a version-1 section loads no week lock");

		// A perk saved under a job id that was renamed loads under the new id.
		LF_TradePerks.ApplyAll(null);
		LF_TradePerks.Pick(KEY, OLD_TRADE, 10, PERK_A);
		JsonSaveContext aliasSave = new JsonSaveContext(false);
		LF_PersistenceComponentSerializer.SerializeStore(persistence, aliasSave);
		string aliased = aliasSave.SaveToString();
		ctx.True(LF_SaveIds.Add(LF_SaveIds.KIND_JOB, OLD_TRADE, TRADE_A), "the test alias for a renamed trade is added");
		LF_TradePerks.ApplyAll(null);
		ReadSection(aliased, ctx);
		ctx.True(LF_TradePerks.PerkAt(KEY, TRADE_A, 10) == PERK_A, "a perk saved under a renamed trade loads under the new id");
		ctx.True(LF_TradePerks.PerkAt(KEY, OLD_TRADE, 10).IsEmpty(), "nothing stays under the old trade id");
		LF_SaveIds.ResetStatics();

		array<ref LF_TradePerkRow> rows = {};
		rows.Insert(LF_TradePerkRow.Create("", TRADE_A, 10, PERK_A));
		rows.Insert(LF_TradePerkRow.Create(KEY, "", 10, PERK_A));
		rows.Insert(LF_TradePerkRow.Create(KEY, TRADE_A, 0, PERK_A));
		rows.Insert(LF_TradePerkRow.Create(KEY, TRADE_A, 10, ""));
		ctx.Equal(0, LF_TradePerks.ApplyAll(rows), "rows with a missing field are skipped");

		// A save from before the section (0.2.1): a fresh boot loads it through OnAbsent with no perk.
		JsonSaveContext oldContext = new JsonSaveContext(false);
		array<string> omit = {LF_TradePerksSavePayload.NAME};
		LF_PersistenceComponentSerializer.SerializeStoreOmitting(persistence, oldContext, omit);
		string oldSave = oldContext.SaveToString();
		ctx.False(oldSave.Contains(LF_SaveSections.OBJECT_PREFIX + LF_TradePerksSavePayload.NAME), "the older save has no tradeperks section");
		LF_TradePerks.ApplyAll(null);
		JsonLoadContext oldLoad = new JsonLoadContext(false);
		ctx.True(oldLoad.LoadFromString(oldSave), "the old save parses");
		ctx.True(LF_PersistenceComponentSerializer.DeserializeStore(persistence, oldLoad), "the pre-section save loads");
		ctx.Equal(0, LF_TradePerks.Count(), "a pre-section save loads with no perk");

		// Every non-reload run (the e2e save boot among them) leaves the e2e perk and its week lock (a
		// change today on the world clock) in the store.
		LF_TradePerks.ApplyAll(null);
		LF_PerkPick.ApplyChanges(null);
		LF_TradePerks.Pick(E2E_KEY, TRADE_A, 10, PERK_A);
		LF_PerkPick.MarkChanged(E2E_KEY, TRADE_A, 10);
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the tradeperks section of a whole-store JSON save, as LF_SaveSections.ReadAll does.
	//! \param asVersion Read as this older section version instead of the saved one; -1 for the saved.
	protected static void ReadSection(string saved, LF_TestContext ctx, int asVersion = -1)
	{
		LF_SavePayload section = LF_SaveSections.Get(LF_TradePerksSavePayload.NAME);
		if (!section)
			return;

		JsonLoadContext context = new JsonLoadContext(false);
		ctx.True(context.LoadFromString(saved), "the save parses");
		ctx.True(context.StartObject(LF_SaveSections.OBJECT_PREFIX + LF_TradePerksSavePayload.NAME), "the tradeperks section opens");
		int savedVersion;
		context.ReadValue(LF_SaveSections.VERSION_KEY, savedVersion);
		ctx.Equal(section.Version(), savedVersion, "the section carries its own version");
		if (asVersion >= 0)
			savedVersion = asVersion;
		ctx.True(section.ReadSection(context, savedVersion), "the tradeperks section reads");
		context.EndObject();
	}
}
