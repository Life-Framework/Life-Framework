// red-proof: 2026-11-23 `tools/cli test --no-build --red LF_Test_HeistBigJobLock` printed `RED-OK crime/heist-big-job-lock (fails with the hook on, 34 assertions)`; RedProof() skips the getaway tick, so the bank run never calls Finish and keeps the lock.
//! #4437: a configured heist job (the bank row) runs under the server-wide big-job lock. With the
//! lock held by `pharmacy`, arming the bank's door is refused with the big-job reason; a completed
//! run calls LF_BigJobLock.Finish and the bank is then in its cooldown (arming refused in words);
//! a run never finished frees the lock after LF_BigJobLock.MAX_RUN_SECONDS. Pure logic: the job
//! rows are built here, the run is driven through its stage methods, and the lock's clock is fixed.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_HeistBigJobLock : LF_Test
{
	protected static const string DOOR = "bigjob-test-door";
	protected static const string VAULT = "bigjob-test-vault";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "crime/heist-big-job-lock";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_HeistStageRow Stage(string kind, string targetId, float radius)
	{
		LF_HeistStageRow stage = new LF_HeistStageRow();
		stage.m_sKind = kind;
		stage.m_sTargetId = targetId;
		stage.m_fRadius = radius;
		return stage;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_HeistSiteRow Job(string id, string doorId, string vaultId)
	{
		LF_HeistSiteRow row = LF_HeistSiteRow.Make(id);
		row.m_iClockSeconds = 720;
		row.m_iMinPoliceOnDuty = 2;
		row.m_aStages.Insert(Stage(LF_HeistKinds.DOOR, doorId, 0));
		row.m_aStages.Insert(Stage(LF_HeistKinds.VAULT, vaultId, 0));
		row.m_aStages.Insert(Stage(LF_HeistKinds.GETAWAY, "", 50));
		return row;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_BigJobLock.ResetStatics();
		LF_HeistRun.ResetStatics();
		int t = 100000;
		LF_BigJobLock.SetClockForTests(t);

		LF_HeistSiteRow bank = Job("bank", DOOR, VAULT);
		string busy = LF_BigJobLock.RefusalKey(LF_EBigJobLock.BUSY);

		// Another big job holds the lock: the bank's door is refused with the big-job reason.
		ctx.Equal(LF_EBigJobLock.OK, LF_BigJobLock.TryBegin("pharmacy", "bigjob-test-medic-robber", LF_BigJobLock.COOLDOWN_PHARMACY, t), "the pharmacy takes the lock");
		ctx.EqualStr(LF_HeistRun.NEED_POLICE, LF_HeistRun.RowArmRefusal(bank, DOOR, 1, t), "one officer on duty: the police minimum refuses first");
		ctx.EqualStr(busy, LF_HeistRun.RowArmRefusal(bank, DOOR, 2, t), "the pharmacy running: arming the bank door is refused as busy");
		ctx.EqualStr("", LF_HeistRun.RowArmRefusal(bank, VAULT, 2, t), "the vault is not the job's door: no refusal");

		// The run's door opening cannot start the bank while the pharmacy holds the lock.
		LF_HeistRun blocked = LF_HeistRun.FromRow("bigjob-site", bank, "0 0 0", 40);
		ctx.NotNull(blocked, "the bank row builds a run");
		if (!blocked)
			return;

		ctx.EqualStr("bank", blocked.GetJobKey(), "the run's big-job key is the row id");
		ctx.False(blocked.DoorOpened("bigjob-test-robber", 1000), "the door opening is refused while another big job runs");
		ctx.Equal(LF_EHeistState.IDLE, blocked.GetState(), "the refused run stays idle");

		ctx.True(LF_BigJobLock.Finish("pharmacy", t), "the pharmacy job ends");
		ctx.EqualStr("", LF_HeistRun.RowArmRefusal(bank, DOOR, 2, t), "the lock free and no bank cooldown: arming may go on");

		// A completed run: the door takes the lock, the getaway releases it into the bank's cooldown.
		LF_HeistRun run = LF_HeistRun.FromRow("bigjob-site", bank, "0 0 0", 40);
		ctx.True(run.DoorOpened("bigjob-test-robber", 1000), "the door opening starts the bank run");
		ctx.True(run.HoldsBigJobLock(), "the bank run holds the lock");
		ctx.EqualStr("bank", LF_BigJobLock.RunningJob(t), "the server-wide lock names the bank");
		ctx.True(run.VaultOpened("bigjob-test-robber", 2000), "the vault opens");
		array<vector> away = {"500 0 500"};
		if (!RedProof())
			ctx.True(run.Tick(3000, away), "the crew out of the area finishes the run");

		ctx.Equal(LF_EHeistState.SUCCEEDED, run.GetState(), "the run is complete");
		ctx.False(run.HoldsBigJobLock(), "the finished run no longer holds the lock");

		ctx.EqualStr("", LF_BigJobLock.RunningJob(t), "the completed run called Finish: the lock is free");
		ctx.Equal(LF_BigJobLock.COOLDOWN_BANK, LF_BigJobLock.CooldownLeft("bank", t), "the bank is in its 3 h cooldown");
		string cooling = LF_HeistRun.RowArmRefusal(bank, DOOR, 2, t);
		ctx.True(!cooling.IsEmpty() && cooling.Get(0) != "#", "the cooldown refusal reaches the player in words");
		ctx.True(cooling.Contains("180"), "the cooldown refusal names the minutes left, not seconds");
		ctx.EqualStr("", LF_HeistRun.RowArmRefusal(bank, DOOR, 2, t + LF_BigJobLock.COOLDOWN_BANK), "after 3 h the bank may be armed again");

		// A run never finished: the lock frees itself after MAX_RUN_SECONDS.
		LF_HeistSiteRow other = Job("bigjob-test-depot", "bigjob-test-depot-door", "bigjob-test-depot-vault");
		LF_HeistRun lost = LF_HeistRun.FromRow("bigjob-depot-site", other, "0 0 0", 40);
		ctx.True(lost.DoorOpened("bigjob-test-robber", 1000), "a second big job starts once the lock is free");
		ctx.EqualStr(busy, LF_HeistRun.RowArmRefusal(bank, DOOR, 2, t), "while it runs, the bank is refused as busy (before its own cooldown)");
		int late = t + LF_BigJobLock.MAX_RUN_SECONDS;
		ctx.EqualStr("", LF_BigJobLock.RunningJob(late), "never finished: the lock is free after MAX_RUN_SECONDS");
		ctx.True(LF_BigJobLock.CooldownLeft("bigjob-test-depot", late) > 0, "the lost job went into its cooldown");

		// A run stopped mid-way (the site deleted) releases its lock at once.
		LF_HeistSiteRow third = Job("bigjob-test-third", "bigjob-test-third-door", "bigjob-test-third-vault");
		LF_HeistRun stopped = LF_HeistRun.FromRow("bigjob-third-site", third, "0 0 0", 40);
		ctx.True(stopped.DoorOpened("bigjob-test-robber", 1000), "a third big job starts");
		stopped.Stop();
		ctx.EqualStr("", LF_BigJobLock.RunningJob(late), "a stopped run frees the lock");

		LF_BigJobLock.ResetStatics();
		LF_HeistRun.ResetStatics();
	}
}
