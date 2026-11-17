// red-proof: observed 2026-11-18 with `test --no-build --red LF_Test_TrialExpiryDuty,LF_Test_TrialExpiryClockOut` (RED-OK, log tmp/logs/test-20261118-004832.log): the hook expires the trial before the settle (the old order), so 'the seconds earned before the trial ended promote the officer (expected 1, got 0)' and 'the promotion ended the trial: the roster row stays' fail. Green with the fix.
// tier: LOGIC
//! police/trial-expiry-duty (#2416): which on-duty officers the trial sweep ends, and in what
//! order. A promotion earned in seconds the clock had not settled yet ends the trial before the
//! roster row goes. A promoted officer, an officer still on trial and one an admin put back on the
//! roster stay on duty; with no body the sweep changes nothing (the clock-out needs the body).
class LF_Test_TrialExpiryDuty : LF_Test
{
	protected static const string EXPIRED = "trial-duty-expired";
	protected static const string PROMOTED = "trial-duty-promoted";
	protected static const string RUNNING = "trial-duty-running";
	protected static const string RESTORED = "trial-duty-restored";
	protected static const string EARNER = "trial-duty-earner";
	protected static const int START = 2000000;
	protected static const int HOUR = 3600;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "police/trial-expiry-duty";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		string path = "$profile:LifeFramework/test_trial_duty.txt";
		LF_PoliceApplications.SetSavePath(path);
		LF_WhitelistManager.SetSavePath("$profile:LifeFramework/test_trial_duty_whitelist.txt");
		LF_WhitelistManager.Reset();
		FileIO.DeleteFile(path);

		Apply(ctx, EXPIRED, 168);
		Apply(ctx, PROMOTED, 168);
		Apply(ctx, RUNNING, 500);
		Apply(ctx, RESTORED, 168);
		Apply(ctx, EARNER, 168);
		LF_PoliceApplications.EndTrial(PROMOTED);

		LF_PlayerAccount expired = OnDuty(EXPIRED);
		LF_PlayerAccount promoted = OnDuty(PROMOTED);
		LF_PlayerAccount running = OnDuty(RUNNING);
		LF_PlayerAccount restored = OnDuty(RESTORED);
		LF_PlayerAccount earner = OnDuty(EARNER);

		ctx.False(LF_PoliceTrialSweep.ShouldEnd(expired), "before the trial ends nobody is ended");

		// The earner has been on duty long enough for the next rank since the last settle. The
		// sweep settles first, so the promotion ends the trial before the roster row can go.
		int needed = LF_PoliceRanks.DutySecondsForNext(LF_PoliceRanks.PATROL);
		earner.StartDutyClock(-(needed + 100) * 1000);
		array<LF_PlayerAccount> online = {earner};
		if (!RedProof())
			LF_PoliceTrialSweep.ExpireAt(START + 168 * HOUR, online);
		else
			LF_PoliceApplications.ExpireTrials(START + 168 * HOUR);

		ctx.Equal(LF_PoliceRanks.PATROL + 1, earner.GetPoliceRank(), "the seconds earned before the trial ended promote the officer");
		ctx.False(LF_PoliceApplications.TrialExpiredFor(EARNER), "the promotion ended the trial: the roster row stays");
		ctx.False(LF_PoliceTrialSweep.ShouldEnd(earner), "the promoted earner is not ended");

		LF_WhitelistManager.AddFactionWhitelist(RESTORED, LF_EFactionType.POLICE);

		ctx.True(LF_PoliceApplications.TrialExpiredFor(EXPIRED), "the roster remembers the expired trial");
		ctx.False(LF_PoliceApplications.TrialExpiredFor(PROMOTED), "a promoted officer's trial did not expire");
		ctx.False(LF_PoliceApplications.TrialExpiredFor(RUNNING), "a running trial did not expire");
		ctx.True(LF_PoliceTrialSweep.ShouldEnd(expired), "an on-duty officer whose trial ended is to be ended");
		ctx.False(LF_PoliceTrialSweep.ShouldEnd(promoted), "a promoted officer stays");
		ctx.False(LF_PoliceTrialSweep.ShouldEnd(running), "an officer still on trial stays");
		ctx.False(LF_PoliceTrialSweep.ShouldEnd(restored), "an officer an admin put back on the roster stays");

		// Without a body (the spawn screen) the desk clock-out cannot run: nothing changes.
		ctx.False(LF_PoliceTrialSweep.EndIfExpired(0, expired, null), "with no body the officer is not ended yet");
		ctx.True(expired.IsOnDuty(), "the officer is still on duty, to be ended by a later sweep");
		ctx.True(expired.GetFaction() == LF_Faction.POLICE, "the faction is untouched");

		ctx.False(LF_PoliceTrialSweep.EndIfExpired(0, promoted, null), "the promoted officer is not ended");
		ctx.True(promoted.IsOnDuty(), "the promoted officer is still on duty");
		ctx.True(running.IsOnDuty(), "the officer on trial is still on duty");
		ctx.True(restored.IsOnDuty(), "the restored officer is still on duty");

		expired.SetOnDuty(false);
		promoted.SetOnDuty(false);
		running.SetOnDuty(false);
		restored.SetOnDuty(false);
		earner.SetOnDuty(false);
		earner.StopDutyClock();
		earner.SetPoliceRank(LF_PoliceRanks.PATROL);
		earner.SetOnDutySeconds(0);
		FileIO.DeleteFile(path);
		FileIO.DeleteFile("$profile:LifeFramework/test_trial_duty_whitelist.txt");
		LF_PoliceApplications.SetSavePath("");
		LF_WhitelistManager.SetSavePath("");
		LF_WhitelistManager.Reset();
	}

	//------------------------------------------------------------------------------------------------
	protected void Apply(LF_TestContext ctx, string uid, int trialHours)
	{
		ctx.EqualStr("", LF_PoliceApplications.Submit(uid, "Officer " + uid, "To help", "Evenings", false), "application filed for " + uid);
		ctx.True(LF_PoliceApplications.ApproveAt(uid, START, trialHours), "approved on trial: " + uid);
	}

	//------------------------------------------------------------------------------------------------
	protected LF_PlayerAccount OnDuty(string uid)
	{
		LF_PlayerAccount account = LF_PlayerAccountManager.GetOrCreate(uid);
		account.SetFaction(LF_Faction.POLICE);
		account.SetPoliceRank(LF_PoliceRanks.PATROL);
		account.SetOnDutySeconds(0);
		account.SetOnDuty(true);
		return account;
	}
}
