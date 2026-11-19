// red-proof: Observed 2026-11-19 with `--no-build --red`: `RED-OK harvest/persist-owned-implements (16 assertions)`, the section read skipped.
//! The harvest section (#3295): the owned implements are written in the "harvest" save section and read back
//! by character key, loading twice ends in the same state, a row with no key or an unknown implement is
//! skipped, and a save from before the section loads with the list as it was. An implement that is out is not
//! saved: the section holds the list only. Restart: the e2e character's reaper survives `wt e2e`.
[LF_TestInfo(LF_TestTier.PERSISTENCE)]
class LF_Test_PersistOwnedImplements : LF_Test
{
	static const string KEY = "test-owned-implements";
	static const string E2E_KEY = "e2e-owned-reaper";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "harvest/persist-owned-implements";
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
		if (LF_E2E.RequiresSection(LF_HarvestSavePayload.NAME))
			ctx.True(LF_OwnedImplements.Owns(E2E_KEY, LF_OwnedImplements.ID_REAPER), "the e2e character's reaper survives the restart");

		return;
#endif

		ctx.NotNull(LF_SaveSections.Get(LF_HarvestSavePayload.NAME), "harvest is a registered save section");

		LF_OwnedImplements.ResetStatics();
		LF_OwnedImplements.Grant(KEY, LF_OwnedImplements.ID_REAPER);

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(SectionKey()), "the save carries the harvest section");
		ctx.Equal(0, LF_PersistenceComponentSerializer.PayloadSinceVersion(LF_HarvestSavePayload.NAME), "harvest is not a legacy positional payload");

		// Restart: the session holds no list.
		LF_OwnedImplements.ResetStatics();
		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the save parses");
		// RedProof(): the load is skipped, so every read-back assertion fails (`--red`).
		if (!RedProof())
			ReadSection(loadContext, ctx);

		ctx.True(LF_OwnedImplements.Owns(KEY, LF_OwnedImplements.ID_REAPER), "the owned reaper is back after the load");

		JsonLoadContext again = new JsonLoadContext(false);
		again.LoadFromString(saved);
		ReadSection(again, ctx);
		ctx.True(LF_OwnedImplements.Owns(KEY, LF_OwnedImplements.ID_REAPER), "loading twice keeps the same list");
		ctx.Equal(1, LF_OwnedImplements.ExportAll().Count(), "loading twice does not duplicate the row");

		array<ref LF_OwnedImplementRow> rows = {};
		rows.Insert(LF_OwnedImplementRow.Create("", LF_OwnedImplements.ID_REAPER));
		rows.Insert(LF_OwnedImplementRow.Create(KEY, "no-such-implement"));
		ctx.Equal(0, LF_OwnedImplements.ApplyAll(rows), "rows with no key or an unknown implement are skipped");

		// A save from before the section: it is absent, and the hook only logs.
		JsonSaveContext oldContext = new JsonSaveContext(false);
		array<string> omit = {LF_HarvestSavePayload.NAME};
		LF_PersistenceComponentSerializer.SerializeStoreOmitting(persistence, oldContext, omit);
		ctx.False(oldContext.SaveToString().Contains(SectionKey()), "the older save has no harvest section");
		LF_SavePayload section = LF_SaveSections.Get(LF_HarvestSavePayload.NAME);
		if (section)
			section.OnAbsent();

		ctx.Equal(0, LF_OwnedImplements.ExportAll().Count(), "a save without the section leaves nobody owning a reaper");

		// Every non-reload run (the e2e save boot among them) leaves the e2e character's reaper in the store.
		LF_OwnedImplements.ResetStatics();
		LF_OwnedImplements.Grant(E2E_KEY, LF_OwnedImplements.ID_REAPER);
	}

	//------------------------------------------------------------------------------------------------
	// The quoted JSON key: a bare "sec_harvest" is also the start of "sec_harvest_deposits" (#3297), so a plain
	// Contains on the prefix matched the neighbouring section and the "older save has no section" check failed.
	protected static string SectionKey()
	{
		return "\"" + LF_SaveSections.OBJECT_PREFIX + LF_HarvestSavePayload.NAME + "\"";
	}

	//------------------------------------------------------------------------------------------------
	protected static void ReadSection(notnull JsonLoadContext context, LF_TestContext ctx)
	{
		LF_SavePayload section = LF_SaveSections.Get(LF_HarvestSavePayload.NAME);
		if (!section)
			return;

		ctx.True(context.StartObject(LF_SaveSections.OBJECT_PREFIX + LF_HarvestSavePayload.NAME), "the harvest section opens");
		int savedVersion;
		context.ReadValue(LF_SaveSections.VERSION_KEY, savedVersion);
		ctx.Equal(section.Version(), savedVersion, "the section carries its own version");
		ctx.True(section.ReadSection(context, savedVersion), "the harvest section reads");
		context.EndObject();
	}
}
