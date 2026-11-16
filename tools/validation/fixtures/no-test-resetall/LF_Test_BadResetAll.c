// Fixture for no-test-resetall-selftest.mjs: NOT in addons/, so the real validate never sees it.
// tier: LOGIC
// red-proof: fixture only.
class LF_Test_BadResetAll : LF_Test
{
	override string GetName() { return "fixture/bad-reset-all"; }

	override void Run(LF_TestContext ctx)
	{
		// A comment naming LF_StaticReset.ResetAll() is fine; the call below is not.
		string note = "LF_StaticReset.ResetAll()";
		LF_StaticReset.ResetAll();
	}
}
