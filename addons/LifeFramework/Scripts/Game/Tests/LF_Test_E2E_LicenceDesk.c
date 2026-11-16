// red-proof: RED-OK e2e/licence-desk, observed 2026-11-16 with `tools\cli test --no-build --red LF_Test_E2E_LicenceDesk`:
// with the in-game day not advanced, `- at the due minute the permit is issued (expected 0, got 2)`. Hook off: green.
// tier: PERSISTENCE
//! The licence desk across a restart (#1766, `tools\cli wt e2e <slug>`). The save boot leaves an
//! open application (due one in-game day after T0) and a second application the police revoked.
//! The reload boot (LF_E2E_LOAD_BOOT) finds the first still waiting and issues it at the due minute;
//! the revoked one stays gone, so the due minute issues nothing. Held permits are not checked across
//! the restart: the shared e2e restarts with keepLicences off (LF_Test_PersistSwitchesRestart),
//! which clears every saved licence by design. In a normal suite run both halves run in one boot.
class LF_Test_E2E_LicenceDesk : LF_Test
{
	protected static const string APPLICANT = "e2e-licence-applicant";
	protected static const string REVOKED = "e2e-licence-revoked";
	protected static const int T0 = 14400000;
	protected static const int FEE = 300;

	override string GetName()
	{
		return "e2e/licence-desk";
	}

	override void Run(LF_TestContext ctx)
	{
		int left;
		LF_LicenceDesk.SetNowForTest(T0);
#ifndef LF_E2E_LOAD_BOOT
		Seed(ctx);
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

#ifdef LF_E2E_LOAD_BOOT
		LF_Progression.Forget(APPLICANT);
		LF_Progression.Forget(REVOKED);
#else
		// Leave the save boot's state for the shutdown save: seed it again after the check above.
		LF_Progression.Forget(APPLICANT);
		LF_Progression.Forget(REVOKED);
		Seed(ctx);
#endif
		LF_LicenceDesk.SetNowForTest(-1);
		LF_LicencesConfig.SetDeskForTest(-1, -1);
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
