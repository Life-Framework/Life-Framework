// red-proof: Observed 2026-11-09 with `tools\cli test --no-build --only LF_Test_ZoneRules`: asserting the lawless zone reads "safe" instead of "lawless" failed (`the lawless zone stamps lawless`); restored. Also 2026-11-12: asserting the table fine multiplier is 2.0 instead of 2.5 failed (`a lawless entry scales fines`); restored.

// tier: LOGIC
//! zones/rules: rule lookup by shape (circle, box turned by yaw, polyline corridor), strictest rule
//! wins on overlap, a zone change reaches the bus, the incident log stamps the zone, and lawless
//! keeps wanted unless config says so.
class LF_Test_ZoneRules : LF_Test
{
	override string GetName()
	{
		return "zones/rules";
	}

	override void Run(LF_TestContext ctx)
	{
		array<ref LF_IncidentRecord> before = LF_IncidentLog.ExportAll();

		// Circle, flat distance.
		ctx.True(LF_Zones.RegisterCircle("zr-circle", LF_ZoneRule.LAWLESS, "5000 0 5000", 10), "a circle zone registers");
		ctx.True(LF_Zones.RuleAt("5006 40 5006") == LF_ZoneRule.LAWLESS, "a point 8.5 m out, 40 m up, is inside the circle");
		ctx.True(LF_Zones.RuleAt("5012 0 5000") == LF_ZoneRule.NORMAL, "12 m out is normal");
		ctx.EqualStr("lawless", LF_Zones.RuleName(LF_Zones.RuleAt("5001 0 5001")), "the lawless zone stamps lawless");

		// Box turned 90 degrees: long axis (x) now runs along world z.
		LF_ZoneArea box = new LF_ZoneArea();
		box.m_sName = "zr-box";
		box.m_eRule = LF_ZoneRule.SAFE;
		box.m_eShape = LF_ZoneShape.BOX;
		box.m_vCenter = "6000 0 6000";
		box.m_fYaw = 90;
		box.m_vHalfExtents = "20 0 5";
		ctx.True(LF_Zones.Register(box), "a box zone registers");
		ctx.True(LF_Zones.IsSafe("6000 0 6018"), "18 m along the turned long axis is inside");
		ctx.False(LF_Zones.IsSafe("6018 0 6000"), "18 m across the turned short axis is outside");

		// Polyline corridor.
		LF_ZoneArea line = new LF_ZoneArea();
		line.m_sName = "zr-line";
		line.m_eRule = LF_ZoneRule.LAWLESS;
		line.m_eShape = LF_ZoneShape.POLYLINE;
		line.m_fRadius = 3;
		line.m_aPoints.Insert("7000 0 7000");
		line.m_aPoints.Insert("7100 0 7000");
		line.m_aPoints.Insert("7100 0 7100");
		ctx.True(LF_Zones.Register(line), "a polyline zone registers");
		ctx.True(LF_Zones.RuleAt("7050 0 7002") == LF_ZoneRule.LAWLESS, "beside the first leg is inside the corridor");
		ctx.True(LF_Zones.RuleAt("7102 0 7060") == LF_ZoneRule.LAWLESS, "beside the second leg is inside");
		ctx.True(LF_Zones.RuleAt("7050 0 7050") == LF_ZoneRule.NORMAL, "the middle of the bend is outside");

		// Strictest wins on overlap.
		LF_Zones.RegisterCircle("zr-overlap-safe", LF_ZoneRule.SAFE, "5004 0 5000", 2);
		ctx.True(LF_Zones.RuleAt("5004 0 5000") == LF_ZoneRule.SAFE, "safe beats lawless where they overlap");

		// Bad shapes are refused.
		ctx.False(LF_Zones.RegisterCircle("zr-bad", LF_ZoneRule.SAFE, "0 0 0", 0), "a zero radius is refused");
		LF_ZoneArea empty = new LF_ZoneArea();
		empty.m_sName = "zr-empty";
		empty.m_eShape = LF_ZoneShape.POLYLINE;
		empty.m_fRadius = 2;
		ctx.False(LF_Zones.Register(empty), "a polyline with no points is refused");

		// The incident log stamps the zone, and the stamp survives Copy.
		LF_IncidentLog.ResetStatics();
		LF_IncidentRecord record = LF_IncidentLog.Append(LF_IncidentConfig.KIND_DEATH, 1000, "zr-victim", "zr-killer", "5001 0 5001", 0, "rifle", false);
		if (record)
		{
			ctx.EqualStr("lawless", record.m_sZone, "a death in the lawless zone is stamped lawless");
			ctx.EqualStr("lawless", record.Copy().m_sZone, "the stamp survives Copy");
		}
		record = LF_IncidentLog.Append(LF_IncidentConfig.KIND_DEATH, 1001, "zr-victim", "zr-killer", "9000 0 9000", 0, "rifle", false);
		if (record)
			ctx.EqualStr("normal", record.m_sZone, "a death elsewhere is stamped normal");

		// Wanted stays on in lawless unless config says so (the shipped config says no).
		ctx.False(LF_Zones.WantedSuppressedAt("5001 0 5001"), "lawless does not switch off wanted by default");

		// The per-rule table (#755): an entry changes what its rule does, one answer for every caller.
		LF_ZoneConfig table = new LF_ZoneConfig();
		table.m_aRuleSets = {};
		LF_ZoneRuleSet lawlessSet = new LF_ZoneRuleSet();
		lawlessSet.m_eRule = LF_ZoneRule.LAWLESS;
		lawlessSet.m_bSuppressesWanted = true;
		lawlessSet.m_bAllowsRobbery = false;
		lawlessSet.m_fFineMultiplier = 2.5;
		lawlessSet.m_bCopsEnter = false;
		table.m_aRuleSets.Insert(lawlessSet);
		LF_ZoneRuleSet resolved = LF_ZoneConfig.Resolve(table, LF_ZoneRule.LAWLESS);
		ctx.True(resolved.m_bSuppressesWanted, "a lawless entry can suppress wanted");
		ctx.False(resolved.m_bAllowsRobbery, "a lawless entry can forbid robbery");
		ctx.True(resolved.m_fFineMultiplier == 2.5, "a lawless entry scales fines");
		ctx.False(resolved.m_bCopsEnter, "a lawless entry can keep cops out");
		ctx.True(LF_ZoneConfig.Resolve(table, LF_ZoneRule.SAFE).m_bBlocksPlayerDamage, "a rule with no entry keeps its default");
		ctx.True(LF_ZoneRule.NORMAL == 0 && LF_ZoneRule.SAFE == 1 && LF_ZoneRule.LAWLESS == 2, "rule ids are stable");

		// A zone change reaches the bus.
		LF_ZoneChangeProbe probe = new LF_ZoneChangeProbe();
		LF_EventBus.Subscribe(LF_OnZoneChanged, probe);
		LF_Zones.RegisterCircle("zr-probe", LF_ZoneRule.SAFE, "8000 0 8000", 4);
		LF_Zones.Unregister("zr-probe");
		LF_EventBus.Unsubscribe(LF_OnZoneChanged, probe);
		if (Replication.IsServer())
		{
			ctx.Equal(2, probe.m_iSeen, "adding and removing a zone published two events");
			ctx.True(probe.m_bRemoved, "the last event was the removal");
			ctx.EqualStr("safe", probe.m_sRule, "the event names the rule set");
		}

		LF_Zones.Unregister("zr-circle");
		LF_Zones.Unregister("zr-box");
		LF_Zones.Unregister("zr-line");
		LF_Zones.Unregister("zr-overlap-safe");
		ctx.True(LF_Zones.RuleAt("5001 0 5001") == LF_ZoneRule.NORMAL, "removed zones rule nothing");
		LF_IncidentLog.ResetStatics();
		LF_IncidentLog.ApplyAll(before);
	}
}

//------------------------------------------------------------------------------------------------
class LF_ZoneChangeProbe : LF_EventSubscriber
{
	int m_iSeen;
	bool m_bRemoved;
	string m_sRule;

	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnZoneChanged changed = LF_OnZoneChanged.Cast(busEvent);
		if (!changed)
			return false;

		m_iSeen++;
		m_bRemoved = changed.m_bRemoved;
		m_sRule = changed.m_sRule;
		return true;
	}
}
