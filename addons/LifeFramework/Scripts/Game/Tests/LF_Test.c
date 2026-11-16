//! Setup cost tiers. fast = pure logic, no world or resource dependency.
//! world = needs the DebugWorld loaded (prefabs, layouts, spawning).
//! persistence = reserved for the public-API save/reload round trips
//! (roadmap Phase 1); runs in the all tier until the first suite lands.
//! perf = a timing gate. It has no place in fast or all (a timing measure is only sharp on an idle
//! machine, and it flakes under load); `tools\cli test --tier perf` runs these alone, with more
//! repeats (LF_TestManager.PerfRun()).
enum LF_TestTier
{
	LOGIC,
	WORLD,
	PERSISTENCE,
	PERF
}

class LF_Test
{
	protected LF_TestTier m_Tier;
	protected bool m_bRedProof;
	protected bool m_bPackTest;

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
	//! Set by LF_TestManager.RegisterPack: the test lives in a pack (its Scripts/Game/Tests) and runs
	//! only when named, which the pack leg of `tools\cli test` does under the pack's mission header.
	void SetPackTest(bool on)
	{
		m_bPackTest = on;
	}

	//------------------------------------------------------------------------------------------------
	bool IsPackTest()
	{
		return m_bPackTest;
	}

	//------------------------------------------------------------------------------------------------
	//! Set by LF_TestManager. True only in a `tools\cli test --red` run (docs/roadmap/testing/README.md "File rules").
	void SetRedProof(bool on)
	{
		m_bRedProof = on;
	}

	//------------------------------------------------------------------------------------------------
	//! The red-proof hook, checked in ONE place of Run(): when true, break what the assertions
	//! guard, most simply by skipping the act (`if (!RedProof()) LF_X.Use(true);`). A run with
	//! --red passes only if every listed test then FAILS; a test that still passes is hollow.
	//! False in every other run, so a test behaves exactly as written there.
	bool RedProof()
	{
		return m_bRedProof;
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
