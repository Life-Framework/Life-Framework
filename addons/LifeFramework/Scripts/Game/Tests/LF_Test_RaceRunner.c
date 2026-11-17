// red-proof: `test --no-build --red LF_Test_RaceRunner` -> RED PROOF OK (2026-11-16): with the hook on, the first Join is skipped, so no fee leaves carried cash, the pot stays 0 and the lobby never opens.
// tier: WORLD
//! Race runner (#1923): the entry fee leaves carried cash into the pot once, too few entrants
//! refunds, a leaver forfeits, a finish pays the pot by the split, and a session end mid-race
//! refunds everyone still in (the pot is session state). Bodies are fixture characters.
class LF_Test_RaceRunner : LF_Test
{
	protected static const string UID_A = "race-runner-a";
	protected static const string UID_B = "race-runner-b";
	protected static const string UID_C = "race-runner-c";

	override string GetName()
	{
		return "racing/runner";
	}

	protected LF_RaceCourseRecord BuildCourse()
	{
		LF_RaceCourseRecord course = new LF_RaceCourseRecord();
		course.m_sId = "test_runner";
		course.m_aCheckpoints = {"cp_a", "cp_b"};
		course.m_iLaps = 1;
		course.m_iEntryFee = 50;
		course.m_iMinEntrants = 2;
		course.m_iLobbySeconds = 600;
		course.m_aPayoutPercent = {70, 30};
		return course;
	}

	protected void SetCash(IEntity body, int amount)
	{
		LF_MoneyUtils.RemoveCash(body, LF_MoneyUtils.GetCash(body));
		LF_MoneyUtils.AddCash(body, amount);
	}

