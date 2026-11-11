// red-proof: Observed 2026-11-12 with `tools\cli test --no-build --only LF_Test_ZoneRuleSetBackcompat`: asserting the shipped SAFE rule does not block player damage failed (`shipped: safe blocks player damage`); restored.
// tier: LOGIC
//! zones/ruleset-backcompat (#755): a config with only the two old booleans resolves to the rule
//! sets today's zones had, and the shipped table matches that. A table entry overrides the flags.
class LF_Test_ZoneRuleSetBackcompat : LF_Test
{
	override string GetName()
	{
		return "zones/ruleset-backcompat";
	}

	override void Run(LF_TestContext ctx)
	{
		// No config at all (bad file): safe blocks, lawless keeps wanted.
		ctx.True(LF_ZoneConfig.Resolve(null, LF_ZoneRule.SAFE).m_bBlocksPlayerDamage, "no config: safe blocks player damage");
		ctx.False(LF_ZoneConfig.Resolve(null, LF_ZoneRule.LAWLESS).m_bSuppressesWanted, "no config: lawless keeps wanted");

		// Old shape: flags only, no table.
		LF_ZoneConfig legacy = new LF_ZoneConfig();
		legacy.m_bSafeBlocksPlayerDamage = true;
		legacy.m_bLawlessSuppressesWanted = false;
		LF_ZoneRuleSet safe = LF_ZoneConfig.Resolve(legacy, LF_ZoneRule.SAFE);
		ctx.True(safe.m_bBlocksPlayerDamage, "legacy flags: safe blocks player damage");
		ctx.False(safe.m_bSuppressesWanted, "legacy flags: safe does not touch wanted");
		ctx.False(LF_ZoneConfig.Resolve(legacy, LF_ZoneRule.LAWLESS).m_bSuppressesWanted, "legacy flags: lawless keeps wanted");
		ctx.False(LF_ZoneConfig.Resolve(legacy, LF_ZoneRule.LAWLESS).m_bBlocksPlayerDamage, "legacy flags: lawless does not block damage");
		LF_ZoneRuleSet normal = LF_ZoneConfig.Resolve(legacy, LF_ZoneRule.NORMAL);
		ctx.False(normal.m_bBlocksPlayerDamage, "legacy flags: normal blocks nothing");
		ctx.False(normal.m_bSuppressesWanted, "legacy flags: normal suppresses nothing");

		// New fields default to "nothing changes".
		ctx.True(safe.m_bAllowsRobbery && safe.m_bAllowsVehicleTheft && safe.m_bAllowsWeaponDraw && safe.m_bCopsEnter, "legacy flags: the new permissions stay on");
		ctx.True(safe.m_fFineMultiplier == 1, "legacy flags: fines are not scaled");

		// The legacy flags still work when set the other way.
		legacy.m_bSafeBlocksPlayerDamage = false;
		legacy.m_bLawlessSuppressesWanted = true;
		ctx.False(LF_ZoneConfig.Resolve(legacy, LF_ZoneRule.SAFE).m_bBlocksPlayerDamage, "legacy flags: safe can be switched off");
		ctx.True(LF_ZoneConfig.Resolve(legacy, LF_ZoneRule.LAWLESS).m_bSuppressesWanted, "legacy flags: lawless can suppress wanted");

		// A table entry overrides the flags for its own rule only.
		LF_ZoneRuleSet entry = new LF_ZoneRuleSet();
		entry.m_eRule = LF_ZoneRule.LAWLESS;
		entry.m_bSuppressesWanted = false;
		legacy.m_aRuleSets = {};
		legacy.m_aRuleSets.Insert(entry);
		ctx.False(LF_ZoneConfig.Resolve(legacy, LF_ZoneRule.LAWLESS).m_bSuppressesWanted, "a lawless table entry beats the legacy flag");
		ctx.False(LF_ZoneConfig.Resolve(legacy, LF_ZoneRule.SAFE).m_bBlocksPlayerDamage, "a rule without an entry still reads its legacy flag");

		// The shipped config: today's behaviour, through the table.
		LF_ZoneConfig shipped = LF_ZoneConfig.Get();
		ctx.True(shipped != null, "the shipped zone config loads");
		ctx.True(LF_ZoneConfig.RuleSetFor(LF_ZoneRule.SAFE).m_bBlocksPlayerDamage, "shipped: safe blocks player damage");
		ctx.False(LF_ZoneConfig.RuleSetFor(LF_ZoneRule.LAWLESS).m_bSuppressesWanted, "shipped: lawless keeps wanted");
		ctx.False(LF_ZoneConfig.RuleSetFor(LF_ZoneRule.LAWLESS).m_bBlocksPlayerDamage, "shipped: lawless blocks nothing");
		ctx.False(LF_ZoneConfig.RuleSetFor(LF_ZoneRule.NORMAL).m_bBlocksPlayerDamage, "shipped: normal blocks nothing");

		// And through the service, at a position.
		LF_Zones.RegisterCircle("zb-safe", LF_ZoneRule.SAFE, "5100 0 5100", 5);
		LF_Zones.RegisterCircle("zb-lawless", LF_ZoneRule.LAWLESS, "5200 0 5200", 5);
		ctx.True(LF_Zones.BlocksPlayerDamageAt("5101 0 5100"), "a safe zone blocks damage at its position");
		ctx.False(LF_Zones.BlocksPlayerDamageAt("5201 0 5200"), "a lawless zone does not block damage");
		ctx.False(LF_Zones.WantedSuppressedAt("5201 0 5200"), "a lawless zone keeps wanted by default");
		ctx.False(LF_Zones.BlocksPlayerDamageAt("9100 0 9100"), "open ground blocks nothing");
		ctx.True(LF_Zones.IsLawless("5201 0 5200"), "IsLawless reads the rule");
		ctx.False(LF_Zones.IsLawless("5101 0 5100"), "a safe zone is not lawless");
		LF_Zones.Unregister("zb-safe");
		LF_Zones.Unregister("zb-lawless");
	}
}
