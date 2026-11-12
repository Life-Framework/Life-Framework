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

		// The placed lawless zone, whichever world places it: read its centre and edge, no town coordinates.
		LF_ZoneArea quarry = LF_Zones.FindFirstByRule(LF_ZoneRule.LAWLESS);
		ctx.True(quarry != null, "the world places a lawless zone that registered itself");
		if (quarry)
		{
			vector centre = quarry.m_vCenter;
			ctx.EqualStr("lawless", LF_Zones.RuleName(quarry.m_eRule), "the placed zone carries the lawless rule");
			ctx.True(quarry.m_fRadius > 0, "the placed lawless zone has a radius");
			ctx.True(quarry.Contains(centre), "the lawless zone contains its centre");
			ctx.True(quarry.Contains(centre + Vector(quarry.m_fRadius * 0.9, 5, 0)), "the zone edge, 5 m up, is inside");
			ctx.False(quarry.Contains(centre + Vector(quarry.m_fRadius + 10, 0, 0)), "10 m past the edge is outside the zone");
		}

		// A lawless circle beside a safe one: the spawned fixture rule order, safe beats lawless.
		ctx.True(LF_Zones.RegisterCircle("qz-lawless", LF_ZoneRule.LAWLESS, "8000 0 8000", 30), "a lawless fixture zone registers");
		ctx.True(LF_Zones.RegisterCircle("qz-safe", LF_ZoneRule.SAFE, "8040 0 8000", 10), "a safe fixture zone registers");
		ctx.EqualStr("lawless", LF_Zones.RuleName(LF_Zones.RuleAt("8000 0 8000")), "the fixture quarry is lawless");
		ctx.EqualStr("safe", LF_Zones.RuleName(LF_Zones.RuleAt("8040 0 8000")), "the fixture plaza is safe");
		ctx.EqualStr("normal", LF_Zones.RuleName(LF_Zones.RuleAt("8020 0 8100")), "open ground between them is normal");
		LF_Zones.Unregister("qz-lawless");
		LF_Zones.Unregister("qz-safe");

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
