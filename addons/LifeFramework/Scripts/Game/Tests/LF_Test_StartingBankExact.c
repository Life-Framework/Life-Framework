// red-proof: observed 2026-11-27 with `tools\cli test --no-build --only LF_Test_StartingBankExact` on the unfixed Math.Max sites:
// `- the welcome guide reads a 2^24 + 1 opening bank exactly (expected 16777217, got 16777216)` and the same for the spawn logic;
// green after both moved to LF_Utils.MaxInt.

//! #2787 follow-up: the opening bank is money, and Enforce `Math.Max` works in floats, so a configured bank above
//! 2^24 (16,777,216) came back rounded. LF_SpawnLogic.GetStartingBank and LF_WelcomeGuideSheet.StartingBank clamp
//! it at zero with LF_Utils.MaxInt now; this feeds them 2^24 + 1 through the loaded character config.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_StartingBankExact : LF_Test
{
	// The first int a float cannot hold (2^24 + 1).
	protected static const int FLOAT_EDGE = 16777217;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "money/starting-bank-exact";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_CharacterConfig config = LF_CharacterConfig.Get();
		ctx.True(config != null, "the character config loads");
		if (!config)
			return;

		int configured = config.m_iStartingBank;
		if (!RedProof())
			config.m_iStartingBank = FLOAT_EDGE;

		ctx.Equal(FLOAT_EDGE, LF_WelcomeGuideSheet.StartingBank(), "the welcome guide reads a 2^24 + 1 opening bank exactly");

		LF_SpawnLogic logic = LF_SpawnLogic.GetInstance();
		if (logic)
			ctx.Equal(FLOAT_EDGE, logic.GetStartingBank(), "the spawn logic reads a 2^24 + 1 opening bank exactly");

		config.m_iStartingBank = configured;
	}
};
