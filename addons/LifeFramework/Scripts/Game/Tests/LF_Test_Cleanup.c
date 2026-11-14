// red-proof: 2026-11-14, distance check in LF_CleanupRule.Verdict disabled: `--only LF_Test_Cleanup` failed `one near a player stays (expected 'player near', got '')`. Restored, green.
// tier: LOGIC
//! World cleanup rule (#690): the verdict from measured facts, with no world.
class LF_Test_Cleanup : LF_Test
{
	override string GetName()
	{
		return "cleanup/rule";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_CleanupRule rule = LF_CleanupRule.Make("item", 900, 50, 100);
		ctx.EqualStr("", rule.Verdict(1000, 500, 100, "Apple.et"), "old, far, any damage: it goes");
		ctx.EqualStr("too young", rule.Verdict(100, 500, 100, "Apple.et"), "a young one stays");
		ctx.EqualStr("player near", rule.Verdict(1000, 10, 100, "Apple.et"), "one near a player stays");

		rule.m_iMaxHealthPercent = 50;
		ctx.EqualStr("damage state", rule.Verdict(1000, 500, 90, "Apple.et"), "a healthy one stays when the rule wants wrecks");
		ctx.EqualStr("", rule.Verdict(1000, 500, 20, "Apple.et"), "a wreck goes");

		rule.m_aExemptPrefabs = {"Apple"};
		ctx.EqualStr("exempt prefab", rule.Verdict(1000, 500, 20, "Prefabs/Apple.et"), "an exempt prefab stays");

		rule.m_aExemptPrefabs = null;
		rule.m_bEnabled = false;
		ctx.EqualStr("kind off", rule.Verdict(1000, 500, 20, "Apple.et"), "a switched-off kind sweeps nothing");
	}
}
