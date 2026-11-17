// red-proof: 2026-11-18 batched `--red` RED PROOF OK: with the hook on the ticks are skipped, so the AFK racer is never dropped ("past the grace the AFK racer is dropped and the race settles" fails) and the settle delay never runs. Green with the fix.
// tier: WORLD
//! #2406: one AFK or dead racer no longer holds every prize. A racer with no living body past the course's
//! AFK grace is dropped (the fee stays in the pot) and the race settles for the finishers; with the
//! settle delay set, the race also pays out that long after the first finisher, and a racer still out
//! forfeits. Time is passed to LF_RaceRunner.TickAt, so nothing waits. Bodies are fixture characters.
class LF_Test_RaceAfkSettle : LF_Test
{
	protected static const string UID_A = "race-afk-a";
	protected static const string UID_B = "race-afk-b";

	override string GetName()
	{
		return "racing/afk-settle";
	}

	protected LF_RaceCourseRecord BuildCourse(string id, int afkGrace, int settle)
	{
		LF_RaceCourseRecord course = new LF_RaceCourseRecord();
		course.m_sId = id;
		course.m_aCheckpoints = {"cp_a", "cp_b"};
		course.m_iLaps = 1;
		course.m_iEntryFee = 50;
		course.m_iMinEntrants = 2;
		course.m_iLobbySeconds = 600;
		course.m_aPayoutPercent = {100};
		course.m_iAfkGraceSeconds = afkGrace;
		course.m_iSettleSeconds = settle;
		return course;
	}

	protected void SetCash(IEntity body, int amount)
	{
		LF_MoneyUtils.RemoveCash(body, LF_MoneyUtils.GetCash(body));
		LF_MoneyUtils.AddCash(body, amount);
	}

	protected float WorldNow()
	{
		return GetGame().GetWorld().GetWorldTime();
	}

	override void Run(LF_TestContext ctx)
	{
		LF_RaceRunner.ResetStatics();
		IEntity a = LF_TestCharacterFixture.Spawn(ctx);
		IEntity b = LF_TestCharacterFixture.Spawn(ctx);
		if (!a || !b)
		{
			Cleanup(a, b);
			return;
		}

		// An AFK racer: B has no body any more. A finishes first and waits for nobody but B.
		LF_RaceCourseRecord afk = BuildCourse("test_afk", 30, 0);
		SetCash(a, 100);
		SetCash(b, 100);
		ctx.True(LF_RaceRunner.Join(afk, UID_A, a) == LF_ERaceJoin.JOINED, "A joins the AFK race");
		ctx.True(LF_RaceRunner.Join(afk, UID_B, b) == LF_ERaceJoin.JOINED, "B joins the AFK race");
		ctx.True(LF_RaceRunner.CloseLobby("test_afk"), "the AFK race starts");
		ctx.True(LF_RaceRunner.IsTicking(), "a running race ticks");
		ctx.True(LF_RaceRunner.PassAt("test_afk", UID_A, "cp_a", 1000) == LF_ERaceCheck.ACCEPTED, "A passes cp_a");
		ctx.True(LF_RaceRunner.PassAt("test_afk", UID_A, "cp_b", 2000) == LF_ERaceCheck.FINISHED, "A finishes");
		ctx.Equal(50, LF_MoneyUtils.GetCash(a), "nothing is paid while B is still in");

		LF_TestCharacterFixture.Delete(b);
		b = null;
		float now = WorldNow();
		if (!RedProof())
		{
			LF_RaceRunner.TickAt(now);
			ctx.Equal(2, LF_RaceRunner.GetEntrantCount("test_afk"), "B has no body but is inside the grace");
			LF_RaceRunner.TickAt(now + 29000);
			ctx.Equal(2, LF_RaceRunner.GetEntrantCount("test_afk"), "B is still inside the 30 s grace at 29 s");
			LF_RaceRunner.TickAt(now + 31000);
		}

		ctx.True(LF_RaceRunner.GetState("test_afk") == LF_ERaceState.IDLE, "past the grace the AFK racer is dropped and the race settles");
		ctx.Equal(150, LF_MoneyUtils.GetCash(a), "the finisher is paid the whole pot, B's forfeited fee with it");
		ctx.Equal(0, LF_RaceRunner.GetPot("test_afk"), "the paid pot is empty");

		// The settle delay: nobody is AFK, B simply never finishes. The race pays 60 s after A's finish.
		IEntity c = LF_TestCharacterFixture.Spawn(ctx);
		if (!c)
		{
			Cleanup(a, b);
			return;
		}

		LF_RaceCourseRecord late = BuildCourse("test_settle", 0, 60);
		SetCash(a, 100);
		SetCash(c, 100);
		ctx.True(LF_RaceRunner.Join(late, UID_A, a) == LF_ERaceJoin.JOINED, "A joins the settle race");
		ctx.True(LF_RaceRunner.Join(late, "race-afk-c", c) == LF_ERaceJoin.JOINED, "C joins the settle race");
		ctx.True(LF_RaceRunner.CloseLobby("test_settle"), "the settle race starts");
		LF_RaceRunner.PassAt("test_settle", UID_A, "cp_a", 1000);
		ctx.True(LF_RaceRunner.PassAt("test_settle", UID_A, "cp_b", 2000) == LF_ERaceCheck.FINISHED, "A finishes the settle race");
		now = WorldNow();
		if (!RedProof())
		{
			LF_RaceRunner.TickAt(now + 30000);
			ctx.True(LF_RaceRunner.GetState("test_settle") == LF_ERaceState.RACING, "30 s after the first finish the race still waits");
			LF_RaceRunner.TickAt(now + 65000);
		}

		ctx.True(LF_RaceRunner.GetState("test_settle") == LF_ERaceState.IDLE, "past the settle delay the race pays out");
		ctx.Equal(150, LF_MoneyUtils.GetCash(a), "the finisher takes the pot");
		ctx.Equal(50, LF_MoneyUtils.GetCash(c), "the racer still out forfeits the fee");
		ctx.False(LF_RaceRunner.IsTicking(), "no race is running, so the tick is off");

		LF_TestCharacterFixture.Delete(c);
		Cleanup(a, b);
	}

	protected void Cleanup(IEntity a, IEntity b)
	{
		LF_RaceRunner.ResetStatics();
		if (a)
			LF_TestCharacterFixture.Delete(a);
		if (b)
			LF_TestCharacterFixture.Delete(b);
	}
}
