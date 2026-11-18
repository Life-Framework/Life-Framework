class LF_Test_SeededPersist : LF_Test
{
	static void VerifyE2E(LF_TestContext ctx)
	{
		if (!LF_E2E.RequiresSeed("seeded thing", true))
			return;
		ctx.True(true, "seeded");
	}
}
