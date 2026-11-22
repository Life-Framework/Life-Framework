// red-proof: RED-OK licence/truck-check, observed 2026-11-22 with `tools/cli test --no-build --red LF_Test_TruckCheck`: with Start skipped `a run is open`, `the run expects gate 0 (expected 0, got -1)` and `a second start while running is refused` fail. Hook off: green (--only).
//! licence/truck-check (WK-W18, N13): the truck licence check. Starting takes the $300 fee (short cash
//! is refused, nothing taken); the course passes on the gates in order and grants the licence; a gate
//! out of order fails the run and sets a 10-minute retake, refused until it ends and the fee taken
//! again on the retake; a held licence is refused and charges nothing.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_TruckCheck : LF_Test
{
	protected static const string UID = "lf-test-truck-check";
	protected static const int T0 = 1000000;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "licence/truck-check";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_Progression.Forget(UID);
		LF_DrivingCheck.ResetStatics();
		LF_LicenceDesk.SetNowForTest(T0);
		ctx.Equal(300, LF_DrivingCheck.FEE, "the truck check fee is $300");
		ctx.Equal(10, LF_DrivingCheck.RETAKE_MINUTES, "the retake wait is 10 minutes");

		int left;
		LF_TestCashWallet wallet = LF_TestCashWallet.Create(LF_DrivingCheck.FEE - 1, 100000);

		// Short of the fee: refused, nothing taken, no run.
		LF_EDrivingCheck outcome = LF_EDrivingCheck.FAILED;
		if (!RedProof())
			outcome = LF_DrivingCheck.StartFor(UID, wallet, left);

		ctx.Equal(LF_EDrivingCheck.NOT_ENOUGH_CASH, outcome, "short of the fee, the check is refused");
		ctx.Equal(LF_DrivingCheck.FEE - 1, wallet.Carried(), "a short fee takes nothing");
		ctx.False(LF_DrivingCheck.IsRunning(UID), "and opens no run");

		// With the fee: the fee is taken once and the run is open at gate 0.
		wallet.Put(LF_DrivingCheck.FEE);
		outcome = LF_EDrivingCheck.FAILED;
		if (!RedProof())
			outcome = LF_DrivingCheck.StartFor(UID, wallet, left);

		ctx.Equal(LF_EDrivingCheck.STARTED, outcome, "with the fee the check starts");
		ctx.Equal(LF_DrivingCheck.FEE - 1, wallet.Carried(), "the fee is taken once");
		ctx.True(LF_DrivingCheck.IsRunning(UID), "a run is open");
		ctx.Equal(0, LF_DrivingCheck.NextGate(UID), "the run expects gate 0");
		ctx.Equal(LF_EDrivingCheck.ALREADY_RUNNING, LF_DrivingCheck.StartFor(UID, wallet, left), "a second start while running is refused");

		// A gate out of order fails the run: 10-minute retake, no licence, no refund.
		outcome = LF_DrivingCheck.ReachGate(UID, 2);
		ctx.Equal(LF_EDrivingCheck.FAILED_COURSE, outcome, "a gate out of order fails the course");
		ctx.False(LF_DrivingCheck.IsRunning(UID), "the failed run is closed");
		ctx.False(LF_DrivingCheck.HoldsTruckLicence(UID), "a failed course grants nothing");
		ctx.Equal(LF_DrivingCheck.FEE - 1, wallet.Carried(), "a failed course refunds nothing");
		ctx.Equal(LF_EDrivingCheck.NO_RUN, LF_DrivingCheck.ReachGate(UID, 0), "a gate with no run open is ignored");
		ctx.Equal(T0 + LF_DrivingCheck.RETAKE_MINUTES, LF_DrivingCheck.RetryMinute(UID), "the retake opens 10 minutes on");

		// Before the wait ends: refused, nothing taken.
		wallet.Put(LF_DrivingCheck.FEE);
		LF_LicenceDesk.SetNowForTest(T0 + 4);
		outcome = LF_DrivingCheck.StartFor(UID, wallet, left);
		ctx.Equal(LF_EDrivingCheck.RETAKE_WAIT, outcome, "a retake inside the wait is refused");
		ctx.Equal(6, left, "the desk says 6 minutes remain");
		ctx.Equal(LF_DrivingCheck.FEE * 2 - 1, wallet.Carried(), "a refused retake takes nothing");

		// After the wait: the fee is taken again; the gates in order pass and grant the licence.
		LF_LicenceDesk.SetNowForTest(T0 + LF_DrivingCheck.RETAKE_MINUTES);
		outcome = LF_DrivingCheck.StartFor(UID, wallet, left);
		ctx.Equal(LF_EDrivingCheck.STARTED, outcome, "once the wait ends the retake starts");
		ctx.Equal(LF_DrivingCheck.FEE - 1, wallet.Carried(), "the retake takes the fee again");
		ctx.Equal(-1, LF_DrivingCheck.RetryMinute(UID), "the wait flag is cleared");

		int gate;
		for (gate = 0; gate < LF_DrivingCheck.GATE_COUNT - 1; gate++)
		{
			ctx.Equal(LF_EDrivingCheck.GATE_OK, LF_DrivingCheck.ReachGate(UID, gate), "gate " + gate.ToString() + " in order moves the run on");
			ctx.False(LF_DrivingCheck.HoldsTruckLicence(UID), "no licence before the last gate");
		}

		outcome = LF_DrivingCheck.ReachGate(UID, LF_DrivingCheck.GATE_COUNT - 1);
		ctx.Equal(LF_EDrivingCheck.PASSED, outcome, "the last gate in order passes the course");
		ctx.True(LF_DrivingCheck.HoldsTruckLicence(UID), "passing grants the truck licence");
		ctx.False(LF_DrivingCheck.IsRunning(UID), "the passed run is closed");

		// The licence is in the saved progression record and survives the save round trip (in-session).
		array<ref LF_ProgressionRecord> exported = LF_Progression.ExportAll();
		LF_Progression.Forget(UID);
		ctx.False(LF_DrivingCheck.HoldsTruckLicence(UID), "forgotten, the store holds no truck licence");
		LF_Progression.ApplyAll(exported);
		ctx.True(LF_DrivingCheck.HoldsTruckLicence(UID), "the truck licence survives the save record round trip");

		// A holder is refused and charged nothing.
		outcome = LF_DrivingCheck.StartFor(UID, wallet, left);
		ctx.Equal(LF_EDrivingCheck.ALREADY_HELD, outcome, "a holder is refused");
		ctx.Equal(LF_DrivingCheck.FEE - 1, wallet.Carried(), "and charged nothing");

		LF_Progression.Forget(UID);
		LF_DrivingCheck.ResetStatics();
		LF_LicenceDesk.ResetStatics();
	}
}
