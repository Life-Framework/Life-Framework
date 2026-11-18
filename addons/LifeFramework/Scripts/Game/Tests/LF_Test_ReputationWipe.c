// red-proof: observed 2026-11-18 on the unfixed code, `tools/cli test --no-build --only LF_Test_ReputationWipe` (log test-20261118-002706): "a character wipe resets standing (expected 0, got 150)"; green after the fix (log test-20261118-002725).
//! reputation/wipe (#2771): the admin character wipe (LF_Progression.Forget) resets saved standing,
//! as docs/architecture/reputation.md section 6 says. Another character's standing stays.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_ReputationWipe : LF_Test
{
	static const string UID = "test-reputation-wipe";
	static const string OTHER = "test-reputation-wipe-other";

	override string GetName()
	{
		return "reputation/wipe";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ReputationConfig.UseForTest(LF_Test_ReputationRules.MakeConfig());
		LF_Reputation.Forget(UID);
		LF_Reputation.Forget(OTHER);

		LF_Reputation.Add(UID, "POLICE", 150, "test");
		LF_Reputation.Add(OTHER, "POLICE", 150, "test");
		ctx.Equal(150, LF_Reputation.Points(UID, "POLICE"), "the character earned standing");

		// RedProof: skip the wipe, so the standing outlives it (`--red`).
		if (!RedProof())
			LF_Progression.Forget(UID);

		ctx.Equal(0, LF_Reputation.Points(UID, "POLICE"), "a character wipe resets standing");
		ctx.Equal(150, LF_Reputation.Points(OTHER, "POLICE"), "another character's standing stays");

		LF_Reputation.Forget(UID);
		LF_Reputation.Forget(OTHER);
		LF_ReputationConfig.Reload();
	}
}
