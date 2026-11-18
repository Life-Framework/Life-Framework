// red-proof: observed 2026-11-18 `tools\cli test --no-build --red LF_Test_StallConfigOverlay` printed `RED PROOF OK`; with no pack source the overlay checks failed (the pack's m_bEnabled, market day 0, hours), `--only` then passed 7/7.
//! #3182: the stall config reads pack overlays like LF_MarketConfig. An overlay root's fields replace the core
//! ones (it switches stalls on and sets the hours; a field it leaves alone stays core), `m_iMarketDay` 0 opens
//! the market on every day of the week, and without a pack the core file answers alone (off, day 5, 08:00 to
//! 18:00). The test overlay (Configs/Tests/LF_Test_StallOverlay.conf) is reached through a mission header built
//! in code; the flagship's own overlay is checked in the pack leg (everon/stalls).
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_StallConfigOverlay : LF_Test
{
	static const ResourceName OVERLAY = "{5B2E8A41D7C93F07}Configs/Tests/LF_Test_StallOverlay.conf";

	override string GetName()
	{
		return "market/stall-config-overlay";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_ConfigService.Reset();
		LF_StallConfig.Reload();

		// No pack: the core file alone.
		LF_StallConfig core = LF_StallConfig.Get();
		ctx.True(core != null, "the core stall config loads");
		if (!core)
			return;

		ctx.False(core.m_bEnabled, "without a pack stalls are off");
		ctx.Equal(5, core.m_iMarketDay, "without a pack the market day is 5");
		ctx.Equal(8, core.m_iOpenHour, "without a pack the market opens at 08:00");
		ctx.Equal(18, core.m_iCloseHour, "without a pack the market closes at 18:00");
		ctx.Equal(50, core.m_iPitchFee, "without a pack the core pitch fee stands");
		ctx.False(LF_StallRules.IsMarketOpen(core, 7 * 10 + 5, 19), "without a pack day 5 is shut at 19:00");

		LF_MissionHeader header = new LF_MissionHeader();
		header.m_aLF_ConfigOverlays = {};
		header.m_aLF_ConfigOverlays.Insert(OVERLAY);
		LF_MissionConfigPackSource source = new LF_MissionConfigPackSource();
		source.SetHeader(header);
		if (!RedProof())
			LF_ConfigService.SetPackSource(source);

		LF_StallConfig.Reload();
		LF_StallConfig merged = LF_StallConfig.Get();
		ctx.True(merged != null, "the merged stall config loads");
		if (!merged)
		{
			LF_ConfigService.Reset();
			return;
		}

		ctx.True(merged.m_bEnabled, "the pack's m_bEnabled switches stalls on");
		ctx.Equal(0, merged.m_iMarketDay, "the pack sets market day 0 (every day)");
		ctx.Equal(16, merged.m_iOpenHour, "the pack sets the opening hour");
		ctx.Equal(22, merged.m_iCloseHour, "the pack sets the closing hour");
		ctx.Equal(20, merged.m_iPitchFee, "the pack sets the pitch fee");
		ctx.Equal(6, merged.m_iMaxListings, "a field the pack leaves alone stays core");
		ctx.Equal(5, merged.m_iCutPct, "the cut stays core");

		// Market day 0: every day of the week, inside the hours only.
		for (int day = 100; day < 107; day++)
		{
			ctx.True(LF_StallRules.IsMarketOpen(merged, day, 19), string.Format("day %1 is open at 19:00", day));
			ctx.False(LF_StallRules.IsMarketOpen(merged, day, 23), string.Format("day %1 is closed at 23:00", day));
			ctx.False(LF_StallRules.IsMarketOpen(merged, day, 15), string.Format("day %1 is closed at 15:00", day));
		}

		ctx.True(LF_StallRules.IsMarketOpen(merged, 100, 16), "open at the opening hour");
		ctx.False(LF_StallRules.IsMarketOpen(merged, 100, 22), "closed at the closing hour");

		// A named weekday still gates the market (1..6).
		LF_StallConfig friday = LF_StallConfig.Make(true, 5, 16, 22, 20, 6, 1, 1000, 5);
		ctx.True(LF_StallRules.IsMarketOpen(friday, 12, 19), "market day 5: day 12 (12 mod 7 = 5) is open");
		ctx.False(LF_StallRules.IsMarketOpen(friday, 13, 19), "market day 5: day 13 is shut");

		// A pack that breaks the table is refused: stalls stay off.
		LF_StallConfig bad = LF_StallConfig.Make(true, 0, 22, 16, 20, 6, 1, 1000, 5);
		ctx.False(bad.Sanitize(), "closing before opening is refused");
		ctx.False(bad.m_bEnabled, "a refused table is switched off");

		LF_ConfigService.Reset();
		LF_StallConfig.Reload();
		LF_StallConfig after = LF_StallConfig.Get();
		ctx.True(after != null && !after.m_bEnabled, "Reset and Reload bring the core file back");
	}
}
