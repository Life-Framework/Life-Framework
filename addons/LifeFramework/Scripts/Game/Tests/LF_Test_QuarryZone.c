// red-proof: asserted the quarry gate reads "normal" and ran `tools\cli test --no-build --only LF_Test_QuarryZone` (2026-11-09);
// it failed: the quarry gate is lawless (expected 'normal', got 'lawless'). Restored.
// red-proof (#1893 fixtures): 2026-11-16 on GridWorld, before the test registered its own quarry, `--only LF_Test_QuarryZone
// --world Worlds/GridWorld/GridWorld.ent` failed `- the world places a lawless zone that registered itself`. The RedProof()
// hook skips the fixture zone and the same assertions go red.

// tier: WORLD
//! The lawless zone rule on a fixture quarry the test registers and removes itself (GridWorld places no
//! town zones on purpose, docs/architecture/three-worlds.md), plus the annex lawless signs.
class LF_Test_QuarryZone : LF_Test
{
	//! Far from the grid annex (x 18..114, z 18..114) and its gate spawn.
	static const string QUARRY_NAME = "qz-fixture-quarry";
	static const float QUARRY_X = 400;
	static const float QUARRY_Z = 400;
	static const float QUARRY_RADIUS = 40;

	override string GetName()
	{
		return "zones/quarry";
	}

	override void Run(LF_TestContext ctx)
	{
		BaseWorld world = GetGame().GetWorld();
		ctx.True(world != null, "a world is loaded");
		if (!world)
			return;

		vector quarryCentre = Vector(QUARRY_X, 0, QUARRY_Z);

		// The fixture quarry: a lawless circle, read back by name so a world zone never stands in for it.
		if (!RedProof())
			ctx.True(LF_Zones.RegisterCircle(QUARRY_NAME, LF_ZoneRule.LAWLESS, quarryCentre, QUARRY_RADIUS), "the fixture quarry registers");

		LF_ZoneArea quarry = LF_Zones.Find(QUARRY_NAME);
		ctx.True(quarry != null, "the fixture lawless zone is registered");
		ctx.True(LF_Zones.FindFirstByRule(LF_ZoneRule.LAWLESS) != null, "a lawless zone is found by its rule");
		if (quarry)
		{
			vector centre = quarry.m_vCenter;
			ctx.EqualStr("lawless", LF_Zones.RuleName(quarry.m_eRule), "the quarry carries the lawless rule");
			ctx.EqualStr("lawless", LF_Zones.RuleName(LF_Zones.RuleAt(centre)), "the quarry centre reads lawless");
			ctx.True(quarry.Contains(centre + Vector(quarry.m_fRadius * 0.9, 5, 0)), "the zone edge, 5 m up, is inside");
			ctx.False(quarry.Contains(centre + Vector(quarry.m_fRadius + 10, 0, 0)), "10 m past the edge is outside the zone");
		}

		// A lawless circle beside a safe one: safe beats lawless, open ground between them is normal.
		ctx.True(LF_Zones.RegisterCircle("qz-lawless", LF_ZoneRule.LAWLESS, "8000 0 8000", 30), "a lawless fixture zone registers");
		ctx.True(LF_Zones.RegisterCircle("qz-safe", LF_ZoneRule.SAFE, "8040 0 8000", 10), "a safe fixture zone registers");
		ctx.EqualStr("lawless", LF_Zones.RuleName(LF_Zones.RuleAt("8000 0 8000")), "the fixture quarry is lawless");
		ctx.EqualStr("safe", LF_Zones.RuleName(LF_Zones.RuleAt("8040 0 8000")), "the fixture plaza is safe");
		ctx.EqualStr("normal", LF_Zones.RuleName(LF_Zones.RuleAt("8020 0 8100")), "open ground between them is normal");

		// Teardown: no fixture zone outlives the test.
		LF_Zones.Unregister("qz-lawless");
		LF_Zones.Unregister("qz-safe");
		LF_Zones.Unregister(QUARRY_NAME);
		ctx.True(LF_Zones.Find(QUARRY_NAME) == null, "the fixture quarry is removed");
		ctx.EqualStr("normal", LF_Zones.RuleName(LF_Zones.RuleAt(quarryCentre)), "the removed quarry leaves normal ground");

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
