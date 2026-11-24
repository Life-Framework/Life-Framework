// red-proof: RED-OK harness/book-charge, observed 2026-11-24 with `tools/cli test --red LF_Test_BookingDeskSuspectId,LF_Test_BookChargeHarness`: with the filing skipped, `the charge sheet reads one open code (expected 1, got 0)`. Green: `--only` passed.
//! Harness verb `bookcharge` (#4699): files one open crime report and a wanted level against an account,
//! so the booking desk's charge sheet has a fineable code to read. Harness-only: a build without LF_HARNESS
//! refuses it.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_BookChargeHarness : LF_Test
{
	protected static const string ACCOUNT = "test-bookcharge-account";

	override string GetName()
	{
		return "harness/book-charge";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_WantedReports.ClearFor(ACCOUNT);
		LF_PlayerAccount account = LF_PlayerAccountManager.GetOrCreate(ACCOUNT);
		account.SetWantedLevel(0);
		ctx.True(account != null, "the account exists");
		ctx.Equal(0, LF_WantedReports.CountFor(ACCOUNT), "a fresh account has no open report");

		int reports = 0;
		if (!RedProof())
			reports = LF_BookChargeHarness.File(ACCOUNT, LF_BookChargeHarness.OffenceFor("weapontheft"));

		ctx.Equal(1, reports, "the filing leaves one open report");
		ctx.Equal(1, account.GetWantedLevel(), "and the account is wanted");

		array<string> ids = {};
		LF_ChargeSheetDesk.OpenCodeIds(ACCOUNT, ids);
		ctx.Equal(1, ids.Count(), "the charge sheet reads one open code");
		ctx.Equal(-1, LF_BookChargeHarness.File("", LF_WantedReports.OFFENCE_ROBBERY), "no account is refused");
		ctx.Equal(-1, LF_BookChargeHarness.File(ACCOUNT, ""), "no offence is refused");
		ctx.EqualStr("", LF_BookChargeHarness.OffenceFor("nonsense"), "an unknown word names no offence");

		ctx.True(LF_BookChargeHarness.IsVerb("bookcharge weapontheft"), "bookcharge <word> is this harness verb");
		ctx.False(LF_BookChargeHarness.IsVerb("bookchargeall x"), "a longer word is not");
		ctx.False(LF_DevGate.HarnessBuild(), "the test suite is not a harness build");
		ctx.EqualStr("error harness_only", LF_BookChargeHarness.Run(null, "bookcharge weapontheft"), "a build without LF_HARNESS refuses the verb");

		LF_WantedReports.ClearFor(ACCOUNT);
		account.SetWantedLevel(0);
	}
}
