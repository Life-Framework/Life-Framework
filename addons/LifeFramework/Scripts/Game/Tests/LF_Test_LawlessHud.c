// tier: LOGIC
// red-proof: Observed 2026-11-10 with tools\cli test --no-build --only LF_Test_LawlessHud: asserting inside-to-outside is ENTERED failed (inside to outside is a leave); restored.
//! lawless HUD (#591): the enter/exit state rule, and that a lawless zone reads as lawless to the
//! local client (the tag and hint are driven by LF_Zones.RuleAt).
class LF_Test_LawlessHud : LF_Test
{
	override string GetName()
	{
		return "lawless-hud/state-rule";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.True(LF_LawlessState.Transition(false, true) == LF_LawlessState.ENTERED, "outside to inside is an enter");
		ctx.True(LF_LawlessState.Transition(true, false) == LF_LawlessState.LEFT, "inside to outside is a leave");
		ctx.True(LF_LawlessState.Transition(true, true) == LF_LawlessState.NONE, "staying inside fires nothing");
		ctx.True(LF_LawlessState.Transition(false, false) == LF_LawlessState.NONE, "staying outside fires nothing");

		ctx.True(LF_Zones.RegisterCircle("lh-zone", LF_ZoneRule.LAWLESS, "7000 0 7000", 10), "a lawless zone registers");
		bool wasInside = false;
		bool inside = LF_Zones.RuleAt("7003 0 7000") == LF_ZoneRule.LAWLESS;
		ctx.True(LF_LawlessState.Transition(wasInside, inside) == LF_LawlessState.ENTERED, "a joiner who starts inside sees the enter");
		wasInside = inside;
		inside = LF_Zones.RuleAt("7020 0 7000") == LF_ZoneRule.LAWLESS;
		ctx.True(LF_LawlessState.Transition(wasInside, inside) == LF_LawlessState.LEFT, "walking out is a leave");
		LF_Zones.Unregister("lh-zone");
	}
}
