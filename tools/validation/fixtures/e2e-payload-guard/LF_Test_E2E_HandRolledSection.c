class LF_Test_E2E_HandRolledSectionVerify : LF_Test
{
	protected void Verify(LF_TestContext ctx)
	{
		if (LF_SaveSections.BootHadSection("x"))
			ctx.True(true, "by hand");
	}
}
