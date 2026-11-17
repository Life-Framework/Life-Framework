// red-proof: 2026-11-18 batched `--red` RED PROOF OK: with the hook on the leaver is never taken out, so the fee stays in the pot, the lobby does not close and the tick stays on. Green with the fix.
// tier: WORLD
//! #2406: a racer who leaves while the race is still in its LOBBY gets the fee back at once (to the bank,
//! never to the leaving body) and an emptied lobby closes. And the 250 ms checkpoint tick runs only
//! while a race is RACING: a lobby open beside a finished race does not keep it on.
class LF_Test_RaceLobbyLeave : LF_Test
{
	protected static const string UID_A = "race-lobby-a";
	protected static const string UID_B = "race-lobby-b";
	protected static const string UID_C = "race-lobby-c";

	override string GetName()
	{
		return "racing/lobby-leave";
	}

	protected LF_RaceCourseRecord BuildCourse(string id)
	{
		LF_RaceCourseRecord course = new LF_RaceCourseRecord();
		course.m_sId = id;
		course.m_aCheckpoints = {"cp_a", "cp_b"};
		course.m_iLaps = 1;
		course.m_iEntryFee = 50;
		course.m_iMinEntrants = 2;
		course.m_iLobbySeconds = 600;
		course.m_aPayoutPercent = {100};
		return course;
	}

	protected void SetCash(IEntity body, int amount)
	{
		LF_MoneyUtils.RemoveCash(body, LF_MoneyUtils.GetCash(body));
		LF_MoneyUtils.AddCash(body, amount);
	}

	protected int Bank(string uid)
	{
		LF_BankAccount account = LF_ATMManager.GetOrCreate(uid);
		if (!account)
			return -1;

		return account.GetBalance();
	}

	override void Run(LF_TestContext ctx)
	{
		LF_RaceRunner.ResetStatics();
		IEntity a = LF_TestCharacterFixture.Spawn(ctx);
		IEntity b = LF_TestCharacterFixture.Spawn(ctx);
		IEntity c = LF_TestCharacterFixture.Spawn(ctx);
		if (!a || !b || !c)
		{
			Cleanup(a, b, c);
			return;
		}

		SetCash(a, 100);
		SetCash(b, 100);
		SetCash(c, 100);

		// A leaver in the lobby: the fee leaves the pot and goes back at once.
		LF_RaceCourseRecord lobby = BuildCourse("test_lobby_leave");
		ctx.True(LF_RaceRunner.Join(lobby, UID_A, a) == LF_ERaceJoin.JOINED, "A joins");
		ctx.True(LF_RaceRunner.Join(lobby, UID_B, b) == LF_ERaceJoin.JOINED, "B joins");
		ctx.Equal(100, LF_RaceRunner.GetPot("test_lobby_leave"), "two fees in the pot");
		int bankBefore = Bank(UID_B);
		if (!RedProof())
			ctx.True(LF_RaceRunner.Forfeit(UID_B), "the lobby leaver is dropped");

		ctx.Equal(50, LF_RaceRunner.GetPot("test_lobby_leave"), "the leaver's fee is out of the pot");
		ctx.Equal(1, LF_RaceRunner.GetEntrantCount("test_lobby_leave"), "one racer is left in the lobby");
		ctx.Equal(bankBefore + 50, Bank(UID_B), "the leaver's fee is in their bank");
		ctx.Equal(50, LF_MoneyUtils.GetCash(b), "nothing is paid to the leaving body");

		// The last one leaves: the lobby closes and holds nothing.
		int bankA = Bank(UID_A);
		if (!RedProof())
			LF_RaceRunner.Forfeit(UID_A);

		ctx.True(LF_RaceRunner.GetState("test_lobby_leave") == LF_ERaceState.IDLE, "an emptied lobby closes");
		ctx.Equal(0, LF_RaceRunner.GetPot("test_lobby_leave"), "and its pot is empty");
		ctx.Equal(bankA + 50, Bank(UID_A), "the last leaver is refunded too");
		ctx.False(LF_RaceRunner.CloseLobby("test_lobby_leave"), "there is no lobby left to close");

		// The tick: one race running and one lobby open. When the race ends the tick stops.
		SetCash(a, 100);
		SetCash(b, 100);
		SetCash(c, 100);
		LF_RaceCourseRecord running = BuildCourse("test_tick_run");
		LF_RaceCourseRecord waiting = BuildCourse("test_tick_wait");
		LF_RaceRunner.Join(running, UID_A, a);
		LF_RaceRunner.Join(running, UID_B, b);
		ctx.True(LF_RaceRunner.CloseLobby("test_tick_run"), "the first race starts");
		ctx.True(LF_RaceRunner.IsTicking(), "a running race ticks");
		ctx.True(LF_RaceRunner.Join(waiting, UID_C, c) == LF_ERaceJoin.JOINED, "C opens a second lobby");
		LF_RaceRunner.PassAt("test_tick_run", UID_A, "cp_a", 1000);
		LF_RaceRunner.PassAt("test_tick_run", UID_A, "cp_b", 2000);
		LF_RaceRunner.PassAt("test_tick_run", UID_B, "cp_a", 1500);
		LF_RaceRunner.PassAt("test_tick_run", UID_B, "cp_b", 3000);
		ctx.True(LF_RaceRunner.GetState("test_tick_run") == LF_ERaceState.IDLE, "the first race is paid and over");
		ctx.True(LF_RaceRunner.GetState("test_tick_wait") == LF_ERaceState.LOBBY, "the second race is still a lobby");
		ctx.False(LF_RaceRunner.IsTicking(), "only a lobby is open, so the checkpoint tick is off");

		LF_RaceRunner.AbandonAll("test end");
		Cleanup(a, b, c);
	}

	protected void Cleanup(IEntity a, IEntity b, IEntity c)
	{
		LF_RaceRunner.ResetStatics();
		if (a)
			LF_TestCharacterFixture.Delete(a);
		if (b)
			LF_TestCharacterFixture.Delete(b);
		if (c)
			LF_TestCharacterFixture.Delete(c);
	}
}
