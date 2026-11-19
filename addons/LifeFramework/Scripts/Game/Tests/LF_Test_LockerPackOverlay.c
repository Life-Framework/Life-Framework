// red-proof: observed 2026-11-19 `tools\cli test --no-build --red LF_Test_LockerPackOverlay` printed `RED-OK lockers/pack-overlay (fails with the hook on, 9 assertions)`; first failure `the pack sets the rent to 35 (expected 35, got 100)`. `--only` passes.
//! #3192: the locker config reads pack overlays like LF_PropertyConfig and LF_MarketConfig. A pack root sets
//! the rent price and period; a field it leaves out keeps the core value; a value out of range (a negative
//! reclaim fee, a zero weight capacity) is refused and the core value stays. Without a pack the core file
//! answers. The test overlay (Configs/Tests/LF_Test_LockerOverlay.conf) is reached through a mission header
//! built in code.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_LockerPackOverlay : LF_Test
{
	static const ResourceName OVERLAY = "{4C7A1E93B5D20F68}Configs/Tests/LF_Test_LockerOverlay.conf";

	override string GetName()
	{
		return "lockers/pack-overlay";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_ConfigService.Reset();
		LF_LockerConfig.Reload();

		ctx.Equal(100, LF_LockerConfig.RentPrice(), "without a pack the core rent is 100");
		ctx.Equal(7, LF_LockerConfig.RentPeriodDays(), "without a pack the core period is 7 days");

		LF_MissionHeader header = new LF_MissionHeader();
		header.m_aLF_ConfigOverlays = {};
		header.m_aLF_ConfigOverlays.Insert(OVERLAY);
		LF_MissionConfigPackSource source = new LF_MissionConfigPackSource();
		source.SetHeader(header);
		if (!RedProof())
			LF_ConfigService.SetPackSource(source);

		LF_LockerConfig.Reload();
		ctx.Equal(35, LF_LockerConfig.RentPrice(), "the pack sets the rent to 35");
		ctx.Equal(3, LF_LockerConfig.RentPeriodDays(), "the pack sets the period to 3 days");
		ctx.Equal(2, LF_LockerConfig.GracePeriodDays(), "a field the pack leaves out keeps the core 2 days");
		ctx.Equal(50, LF_LockerConfig.ReclaimFee(), "a negative reclaim fee is refused, the core 50 stays");
		ctx.True(Math.AbsFloat(LF_LockerConfig.WeightCapacity() - 60) < 0.001, "a zero capacity is refused, the core 60 kg stays");

		LF_ConfigService.Reset();
		LF_LockerConfig.Reload();
	}
}
