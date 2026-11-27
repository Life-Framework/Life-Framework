// red-proof: with `RedProof()` on the test config is never installed, so the clerk reads the shipped file and the defaults
// instead of the values the test set: the exact fee, the free replacement, the two clamps and the missing-config fallbacks fail.

//! identity/photo-id-config-clamp: what the town clerk charges for a replacement identity card and how long a shown card
//! stays up (LF_PhotoIdCardConfig). The fee is the config value, 0 means free (not the default), a value outside the schema
//! is held to it (no negative fee that would pay the player, no fee past 100,000), the show time is held to 2..60 seconds,
//! and with no config at all the clerk charges the default fee and shows for the default time.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_PhotoIdCardConfigClamp : LF_Test
{
	override string GetName()
	{
		return "identity/photo-id-config-clamp";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_PhotoIdCardConfig Make(int fee, float seconds)
	{
		LF_PhotoIdCardConfig config = new LF_PhotoIdCardConfig();
		config.m_iReplacementFee = fee;
		config.m_fShowSeconds = seconds;
		return config;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		int plainFee = -1;
		float plainShow = -1;
		int freeFee = -1;
		int negativeFee = -1;
		int hugeFee = -1;
		float shortShow = -1;
		float longShow = -1;
		int missingFee = -1;
		float missingShow = -1;
		if (!RedProof())
		{
			LF_PhotoIdCardConfig.UseForTest(Make(250, 12));
			plainFee = LF_PhotoIdCardConfig.ReplacementFee();
			plainShow = LF_PhotoIdCardConfig.ShowSeconds();

			LF_PhotoIdCardConfig.UseForTest(Make(0, 8));
			freeFee = LF_PhotoIdCardConfig.ReplacementFee();

			LF_PhotoIdCardConfig.UseForTest(Make(-5, 0.5));
			negativeFee = LF_PhotoIdCardConfig.ReplacementFee();
			shortShow = LF_PhotoIdCardConfig.ShowSeconds();

			LF_PhotoIdCardConfig.UseForTest(Make(999999999, 500));
			hugeFee = LF_PhotoIdCardConfig.ReplacementFee();
			longShow = LF_PhotoIdCardConfig.ShowSeconds();

			LF_PhotoIdCardConfig.UseForTest(null);
			missingFee = LF_PhotoIdCardConfig.ReplacementFee();
			missingShow = LF_PhotoIdCardConfig.ShowSeconds();
			LF_PhotoIdCardConfig.Reload();
		}

		ctx.Equal(250, plainFee, "the clerk charges the configured fee");
		ctx.EqualFloat(12, plainShow, 0.001, "a shown card stays up for the configured time");
		ctx.Equal(0, freeFee, "a fee of 0 is a free replacement, not the default");
		ctx.Equal(LF_PhotoIdCardConfig.FEE_MIN, negativeFee, "a negative fee is held at the schema minimum");
		ctx.Equal(LF_PhotoIdCardConfig.FEE_MAX, hugeFee, "a fee past the limit is held at the schema maximum");
		ctx.EqualFloat(LF_PhotoIdCardConfig.SHOW_MIN, shortShow, 0.001, "a show time under 2 seconds is held at 2");
		ctx.EqualFloat(LF_PhotoIdCardConfig.SHOW_MAX, longShow, 0.001, "a show time over 60 seconds is held at 60");
		ctx.Equal(LF_PhotoIdCardConfig.DEFAULT_FEE, missingFee, "no config: the default fee");
		ctx.EqualFloat(LF_PhotoIdCardConfig.DEFAULT_SHOW_SECONDS, missingShow, 0.001, "no config: the default show time");
	}
}
