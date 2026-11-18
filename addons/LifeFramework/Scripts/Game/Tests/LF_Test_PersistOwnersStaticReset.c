// red-proof: 2026-11-17 `tools\cli test --no-build --red LF_Test_PersistOwnersStaticReset,LF_Test_LogoutBodyUnpaidTakeBack`: RED-OK persist/owners-static-reset, `a world end drops the bank accounts` fails with the hooks skipped. Green with the hooks (--only).
// tier: LOGIC
//! Bank accounts, wanted reports and pending farm rows are saved stores with no world-end reset
//! before this hook (#review 2026-11-17): in one process (a scenario restart, a Workbench play) the
//! next world started with the last world's balances and posters and wrote them into its own save.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_PersistOwnersStaticReset : LF_Test
{
	protected static const string UID = "persist-owners-reset-uid";

	override string GetName()
	{
		return "persist/owners-static-reset";
	}

	override void Run(LF_TestContext ctx)
	{
		// Keep what the session holds: the hooks drop every account and report. The farm hook also
		// forgets the live plot components, so it is only checked to be registered.
		array<ref LF_BankAccountRecord> bankBefore = LF_ATMManager.ExportAll();
		array<ref LF_WantedReportRecord> wantedBefore = LF_WantedReports.ExportAll();

		LF_ATMManager atm = LF_ATMManager.GetInstance();
		atm.CreateAccount(UID);
		atm.Deposit(UID, 90);
		ctx.NotNull(atm.GetAccount(UID), "the bank holds the test account");

		array<ref LF_StaticResetHook> hooks = {new LF_ATMManager_StaticResetHook(), new LF_WantedReports_StaticResetHook()};
		if (!RedProof())
		{
			foreach (LF_StaticResetHook hook : hooks)
				hook.Apply();
		}

		ctx.True(LF_ATMManager.GetInstance().GetAccount(UID) == null, "a world end drops the bank accounts");
		ctx.True(LF_StaticReset.Registered().Contains("LF_ATMManager"), "the bank registers its reset hook");
		ctx.True(LF_StaticReset.Registered().Contains("LF_WantedReports"), "the wanted reports register their reset hook");
		ctx.True(LF_StaticReset.Registered().Contains("LF_FarmPlots"), "the farm plots register their reset hook");

		LF_ATMManager.ApplyAll(bankBefore);
		LF_WantedReports.ApplyAll(wantedBefore);
		if (LF_ATMManager.GetInstance().GetAccount(UID))
			LF_ATMManager.GetInstance().Withdraw(UID, 90);
	}
}
