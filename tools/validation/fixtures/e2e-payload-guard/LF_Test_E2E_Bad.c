// The #2798 shape: a new payload's pass-2 check with no guard.
class LF_Test_E2E_BadVerify : LF_Test
{
	protected void Verify(LF_TestContext ctx)
	{
		LF_Test_NewPersist.VerifyE2E(ctx);
	}
}
