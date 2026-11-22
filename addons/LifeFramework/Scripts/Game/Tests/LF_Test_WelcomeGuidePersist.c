// red-proof: observed 2026-11-22 `tools/cli test --no-build --red LF_Test_WelcomeGuide,LF_Test_WelcomeGuideWorld,LF_Test_WelcomeGuidePersist`: RED PROOF OK (all 3 fail with the hook on); quests/welcome-guide-persist fails its read-back-after-load assertion. Green with `--only` (14 assertions).
//! The welcome guide section: a pending guide is written in the "welcomeguide" save section and read
//! back by character id, loading twice ends in the same state, and a save from before the section has
//! none. Restart: the e2e character's pending guide survives `wt e2e`.
[LF_TestInfo(LF_TestTier.PERSISTENCE)]
class LF_Test_WelcomeGuidePersist : LF_Test
{
	static const string ID = "test-welcome-guide-persist";
	static const string E2E_ID = "e2e-welcome-guide";

	override string GetName()
	{
		return "quests/welcome-guide-persist";
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
		if (LF_E2E.RequiresSection(LF_WelcomeGuideSavePayload.NAME))
			ctx.True(LF_WelcomeGuide.IsPending(E2E_ID), "the e2e character's pending guide survives the restart");

		return;
#endif

		ctx.NotNull(LF_SaveSections.Get(LF_WelcomeGuideSavePayload.NAME), "welcomeguide is a registered save section");

		array<ref LF_WelcomeGuideRow> seed = {};
		seed.Insert(LF_WelcomeGuideRow.Create(ID));
		LF_WelcomeGuide.ApplyAll(seed);

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(LF_SaveSections.OBJECT_PREFIX + LF_WelcomeGuideSavePayload.NAME), "the save carries the welcomeguide section");
		ctx.Equal(0, LF_PersistenceComponentSerializer.PayloadSinceVersion(LF_WelcomeGuideSavePayload.NAME), "welcomeguide is not a legacy positional payload");

		// Restart: the session holds no pending guide.
		LF_WelcomeGuide.ApplyAll(null);
		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the save parses");
		// RedProof(): the load is skipped, so every read-back assertion fails (`--red`).
		if (!RedProof())
			ReadSection(loadContext, ctx);

		ctx.True(LF_WelcomeGuide.IsPending(ID), "the pending guide is back after the load");

		JsonLoadContext again = new JsonLoadContext(false);
		again.LoadFromString(saved);
		ReadSection(again, ctx);
		ctx.True(LF_WelcomeGuide.IsPending(ID), "loading twice keeps the guide pending");
		ctx.Equal(1, LF_WelcomeGuide.Count(), "loading twice adds no second row");

		// A save from before the section: nothing pending.
		JsonSaveContext oldContext = new JsonSaveContext(false);
		array<string> omit = {LF_WelcomeGuideSavePayload.NAME};
		LF_PersistenceComponentSerializer.SerializeStoreOmitting(persistence, oldContext, omit);
		ctx.False(oldContext.SaveToString().Contains(LF_SaveSections.OBJECT_PREFIX + LF_WelcomeGuideSavePayload.NAME), "the older save has no welcomeguide section");

		// Every non-reload run (the e2e save boot among them) leaves the e2e row in the store.
		array<ref LF_WelcomeGuideRow> e2e = {};
		e2e.Insert(LF_WelcomeGuideRow.Create(E2E_ID));
		LF_WelcomeGuide.ApplyAll(e2e);
	}

	//------------------------------------------------------------------------------------------------
	protected static void ReadSection(notnull JsonLoadContext context, LF_TestContext ctx)
	{
		LF_SavePayload section = LF_SaveSections.Get(LF_WelcomeGuideSavePayload.NAME);
		if (!section)
			return;

		ctx.True(context.StartObject(LF_SaveSections.OBJECT_PREFIX + LF_WelcomeGuideSavePayload.NAME), "the welcomeguide section opens");
		int savedVersion;
		context.ReadValue(LF_SaveSections.VERSION_KEY, savedVersion);
		ctx.Equal(section.Version(), savedVersion, "the section carries its own version");
		ctx.True(section.ReadSection(context, savedVersion), "the welcomeguide section reads");
		context.EndObject();
	}
}
