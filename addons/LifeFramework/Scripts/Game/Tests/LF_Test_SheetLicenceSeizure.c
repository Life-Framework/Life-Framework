// red-proof: RED-OK licence/sheet-seizure, observed 2026-11-23 with `tools\cli test --no-build --red LF_Test_SheetLicenceSeizure`: with the sheet booking skipped, `cell: the driving ban is the rule's 3 days (expected 3, got 0)` and `the driving licence leaves the saved record`. Green: `--only` passed with LF_Test_LicenceSeizure.
//! The signed charge sheet takes the licences (#4428, police L7): a sheet with a driving code bans the
//! driving licence for the rule's days whether the suspect pays or goes to the cell, once; on the cell
//! path the jail's own arrest event books nothing beside it (an open firearm report the officer did not
//! put on the sheet keeps the permit); a sheet with no mapped code takes nothing.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_SheetLicenceSeizure : LF_Test
{
	protected static const string UID_PAY = "test-sheetseize-pay";
	protected static const string UID_CELL = "test-sheetseize-cell";
	protected static const string UID_NONE = "test-sheetseize-none";
	protected static const string RECKLESS = "#LF-Ticket_OffenceRecklessDriving";
	protected static const string BRANDISHING = "#LF-Ticket_OffenceBrandishing";
	protected static const string UNMAPPED = "unmapped-offence";
	protected static const int T0 = 10000000;	// in-game minute the test pins the clock at

	override string GetName()
	{
		return "licence/sheet-seizure";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_LicenceBanRow> before = LF_LicenceBans.ExportAll(LF_LicenceBans.Now());
		LF_LicenceBans.ResetStatics();
		LF_LicenceSeizureSheet.ResetStatics();
		LF_LicenceDesk.SetNowForTest(T0);
		array<LF_ELicenseType> held = {LF_ELicenseType.UNEMPLOYED, LF_ELicenseType.FIREARM_PERMIT, LF_ELicenseType.DRIVING_LICENCE};
		LF_Progression.NoteLicenses(UID_PAY, held);
		LF_Progression.NoteLicenses(UID_CELL, held);
		LF_Progression.NoteLicenses(UID_NONE, held);
		ctx.True(LF_LicenceSeizure.EnsureSubscribed(), "the report seizure listens to LF_OnArrested");

		RunPay(ctx);
		RunCell(ctx);
		RunNone(ctx);

		LF_WantedReports.ClearFor(UID_CELL);
		LF_Progression.Forget(UID_PAY);
		LF_Progression.Forget(UID_CELL);
		LF_Progression.Forget(UID_NONE);
		LF_LicenceSeizureSheet.ResetStatics();
		LF_LicenceDesk.SetNowForTest(-1);
		LF_LicenceBans.ResetStatics();
		LF_LicenceBans.ApplyAll(before, LF_LicenceBans.Now());
	}

	//------------------------------------------------------------------------------------------------
	//! Paid at the desk: no sentence, the sheet alone takes the licence.
	protected void RunPay(LF_TestContext ctx)
	{
		LF_BookingOffer offer = MakeOffer(UID_PAY, RECKLESS);
		int seized;
		// RedProof(): the sheet books nothing, so the ban assertions fail (`--red`).
		if (!RedProof())
			seized = LF_LicenceSeizureSheet.Signed(offer, null);

		ctx.Equal(1, seized, "pay now: the reckless driving sheet seizes one licence");
		ctx.True(LF_LicenceBans.IsBanned(UID_PAY, LF_ELicenseType.DRIVING_LICENCE), "pay now: the driving licence is banned");
		ctx.Equal(3, LF_LicenceSeizure.DaysLeft(UID_PAY, LF_ELicenseType.DRIVING_LICENCE), "pay now: the ban is the rule's 3 days");
		ctx.False(SavedHas(UID_PAY, LF_ELicenseType.DRIVING_LICENCE), "pay now: the driving licence leaves the saved record");
		ctx.True(SavedHas(UID_PAY, LF_ELicenseType.FIREARM_PERMIT), "pay now: the permit, on no code, stays");
		ctx.Equal(0, LF_LicenceSeizureSheet.Signed(offer, null), "pay now: the same sheet seizes nothing a second time");
	}

	//------------------------------------------------------------------------------------------------
	//! To the cell: the jail publishes LF_OnArrested while the desk settles; the sheet books, the reports do not.
	protected void RunCell(LF_TestContext ctx)
	{
		LF_WantedReports.Add(UID_CELL, BRANDISHING, "", "", "grid 0-0", true);
		LF_WantedReports.Add(UID_CELL, RECKLESS, "", "", "grid 0-0", true);
		LF_BookingOffer offer = MakeOffer(UID_CELL, RECKLESS);

		LF_LicenceSeizureSheet.Hold(UID_CELL);
		LF_EventBus.Publish(LF_OnArrested.Create(UID_CELL, 300, 2));
		LF_LicenceSeizureSheet.Release(UID_CELL);
		ctx.False(LF_LicenceSeizureSheet.IsHeld(UID_CELL), "cell: the hold ends with the settlement");

		int seized;
		if (!RedProof())
			seized = LF_LicenceSeizureSheet.Signed(offer, null);

		ctx.Equal(1, seized, "cell: the reckless driving sheet seizes one licence");
		ctx.Equal(3, LF_LicenceSeizure.DaysLeft(UID_CELL, LF_ELicenseType.DRIVING_LICENCE), "cell: the driving ban is the rule's 3 days");
		ctx.False(SavedHas(UID_CELL, LF_ELicenseType.DRIVING_LICENCE), "cell: the driving licence leaves the saved record");
		ctx.False(LF_LicenceBans.IsBanned(UID_CELL, LF_ELicenseType.FIREARM_PERMIT), "cell: the arrest event booked no ban beside the sheet");
		ctx.True(SavedHas(UID_CELL, LF_ELicenseType.FIREARM_PERMIT), "cell: the permit, off the sheet, stays");
		ctx.Equal(0, LF_LicenceSeizureSheet.Signed(offer, null), "cell: the same sheet seizes nothing a second time");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunNone(LF_TestContext ctx)
	{
		LF_BookingOffer offer = MakeOffer(UID_NONE, UNMAPPED);
		ctx.Equal(0, LF_LicenceSeizureSheet.Signed(offer, null), "a sheet with no mapped code seizes nothing");
		ctx.False(LF_LicenceBans.IsBanned(UID_NONE, LF_ELicenseType.DRIVING_LICENCE), "no mapped code: the driving licence is not banned");
		ctx.True(SavedHas(UID_NONE, LF_ELicenseType.DRIVING_LICENCE), "no mapped code: the driving licence stays");
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_BookingOffer MakeOffer(string uid, string code)
	{
		LF_BookingOffer offer = new LF_BookingOffer();
		offer.m_sSuspectUid = uid;
		offer.m_sOfficerUid = "test-sheetseize-officer";
		offer.m_aCodes.Insert(code);
		return offer;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool SavedHas(string uid, LF_ELicenseType licence)
	{
		array<LF_ELicenseType> saved = {};
		LF_Progression.GetLicenses(uid, saved);
		return saved.Contains(licence);
	}
}
