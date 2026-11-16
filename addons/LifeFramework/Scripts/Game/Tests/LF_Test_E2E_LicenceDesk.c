// red-proof: RED-OK e2e/licence-desk, observed 2026-11-16 with `tools\cli test --no-build --red LF_Test_E2E_LicenceDesk`:
// with the in-game day not advanced, `- at the due minute the permit is issued (expected 0, got 2)`. Hook off: green.
// red-proof: RED-OK e2e/licence-desk (#1995), observed 2026-11-16 with the same batched `--red` run:
// with the in-game day not advanced, `- a conviction since the application holds the permit back at collection (expected 6, got 2)`. Hook off: green.
// tier: PERSISTENCE
//! The licence desk across a restart (#1766, `tools\cli wt e2e <slug>`). The save boot leaves an
//! open application (due one in-game day after T0) and a second application the police revoked.
//! The reload boot (LF_E2E_LOAD_BOOT) finds the first still waiting and issues it at the due minute;
//! the revoked one stays gone, so the due minute issues nothing. Held permits are not checked across
//! the restart: the shared e2e restarts with keepLicences off (LF_Test_PersistSwitchesRestart),
//! which clears every saved licence by design. In a normal suite run both halves run in one boot.
//! #1995 / #1914: two more accounts are convicted on the real game clock in the save boot (one before
//! applying, one after applying and paying). The reload boot finds both convictions back from the
//! "convictions" payload and the desk still refuses the first at application and holds back the
//! second's permit at collection.
class LF_Test_E2E_LicenceDesk : LF_Test
{
	protected static const string APPLICANT = "e2e-licence-applicant";
	protected static const string REVOKED = "e2e-licence-revoked";
	protected static const string CONVICTED_FIRST = "e2e-licence-convicted-first";
	protected static const string CONVICTED_AFTER = "e2e-licence-convicted-after";
	protected static const int T0 = 14400000;
	protected static const int FEE = 300;

	override string GetName()
	{
		return "e2e/licence-desk";
	}

	override void Run(LF_TestContext ctx)
	{
		int left;
		array<ref LF_ConvictionRow> convictions = LF_ConvictionRecord.ExportAll();
		LF_LicenceDesk.SetNowForTest(T0);
#ifndef LF_E2E_LOAD_BOOT
		Seed(ctx);
		SeedConvictions(ctx);
#endif
		ctx.Equal(T0 + 1440, LF_LicenceDesk.DueMinutes(APPLICANT), "the open application keeps its due minute");
		LF_TestCashWallet wallet = LF_TestCashWallet.Create(1000, 1000);
		LF_LicenceDesk.SetNowForTest(T0 + 100);
		ctx.Equal(LF_ELicenceDesk.WAITING, LF_LicenceDesk.VisitFor(null, APPLICANT, wallet, left), "before the day is up the applicant still waits");
		ctx.Equal(1340, left, "with the rest of the day left");
		if (!RedProof())
			LF_LicenceDesk.SetNowForTest(T0 + 1440);

		ctx.Equal(LF_ELicenceDesk.ISSUED, LF_LicenceDesk.VisitFor(null, APPLICANT, wallet, left), "at the due minute the permit is issued");
		ctx.Equal(1000, wallet.Carried(), "collecting costs nothing more");
		ctx.True(LF_LicenceDesk.Holds(null, APPLICANT), "the applicant holds the permit");
		ctx.Equal(-1, LF_LicenceDesk.DueMinutes(REVOKED), "the revoked application stays revoked");
		ctx.True(LF_LicenceDesk.VisitFor(null, REVOKED, LF_TestCashWallet.Create(0, 0), left) != LF_ELicenceDesk.ISSUED, "at the due minute the revoked application issues nothing");
		ctx.False(LF_LicenceDesk.Holds(null, REVOKED), "so no permit is held");

		// The convictions: still on the record (the reload boot read them from the save) and still binding.
		ctx.True(LF_ConvictionRecord.IsRecent(CONVICTED_FIRST, LF_LicencesConfig.PermitConvictionDays()), "the first conviction is on the record");
		ctx.True(LF_ConvictionRecord.IsRecent(CONVICTED_AFTER, LF_LicencesConfig.PermitConvictionDays()), "the second conviction is on the record");
		LF_TestCashWallet convictedWallet = LF_TestCashWallet.Create(1000, 1000);
		ctx.Equal(LF_ELicenceDesk.CONVICTED, LF_LicenceDesk.VisitFor(null, CONVICTED_FIRST, convictedWallet, left), "a convicted applicant is refused at application");
		ctx.Equal(1000, convictedWallet.Carried(), "and pays nothing");
		ctx.Equal(-1, LF_LicenceDesk.DueMinutes(CONVICTED_FIRST), "and opens no application");
		ctx.Equal(T0 + 1440, LF_LicenceDesk.DueMinutes(CONVICTED_AFTER), "the second applicant's open application kept its due minute");
		ctx.Equal(LF_ELicenceDesk.CONVICTED, LF_LicenceDesk.VisitFor(null, CONVICTED_AFTER, convictedWallet, left), "a conviction since the application holds the permit back at collection");
		ctx.False(LF_LicenceDesk.Holds(null, CONVICTED_AFTER), "so the permit is not held");

#ifdef LF_E2E_LOAD_BOOT
		LF_Progression.Forget(APPLICANT);
		LF_Progression.Forget(REVOKED);
		LF_Progression.Forget(CONVICTED_FIRST);
		LF_Progression.Forget(CONVICTED_AFTER);
#else
		// Leave the save boot's state for the shutdown save: seed it again after the check above.
		LF_Progression.Forget(APPLICANT);
		LF_Progression.Forget(REVOKED);
		Seed(ctx);
#endif
#ifndef LF_E2E_HOST_STOP
		// Not the save boot: leave no test account on the record or in the store.
		LF_ConvictionRecord.ApplyAll(convictions);
		LF_Progression.Forget(CONVICTED_FIRST);
		LF_Progression.Forget(CONVICTED_AFTER);
#endif
		LF_LicenceDesk.SetNowForTest(-1);
		LF_LicencesConfig.SetDeskForTest(-1, -1);
	}

