// red-proof: Observed 2026-11-19 with `toolsi test --no-build --red LF_Test_HandsUpState`: RED-OK crime/hands-up-state (fails with the hook on, 12 assertions); hook off, green.
//! Hands-up answer rules on the server (#3396, LF_HandsUp): only a live offer lets a raise through,
//! ignoring the prompt forces nothing, lowering needs raised hands, and an offer expires.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_HandsUpState : LF_Test
{
	override string GetName()
	{
		return "crime/hands-up-state";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.True(LF_HandsUp.OfferLive(5000, 1000), "an offer until 5000 is live at 1000");
		ctx.False(LF_HandsUp.OfferLive(5000, 5000), "an offer is over at its end");
		ctx.False(LF_HandsUp.OfferLive(0, 1000), "no offer is never live");

		bool offered = true;
		if (RedProof())
			offered = false;
		LF_EHandsUpAnswer raise = LF_HandsUp.Decide(LF_HandsUp.CHOICE_RAISE, offered, false);
		ctx.Equal(LF_EHandsUpAnswer.RAISE, raise, "a live offer lets the target raise their hands");
		ctx.Equal(LF_EHandsUpAnswer.IGNORED, LF_HandsUp.Decide(LF_HandsUp.CHOICE_RAISE, false, false), "a raise with no offer changes nothing");
		ctx.Equal(LF_EHandsUpAnswer.IGNORED, LF_HandsUp.Decide(LF_HandsUp.CHOICE_RAISE, true, true), "a raise while already up changes nothing");
		ctx.Equal(LF_EHandsUpAnswer.REFUSED, LF_HandsUp.Decide(LF_HandsUp.CHOICE_REFUSE, true, false), "a refusal closes the offer");
		ctx.Equal(LF_EHandsUpAnswer.IGNORED, LF_HandsUp.Decide(LF_HandsUp.CHOICE_REFUSE, false, false), "a refusal with no offer changes nothing");
		ctx.Equal(LF_EHandsUpAnswer.LOWER, LF_HandsUp.Decide(LF_HandsUp.CHOICE_LOWER, false, true), "raised hands can be lowered");
		ctx.Equal(LF_EHandsUpAnswer.IGNORED, LF_HandsUp.Decide(LF_HandsUp.CHOICE_LOWER, true, false), "lowering hands that are down changes nothing");
		ctx.Equal(LF_EHandsUpAnswer.IGNORED, LF_HandsUp.Decide(7, true, false), "an unknown choice changes nothing");
	}
}
