// red-proof: RED-OK 2026-11-27, `tools\cli test --no-build --red LF_Test_ConfigOverlayFolderCache`: with the hook skipped, "the world-end hook drops the cache" failed.

//! config/overlay-folder-cache: the overlay folder search is cached, the world-end hook is
//! registered and drops the cache, so a later world searches the resource database again.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_ConfigOverlayFolderCache : LF_Test
{
	override string GetName()
	{
		return "config/overlay-folder-cache";
	}

	override void Run(LF_TestContext ctx)
	{
		array<string> owners = LF_StaticReset.Registered();
		ctx.True(owners && owners.Contains("LF_ConfigOverlay"), "the folder search cache has a world-end hook");

		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		// A parent with no folder beside it; built in two parts because it names no real resource.
		ResourceName parent = "{0000000000000000}" + "Configs/LF_Test_NoSuchOverlayFolder.conf";
		LF_ConfigOverlay.LoadFolderOverlays(parent, LF_ConfigRecord, "test", resources, names);
		ctx.Equal(0, names.Count(), "a folder with no files adds no overlays");
		ctx.True(LF_ConfigOverlay.CachedFolderCount() > 0, "the search result is cached");

		LF_ConfigOverlay_StaticResetHook hook = new LF_ConfigOverlay_StaticResetHook();
		if (!RedProof())
			hook.Apply();
		ctx.Equal(0, LF_ConfigOverlay.CachedFolderCount(), "the world-end hook drops the cache");
	}
}
