// red-proof: with the `RedProof()` hook skipping the pack source, the overlay never loads, so `garage: the pack's impound fee replaces the core fee` (expected 77, got 250) and the slot price and tow delay checks fail; run `tools\cli test --no-build --red LF_Test_GarageOverlay`.
//! #3458: the garage config reads pack overlays, like LF_PropertyConfig (#2977). An overlay sets only the
//! fields it writes; every other field stays core, and without a pack the core file answers alone. The test
//! overlay (Configs/Tests/LF_Test_GarageOverlay.conf) is reached through a mission header built in code.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_GarageOverlay : LF_Test
{
	static const ResourceName OVERLAY = "{E2EB494D1E7BBE99}Configs/Tests/LF_Test_GarageOverlay.conf";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "garage/pack-overlay";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ConfigService.Reset();
		LF_GarageConfig.Reload();

		// No pack: the core file alone.
		ctx.Equal(250, LF_GarageConfig.ImpoundFee(), "without a pack, the impound fee is the core fee");
		ctx.Equal(500, LF_GarageConfig.SlotPriceStart(), "without a pack, the first slot is the core price");
		ctx.Equal(60, LF_GarageConfig.TowAfterMinutes(), "without a pack, the tow delay is the core delay");

		LF_MissionHeader header = new LF_MissionHeader();
		header.m_aLF_ConfigOverlays = {};
		header.m_aLF_ConfigOverlays.Insert(OVERLAY);
		LF_MissionConfigPackSource source = new LF_MissionConfigPackSource();
		source.SetHeader(header);
		if (!RedProof())
			LF_ConfigService.SetPackSource(source);

		LF_GarageConfig.Reload();

		ctx.Equal(77, LF_GarageConfig.ImpoundFee(), "garage: the pack's impound fee replaces the core fee");
		ctx.Equal(1111, LF_GarageConfig.SlotPriceStart(), "garage: the pack's first slot price replaces the core price");
		ctx.Equal(33, LF_GarageConfig.TowAfterMinutes(), "garage: the pack's tow delay replaces the core delay");
		ctx.Equal(2, LF_GarageConfig.BaseSlots(), "garage: a field the pack leaves out stays core");
		ctx.Equal(250, LF_GarageConfig.SlotPriceStep(), "garage: the step the pack leaves out stays core");
		ctx.Equal(150, LF_GarageConfig.TowFee(), "garage: the tow fee the pack leaves out stays core");

		LF_ConfigService.Reset();
		LF_GarageConfig.Reload();
	}
}
