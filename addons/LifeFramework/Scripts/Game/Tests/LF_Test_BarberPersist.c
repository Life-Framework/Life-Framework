// red-proof: observed 2026-11-18 `tools/cli test --no-build --red LF_Test_BarberChangesHair,LF_Test_BarberPersist`: RED PROOF OK; barber/changes-hair fails `the haircut is accepted` (the paid haircut is skipped), barber/persist fails `the paid look is back after the load`. Green with `--only` (20 and 16 assertions).
//! The barber section (#3234): a paid look is written in the "barber" save section and read back by key,
//! loading twice ends in the same state, a row with no key or head is skipped, and a save from before the
//! section loads with no looks. Restart: the e2e character's look survives `wt e2e`.
[LF_TestInfo(LF_TestTier.PERSISTENCE)]
class LF_Test_BarberPersist : LF_Test
{
	static const string KEY = "test-barber-persist";
	static const string E2E_KEY = "e2e-barber-look";
	static const ResourceName HEAD = "{D7C470712EC4D596}Prefabs/Characters/Heads/Head_White_05.et";

	override string GetName()
	{
		return "barber/persist";
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
		if (LF_E2E.RequiresSection(LF_BarberSavePayload.NAME))
			ctx.True(LF_Barber.SavedLook(E2E_KEY) == HEAD, "the e2e character's look survives the restart");

		return;
#endif

		ctx.NotNull(LF_SaveSections.Get(LF_BarberSavePayload.NAME), "barber is a registered save section");

		array<ref LF_BarberLookRow> seed = {};
		seed.Insert(LF_BarberLookRow.Create(KEY, HEAD));
		LF_Barber.ApplyAll(seed);

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(LF_SaveSections.OBJECT_PREFIX + LF_BarberSavePayload.NAME), "the save carries the barber section");
		ctx.Equal(0, LF_PersistenceComponentSerializer.PayloadSinceVersion(LF_BarberSavePayload.NAME), "barber is not a legacy positional payload");

		// Restart: the session holds no look.
		LF_Barber.ApplyAll(null);
		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the save parses");
		// RedProof(): the load is skipped, so every read-back assertion fails (`--red`).
		if (!RedProof())
			ReadSection(loadContext, ctx);

		ctx.True(LF_Barber.SavedLook(KEY) == HEAD, "the paid look is back after the load");

		JsonLoadContext again = new JsonLoadContext(false);
		again.LoadFromString(saved);
		ReadSection(again, ctx);
		ctx.True(LF_Barber.SavedLook(KEY) == HEAD, "loading twice keeps the same look");

		array<ref LF_BarberLookRow> rows = {};
		rows.Insert(LF_BarberLookRow.Create("", HEAD));
		rows.Insert(LF_BarberLookRow.Create(KEY, ""));
		ctx.Equal(0, LF_Barber.ApplyAll(rows), "rows with no key or no head are skipped");

		// A save from before the section: no looks.
		JsonSaveContext oldContext = new JsonSaveContext(false);
		array<string> omit = {LF_BarberSavePayload.NAME};
		LF_PersistenceComponentSerializer.SerializeStoreOmitting(persistence, oldContext, omit);
		ctx.False(oldContext.SaveToString().Contains(LF_SaveSections.OBJECT_PREFIX + LF_BarberSavePayload.NAME), "the older save has no barber section");

		LF_Barber.ApplyAll(null);
		// Every non-reload run (the e2e save boot among them) leaves the e2e look in the store.
		array<ref LF_BarberLookRow> e2e = {};
		e2e.Insert(LF_BarberLookRow.Create(E2E_KEY, HEAD));
		LF_Barber.ApplyAll(e2e);
	}

	//------------------------------------------------------------------------------------------------
	protected static void ReadSection(notnull JsonLoadContext context, LF_TestContext ctx)
	{
		LF_SavePayload section = LF_SaveSections.Get(LF_BarberSavePayload.NAME);
		if (!section)
			return;

		ctx.True(context.StartObject(LF_SaveSections.OBJECT_PREFIX + LF_BarberSavePayload.NAME), "the barber section opens");
		int savedVersion;
		context.ReadValue(LF_SaveSections.VERSION_KEY, savedVersion);
		ctx.Equal(section.Version(), savedVersion, "the section carries its own version");
		ctx.True(section.ReadSection(context, savedVersion), "the barber section reads");
		context.EndObject();
	}
}
