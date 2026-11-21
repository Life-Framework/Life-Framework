// red-proof: observed 2026-11-18 `tools\cli test --no-build --red LF_Test_StallConfigOverlay,LF_Test_EveronStalls` printed `RED PROOF OK`; with an empty header as the pack source it failed `the flagship is open at 19:00` and `the flagship is open at 19:00 on any day`; `--only` then passed.
//! #3182 in game: under the Everon 1989 header (the pack leg of `tools\cli test`), the pack's stall overlay opens
//! the market every day from 16:00 to 22:00 and switches stalls ON (#3279: a rent held at a restart is refunded
//! into the bank by the "stall_rents" save section, so the fee is never eaten).
[LF_TestInfo(LF_TestTier.LOGIC, pack: true)]
class LF_Test_EveronStalls : LF_Test
{
	static const ResourceName OVERLAY = "{5B2E8A41D7C93F06}Configs/Market/LF_Stalls_Everon1989.conf";

	override string GetName()
	{
		return "everon/stalls";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_MissionHeader header = LF_MissionHeader.GetCurrent();
		ctx.NotNull(header, "the pack leg runs under an LF mission header");
		if (header)
			ctx.True(header.m_aLF_ConfigOverlays && header.m_aLF_ConfigOverlays.Find(OVERLAY) != -1, "the header lists the Everon stall overlay");

		if (RedProof())
		{
			LF_MissionConfigPackSource none = new LF_MissionConfigPackSource();
			none.SetHeader(new LF_MissionHeader());
			LF_ConfigService.SetPackSource(none);
		}

		LF_StallConfig.Reload();
		LF_StallConfig config = LF_StallConfig.Get();
		if (RedProof())
		{
			LF_ConfigService.Reset();
			LF_StallConfig.Reload();
		}

		ctx.NotNull(config, "the stall config loads under the pack header");
		if (!config)
			return;

		ctx.Equal(0, config.m_iMarketDay, "the flagship market runs every day");
		ctx.True(LF_StallRules.IsMarketOpen(config, 100, 19), "the flagship is open at 19:00");
		ctx.True(LF_StallRules.IsMarketOpen(config, 103, 19), "the flagship is open at 19:00 on any day");
		ctx.False(LF_StallRules.IsMarketOpen(config, 100, 23), "the flagship is closed at 23:00");
		ctx.False(LF_StallRules.IsMarketOpen(config, 100, 12), "the flagship is closed at noon");
		ctx.True(config.m_bEnabled, "stalls are on for the flagship now that a held rent is refunded at a restart");
	}
}
