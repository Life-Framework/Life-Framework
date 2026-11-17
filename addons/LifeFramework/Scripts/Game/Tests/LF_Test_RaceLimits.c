// red-proof: 2026-11-18 `test --no-build --red LF_Test_RaceLimits,LF_Test_JumpPadSeen,LF_Test_RaceAfkSettle,LF_Test_RaceLobbyLeave,LF_Test_EventForceDespawn` -> RED PROOF OK (all 5 fail with the hook on): with the hook on the minimum stays 2, so "a minimum above the lobby limit is refused" fails. Green with the fix (--only, 17 passed).
// tier: LOGIC
//! #2406: a course whose minimum entrants exceeds the lobby limit is refused (it would take fees and always
//! cancel), a negative AFK grace or settle delay is refused, and the racer's AFK clock and the race's
//! settle clock count in the course's seconds (0 turns each off). Pure script, no world.
class LF_Test_RaceLimits : LF_Test
{
	override string GetName()
	{
		return "racing/limits";
	}

	protected LF_RaceCourseRecord BuildCourse()
	{
		LF_RaceCourseRecord course = new LF_RaceCourseRecord();
		course.m_sId = "test_limits";
		course.m_aCheckpoints = {"cp_a", "cp_b"};
		course.m_iLaps = 1;
		course.m_iEntryFee = 10;
		course.m_iMinEntrants = 2;
		course.m_iLobbySeconds = 30;
		course.m_aPayoutPercent = {100};
		course.m_iAfkGraceSeconds = 60;
		course.m_iSettleSeconds = 120;
		return course;
	}

	override void Run(LF_TestContext ctx)
	{
		LF_RaceCourseRecord course = BuildCourse();
		ctx.True(LF_RaceCourse.IsValid(course), "a course with the default clocks is valid");

		course.m_iMinEntrants = LF_RaceRunner.MAX_ENTRANTS;
		ctx.True(LF_RaceCourse.IsValid(course), "a minimum equal to the lobby limit can still start");

		if (!RedProof())
			course.m_iMinEntrants = LF_RaceRunner.MAX_ENTRANTS + 1;
		else
			course.m_iMinEntrants = 2;
		ctx.False(LF_RaceCourse.IsValid(course), "a minimum above the lobby limit is refused: it would always cancel");

		course = BuildCourse();
		course.m_iAfkGraceSeconds = -1;
		ctx.False(LF_RaceCourse.IsValid(course), "a negative AFK grace is refused");
		course = BuildCourse();
		course.m_iSettleSeconds = -5;
		ctx.False(LF_RaceCourse.IsValid(course), "a negative settle delay is refused");
		course = BuildCourse();
		course.m_iAfkGraceSeconds = 0;
		course.m_iSettleSeconds = 0;
		ctx.True(LF_RaceCourse.IsValid(course), "0 turns both clocks off and is valid");

		// The AFK clock: starts when the body is lost, resets when it is back, 0 never drops.
		LF_RaceEntrant racer = new LF_RaceEntrant();
		ctx.False(racer.AbsentTooLong(true, 1000, 30), "a racer with a body is never absent");
		ctx.False(racer.AbsentTooLong(false, 2000, 30), "the clock starts at the first tick without a body");
		ctx.False(racer.AbsentTooLong(false, 31999, 30), "just inside the grace");
		ctx.True(racer.AbsentTooLong(false, 32000, 30), "at the grace the racer is dropped");
		ctx.False(racer.AbsentTooLong(true, 33000, 30), "a body back resets the clock");
		ctx.False(racer.AbsentTooLong(false, 34000, 30), "the next loss starts a new clock");
		ctx.False(racer.AbsentTooLong(false, 900000, 0), "a grace of 0 never drops a racer");

		// The settle clock: counts from the first finisher, 0 never settles.
		LF_RaceInstance race = new LF_RaceInstance();
		race.m_Course = BuildCourse();
		race.m_Course.m_iSettleSeconds = 60;
		ctx.False(race.SettleDue(1000000), "nobody has finished: nothing is due");
		race.m_fFirstFinishMs = 10000;
		ctx.False(race.SettleDue(69999), "just inside the settle delay");
		ctx.True(race.SettleDue(70000), "at the settle delay the race is due");
		race.m_Course.m_iSettleSeconds = 0;
		ctx.False(race.SettleDue(9000000), "a settle delay of 0 waits for every racer");
	}
}
