class LF_Test_E2E_PayloadSinceVerify : LF_Test
{
	protected void Verify(LF_TestContext ctx)
	{
		int since = LF_PersistenceComponentSerializer.PayloadSinceVersion("reputation");
	}
}
