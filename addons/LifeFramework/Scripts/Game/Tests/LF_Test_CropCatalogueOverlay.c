// red-proof: observed 2026-11-18 `tools\cli test --no-build --red` on this test and the pack leg's crops test printed `RED-OK farming/crop-catalogue-overlay (fails with the hook on, 17 assertions)`; with the hook skipping the pack source the turnip, the patched growth time and yield all fail.
//! #3176: the crop catalogue reads pack overlays like LF_PropertyConfig. A pack row adds a crop by id, the same
//! id patches a core row and leaves the fields it does not name alone, and a new row with an empty or missing
//! stage prefab is dropped with `LF_Debug.Error("Farming", ...)`. Without a pack the core file answers alone.
//! The test overlay (Configs/Tests/LF_Test_CropOverlay.conf) is reached through a mission header built in code.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_CropCatalogueOverlay : LF_Test
{
	static const ResourceName OVERLAY = "{B3E8714F20D5A900}Configs/Tests/LF_Test_CropOverlay.conf";
	static const ResourceName CABBAGE = "{9FEBBF75B1D525EE}Prefabs/Items/Food/Cabbage.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "farming/crop-catalogue-overlay";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ConfigService.Reset();
		LF_CropCatalogue.Reload();

		LF_CropCatalogue core = LF_CropCatalogue.Get();
		ctx.True(core != null, "the core crop catalogue loads");
		if (!core)
			return;

		ctx.True(core.FindIn("example_crop") != null, "without a pack the core example crop stands");
		ctx.True(core.FindIn("test-turnip") == null, "without a pack there is no turnip");

		LF_MissionHeader header = new LF_MissionHeader();
		header.m_aLF_ConfigOverlays = {};
		header.m_aLF_ConfigOverlays.Insert(OVERLAY);
		LF_MissionConfigPackSource source = new LF_MissionConfigPackSource();
		source.SetHeader(header);
		if (!RedProof())
			LF_ConfigService.SetPackSource(source);

		LF_CropCatalogue.Reload();
		LF_CropCatalogue merged = LF_CropCatalogue.Get();
		ctx.True(merged != null, "the merged crop catalogue loads");
		if (!merged)
		{
			LF_ConfigService.Reset();
			LF_CropCatalogue.Reload();
			return;
		}

		LF_CropRow turnip = merged.FindIn("test-turnip");
		ctx.True(turnip != null, "add: the pack's new crop is in the catalogue");
		if (turnip)
		{
			ctx.Equal(300, turnip.m_iGrowthSeconds, "add: the new crop has its growth time");
			ctx.Equal(2, turnip.m_aStagePrefabs.Count(), "add: the new crop has its stage prefabs");
			ctx.True(merged.FindBySeed(CABBAGE) == turnip, "add: the new crop is found by its seed");
			ctx.True(turnip.m_sOp.IsEmpty(), "add: the merge clears the overlay verb");
		}

		LF_CropRow example = merged.FindIn("example_crop");
		ctx.True(example != null, "patch: the core crop is still there");
		if (example)
		{
			ctx.Equal(900, example.m_iGrowthSeconds, "patch: the pack's growth time replaces the core one");
			ctx.Equal(5, example.m_iYield, "patch: the pack's yield replaces the core one");
			ctx.Equal(240, example.m_iWaterMaxSeconds, "patch: a field the pack leaves alone stays core");
			ctx.Equal(1800, example.m_iLifetimeAfterRipe, "patch: the lifetime stays core");
		}

		ctx.True(merged.FindIn("test-blank-stage") == null, "an empty stage prefab drops the row");
		ctx.True(merged.FindIn("test-one-stage") == null, "a single stage drops the row");
		ctx.Equal(2, merged.m_aCrops.Count(), "example patched, turnip added, two bad rows dropped");

		LF_ConfigService.Reset();
		LF_CropCatalogue.Reload();
	}
}
