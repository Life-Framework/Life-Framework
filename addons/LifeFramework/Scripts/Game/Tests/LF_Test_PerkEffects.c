// red-proof: written before LF_PerkEffects existed (the compile failed, 2026-11-23); `--red LF_Test_PerkEffects` skips the three perk picks, so the double-yield, swift-hands and light-touch checks fail. Green with `--only`.
//! perks/effects (WK-W11, #4408): a picked trade perk changes the work. `double_yield` turns a forced
//! winning roll into two units (one without the perk, one on a losing roll, one in another trade),
//! `swift_hands` shortens the gather hold by the configured share, `light_touch` cuts the wear a use
//! puts on the tool in LF_ToolWear. Effects read the perk store (LF_TradePerks.Has) on the server.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_PerkEffects : LF_Test
{
	static const string UID = "test-perk-effects";
	static const string TRADE = "MINER";
	static const string OTHER_TRADE = "FARMER";

	override string GetName()
	{
		return "perks/effects";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_TradePerks.Forget(UID);

		// Without a perk: a roll that would win changes nothing, the hold and the wear are the base.
		ctx.Equal(1, LF_PerkEffects.Yield(UID, TRADE, 1, 0.0), "a miner with no perk gets one unit even on a forced winning roll");
		ctx.EqualFloat(10.0, LF_PerkEffects.HoldSeconds(UID, TRADE, 10.0), 0.001, "no perk: the hold keeps its base time");
		ctx.EqualFloat(1.0, LF_PerkEffects.WearScale(UID, TRADE), 0.001, "no perk: a use wears the tool in full");

		if (!RedProof())
		{
			ctx.True(LF_TradePerks.Pick(UID, TRADE, 10, LF_PerkEffects.PERK_DOUBLE_YIELD), "the miner picks double yield at 10");
			ctx.True(LF_TradePerks.Pick(UID, TRADE, 20, LF_PerkEffects.PERK_SWIFT_HANDS), "the miner picks swift hands at 20");
			ctx.True(LF_TradePerks.Pick("test-perk-light", TRADE, 10, LF_PerkEffects.PERK_LIGHT_TOUCH), "a second miner picks light touch at 10");
		}

		// Double yield: a roll under the chance doubles, a roll over it does not, another trade is untouched.
		ctx.Equal(2, LF_PerkEffects.Yield(UID, TRADE, 1, 0.0), "double yield: a forced winning roll gives two units");
		ctx.Equal(4, LF_PerkEffects.Yield(UID, TRADE, 2, 0.0), "double yield doubles the node's own amount");
		ctx.Equal(1, LF_PerkEffects.Yield(UID, TRADE, 1, 0.999), "double yield: a losing roll gives one unit");
		ctx.Equal(1, LF_PerkEffects.Yield(UID, OTHER_TRADE, 1, 0.0), "a mining perk does nothing for a farmer's yield");
		ctx.Equal(1, LF_PerkEffects.Yield("", TRADE, 1, 0.0), "no character, no perk");
		ctx.True(LF_PerkEffects.DoubleChance(UID, TRADE) <= 0.20, "the double-yield chance stays inside the 20% cap");

		// Swift hands: the hold is shorter by the configured share, and as a speed factor it says the same.
		float share = LF_PerkEffects.SWIFT_SHARE;
		ctx.EqualFloat(10.0 * (1.0 - share), LF_PerkEffects.HoldSeconds(UID, TRADE, 10.0), 0.001, "swift hands shortens the hold by the configured share");
		ctx.EqualFloat(10.0 * (1.0 - share), LF_GatherTools.Duration(10.0, LF_PerkEffects.SpeedFactor(UID, TRADE)), 0.001, "the speed factor gives the same hold");
		ctx.EqualFloat(1.0, LF_PerkEffects.SpeedFactor(UID, OTHER_TRADE), 0.001, "a mining perk does not speed a farmer");
		ctx.True(share <= 0.20, "the swing speed share stays inside the 20% cap");

		// Light touch: the same use costs the tool less, in LF_ToolWear.
		LF_ToolCondition plain = LF_ToolCondition.Create(LF_ToolCondition.TIER_STANDARD);
		LF_ToolCondition light = LF_ToolCondition.Create(LF_ToolCondition.TIER_STANDARD);
		float rate = LF_ToolCondition.WearPerUse(LF_ToolCondition.TIER_STANDARD);
		LF_ToolWear.WearScaled(plain, LF_PerkEffects.WearScale("test-perk-nothing", TRADE));
		LF_ToolWear.WearScaled(light, LF_PerkEffects.WearScale("test-perk-light", TRADE));
		ctx.EqualFloat(100.0 - rate, plain.m_fCondition, 0.001, "without the perk a use costs the tier's rate");
		ctx.EqualFloat(100.0 - rate * (1.0 - LF_PerkEffects.LIGHT_SHARE), light.m_fCondition, 0.001, "light touch lowers the wear per use");
		ctx.True(light.m_fCondition > plain.m_fCondition, "the light-touch tool keeps more condition");
		ctx.True(LF_PerkEffects.LIGHT_SHARE <= 0.40, "the wear cut stays inside the 40% cap");
		LF_ToolCondition unscaled = LF_ToolCondition.Create(LF_ToolCondition.TIER_STANDARD);
		LF_ToolWear.Wear(unscaled);
		ctx.EqualFloat(plain.m_fCondition, unscaled.m_fCondition, 0.001, "Wear keeps the full rate");

		// The tool kind names its trade, so a perk reaches the fell and the fishing hooks too.
		ctx.EqualStr("MINER", LF_ToolWear.TradeOfKind(LF_ToolBench.KIND_PICKAXE), "the pickaxe belongs to mining");
		ctx.EqualStr("LUMBERJACK", LF_ToolWear.TradeOfKind(LF_ToolBench.KIND_AXE), "the axe belongs to logging");
		ctx.EqualStr("FISHER", LF_ToolWear.TradeOfKind(LF_ToolBench.KIND_ROD), "the rod belongs to fishing");

		LF_TradePerks.Forget(UID);
		LF_TradePerks.Forget("test-perk-light");
	}
}
