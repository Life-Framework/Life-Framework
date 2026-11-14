//! Setup cost tiers. fast = pure logic, no world or resource dependency.
//! world = needs the DebugWorld loaded (prefabs, layouts, spawning).
//! persistence = reserved for the public-API save/reload round trips
//! (roadmap Phase 1); runs in the all tier until the first suite lands.
enum LF_TestTier
{
	LOGIC,
	WORLD,
	PERSISTENCE
}

class LF_Test
{
	protected LF_TestTier m_Tier;

	//------------------------------------------------------------------------------------------------
	string GetName()
	{
		return "unnamed-test";
	}

	//------------------------------------------------------------------------------------------------
	void SetTier(LF_TestTier tier)
	{
		m_Tier = tier;
	}

	//------------------------------------------------------------------------------------------------
	LF_TestTier Tier()
	{
		return m_Tier;
	}

	//------------------------------------------------------------------------------------------------
	//! Empty: the test runs everywhere. A reason: the test needs content only the flagship pack
	//! holds, so a core-only run (tools\cli test --core-only, scripting define
	//! LF_CORE_ONLY, #326) skips it with that reason. Keep the list short: a test that names a
	//! pack resource usually wants a neutral core fixture instead.
	string NeedsPack()
	{
		return "";
	}

	//------------------------------------------------------------------------------------------------
	void Run(LF_TestContext ctx)
	{
		ctx.Fail("Test did not override Run()");
	}
}
