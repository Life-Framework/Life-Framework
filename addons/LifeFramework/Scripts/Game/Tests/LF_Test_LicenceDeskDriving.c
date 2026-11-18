// red-proof: RED-OK licence/desk-driving, observed 2026-11-18 with `tools\cli test --no-build --red LF_Test_LicenceDeskDriving`:
// with the desk offer skipped, `the driving licence has its own replication bit` and `a second request is refused as
// already held (expected 3, got 0)` fail. Hook off: green (--only).
//! licence/desk-driving (#3128): the licence desk sells the driving licence at once for the fee in
//! carried cash, no record check. Short cash is refused with nothing taken; a second request is
//! refused as already held and charges nothing; the licence is in the saved licence list and
//! survives the progression store's export and apply (in-session; the restart path is the e2e's).
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_LicenceDeskDriving : LF_Test
{
	protected static const string UID = "lf-test-licence-desk-driving";
	protected static const int FEE = 100;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "licence/desk-driving";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_Progression.Forget(UID);
		LF_LicencesConfig.SetDriverLicenceFeeForTest(FEE);
		ctx.Equal(FEE, LF_LicencesConfig.DriverLicenceFee(), "the driving licence fee is the configured one");
		ctx.Equal(100, LF_LicencesConfig.DEFAULT_DRIVER_LICENCE_FEE, "the core default fee is 100");

		IEntity buyer = LF_TestCharacterFixture.Spawn(ctx);
		if (!buyer)
			return;

		LF_LicenseManagerComponent licences = LF_Component<LF_LicenseManagerComponent>.Find(buyer);
		ctx.NotNull(licences, "the test character has a licence manager");
		LF_EntityCashWallet wallet = new LF_EntityCashWallet(buyer, LF_LicenceDesk.LEDGER_REASON, string.Empty);
		ctx.False(LF_LicenceDesk.HoldsLicence(buyer, UID, LF_LicenceDesk.DRIVING), "a new character holds no driving licence");

		// Short of the fee: refused, nothing taken.
		LF_MoneyUtils.AddCash(buyer, FEE / 2);
		LF_ELicenceDesk outcome = LF_ELicenceDesk.FAILED;
		if (!RedProof())
			outcome = LF_LicenceDesk.BuyDrivingFor(buyer, UID, wallet);

		ctx.Equal(LF_ELicenceDesk.NOT_ENOUGH_CASH, outcome, "short of the fee, the driving licence is refused");
		ctx.EqualStr("#LF-LicenceDesk_NoCash", LF_LicenceDesk.DrivingAnswerKey(outcome), "the refusal names the fee");
		ctx.Equal(FEE / 2, LF_MoneyUtils.GetCash(buyer), "a short fee takes nothing");
		ctx.False(LF_LicenceDesk.HoldsLicence(buyer, UID, LF_LicenceDesk.DRIVING), "and grants nothing");

		// With the fee: granted at once, the fee taken once.
		LF_MoneyUtils.AddCash(buyer, FEE);
		outcome = LF_ELicenceDesk.FAILED;
		if (!RedProof())
			outcome = LF_LicenceDesk.BuyDrivingFor(buyer, UID, wallet);

		ctx.Equal(LF_ELicenceDesk.ISSUED, outcome, "with the fee in cash the desk grants the driving licence");
		ctx.Equal(FEE / 2, LF_MoneyUtils.GetCash(buyer), "the fee is taken");
		ctx.True(LF_LicenceDesk.HoldsLicence(buyer, UID, LF_LicenceDesk.DRIVING), "the body holds the driving licence");
		ctx.True(licences && LF_LicenseBits.Has(LF_LicenseBits.Encode(licences.GetUnlockedLicenses()), LF_LicenceDesk.DRIVING), "the driving licence has its own replication bit");
		ctx.Equal(-1, LF_LicenceDesk.DueMinutes(UID), "no application is opened: no wait");

		// A second request: refused as held, nothing taken.
		outcome = LF_LicenceDesk.BuyDrivingFor(buyer, UID, wallet);
		ctx.Equal(LF_ELicenceDesk.ALREADY_HELD, outcome, "a second request is refused as already held");
		ctx.EqualStr("#LF-LicenceDesk_DrivingAlreadyHeld", LF_LicenceDesk.DrivingAnswerKey(outcome), "the refusal says it is held");
		ctx.Equal(FEE / 2, LF_MoneyUtils.GetCash(buyer), "and charges nothing again");
		ctx.False(LF_LicenceDesk.Holds(buyer, UID), "the driving licence is not the firearm permit");

		// The save record round trip (in-session): the licence list is written and read back by name.
		array<LF_ELicenseType> stored = {};
		LF_Progression.GetLicenses(UID, stored);
		ctx.True(stored.Contains(LF_ELicenseType.DRIVING_LICENCE), "the saved licence list holds the driving licence");
		array<ref LF_ProgressionRecord> exported = LF_Progression.ExportAll();
		LF_Progression.Forget(UID);
		LF_Progression.GetLicenses(UID, stored);
		ctx.False(stored.Contains(LF_ELicenseType.DRIVING_LICENCE), "forgotten, the store holds no driving licence");
		LF_Progression.ApplyAll(exported);
		LF_Progression.GetLicenses(UID, stored);
		ctx.True(stored.Contains(LF_ELicenseType.DRIVING_LICENCE), "the driving licence survives the save record round trip");

		LF_Progression.Forget(UID);
		LF_LicencesConfig.ResetStatics();
		LF_TestCharacterFixture.Delete(buyer);
	}
}
