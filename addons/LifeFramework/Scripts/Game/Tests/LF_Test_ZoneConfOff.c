// red-proof: observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_ZoneConfOff`: with
// `bool m_bAllowsRobbery = true;` put back on LF_ZoneRuleSet, the conf's `m_bAllowsRobbery 0` read as on
// and `- conf 0: robbery is forbidden` failed (only that field; the others had no initializer). Removed; green.
//------------------------------------------------------------------------------------------------
//! zones/conf-off (#1077): a `.conf` that sets a zone rule set's permissions to 0 reads as 0. With
//! member initializers on LF_ZoneRuleSet (`m_bAllowsRobbery = true`) the initializer beat the
//! `.conf` value, so a pack overlay could never turn robbery off in a zone.
// tier: WORLD
class LF_Test_ZoneConfOff : LF_Test
{
	static const ResourceName CONF = "{CBBF8837BF96F37D}Configs/Tests/LF_Test_ZoneRulesOff.conf";

	override string GetName()
	{
		return "zones/conf-off";
	}

	override void Run(LF_TestContext ctx)
	{
		Resource container = BaseContainerTools.LoadContainer(CONF);
		ctx.True(container && container.IsValid() && container.GetResource(), "the test zone conf loads");
		if (!container || !container.IsValid() || !container.GetResource())
			return;

		LF_ZoneConfig config = LF_ZoneConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		ctx.True(config != null, "the test zone conf is an LF_ZoneConfig");
		if (!config)
			return;

		ctx.False(config.m_bSafeBlocksPlayerDamage, "conf 0: the legacy SAFE flag reads 0");
		LF_ZoneRuleSet lawless = config.FindRuleSet(LF_ZoneRule.LAWLESS);
		ctx.True(lawless != null, "conf: the LAWLESS entry is read");
		if (!lawless)
			return;

		ctx.False(lawless.m_bAllowsRobbery, "conf 0: robbery is forbidden");
		ctx.False(lawless.m_bAllowsVehicleTheft, "conf 0: vehicle theft is forbidden");
		ctx.False(lawless.m_bAllowsWeaponDraw, "conf 0: weapon draw is forbidden");
		ctx.True(lawless.m_fFineMultiplier == 0, "conf 0: the fine multiplier reads 0");
		ctx.False(lawless.m_bCopsEnter, "conf 0: cops stay out");

		// What the .conf leaves out keeps the attribute default (shipped LF_Zones.conf relies on it).
		LF_ZoneConfig shipped = LF_ZoneConfig.Get();
		ctx.True(shipped != null, "the shipped zone config loads");
		if (!shipped)
			return;

		LF_ZoneRuleSet normal = shipped.FindRuleSet(LF_ZoneRule.NORMAL);
		ctx.True(normal && normal.m_bAllowsRobbery && normal.m_bAllowsVehicleTheft && normal.m_bAllowsWeaponDraw && normal.m_bCopsEnter, "shipped: an unset permission keeps its attribute default (on)");
		ctx.True(normal && normal.m_fFineMultiplier == 1, "shipped: an unset fine multiplier keeps its attribute default (1)");
	}
}
