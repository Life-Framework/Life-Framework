// red-proof: 2026-11-17 `tools\cli test --no-build --red LF_Test_SaveNewerVersion,LF_Test_PersistStoreStaticReset`: RED PROOF OK (both listed tests fail with the hook on; this one with the reset skipped fails `a world end forgets that the store was read`). Green with the hook (--only).
//! A world end resets the store-read flags (LF_PersistenceComponentSerializer_StaticResetHook). A
//! stale "store loaded" from the last world hid a continued save that never reached the store in the
//! next one (an in-process scenario restart), so autosave overwrote the save on disk.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_PersistStoreStaticReset : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "persist/store-static-reset";
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

		// A store record with no version header: the read is reached and nothing is applied.
		JsonLoadContext empty = new JsonLoadContext(false);
		empty.LoadFromString("{}");
		LF_PersistenceComponentSerializer.DeserializeStore(persistence, empty);
		ctx.True(LF_PersistenceComponentSerializer.WasStoreLoaded(), "this world read its store");

		LF_StaticResetHook hook = new LF_PersistenceComponentSerializer_StaticResetHook();
		if (!RedProof())
			hook.Apply();

		ctx.False(LF_PersistenceComponentSerializer.WasStoreLoaded(), "a world end forgets that the store was read");
		ctx.True(LF_StaticReset.Registered().Contains("LF_PersistenceComponentSerializer"), "the serializer registers its reset hook");
		LF_SaveStamp.ResetForLoad();
	}
}
