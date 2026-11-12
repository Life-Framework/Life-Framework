// red-proof: asserted the quarry gate reads "normal" and ran `tools\cli test --no-build --only LF_Test_QuarryZone` (2026-11-09);
// it failed: the quarry gate is lawless (expected 'normal', got 'lawless'). Restored.

// tier: WORLD
class LF_Test_QuarryZone : LF_Test
{
	override string GetName()
	{
		return "zones/quarry";
	}

	override void Run(LF_TestContext ctx)
	{
		BaseWorld world = GetGame().GetWorld();
		ctx.True(world != null, "DebugWorld is loaded");
		if (!world)
			return;

		ctx.True(LF_Zones.Find("LF_LawlessZone_Quarry") != null, "the quarry zone registered itself");
		ctx.EqualStr("lawless", LF_Zones.RuleName(LF_Zones.RuleAt("178 0 172")), "the quarry gate is lawless");
		ctx.EqualStr("lawless", LF_Zones.RuleName(LF_Zones.RuleAt("178 5 200")), "the quarry edge is lawless");
		ctx.EqualStr("safe", LF_Zones.RuleName(LF_Zones.RuleAt("115 0 250")), "the plaza stays safe");
		ctx.EqualStr("normal", LF_Zones.RuleName(LF_Zones.RuleAt("179 0 244.3")), "the general store stays normal");
		ctx.EqualStr("normal", LF_Zones.RuleName(LF_Zones.RuleAt("229 0 244.3")), "the police station stays normal");

		ctx.True(CarriesSignId(world.FindEntityByName(LF_TestAnnex.SIGN_LAWLESS_ENTRY), "lawless_zone"), "the lane entry sign carries the lawless_zone sign id");
		ctx.True(CarriesSignId(world.FindEntityByName(LF_TestAnnex.SIGN_LAWLESS_PIT), "lawless_zone"), "the quarry sign carries the lawless_zone sign id");
	}

	//------------------------------------------------------------------------------------------------
	protected bool CarriesSignId(IEntity sign, string signId)
	{
		if (!sign)
			return false;

		LF_SignComponent component = LF_Component<LF_SignComponent>.Find(sign);
		if (!component)
			return false;

		return component.GetSignId() == signId;
	}
}
