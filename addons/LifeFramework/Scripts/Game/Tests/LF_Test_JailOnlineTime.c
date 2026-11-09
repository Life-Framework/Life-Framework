// red-proof: Observed 2026-11-08 with `tools\cli test --no-build --only LF_Test_JailOnlineTime`.
// Offline pause: forced `bool online = true;` in LF_Jail.TickOnline; got
// `- the live tick does not count down a prisoner who is not connected (expected 120, got 110)`,
// `- logging off for longer than the sentence keeps the remaining time (expected 110, got 0)`.
// Release once: dropped the `!sentence.m_bServing` guard in LF_Jail.Advance; got
// `- later ticks do not release again (expected 1, got 2)`. Both restored; green.
// manual: dedicated server, two clients. Arrest a wanted player, then log the prisoner off for
// longer than the slip says. Rejoin: the prisoner spawns in the cell and the slip shows the same
// minutes as at logoff.

// tier: LOGIC
class LF_Test_JailOnlineTime : LF_Test
{
	override string GetName()
	{
		return "jail/online-time";
	}

	override void Run(LF_TestContext ctx)
	{
		string officerUid = "jail-online-officer";
		string prisonerUid = "jail-online-prisoner";

		LF_Jail.Configure(60, 300, 1, 5);
		LF_Jail.SetCellOrigin("48 0 64");

		ctx.Equal(0, LF_JailSlipRules.MinutesLeft(0), "no time left reads zero minutes");
		ctx.Equal(1, LF_JailSlipRules.MinutesLeft(1), "one second left reads one minute");
		ctx.Equal(1, LF_JailSlipRules.MinutesLeft(60), "sixty seconds read one minute");
		ctx.Equal(2, LF_JailSlipRules.MinutesLeft(61), "sixty-one seconds read two minutes");

		LF_PlayerAccount officer = LF_PlayerAccountManager.GetOrCreate(officerUid);
		officer.SetFaction(LF_Faction.POLICE);
		officer.SetOnDuty(true);
		LF_PlayerAccount prisoner = LF_PlayerAccountManager.GetOrCreate(prisonerUid);
		prisoner.SetWantedLevel(2);

		ctx.True(LF_Jail.Arrest(officerUid, prisonerUid, null), "the arrest starts a 120 second sentence");
		ctx.EqualFloat(120, LF_Jail.GetRemaining(prisonerUid), 0.01, "the sentence starts at 120 seconds");

		LF_Jail.Tick(10);
		ctx.EqualFloat(120, LF_Jail.GetRemaining(prisonerUid), 0.01, "the live tick does not count down a prisoner who is not connected");

		array<string> online = {prisonerUid};
		array<string> offline = {};
		LF_Jail.TickOnline(10, online);
		ctx.EqualFloat(110, LF_Jail.GetRemaining(prisonerUid), 0.01, "an online prisoner serves time");

		LF_Jail.TickOnline(500, offline);
		ctx.EqualFloat(110, LF_Jail.GetRemaining(prisonerUid), 0.01, "logging off for longer than the sentence keeps the remaining time");
		ctx.True(LF_Jail.IsServing(prisonerUid), "the offline prisoner is still serving on rejoin");

		LF_Jail.TickOnline(10, online);
		ctx.EqualFloat(100, LF_Jail.GetRemaining(prisonerUid), 0.01, "the clock resumes on rejoin");

		LF_Jail.TickOnline(100, online);
		ctx.False(LF_Jail.IsServing(prisonerUid), "the sentence ends after the online time is served");
		ctx.Equal(1, LF_Jail.GetReleaseCount(prisonerUid), "the prisoner is released once");
		ctx.Equal(1, prisoner.GetWantedLevel(), "release reduces wanted by the configured amount");

		LF_Jail.TickOnline(10, online);
		ctx.EqualFloat(0, LF_Jail.Advance(prisonerUid, 10), 0.01, "a finished sentence does not advance");
		ctx.Equal(1, LF_Jail.GetReleaseCount(prisonerUid), "later ticks do not release again");
		ctx.Equal(1, prisoner.GetWantedLevel(), "later ticks do not reduce wanted again");

		LF_Jail.SyncPlacedCell();
	}
}
