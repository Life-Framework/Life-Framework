// A pass-2 file that asks the helper first.
class LF_Test_E2E_GoodVerify : LF_Test
{
	protected void Verify(LF_TestContext ctx)
	{
		// LF_Test_ReputationPersist.VerifyE2E(ctx); a comment is not a call
		if (LF_E2E.RequiresPayload("reputation"))
			LF_Test_ReputationPersist.VerifyE2E(ctx);
		if (LF_E2E.SaveHas("itemkeys"))
			ctx.True(true, "keyed");
		LF_Test_SeededPersist.VerifyE2E(ctx);
	}
}
