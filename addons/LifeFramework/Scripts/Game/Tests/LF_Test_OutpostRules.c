// red-proof: `tools\cli test --no-build --red LF_Test_OutpostRules`; RedProof() skips the outpost zone registration,
// so the SAFE, inside-the-zone and refusal assertions go red.

//! outposts/rules (#4217, N28): the outpost registers a SAFE zone, a trader inside it refuses an on-duty
//! officer in words, and off-duty police, civilians and traders outside an outpost are unaffected.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_OutpostRules : LF_Test
{
	static const string OUTPOST_ID = "test-outpost";
	static const string ZONE_NAME = "outpost:test-outpost";

	override string GetName()
	{
		return "outposts/rules";
	}

	override void Run(LF_TestContext ctx)
	{
		vector centre = Vector(7000, 0, 7000);

		// The pure rule: only an on-duty officer, in an outpost, with the rule on, is refused.
		ctx.True(LF_Outpost.ShouldRefuseService(true, true, true), "a trader in an outpost refuses an on-duty officer");
		ctx.False(LF_Outpost.ShouldRefuseService(false, true, true), "off-duty police are served in an outpost");
		ctx.False(LF_Outpost.ShouldRefuseService(true, false, true), "an on-duty officer is served by a trader outside an outpost");
		ctx.False(LF_Outpost.ShouldRefuseService(true, true, false), "the conf can turn the refusal off");

		// The zone: SAFE inside the circle, nothing outside it.
		if (!RedProof())
			ctx.True(LF_Zones.RegisterCircle(ZONE_NAME, LF_ZoneRule.SAFE, centre, 40), "the outpost zone registers");

		ctx.True(LF_Zones.IsSafe(centre + "10 0 10"), "the SAFE rule applies inside the outpost");
		ctx.True(LF_Zones.RuleAt(centre) == LF_ZoneRule.SAFE, "the outpost reads SAFE");
		ctx.True(LF_Outpost.IsInsideOutpost(centre + "10 0 10"), "a point inside is inside the outpost");
		ctx.False(LF_Outpost.IsInsideOutpost(centre + "100 0 0"), "a point 100 m out is outside the outpost");

		// A SAFE zone that is not an outpost does not make a trader refuse police.
		LF_Zones.RegisterCircle("outpost-test-plaza", LF_ZoneRule.SAFE, "8000 0 8000", 40);
		ctx.True(LF_Zones.IsSafe("8000 0 8000"), "fixture: a plain SAFE zone is safe");
		ctx.False(LF_Outpost.IsInsideOutpost("8000 0 8000"), "a plain SAFE zone is not an outpost");
		LF_Zones.Unregister("outpost-test-plaza");

		// Police may enter: the SAFE rule has no police exemption to remove, and the rule set lets cops in.
		LF_ZoneRuleSet safeRules = LF_ZoneConfig.RuleSetFor(LF_ZoneRule.SAFE);
		ctx.True(safeRules != null && safeRules.m_bCopsEnter, "police may enter a SAFE zone");

		// The refusal is words: both toast keys are present and distinct.
		ctx.True(LF_Outpost.REFUSE_KEY.StartsWith("#"), "the refusal is a localization key");
		ctx.True(LF_Outpost.REFUSE_KEY != LF_Outpost.REFUSE_TITLE_KEY, "the refusal has its own title key");

		// Defaults: a 60 m circle, the refusal on.
		LF_OutpostConfig config = LF_OutpostConfig.CreateDefault();
		ctx.True(config.m_fSafeRadius >= 10, "the outpost radius default is a real circle");
		ctx.True(config.m_bTradersRefuseOnDutyPolice, "the refusal is on by default");

		LF_Zones.Unregister(ZONE_NAME);
		ctx.False(LF_Outpost.IsInsideOutpost(centre), "a removed outpost protects nothing");
	}
}
