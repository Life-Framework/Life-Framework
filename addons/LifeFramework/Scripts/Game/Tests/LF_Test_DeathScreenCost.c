// red-proof: with the cost text builder skipped (RedProof), the bill line, the hospital line and the keeps-your-things line are all missing and every assertion fails.
//! The death screen tells the player what respawning costs (#3152): "You wake at the hospital. The bill is
//! $150. What you carried stays with your body." The bill is the respawn config's, so a pack that sets
//! another one shows it; a bill of 0 drops the bill sentence.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_DeathScreenCost : LF_Test
{
	override string GetName()
	{
		return "ui/death-screen-cost";
	}

	override void Run(LF_TestContext ctx)
	{
		string text = "";
		if (!RedProof())
			text = LF_DeathScreen.BuildCostText(150);

		ctx.True(text.Contains("$150"), "the bill is on the screen: " + text);
		ctx.True(text.Contains("hospital"), "it says where you wake: " + text);
		ctx.True(text.Contains("carried"), "it says the body keeps your things: " + text);

		// Another pack's bill shows its own number, not 150.
		string other = "";
		if (!RedProof())
			other = LF_DeathScreen.BuildCostText(400);
		ctx.True(other.Contains("$400"), "a different bill shows its own amount: " + other);
		ctx.False(other.Contains("$150"), "the default bill is not hard-coded: " + other);

		// No bill: no bill sentence, the other two lines stay.
		string free = "";
		if (!RedProof())
			free = LF_DeathScreen.BuildCostText(0);
		ctx.False(free.Contains("bill"), "a bill of 0 drops the bill sentence: " + free);
		ctx.False(free.Contains("$"), "a bill of 0 shows no amount: " + free);
		ctx.True(free.Contains("hospital") && free.Contains("carried"), "a bill of 0 keeps the other lines: " + free);

		// The config path: the live respawn config's bill, whatever the pack set.
		LF_RespawnConfig config = LF_RespawnConfig.Get();
		ctx.True(config != null, "the respawn config resolves");
		if (config && !RedProof())
		{
			string live = LF_DeathScreen.CostText();
			if (config.m_iBillAmount > 0)
				ctx.True(live.Contains("$" + config.m_iBillAmount.ToString()), "the screen shows the configured bill: " + live);
			else
				ctx.False(live.Contains("$"), "a configured bill of 0 shows no amount: " + live);
		}
		else if (RedProof())
			ctx.True(false, "red proof: the live cost text was not built");
	}
}
