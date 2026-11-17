// red-proof: #2408 follow-up. `--red` skips the refund (ApplyDeposits is not called), so 'the restart gives
// the held deposit back to the bank' fails. On the unfixed code ExportDeposits and ApplyDeposits did not exist
// (nothing was saved, the deposit stayed taken); the restart proof is `wt e2e` (pass 2 reads the bank).
// Observed 2026-11-18 `--red LF_Test_StoreStrangerEject,LF_Test_JobDepositRestart`: RED PROOF OK (expected 650, got 500). Restart proof: `tools/cli wt e2e fix-review-followups`, bank 500 -> 650 (boot save version 49). #2481: rows are LF_JobRunRow (jobruns, save version 49; the separate jobdeposits payload never shipped); re-observed 2026-11-18 `--red`: RED-OK jobs/deposit-restart (17 assertions); the cargo half is LF_Test_JobDepositCargo.
// tier: LOGIC
//! A server restart ends a cargo run; the deposit it held goes back to the player's bank account on
//! the load instead of being kept. In-session round trip of the rows only: the restart itself is `wt e2e`.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_JobDepositRestart : LF_Test
{
	protected static const string UID = "job-deposit-restart";
	protected static const string LIVE_UID = "job-deposit-live";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "jobs/deposit-restart";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ATMManager atm = LF_ATMManager.GetInstance();
		atm.CreateAccount(UID);
		atm.CreateAccount(LIVE_UID);
		ctx.True(atm.Deposit(UID, 500), "the courier banks 500");
		ctx.True(atm.Deposit(LIVE_UID, 500), "the second courier banks 500");

		LF_CivilianJob.ForgetCareer(UID);
		LF_CivilianJob.ForgetCareer(LIVE_UID);
		ctx.True(LF_CivilianJob.SignUp(UID, LF_EJobType.PARCEL), "the courier signs up");
		ctx.True(LF_CivilianJob.StartRun(UID, LF_EJobType.PARCEL), "the run starts");
		LF_CivilianJob.RecordDeposit(UID, 150);

		array<ref LF_JobRunRow> rows = LF_CivilianJob.ExportDeposits();
		int mine = 0;
		foreach (LF_JobRunRow row : rows)
		{
			if (row.m_sPlayerUid == UID)
			{
				mine++;
				ctx.Equal(150, row.m_iAmount, "the saved row holds the deposit");
				ctx.Equal(LF_EJobType.PARCEL, row.m_iJob, "the saved row names the job whose cargo was handed out");
			}
		}

		ctx.Equal(1, mine, "the held deposit is saved once");

		// The restart: the run is gone (session state), the rows are what the save holds.
		LF_CivilianJob.AbandonRun(UID);
		ctx.False(LF_CivilianJob.HasActiveRun(UID), "the run is over after the restart");
		ctx.Equal(0, LF_CivilianJob.GetDeposit(UID), "the restart holds no deposit");

		// A row for a player whose run is live in this session (a re-applied load) is not paid twice.
		ctx.True(LF_CivilianJob.SignUp(LIVE_UID, LF_EJobType.PARCEL), "the second courier signs up");
		ctx.True(LF_CivilianJob.StartRun(LIVE_UID, LF_EJobType.PARCEL), "the second run starts");
		LF_CivilianJob.RecordDeposit(LIVE_UID, 70);
		array<ref LF_JobRunRow> loaded = {};
		loaded.Insert(LF_JobRunRow.Create(UID, 150, LF_EJobType.PARCEL));
		loaded.Insert(LF_JobRunRow.Create(LIVE_UID, 70, LF_EJobType.PARCEL));
		loaded.Insert(LF_JobRunRow.Create("", 99, LF_EJobType.PARCEL));
		loaded.Insert(LF_JobRunRow.Create(UID, 0, LF_EJobType.PARCEL));
		int refunded = 0;
		if (!RedProof())
			refunded = LF_CivilianJob.ApplyDeposits(loaded);

		ctx.Equal(1, refunded, "only the ended run's deposit is given back");

		ctx.Equal(650, atm.GetAccount(UID).GetBalance(), "the restart gives the held deposit back to the bank");
		ctx.Equal(500, atm.GetAccount(LIVE_UID).GetBalance(), "a live run keeps its own deposit and gets no second one");
		ctx.Equal(70, LF_CivilianJob.GetDeposit(LIVE_UID), "the live run still holds its deposit");

		LF_CivilianJob.AbandonRun(LIVE_UID);
		LF_CivilianJob.ForgetCareer(UID);
		LF_CivilianJob.ForgetCareer(LIVE_UID);
	}
}
