// red-proof: 2026-11-19 `tools/cli test --no-build --red LF_Test_EveronHeistBankRun` printed `RED-OK everon/heist-bank-run (fails with the hook on, 5 assertions)`; RedProof() builds the site with no heist id, so no run comes from the pack job.
//! #3748 in game: under the Everon 1989 header the bank heist site builds its run from the pack's
//! `bank` job (staff door, the town bank's vault, 120 m getaway, 12 minute clock, two police on
//! duty, clerk hands-up), and arming the staff door is refused with fewer than two police on duty.
[LF_TestInfo(LF_TestTier.LOGIC, pack: true)]
class LF_Test_EveronHeistBankRun : LF_Test
{
	static const string BANK = "bank";
	static const string STAFF_DOOR = "bank_staff_door";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/heist-bank-run";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		string heistId = BANK;
		if (RedProof())
			heistId = "";

		LF_HeistRun run = LF_HeistRunComponent.BuildRun("bank-site", heistId, "", "", "0 0 0", 40, 600);
		ctx.NotNull(run, "the bank site builds its run from the pack job");
		if (run)
		{
			ctx.EqualStr(STAFF_DOOR, run.GetDoorId(), "stage one is the staff door");
			ctx.EqualStr("bank_vault", run.GetVaultId(), "stage two is the town bank's vault");
			ctx.True(run.GetRadius() == 120, "the getaway is 120 m out");
			ctx.Equal(720000, run.GetClockMs(), "the clock is twelve minutes");
			ctx.Equal(2, run.GetMinPolice(), "the job needs two police on duty");
			ctx.True(run.ClerkMayHandsUp(), "the clerk may choose hands up");
		}

		ctx.EqualStr(LF_HeistRun.NEED_POLICE, LF_HeistRun.ArmRefusal(STAFF_DOOR, 1), "one officer on duty: the staff door is refused");
		ctx.EqualStr("", LF_HeistRun.ArmRefusal(STAFF_DOOR, 2), "two officers on duty: the staff door may be armed");
		ctx.EqualStr("", LF_HeistRun.ArmRefusal("bank_vault", 0), "the vault door is not a job's first stage: no heist refusal");
	}
}
