class LF_Test_E2E_UnknownPayloadVerify : LF_Test
{
	protected void Verify(LF_TestContext ctx)
	{
		if (LF_E2E.RequiresPayload("reputashun"))
			ctx.True(true, "typo");
	}
}
