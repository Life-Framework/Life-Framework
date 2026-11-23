// red-proof: 2026-11-23, `--red LF_Test_GatherWear` skips the wear step, so the tools stay at 100 and the warning checks fail. Green with `--only`.
//! tool/gather-wear (#4300): each mining swing, tree felled and fish caught lowers that tool's
//! condition by the tier's configured amount, only that kind of tool wears, and a tool dropping into
//! a worse band (or worn out) says so in words.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_GatherWear : LF_Test
{
	override string GetName()
	{
		return "tool/gather-wear";
	}

	override void Run(LF_TestContext ctx)
	{
		// The tier table is configured: the shipped file loads and a changed row changes the rate.
		LF_ToolTierConfig shipped = LF_ToolTierConfig.Load(LF_ToolTierConfig.CONFIG_PATH);
		ctx.True(shipped != null, "the tool tier config loads");
		if (!shipped)
			return;

		ctx.EqualFloat(2.0, shipped.WearOf(LF_ToolCondition.TIER_WORN), 0.001, "a worn tool loses 2 per use");
		ctx.EqualFloat(1.0, shipped.WearOf(LF_ToolCondition.TIER_STANDARD), 0.001, "a standard tool loses 1 per use");
		ctx.EqualFloat(0.5, shipped.WearOf(LF_ToolCondition.TIER_FINE), 0.001, "a fine tool loses 0.5 per use");
		ctx.EqualFloat(shipped.WearOf(LF_ToolCondition.TIER_FINE), LF_ToolCondition.WearPerUse(LF_ToolCondition.TIER_FINE), 0.001, "the tool rule reads the config");
		shipped.m_fStandardWear = 4;
		ctx.EqualFloat(4.0, shipped.WearOf(LF_ToolCondition.TIER_STANDARD), 0.001, "a changed config row changes the wear");
		shipped.m_fStandardWear = 0;
		ctx.EqualFloat(LF_ToolTierConfig.DEFAULT_STANDARD, shipped.WearOf(LF_ToolCondition.TIER_STANDARD), 0.001, "a zero row falls back, a tool is never immortal");

		// Gather families and tools: mining wears the pickaxe, logging the axe, foraging nothing.
		ctx.EqualStr(LF_ToolBench.KIND_PICKAXE, LF_ToolWear.KindOfFamily(LF_GatherTools.FAMILY_MINING), "mining wears the pickaxe");
		ctx.EqualStr(LF_ToolBench.KIND_AXE, LF_ToolWear.KindOfFamily("LOGGING"), "logging wears the axe");
		ctx.EqualStr("", LF_ToolWear.KindOfFamily(LF_GatherTools.FAMILY_FORAGING), "foraging wears no tool");

		// The warning: silent while the band holds, said when the tool drops into a worse band, and after
		// every use once it is worn out.
		ctx.EqualStr("", LF_ToolWear.WarnKey(100, 99), "a good tool says nothing");
		ctx.EqualStr("#LF-ToolCondition_Worn", LF_ToolWear.WarnKey(60, 59), "dropping under 60 says the tool is getting worn");
		ctx.EqualStr("", LF_ToolWear.WarnKey(59, 58), "staying in the band says nothing");
		ctx.EqualStr("#LF-ToolCondition_Poor", LF_ToolWear.WarnKey(25, 24), "dropping under 25 says the tool is badly worn");
		ctx.EqualStr("#LF-ToolCondition_WornOut", LF_ToolWear.WarnKey(1, 0), "reaching zero says the tool is worn out");
		ctx.EqualStr("#LF-ToolCondition_WornOut", LF_ToolWear.WarnKey(0, 0), "a worn-out tool says so after every use");

		IEntity character = LF_TestCharacterFixture.Spawn(ctx);
		if (!character)
			return;

		// A player's tools: a swing wears the pickaxe, a fell the axe, a catch the rod, each on its own.
		LF_ToolBench.ResetStatics();
		LF_Utils.SetUidForTests(character, "uid-gather-wear");
		float rate = LF_ToolCondition.WearPerUse(LF_ToolCondition.TIER_STANDARD);
		for (int i = 0; i < 3; i++)
		{
			if (!RedProof())
				LF_ToolWear.OnUse(character, LF_ToolBench.KIND_PICKAXE);
		}
		if (!RedProof())
		{
			LF_ToolWear.OnUse(character, LF_ToolBench.KIND_AXE);
			LF_ToolWear.OnUse(character, LF_ToolBench.KIND_ROD);
		}
		ctx.EqualFloat(100.0 - 3 * rate, LF_ToolBench.ToolOf("uid-gather-wear", LF_ToolBench.KIND_PICKAXE).m_fCondition, 0.001, "three swings cost the pickaxe three uses");
		ctx.EqualFloat(100.0 - rate, LF_ToolBench.ToolOf("uid-gather-wear", LF_ToolBench.KIND_AXE).m_fCondition, 0.001, "one fell costs the axe one use");
		ctx.EqualFloat(100.0 - rate, LF_ToolBench.ToolOf("uid-gather-wear", LF_ToolBench.KIND_ROD).m_fCondition, 0.001, "one catch costs the rod one use");

		// A worn-out tool still works: the use goes through and the condition stays at zero.
		LF_ToolCondition axe = LF_ToolBench.ToolOf("uid-gather-wear", LF_ToolBench.KIND_AXE);
		axe.m_fCondition = 0;
		LF_ToolWear.OnUse(character, LF_ToolBench.KIND_AXE);
		ctx.EqualFloat(0, axe.m_fCondition, 0.001, "a worn-out axe stays at zero");
		ctx.True(axe.Speed() > 0, "a worn-out axe still works");

		// No player identity (an empty uid), or a family with no tool: nothing wears, nothing crashes.
		LF_Utils.SetUidForTests(null, "");
		LF_ToolWear.OnUse(character, LF_ToolBench.KIND_PICKAXE);
		LF_ToolWear.OnGather(character, LF_GatherTools.FAMILY_FORAGING);
		ctx.EqualFloat(100.0 - 3 * rate, LF_ToolBench.ToolOf("uid-gather-wear", LF_ToolBench.KIND_PICKAXE).m_fCondition, 0.001, "a user without identity wears no player's tool");

		LF_ToolBench.ResetStatics();
		LF_TestCharacterFixture.Delete(character);
	}
}
