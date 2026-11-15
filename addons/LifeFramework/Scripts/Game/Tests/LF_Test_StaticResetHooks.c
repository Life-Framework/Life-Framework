// red-proof: 2026-11-16 `tools\cli test --no-build --red LF_Test_StaticResetHooks` (lf-cloud) skipped the
// listing (RedProof) and every assertion failed: "red: LF_Session registers its reset hook" ... RED-OK
// "RED PROOF OK (all 1 listed test(s) fail with the hook on)". Green on the migrated hooks (--only, 2 passed).

// tier: LOGIC
//! World-end static reset is registered per feature (Core/LF_StaticReset.c holds no list): every
//! owning file adds a `modded class LF_StaticResetHooks`, and the chain reaches all of them. Listing
//! runs with apply false, so the test resets nothing. Spot checks cover a Core file that sorts before
//! LF_StaticReset.c (LF_Places, Core/Config/LF_ConfigStream), files with two hooked classes, and
//! the last entry the central list had (LF_PoliceRosterNotice).
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

		ctx.True(done.Count() >= 109, string.Format("every migrated feature reset is registered (%1 hooks)", done.Count()));

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
