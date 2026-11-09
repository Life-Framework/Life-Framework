// red-proof: observed 2026-11-09 with `tools\cli test --no-build --only LF_Test_FirstAidRules`: LF_FirstAid.Grant returning `add` (no cap) failed `the cap trims the grant (expected 10, got 30)`; restored, green.

// tier: LOGIC
//! The pure rules of first aid on a downed player (LF_FirstAid): the cap trims what is added and
//! never takes time away, only medics add time when config says medic only, and morphine (or any
//! unknown kind) adds nothing.
class LF_Test_FirstAidRules : LF_Test
{
	override string GetName()
	{
		return "medic/first-aid-rules";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.EqualFloat(30, LF_FirstAid.Grant(100, 30, 300), 0.001, "a bandage adds its seconds");
		ctx.EqualFloat(10, LF_FirstAid.Grant(290, 30, 300), 0.001, "the cap trims the grant");
		ctx.EqualFloat(0, LF_FirstAid.Grant(300, 30, 300), 0.001, "at the cap nothing is added");
		ctx.EqualFloat(0, LF_FirstAid.Grant(350, 30, 300), 0.001, "above the cap nothing is added and nothing is taken");
		ctx.EqualFloat(0, LF_FirstAid.Grant(100, 0, 300), 0.001, "a zero item adds nothing");

		ctx.True(LF_FirstAid.MayApply(true, LF_EJobType.UNEMPLOYED), "anyone may when config says anyone");
		ctx.False(LF_FirstAid.MayApply(false, LF_EJobType.UNEMPLOYED), "a civilian may not when config says medic only");
		ctx.True(LF_FirstAid.MayApply(false, LF_EJobType.MEDIC), "a medic may when config says medic only");

		ctx.EqualFloat(0, LF_FirstAid.SecondsFor("morphine"), 0.001, "morphine adds nothing");
		ctx.True(LF_FirstAid.SecondsFor(LF_FirstAid.KIND_BANDAGE) > 0, "a bandage adds time");
		ctx.True(LF_FirstAid.SecondsFor(LF_FirstAid.KIND_TOURNIQUET) > 0, "a tourniquet adds time");
		ctx.True(LF_FirstAid.SecondsFor(LF_FirstAid.KIND_SALINE) > 0, "a saline bag adds time");
	}
}
