// red-proof: see the PR for #2755 (the probe hook below has no registration line; on the old static-initializer
// registry the run failed "LF_Test_StaticResetHooks_Probe is found without registering"; on discovery it passes).
// #2034: on main c32a9ee9 (123 modded layers) `--only LF_Test_StaticResetHooks` crashed with VME "Stack overflow" in
// LF_StaticResetHooks.Run (logs_2026-11-16_10-08-33); on the flat registry it passed (128 hooks, logs_2026-11-16_10-09-15).

// tier: LOGIC
//! World-end static reset is found by reflection (#2755): LF_StaticReset walks every LF_StaticResetHook
//! subclass, so a hook needs no static initializer (each one cost a share of the Game module's single
//! static-init function, which the engine caps). Listing runs no hook, so the test resets nothing.
//! Spot checks cover a Core file that sorts before LF_StaticReset.c (LF_Places, Core/Config/LF_ConfigStream),
//! files with two hooked classes, and the last entry the central list had (LF_PoliceRosterNotice).
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_StaticResetHooks : LF_Test
{
	override string GetName()
	{
		return "staticreset/hooks";
	}

	override void Run(LF_TestContext ctx)
	{
		array<string> done = new array<string>();
		if (!RedProof())
			done = LF_StaticReset.Registered();

		// Every subclass of LF_StaticResetHook (GetInheritedTypes lists the base too).
		array<typename> types = {};
		typename base = LF_StaticResetHook;
		base.GetInheritedTypes(types);
		int hookTypes;
		foreach (typename t : types)
		{
			if (t != base)
				hookTypes++;
		}

		ctx.True(hookTypes >= 200, string.Format("the hook classes are visible to reflection (%1)", hookTypes));
		ctx.Equal(hookTypes, done.Count(), "every LF_StaticResetHook subclass is discovered, once");
		ctx.True(done.Contains("LF_Test_StaticResetHooks_Probe"), "LF_Test_StaticResetHooks_Probe is found without registering");

		array<string> expect = {"LF_Places", "LF_ConfigStream", "LF_EventBus", "LF_Breach", "LF_Payphone", "LF_ServiceCallDispatch", "LF_StaffRelay", "LF_Session", "LF_PoliceRosterNotice", "LF_WorldReset"};
		foreach (string cls : expect)
		{
			ctx.True(done.Contains(cls), cls + " registers its reset hook");
		}

		set<string> seen = new set<string>();
		int dupes;
		foreach (string name : done)
		{
			if (seen.Contains(name))
				dupes++;
			seen.Insert(name);
		}
		ctx.Equal(0, dupes, "no class registers its reset twice");
	}
}

//------------------------------------------------------------------------------------------------
//! A hook with no registration line at all: discovery alone must find it. Apply only bumps its own counter.
class LF_Test_StaticResetHooks_Probe : LF_StaticResetHook
{
	static int s_iApplied;

	override string Owner()
	{
		return "LF_Test_StaticResetHooks_Probe";
	}

	override void Apply()
	{
		s_iApplied++;
	}
}
