// red-proof: Observed 2026-11-08 with `tools\cli test --no-build --tier all`: replaced the
// carriedHolders.Find(holderId) check in LF_IdCardRules.Decide with `if (false)`; got
// `[LFTEST] FAIL creation/id-card-rules`,
// `- a second card for the same character is refused (expected 2, got 0)`. Restored; green.
// tier: LOGIC
class LF_Test_IdCardRules : LF_Test
{
	override string GetName()
	{
		return "creation/id-card-rules";
	}

	override void Run(LF_TestContext ctx)
	{
		array<string> none = {};
		ctx.Equal(LF_EIdCardGrant.NEW, LF_IdCardRules.Decide("char-a", none), "no card carried: a new card");
		ctx.Equal(LF_EIdCardGrant.REFUSED_NO_HOLDER, LF_IdCardRules.Decide("", none), "no holder id: refused");

		array<string> own = {"char-b", "char-a"};
		ctx.Equal(LF_EIdCardGrant.REFUSED_HELD, LF_IdCardRules.Decide("char-a", own), "a second card for the same character is refused");

		array<string> other = {"char-b"};
		ctx.Equal(LF_EIdCardGrant.NEW, LF_IdCardRules.Decide("char-a", other), "carrying someone else's card still grants your own");

		array<string> blank = {"char-b", ""};
		ctx.Equal(LF_EIdCardGrant.REISSUE_BLANK, LF_IdCardRules.Decide("char-a", blank), "a blank carried card is printed instead of adding one");

		ctx.Equal(1, LF_IdCardRules.PickShown("char-a", own), "your own card is the one you show");
		ctx.Equal(0, LF_IdCardRules.PickShown("char-c", own), "without your own, the first printed card is shown");
		array<string> onlyBlank = {""};
		ctx.Equal(-1, LF_IdCardRules.PickShown("char-a", onlyBlank), "a blank card is never shown");
		ctx.Equal(-1, LF_IdCardRules.PickShown("char-a", none), "nothing carried, nothing shown");
	}
}
