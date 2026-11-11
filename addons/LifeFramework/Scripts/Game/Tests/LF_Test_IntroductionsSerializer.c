// red-proof: Observed 2026-11-08 with `tools\cli test --no-build --only` (all four broken in one run, 0/4 passed), then restored; green. Commenting out LF_Introductions.ApplyAll in the serializer's introductions read failed
// `[LFTEST] FAIL interaction/introductions-serializer`, `- the pair is known again after the load`.
// tier: WORLD
class LF_Test_IntroductionsSerializer : LF_Test
{
	protected static const string KNOWER_UID = "intro-save-knower";
	protected static const string KNOWN_UID = "intro-save-known";

	override string GetName()
	{
		return "interaction/introductions-serializer";
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

		array<ref LF_IntroductionRecord> before = LF_Introductions.ExportAll();
		LF_Introductions.Introduce(KNOWER_UID, KNOWN_UID);

		// Save through the real serializer write path.
		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(KNOWER_UID), "the save carries the introductions payload");

		// Restart: the session no longer knows the pair.
		LF_Introductions.ApplyAll(before);
		ctx.False(LF_Introductions.Knows(KNOWER_UID, KNOWN_UID), "the restarted session starts without the pair");

		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the save parses");
		int version;
		loadContext.ReadValue("version", version);
		ctx.Equal(23, version, "the save is written at version 23 (introductions is payload 8)");
		LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, loadContext, "introductions");
		ctx.True(LF_Introductions.Knows(KNOWER_UID, KNOWN_UID), "the pair is known again after the load");
		ctx.False(LF_Introductions.Knows(KNOWN_UID, KNOWER_UID), "the load does not invent the reverse pair");

		LF_Introductions.ApplyAll(before);
	}
}