	//------------------------------------------------------------------------------------------------
	//! Two convictions on the real game clock (the "convictions" payload keeps the day across the
	//! restart): one before applying, one after applying and paying. Once per boot.
	protected void SeedConvictions(LF_TestContext ctx)
	{
		int left;
		LF_LicencesConfig.SetDeskForTest(FEE, 1);
		LF_LicenceDesk.SetNowForTest(T0);
		LF_ConvictionRecord.Record(CONVICTED_FIRST);
		ctx.Equal(LF_ELicenceDesk.CONVICTED, LF_LicenceDesk.VisitFor(null, CONVICTED_FIRST, LF_TestCashWallet.Create(FEE, FEE), left), "a convicted player is refused at application");
		ctx.Equal(LF_ELicenceDesk.APPLIED, LF_LicenceDesk.VisitFor(null, CONVICTED_AFTER, LF_TestCashWallet.Create(FEE, FEE), left), "a clean player applies");
		LF_ConvictionRecord.Record(CONVICTED_AFTER);
	}

	//------------------------------------------------------------------------------------------------
	protected void Seed(LF_TestContext ctx)
	{
		int left;
		LF_LicenceDesk.SetNowForTest(T0);
		LF_LicencesConfig.SetDeskForTest(FEE, 1);
		ctx.Equal(LF_ELicenceDesk.APPLIED, LF_LicenceDesk.VisitFor(null, REVOKED, LF_TestCashWallet.Create(FEE, FEE), left), "the second player applies");
		ctx.True(LF_LicenceDesk.RevokeFor(null, REVOKED, "e2e-officer"), "the police revoke the second application");
		ctx.Equal(-1, LF_LicenceDesk.DueMinutes(REVOKED), "the revoked application is closed");
		LF_TestCashWallet wallet = LF_TestCashWallet.Create(FEE, FEE);
		ctx.Equal(LF_ELicenceDesk.APPLIED, LF_LicenceDesk.VisitFor(null, APPLICANT, wallet, left), "the applicant applies");
		ctx.Equal(0, wallet.Carried(), "the fee is taken");
		LF_LicenceDesk.SetNowForTest(T0);
	}
}
