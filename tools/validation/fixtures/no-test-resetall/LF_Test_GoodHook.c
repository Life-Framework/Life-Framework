// Fixture for no-test-resetall-selftest.mjs: the allowed shape (own feature's statics only).
// tier: LOGIC
// red-proof: fixture only.
class LF_Test_GoodHook : LF_Test
{
	override string GetName() { return "fixture/good-hook"; }

	/* LF_StaticReset.ResetAll() in a block comment is fine too */
	override void Run(LF_TestContext ctx)
	{
		LF_Alpha.ResetStatics();
	}
}
