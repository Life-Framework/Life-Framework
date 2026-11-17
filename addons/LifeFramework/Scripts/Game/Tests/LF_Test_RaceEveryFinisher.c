// red-proof: Observed 2026-11-16 with `tools/cli test --no-build --only LF_Test_RaceEveryFinisher` on the unfixed code: failed "the runner-up's first time is a record (expected 3000, got -1)" and the third's, green once the finish carries every finisher.
// tier: WORLD
//! #2160 (from #2064): every finisher can set a personal best, not only the winner. A real
//! three-racer race runs through LF_RaceRunner and settles; the record board holds all three times,
//! and the event still names the winner and the winner's time for older subscribers. A leaver is
//! not a finisher and gets no record. Records are saved and restored around the run (no ResetAll).
class LF_Test_RaceEveryFinisher : LF_Test
{
	protected static const string UID_A = "race-every-a";
	protected static const string UID_B = "race-every-b";
	protected static const string UID_C = "race-every-c";
	protected static const string UID_D = "race-every-d";
	protected static const string COURSE = "test_every_finisher";

	override string GetName()
	{
		return "racing/every-finisher";
	}

	protected void SetCash(IEntity body, int amount)
	{
		LF_MoneyUtils.RemoveCash(body, LF_MoneyUtils.GetCash(body));
		LF_MoneyUtils.AddCash(body, amount);
	}

	override void Run(LF_TestContext ctx)
	{
		LF_RaceRunner.ResetStatics();
		array<ref LF_RaceRecordRow> before = LF_RaceRecords.ExportAll();
		LF_RaceRecords.ApplyAll(null);

		LF_RaceCourseRecord course = new LF_RaceCourseRecord();
		course.m_sId = COURSE;
		course.m_aCheckpoints = {"cp_a", "cp_b"};
		course.m_iLaps = 1;
		course.m_iEntryFee = 50;
		course.m_iMinEntrants = 2;
		course.m_iLobbySeconds = 600;
		course.m_aPayoutPercent = {70, 30};

		IEntity a = LF_TestCharacterFixture.Spawn(ctx);
		IEntity b = LF_TestCharacterFixture.Spawn(ctx);
		IEntity c = LF_TestCharacterFixture.Spawn(ctx);
		IEntity d = LF_TestCharacterFixture.Spawn(ctx);
		if (!a || !b || !c || !d)
		{
			Cleanup(before, a, b, c, d, null);
			return;
		}

		LF_Test_RaceEveryFinisherListener listener = new LF_Test_RaceEveryFinisherListener();
		LF_EventBus.Subscribe(LF_OnRaceFinished, listener);
		LF_RaceRecords.EnsureSubscribed();

		SetCash(a, 100);
		SetCash(b, 100);
		SetCash(c, 100);
		SetCash(d, 100);
		ctx.True(LF_RaceRunner.Join(course, UID_A, a) == LF_ERaceJoin.JOINED, "A joins");
		ctx.True(LF_RaceRunner.Join(course, UID_B, b) == LF_ERaceJoin.JOINED, "B joins");
		ctx.True(LF_RaceRunner.Join(course, UID_C, c) == LF_ERaceJoin.JOINED, "C joins");
		ctx.True(LF_RaceRunner.Join(course, UID_D, d) == LF_ERaceJoin.JOINED, "D joins");
		ctx.True(LF_RaceRunner.CloseLobby(COURSE), "four entrants start the race");
		ctx.True(LF_RaceRunner.Forfeit(UID_D), "D leaves before the finish");

		// Finish order A, B, C; the third place takes no prize but still has a time.
		LF_RaceRunner.PassAt(COURSE, UID_A, "cp_a", 1000);
		LF_RaceRunner.PassAt(COURSE, UID_B, "cp_a", 1500);
		LF_RaceRunner.PassAt(COURSE, UID_C, "cp_a", 1800);
		ctx.True(LF_RaceRunner.PassAt(COURSE, UID_A, "cp_b", 2000) == LF_ERaceCheck.FINISHED, "A finishes first");
		ctx.True(LF_RaceRunner.PassAt(COURSE, UID_B, "cp_b", 3000) == LF_ERaceCheck.FINISHED, "B finishes second");
		ctx.True(LF_RaceRunner.PassAt(COURSE, UID_C, "cp_b", 4000) == LF_ERaceCheck.FINISHED, "C finishes third and the race settles");

		ctx.Equal(1, listener.m_iCount, "the finish is published once");
		ctx.EqualStr(UID_A, listener.m_sWinner, "the event still names the winner");
		ctx.Equal(2000, listener.m_iWinnerMs, "the event still carries the winner's time");
		ctx.Equal(3, listener.m_iFinishers, "the event carries every finisher");

		ctx.Equal(2000, LF_RaceRecords.BestOf(COURSE, UID_A), "the winner's time is a record");
		ctx.Equal(3000, LF_RaceRecords.BestOf(COURSE, UID_B), "the runner-up's first time is a record");
		ctx.Equal(4000, LF_RaceRecords.BestOf(COURSE, UID_C), "the third's first time is a record");
		ctx.Equal(-1, LF_RaceRecords.BestOf(COURSE, UID_D), "a leaver is not a finisher and has no record");
		ctx.Equal(3, LF_RaceRecords.Count(), "three records, one per finisher");

		// The event offers the times as given: a slower run does not beat a record, a faster one does.
		LF_OnRaceFinished second = LF_OnRaceFinished.Create(COURSE, UID_C, 150, 3, 3500);
		second.AddFinisher(UID_C, 3500);
		second.AddFinisher(UID_B, 3200);
		LF_EventBus.Publish(second);
		ctx.Equal(3500, LF_RaceRecords.BestOf(COURSE, UID_C), "a faster finish replaces the third's record");
		ctx.Equal(3000, LF_RaceRecords.BestOf(COURSE, UID_B), "a slower finish leaves the runner-up's record");

		// An event with no finisher list (an older publisher) still records the winner.
		LF_EventBus.Publish(LF_OnRaceFinished.Create(COURSE, "race-every-solo", 100, 2, 2500));
		ctx.Equal(2500, LF_RaceRecords.BestOf(COURSE, "race-every-solo"), "a finish with only a winner records the winner");

		Cleanup(before, a, b, c, d, listener);
	}

	protected void Cleanup(array<ref LF_RaceRecordRow> before, IEntity a, IEntity b, IEntity c, IEntity d, LF_Test_RaceEveryFinisherListener listener)
	{
		if (listener)
			LF_EventBus.Unsubscribe(LF_OnRaceFinished, listener);

		LF_RaceRunner.ResetStatics();
		LF_RaceRecords.ApplyAll(before);
		if (a)
			LF_TestCharacterFixture.Delete(a);
		if (b)
			LF_TestCharacterFixture.Delete(b);
		if (c)
			LF_TestCharacterFixture.Delete(c);
		if (d)
			LF_TestCharacterFixture.Delete(d);
	}
}

//------------------------------------------------------------------------------------------------
class LF_Test_RaceEveryFinisherListener : LF_EventSubscriber
{
	int m_iCount;
	string m_sWinner;
	int m_iWinnerMs;
	int m_iFinishers;

	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnRaceFinished finished = LF_OnRaceFinished.Cast(busEvent);
		if (!finished)
			return true;

		// The first publish is the real race; the later ones are the test's own and not counted.
		m_iCount++;
		if (m_iCount == 1)
		{
			m_sWinner = finished.m_sWinnerUid;
			m_iWinnerMs = finished.m_iWinnerMs;
			m_iFinishers = finished.m_aFinisherUids.Count();
		}

		return true;
	}
}
