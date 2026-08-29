//! Setup cost tiers. fast = pure logic, no world or resource dependency.
//! world = needs the DebugWorld loaded (prefabs, layouts, spawning).
//! persistence = reserved for the public-API save/reload round trips
//! (roadmap Phase 1); runs in the all tier until the first suite lands.
enum EL_TestTier
{
	LOGIC,
	WORLD,
	PERSISTENCE
}

class EL_Test
{
	protected EL_TestTier m_Tier;

	//------------------------------------------------------------------------------------------------
	string GetName()
	{
		return "unnamed-test";
	}

	//------------------------------------------------------------------------------------------------
	void SetTier(EL_TestTier tier)
	{
		m_Tier = tier;
	}

	//------------------------------------------------------------------------------------------------
	EL_TestTier Tier()
	{
		return m_Tier;
	}

	//------------------------------------------------------------------------------------------------
	void Run(EL_TestContext ctx)
	{
		ctx.Fail("Test did not override Run()");
	}
}
