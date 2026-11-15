// red-proof: 2026-11-14, cli test --no-build --red LF_Test_TestLogicWorld (the hook asks with tier WORLD):
//   RED-OK tests/logic-world: a LOGIC test runs in the minimal world (expected '', got 'unit/a is tier=world ...')
// tier: LOGIC
//! tests/logic-world: the minimal LOGIC world's guard (Worlds/TestLogic, tools/lib/test-world.mjs).
//! A LOGIC test may run there; a WORLD or PERSISTENCE test is refused by name and pointed at
//! DebugWorld; a failing LOGIC test gets a hint naming it and the WORLD tier.
class LF_Test_TestLogicWorld : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "tests/logic-world";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_TestTier logic = LF_TestTier.LOGIC;
		if (RedProof())
			logic = LF_TestTier.WORLD;

		ctx.EqualStr("", LF_TestManager.LogicWorldRefusal(logic, "unit/a"), "a LOGIC test runs in the minimal world");

		string world = LF_TestManager.LogicWorldRefusal(LF_TestTier.WORLD, "unit/b");
		ctx.True(world.Contains("unit/b"), "the WORLD refusal names the test");
		ctx.True(world.Contains("tier=world"), "the WORLD refusal names its tier");
		ctx.True(world.Contains("DebugWorld"), "the WORLD refusal points at DebugWorld");

		string persistence = LF_TestManager.LogicWorldRefusal(LF_TestTier.PERSISTENCE, "unit/c");
		ctx.True(persistence.Contains("unit/c") && persistence.Contains("tier=persistence"), "a PERSISTENCE test is refused by name");

		string hint = LF_TestManager.LogicWorldHint("unit/d");
		ctx.True(hint.Contains("unit/d") && hint.Contains("WORLD"), "the failure hint names the test and the WORLD tier");
		ctx.True(hint.Contains(LF_TestManager.LOGIC_WORLD), "the failure hint names the minimal world");
	}
}
