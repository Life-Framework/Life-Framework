// red-proof: RED-OK residents/resident-memory (fails with the hook on: nothing is applied)
//! Resident memory as pure rules (#2859): help raises, crime lowers, clamp, tiers, cooldown, gossip one hop.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_ResidentMemory : LF_Test
{
	override string GetName()
	{
		return "residents/resident-memory";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_ResidentMemory.ResetStatics();
		LF_ResidentMemoryConfig config = new LF_ResidentMemoryConfig();
		config.m_iCooldownSeconds = 600;
		config.m_iGossipPercent = 50;
		config.m_iRefusesBelow = -60;
		config.m_iWaryBelow = -20;
		config.m_iFriendlyFrom = 20;
		config.m_iRegularFrom = 60;
		config.m_aReasons = {};
		config.m_aReasons.Insert(LF_ResidentMemoryReason.Make("helped", 10, false));
		config.m_aReasons.Insert(LF_ResidentMemoryReason.Make("crime_seen", -30, true));
		config.m_aKnows = {};
		config.m_aKnows.Insert(LF_ResidentKnowsDef.Make("t_a", {"t_b", "t_c"}));
		config.m_aKnows.Insert(LF_ResidentKnowsDef.Make("t_b", {"t_d"}));
		LF_ResidentMemoryConfig.UseForTest(config);

		bool applied;
		// Red-proof hook: with --red nothing is applied, so every assertion on a change must fail.
		if (!RedProof())
			applied = LF_ResidentMemory.Apply("t_a", "u1", "helped", 1000);

		ctx.True(applied, "helped applies");
		ctx.True(LF_ResidentMemory.Score("t_a", "u1") == 10, "helped raises the score by the award");
		ctx.True(LF_ResidentMemory.Tier("t_a", "u1") == "neutral", "10 reads neutral");

		ctx.True(!LF_ResidentMemory.Apply("t_a", "u1", "helped", 1599), "the same reason inside the cooldown is refused");
		ctx.True(LF_ResidentMemory.Score("t_a", "u1") == 10, "a cooled-down award changes nothing");
		ctx.True(LF_ResidentMemory.Apply("t_a", "u1", "helped", 1600), "after the cooldown it counts again");
		ctx.True(LF_ResidentMemory.Tier("t_a", "u1") == "friendly", "20 reads friendly");

		ctx.True(!LF_ResidentMemory.Apply("t_a", "u1", "bogus", 2000), "an unknown reason is refused");
		ctx.True(LF_ResidentMemory.Score("t_a", "u1") == 20, "a refused reason changes nothing");

		// Crime lowers and gossips one hop at half: -15 to B and C, nothing to D.
		ctx.True(LF_ResidentMemory.Apply("t_a", "u2", "crime_seen", 3000), "crime_seen applies");
		ctx.True(LF_ResidentMemory.Score("t_a", "u2") == -30, "crime lowers the score");
		ctx.True(LF_ResidentMemory.Tier("t_a", "u2") == "wary", "-30 reads wary");
		ctx.True(LF_ResidentMemory.Score("t_b", "u2") == -15 && LF_ResidentMemory.Score("t_c", "u2") == -15, "the residents A knows hear half");
		ctx.True(LF_ResidentMemory.Score("t_d", "u2") == 0, "gossip stops after one hop");
		ctx.True(LF_ResidentMemory.Score("t_b", "u1") == 0, "gossip is about that character only");

		// Rounding toward zero: half of -5 is -2.
		config.m_aReasons.Insert(LF_ResidentMemoryReason.Make("slight", -5, true));
		LF_ResidentMemory.Apply("t_a", "u3", "slight", 4000);
		ctx.True(LF_ResidentMemory.Score("t_b", "u3") == -2, "a shared -2.5 rounds toward zero");

		// Clamp and the top and bottom tiers.
		int now = 10000;
		for (int i = 0; i < 20; i++)
		{
			now += 700;
			LF_ResidentMemory.Apply("t_e", "u4", "helped", now);
			LF_ResidentMemory.Apply("t_e", "u5", "crime_seen", now);
		}
		ctx.True(LF_ResidentMemory.Score("t_e", "u4") == 100, "the score clamps at 100");
		ctx.True(LF_ResidentMemory.Score("t_e", "u5") == -100, "the score clamps at -100");
		ctx.True(LF_ResidentMemory.Tier("t_e", "u4") == "regular", "100 reads regular");
		ctx.True(LF_ResidentMemory.Tier("t_e", "u5") == "refuses", "-100 reads refuses");

		LF_ResidentMemory.ResetStatics();
		LF_ResidentMemoryConfig.ResetStatics();
	}
}
