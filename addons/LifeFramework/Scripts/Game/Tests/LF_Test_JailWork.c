// red-proof: Observed 2026-11-11 with `toolsi test --no-build --only LF_Test_JailWork`: replaced the cooldown test in
// LF_JailWorkRules.Check with `if (false)`; got `- a second task inside the cooldown is refused (expected 3, got 0)`
// and `- the second scrub takes 30 more seconds off`. Restored; green.

// tier: LOGIC
class LF_Test_JailWork : LF_Test
{
	override string GetName()
	{
		return "jail/work";
	}

	override void Run(LF_TestContext ctx)
	{
		string officerUid = "jail-work-officer";
		string prisonerUid = "jail-work-prisoner";
		string freeUid = "jail-work-free";

		LF_Jail.Configure(60, 300, 1, 5);
		LF_Jail.SetCellOrigin("48 0 64");
		LF_JailWork.ResetStatics();

		LF_PlayerAccount officer = LF_PlayerAccountManager.GetOrCreate(officerUid);
		officer.SetFaction(LF_Faction.POLICE);
		officer.SetOnDuty(true);
		LF_PlayerAccount prisoner = LF_PlayerAccountManager.GetOrCreate(prisonerUid);
		prisoner.SetWantedLevel(3);

		ctx.True(LF_Jail.Arrest(officerUid, prisonerUid, null), "the arrest starts a 180 second sentence");
		ctx.EqualFloat(180, LF_Jail.GetRemaining(prisonerUid), 0.01, "the sentence starts at 180 seconds");

		ctx.Equal(LF_EJailWorkCheck.OK, LF_JailWork.Complete(prisonerUid, "scrub", 30, 45, 100), "one task is accepted");
		ctx.EqualFloat(150, LF_Jail.GetRemaining(prisonerUid), 0.01, "one task takes the configured 30 seconds off, once");

		ctx.Equal(LF_EJailWorkCheck.COOLDOWN, LF_JailWork.Complete(prisonerUid, "scrub", 30, 45, 120), "a second task inside the cooldown is refused");
		ctx.EqualFloat(150, LF_Jail.GetRemaining(prisonerUid), 0.01, "a refused task takes nothing off");

		ctx.Equal(LF_EJailWorkCheck.OK, LF_JailWork.Complete(prisonerUid, "laundry", 20, 45, 120), "another job has its own cooldown");
		ctx.EqualFloat(130, LF_Jail.GetRemaining(prisonerUid), 0.01, "the laundry task takes its own 20 seconds off");

		ctx.Equal(LF_EJailWorkCheck.OK, LF_JailWork.Complete(prisonerUid, "scrub", 30, 45, 145), "the same job works again once the cooldown has run");
		ctx.EqualFloat(100, LF_Jail.GetRemaining(prisonerUid), 0.01, "the second scrub takes 30 more seconds off");

		ctx.Equal(LF_EJailWorkCheck.NOT_SERVING, LF_JailWork.Complete(freeUid, "scrub", 30, 45, 500), "a player who is not serving gains nothing");
		ctx.Equal(LF_EJailWorkCheck.BAD_CONFIG, LF_JailWork.Complete(prisonerUid, "scrub", 0, 45, 900), "a job with no seconds is refused");
		ctx.EqualFloat(100, LF_Jail.GetRemaining(prisonerUid), 0.01, "refusals leave the remaining time alone");

		// The pure rule on its own.
		ctx.Equal(LF_EJailWorkCheck.BUSY, LF_JailWorkRules.Check(true, true, 30, -1, 10, 45), "a prisoner already working is refused");
		ctx.EqualFloat(15, LF_JailWorkRules.CooldownLeft(100, 130, 45), 0.01, "cooldown left counts down");
		ctx.EqualFloat(0, LF_JailWorkRules.CooldownLeft(100, 200, 45), 0.01, "cooldown left stops at zero");
		ctx.EqualStr("2:05", LF_JailWorkRules.FormatTime(125), "time left reads m:ss");
		ctx.EqualStr("0:07", LF_JailWorkRules.FormatTime(6.2), "a fraction rounds up");

		// Work that would run past zero ends the sentence once, never below zero.
		LF_JailWork.ResetStatics();
		ctx.Equal(LF_EJailWorkCheck.OK, LF_JailWork.Complete(prisonerUid, "big", 500, 45, 1000), "a long task is accepted");
		ctx.False(LF_Jail.IsServing(prisonerUid), "a task past the remaining time ends the sentence");
		ctx.Equal(1, LF_Jail.GetReleaseCount(prisonerUid), "the sentence ends once");
		ctx.Equal(LF_EJailWorkCheck.NOT_SERVING, LF_JailWork.Complete(prisonerUid, "big", 500, 45, 2000), "a released prisoner cannot work");

		LF_Jail.SyncPlacedCell();
	}
}
