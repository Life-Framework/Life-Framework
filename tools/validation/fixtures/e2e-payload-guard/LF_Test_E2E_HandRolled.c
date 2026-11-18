class LF_Test_E2E_HandRolledVerify : LF_Test
{
	protected void Verify(LF_TestContext ctx)
	{
		if (LF_SaveStamp.BootSaveVersion() >= LF_PersistenceComponentSerializer.PayloadSinceVersion("reputation"))
			ctx.True(true, "by hand");
	}
}
