// red-proof: all four tests were observed failing in the headless server boot
// before fixes: WorldLoaded failed when GetWorld() returned null during a
// broken world load; MoneyStackPrefab failed with a null/invalid prefab when
// the prefab GUID path was wrong ({0000000000000000}); MathStringSanity and
// ContextSelfTest fail on any perturbed expectation (e.g. AbsInt(-5) -> 4).

// tier: WORLD
class LF_Test_WorldLoaded : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "engine/world-loaded";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		ctx.True(GetGame() != null, "GetGame() returns an instance");
		World world = GetGame().GetWorld();
		ctx.True(world != null, "a world is loaded");
	}
}

//------------------------------------------------------------------------------------------------
// tier: WORLD
class LF_Test_MoneyStackPrefab : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "resources/money-stack-prefab";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		Resource prefab = Resource.Load(LF_MoneyUtils.PREFAB_CASH);
		ctx.True(prefab != null && prefab.IsValid(), "MoneyStack prefab loads and is valid: " + LF_MoneyUtils.PREFAB_CASH);
	}
}

//------------------------------------------------------------------------------------------------
// tier: LOGIC
class LF_Test_MathStringSanity : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "engine/math-string-sanity";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		ctx.Equal(5, Math.AbsInt(-5), "Math.AbsInt");
		ctx.Equal(6, 2 * 3, "integer multiplication");
		ctx.Equal(3, "abc".Length(), "string length");
		ctx.EqualStr("AB", "A" + "B", "string concatenation");
	}
}

//------------------------------------------------------------------------------------------------
// tier: LOGIC
class LF_Test_ContextSelfTest : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "framework/context-selftest";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_TestContext probe = new LF_TestContext();

		probe.True(true, "passing True records no failure");
		probe.False(false, "passing False records no failure");
		ctx.Equal(0, probe.FailureCount(), "no failures recorded for passing asserts");

		probe.True(false, "expected failure one");
		probe.Fail("expected failure two");
		ctx.Equal(2, probe.FailureCount(), "failures recorded");
	}
}
