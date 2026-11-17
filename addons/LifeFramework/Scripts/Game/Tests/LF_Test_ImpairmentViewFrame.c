// red-proof: 2026-11-17 `tools/cli test --no-build --red LF_Test_ImpairmentViewFrame,...`: RED-OK survival/impairment-view-frame, the old rule (every body on a rendering client) failed "a sober local player and 127 proxies (64 of them drunk) run no sway frame (expected 0, got 128)". Green with the fix (--only, 8 passed).
// tier: LOGIC
//! #2404: the drunk sway frame runs on the local player's own body only, and only while it sways or
//! still has sway to unwind. Before, every character's survival component armed EOnFrame on every
//! client, proxies included: about 127 script calls a frame per client at 128 players.
class LF_Test_ImpairmentViewFrame : LF_Test
{
	override string GetName()
	{
		return "survival/impairment-view-frame";
	}

	override void Run(LF_TestContext ctx)
	{
		// A synthetic 128-player client: body 0 is the local player's, the rest are proxies; half drunk.
		int armed;
		for (int i = 0; i < 128; i++)
		{
			bool local = i == 0;
			int drunk = i % 2;
			float level = drunk * 30;
			bool wants = LF_ImpairmentView.WantsFrame(false, local, level, false);
			if (RedProof())
				wants = true; // the old rule: every body on a rendering client
			if (wants)
				armed++;
		}
		ctx.Equal(0, armed, "a sober local player and 127 proxies (64 of them drunk) run no sway frame");

		ctx.True(LF_ImpairmentView.WantsFrame(false, true, 30, false), "the drunk local player sways");
		ctx.True(LF_ImpairmentView.WantsFrame(false, true, 0, true), "a sobered local player still unwinds the last offset");
		ctx.False(LF_ImpairmentView.WantsFrame(false, true, 0, false), "a sober local player with nothing to unwind runs no frame");
		ctx.False(LF_ImpairmentView.WantsFrame(false, false, 30, true), "another player's drunk body runs no frame here");
		ctx.False(LF_ImpairmentView.WantsFrame(true, true, 30, false), "a dedicated server runs no sway frame");
	}
}
