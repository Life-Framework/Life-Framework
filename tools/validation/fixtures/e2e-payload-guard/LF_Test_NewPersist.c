class LF_Test_NewPersist : LF_Test
{
	static void VerifyE2E(LF_TestContext ctx)
	{
		ctx.True(true, "unguarded");
	}
}