	override void Run(LF_TestContext ctx)
	{
		LF_RaceRunner.ResetStatics();
		LF_RaceCourseRecord course = BuildCourse();
		string id = course.m_sId;

		IEntity a = LF_TestCharacterFixture.Spawn(ctx);
		IEntity b = LF_TestCharacterFixture.Spawn(ctx);
		IEntity c = LF_TestCharacterFixture.Spawn(ctx);
		if (!a || !b || !c)
		{
			Cleanup(a, b, c, null);
			return;
		}

		LF_Test_RaceRunnerListener listener = new LF_Test_RaceRunnerListener();
		LF_EventBus.Subscribe(LF_OnRaceFinished, listener);

		SetCash(a, 100);
		SetCash(b, 100);
		SetCash(c, 30);

		// The fee goes into the pot once.
		LF_ERaceJoin joined = LF_ERaceJoin.CLOSED;
		if (!RedProof())
			joined = LF_RaceRunner.Join(course, UID_A, a);
		ctx.True(joined == LF_ERaceJoin.JOINED, "a racer with the fee joins");
		ctx.Equal(50, LF_MoneyUtils.GetCash(a), "the fee leaves carried cash");
		ctx.Equal(50, LF_RaceRunner.GetPot(id), "the fee is in the pot");
		ctx.True(LF_RaceRunner.GetState(id) == LF_ERaceState.LOBBY, "the first entrant opens the lobby");
		ctx.True(LF_RaceRunner.Join(course, UID_A, a) == LF_ERaceJoin.ALREADY_IN, "joining twice is refused");
		ctx.Equal(50, LF_MoneyUtils.GetCash(a), "joining twice takes nothing");
		ctx.Equal(50, LF_RaceRunner.GetPot(id), "joining twice adds nothing to the pot");
		ctx.True(LF_RaceRunner.Join(course, UID_C, c) == LF_ERaceJoin.NOT_ENOUGH_CASH, "30 in the pocket does not pay a 50 fee");
		ctx.Equal(30, LF_MoneyUtils.GetCash(c), "a refused join takes nothing");
		ctx.True(LF_RaceRunner.Join(course, "", c) == LF_ERaceJoin.NO_PLAYER, "no uid is refused");
		ctx.True(LF_RaceRunner.Join(null, UID_B, b) == LF_ERaceJoin.BAD_COURSE, "no course is refused");
		ctx.Equal(100, LF_MoneyUtils.GetCash(b), "a refused course takes nothing");

		// Fewer than the minimum entrants: the lobby closes and everyone is refunded.
		ctx.False(LF_RaceRunner.CloseLobby(id), "one entrant cannot start a two-entrant race");
		ctx.Equal(100, LF_MoneyUtils.GetCash(a), "the lone entrant gets the fee back");
		ctx.Equal(0, LF_RaceRunner.GetPot(id), "the cancelled pot is empty");
		ctx.True(LF_RaceRunner.GetState(id) == LF_ERaceState.IDLE, "the cancelled race is idle");

		// A full lobby starts; the lobby is closed to late joiners.
		SetCash(c, 50);
		ctx.True(LF_RaceRunner.Join(course, UID_A, a) == LF_ERaceJoin.JOINED, "A joins the second race");
		ctx.True(LF_RaceRunner.Join(course, UID_B, b) == LF_ERaceJoin.JOINED, "B joins");
		ctx.True(LF_RaceRunner.Join(course, UID_C, c) == LF_ERaceJoin.JOINED, "C joins");
		ctx.Equal(150, LF_RaceRunner.GetPot(id), "three fees in the pot");
		ctx.True(LF_RaceRunner.CloseLobby(id), "three entrants start the race");
		ctx.True(LF_RaceRunner.GetState(id) == LF_ERaceState.RACING, "the race is running");
		ctx.False(LF_RaceRunner.CloseLobby(id), "a running race has no lobby to close");
		ctx.True(LF_RaceRunner.Join(course, "race-runner-late", b) == LF_ERaceJoin.CLOSED, "a running race takes no entrant");

		// A leaver forfeits: the fee stays in the pot and nothing comes back.
		ctx.True(LF_RaceRunner.Forfeit(UID_C), "the leaver is dropped");
		ctx.Equal(0, LF_MoneyUtils.GetCash(c), "the leaver gets nothing back");
		ctx.Equal(150, LF_RaceRunner.GetPot(id), "the forfeited fee stays in the pot");
		ctx.False(LF_RaceRunner.Forfeit(UID_C), "a second forfeit finds nobody");
		ctx.True(LF_RaceRunner.PassAt(id, UID_C, "cp_a", 500) == LF_ERaceCheck.CLOSED, "a leaver's checkpoint is refused");

		// Checkpoints in order; the race pays when everyone still in has finished.
		ctx.True(LF_RaceRunner.PassAt(id, UID_A, "cp_b", 900) == LF_ERaceCheck.SKIPPED, "a shortcut is refused by the course rules");
		ctx.True(LF_RaceRunner.PassAt(id, UID_A, "cp_a", 1000) == LF_ERaceCheck.ACCEPTED, "A passes cp_a");
		ctx.True(LF_RaceRunner.PassAt(id, UID_A, "cp_b", 2000) == LF_ERaceCheck.FINISHED, "A finishes first");
		ctx.Equal(50, LF_MoneyUtils.GetCash(a), "nothing is paid while a racer is still out");
		ctx.False(LF_RaceRunner.Forfeit(UID_A), "a finisher who leaves keeps the place");
		ctx.True(LF_RaceRunner.PassAt(id, UID_B, "cp_a", 1500) == LF_ERaceCheck.ACCEPTED, "B passes cp_a");
		ctx.True(LF_RaceRunner.PassAt(id, UID_B, "cp_b", 3000) == LF_ERaceCheck.FINISHED, "B finishes second");
		ctx.Equal(155, LF_MoneyUtils.GetCash(a), "the winner takes 70 percent of 150");
		ctx.Equal(95, LF_MoneyUtils.GetCash(b), "second place takes 30 percent of 150");
		ctx.Equal(0, LF_MoneyUtils.GetCash(c), "the leaver is paid nothing");
		ctx.Equal(0, LF_RaceRunner.GetPot(id), "the paid pot is empty");
		ctx.True(LF_RaceRunner.GetState(id) == LF_ERaceState.IDLE, "the paid race is over");
		ctx.Equal(1, listener.m_iCount, "LF_OnRaceFinished is published once");
		ctx.EqualStr(UID_A, listener.m_sWinner, "the event names the winner");
		ctx.Equal(150, listener.m_iPot, "the event carries the pot");
		ctx.True(LF_RaceRunner.PassAt(id, UID_A, "cp_a", 4000) == LF_ERaceCheck.CLOSED, "a finished race takes no checkpoint");
		ctx.Equal(155, LF_MoneyUtils.GetCash(a), "a pass after the payout pays nothing");

		// A session end mid-race refunds everyone still in, and a leaver too (a shutdown may
		// disconnect players before the game end).
		SetCash(c, 50);
		ctx.True(LF_RaceRunner.Join(course, UID_A, a) == LF_ERaceJoin.JOINED, "A joins the third race");
		ctx.True(LF_RaceRunner.Join(course, UID_B, b) == LF_ERaceJoin.JOINED, "B joins the third race");
		ctx.True(LF_RaceRunner.Join(course, UID_C, c) == LF_ERaceJoin.JOINED, "C joins the third race");
		ctx.True(LF_RaceRunner.CloseLobby(id), "the third race starts");
		LF_RaceRunner.PassAt(id, UID_A, "cp_a", 1000);
		LF_RaceRunner.Forfeit(UID_C);
		ctx.Equal(3, LF_RaceRunner.AbandonAll("test session end"), "the session end refunds three racers");
		ctx.Equal(50, LF_MoneyUtils.GetCash(c), "the leaver has the fee back at the session end");
		ctx.Equal(155, LF_MoneyUtils.GetCash(a), "A has the fee back after the session end");
		ctx.Equal(95, LF_MoneyUtils.GetCash(b), "B has the fee back after the session end");
		ctx.Equal(0, LF_RaceRunner.GetPot(id), "the abandoned pot is empty");
		ctx.True(LF_RaceRunner.GetState(id) == LF_ERaceState.IDLE, "the abandoned race is idle");
		ctx.Equal(0, LF_RaceRunner.AbandonAll("test second end"), "a second session end refunds nothing");
		ctx.Equal(1, listener.m_iCount, "an abandoned race publishes no finish");

		// Detection follows the path between two ticks, so a fast car cannot jump a checkpoint.
		ctx.True(LF_RaceCheckpointComponent.PathWithin("0 0 0", 12, "-30 0 5", "30 0 5"), "a path through the radius between ticks counts");
		ctx.False(LF_RaceCheckpointComponent.PathWithin("0 0 0", 12, "-30 0 20", "30 0 20"), "a path that stays outside does not count");
		ctx.True(LF_RaceCheckpointComponent.PathWithin("0 0 0", 12, "3 0 3", "3 0 3"), "standing inside counts");

		Cleanup(a, b, c, listener);
	}

	protected void Cleanup(IEntity a, IEntity b, IEntity c, LF_Test_RaceRunnerListener listener)
	{
		if (listener)
			LF_EventBus.Unsubscribe(LF_OnRaceFinished, listener);

		LF_RaceRunner.ResetStatics();
		if (a)
			LF_TestCharacterFixture.Delete(a);
		if (b)
			LF_TestCharacterFixture.Delete(b);
		if (c)
			LF_TestCharacterFixture.Delete(c);
	}
}

//------------------------------------------------------------------------------------------------
class LF_Test_RaceRunnerListener : LF_EventSubscriber
{
	int m_iCount;
	string m_sWinner;
	int m_iPot;

	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnRaceFinished finished = LF_OnRaceFinished.Cast(busEvent);
		if (!finished)
			return true;

		m_iCount++;
		m_sWinner = finished.m_sWinnerUid;
		m_iPot = finished.m_iPot;
		return true;
	}
}
