// red-proof: RED-OK vehicles/inspection, observed 2026-11-19 with `tools\cli test --no-build --red LF_Test_Inspection`: with the stamp skipped, `a check on the last valid day reads valid (expected 1, got 0)`, `... on the expiry day reads expired (expected 2, got 0)`. Hook off: green.
//! Inspection stickers (#3433, card Q8): an officer's inspection stamps a sticker that is valid for N game
//! days; a check reads valid before the expiry day, expired from it, and none for a car never inspected.
//! Session only until the save card (Q10); the rule takes the game day as a number, so no world clock is needed.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_Inspection : LF_Test
{
	protected static const string CAR = "lf-test-inspection-car";
	protected static const string OTHER = "lf-test-inspection-other";
	protected static const int DAY = 20000;	// an arbitrary game day (days since 1970-01-01)

	override string GetName()
	{
		return "vehicles/inspection";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_Inspection.ResetStatics();

		ctx.Equal(LF_EInspectionStatus.NONE, LF_Inspection.Check(CAR, DAY), "a car never inspected reads none");

		int expiry = -1;
		if (!RedProof())
			expiry = LF_Inspection.Inspect(CAR, DAY, 3);

		ctx.Equal(DAY + 3, expiry, "inspecting for 3 days sets the expiry three days out");
		ctx.Equal(LF_EInspectionStatus.VALID, LF_Inspection.Check(CAR, DAY), "a check the same day reads valid");
		ctx.Equal(LF_EInspectionStatus.VALID, LF_Inspection.Check(CAR, DAY + 2), "a check on the last valid day reads valid");
		ctx.Equal(LF_EInspectionStatus.EXPIRED, LF_Inspection.Check(CAR, DAY + 3), "a check on the expiry day reads expired");
		ctx.Equal(LF_EInspectionStatus.EXPIRED, LF_Inspection.Check(CAR, DAY + 30), "a check long after reads expired");
		ctx.Equal(LF_EInspectionStatus.NONE, LF_Inspection.Check(OTHER, DAY), "another car is still none");

		// A new inspection replaces the old sticker.
		LF_Inspection.Inspect(CAR, DAY + 30, 3);
		ctx.Equal(LF_EInspectionStatus.VALID, LF_Inspection.Check(CAR, DAY + 31), "a fresh inspection makes the car valid again");

		// The default window is set by the feature, never zero.
		ctx.True(LF_Inspection.VALID_DAYS > 0, "the default window is at least one day");
		int defaultExpiry = LF_Inspection.Inspect(OTHER, DAY);
		ctx.Equal(DAY + LF_Inspection.VALID_DAYS, defaultExpiry, "the default inspection uses the default window");

		// Bad input degrades: an empty id is refused, a zero window stamps nothing.
		ctx.Equal(-1, LF_Inspection.Inspect("", DAY, 3), "an empty vehicle id is refused");
		ctx.Equal(LF_EInspectionStatus.NONE, LF_Inspection.Check("", DAY), "an empty vehicle id reads none");
		ctx.Equal(-1, LF_Inspection.Inspect("lf-test-inspection-zero", DAY, 0), "a zero-day window is refused");
		ctx.Equal(LF_EInspectionStatus.NONE, LF_Inspection.Check("lf-test-inspection-zero", DAY), "a refused inspection leaves no sticker");

		LF_Inspection.ResetStatics();
	}
}
