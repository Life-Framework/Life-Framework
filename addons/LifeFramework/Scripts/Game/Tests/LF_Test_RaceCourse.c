// red-proof: `test --no-build --red LF_Test_RaceCourse` -> RED PROOF OK (all 1 listed test(s) fail with the hook on): skipping the first in-order Pass leaves cp_b not next, the shortcut check sees progress 0, and a repeat of cp_a is no longer refused.
// tier: LOGIC
//! Race course (#1922): checkpoints count in order, a skipped one is refused, laps count, the
//! finish order is by time. A course here is built in script from ids; no world, no coordinates.
class LF_Test_RaceCourse : LF_Test
{
	protected LF_RaceCourseRecord BuildCourse(int laps)
	{
		LF_RaceCourseRecord course = new LF_RaceCourseRecord();
		course.m_sId = "test_loop";
		course.m_aCheckpoints = {"cp_a", "cp_b", "cp_c"};
		course.m_iLaps = laps;
		course.m_iEntryFee = 50;
		course.m_iMinEntrants = 2;
		course.m_iLobbySeconds = 30;
		course.m_aPayoutPercent = {60, 30, 10};
		return course;
	}

	override string GetName()
	{
		return "racing/course";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_RaceCourseRecord course = BuildCourse(2);
		ctx.True(LF_RaceCourse.IsValid(course), "a complete course is valid");

		// In order: the next checkpoint advances one at a time.
		LF_RaceProgress racer = new LF_RaceProgress();
		ctx.EqualStr("cp_a", LF_RaceCourse.NextCheckpoint(course, racer), "the first checkpoint is next at the start");
		LF_ERaceCheck verdict = LF_ERaceCheck.UNKNOWN;
		if (!RedProof())
			verdict = LF_RaceCourse.Pass(course, racer, "cp_a", 1000);
		ctx.True(verdict == LF_ERaceCheck.ACCEPTED, "the first checkpoint in order is accepted");
		ctx.EqualStr("cp_b", LF_RaceCourse.NextCheckpoint(course, racer), "the second checkpoint is next after the first");

		// A skipped checkpoint is refused and the progress does not move.
		verdict = LF_RaceCourse.Pass(course, racer, "cp_c", 1500);
		ctx.True(verdict == LF_ERaceCheck.SKIPPED, "cp_c before cp_b is a shortcut and is refused");
		ctx.Equal(1, racer.m_iNextIndex, "a refused shortcut leaves the progress alone");

		// A repeat, a stranger and a null course are refused too.
		ctx.True(LF_RaceCourse.Pass(course, racer, "cp_a", 1600) == LF_ERaceCheck.REPEATED, "a checkpoint already passed this lap is refused");
		ctx.True(LF_RaceCourse.Pass(course, racer, "cp_zzz", 1700) == LF_ERaceCheck.UNKNOWN, "an id outside the course is refused");
		ctx.True(LF_RaceCourse.Pass(null, racer, "cp_b", 1800) == LF_ERaceCheck.CLOSED, "no course is refused, not a crash");
		ctx.Equal(1, racer.m_iNextIndex, "refusals never advance the racer");

		// Laps: the last checkpoint closes a lap, the last of the final lap finishes.
		ctx.True(LF_RaceCourse.Pass(course, racer, "cp_b", 2000) == LF_ERaceCheck.ACCEPTED, "cp_b is accepted in order");
		ctx.True(LF_RaceCourse.Pass(course, racer, "cp_c", 3000) == LF_ERaceCheck.LAP_DONE, "the last checkpoint of lap one closes the lap");
		ctx.Equal(1, racer.m_iLapsDone, "one lap is counted");
		ctx.EqualStr("cp_a", LF_RaceCourse.NextCheckpoint(course, racer), "lap two starts at the first checkpoint again");
		ctx.True(!racer.m_bFinished, "a racer with a lap left has not finished");
		ctx.True(LF_RaceCourse.Pass(course, racer, "cp_a", 4000) == LF_ERaceCheck.ACCEPTED, "lap two cp_a");
		ctx.True(LF_RaceCourse.Pass(course, racer, "cp_c", 4500) == LF_ERaceCheck.SKIPPED, "a shortcut on lap two is refused as well");
		ctx.True(LF_RaceCourse.Pass(course, racer, "cp_b", 5000) == LF_ERaceCheck.ACCEPTED, "lap two cp_b");
		ctx.True(LF_RaceCourse.Pass(course, racer, "cp_c", 6000) == LF_ERaceCheck.FINISHED, "the last checkpoint of the last lap finishes");
		ctx.True(racer.m_bFinished, "the racer is finished");
		ctx.Equal(2, racer.m_iLapsDone, "both laps are counted");
		ctx.Equal(6000, racer.m_iFinishMs, "the finish time is the time of the last checkpoint");
		ctx.EqualStr("", LF_RaceCourse.NextCheckpoint(course, racer), "a finished racer has no next checkpoint");
		ctx.True(LF_RaceCourse.Pass(course, racer, "cp_a", 7000) == LF_ERaceCheck.CLOSED, "a finished racer cannot pass again");
		ctx.Equal(6000, racer.m_iFinishMs, "a refused pass after the finish keeps the finish time");

		// A one-lap course finishes on the first lap's last checkpoint.
		LF_RaceCourseRecord sprint = BuildCourse(1);
		LF_RaceProgress runner = new LF_RaceProgress();
		LF_RaceCourse.Pass(sprint, runner, "cp_a", 100);
		LF_RaceCourse.Pass(sprint, runner, "cp_b", 200);
		ctx.True(LF_RaceCourse.Pass(sprint, runner, "cp_c", 300) == LF_ERaceCheck.FINISHED, "a one-lap course finishes at its last checkpoint");

		// The finish order is by time; a racer who never finished is left out; a tie keeps entry order.
		array<int> times = {9000, -1, 7000, 9000, 8000};
		array<int> order = {};
		LF_RaceCourse.FinishOrder(times, order);
		ctx.Equal(4, order.Count(), "four racers finished, one did not");
		ctx.Equal(2, order[0], "the fastest racer is first");
		ctx.Equal(4, order[1], "the next fastest is second");
		ctx.Equal(0, order[2], "a tie goes to the earlier entrant");
		ctx.Equal(3, order[3], "the later of the tied entrants is after");

		// The payout split divides the pot by place and rounds down.
		ctx.Equal(120, LF_RaceCourse.PayoutFor(course, 1, 200), "first place takes 60 percent");
		ctx.Equal(60, LF_RaceCourse.PayoutFor(course, 2, 200), "second place takes 30 percent");
		ctx.Equal(0, LF_RaceCourse.PayoutFor(course, 4, 200), "a place past the split gets nothing");
		ctx.Equal(3, LF_RaceCourse.PayoutFor(course, 3, 33), "a share rounds down");

		// Bad courses are refused and named in the log.
		LF_RaceCourseRecord bad = BuildCourse(0);
		ctx.False(LF_RaceCourse.IsValid(bad), "zero laps is invalid");
		bad = BuildCourse(1);
		bad.m_aCheckpoints = {"cp_a", "cp_a"};
		ctx.False(LF_RaceCourse.IsValid(bad), "a duplicate checkpoint id is invalid");
		bad = BuildCourse(1);
		bad.m_aCheckpoints = {"cp_a"};
		ctx.False(LF_RaceCourse.IsValid(bad), "a single checkpoint is not a course");
		bad = BuildCourse(1);
		bad.m_aPayoutPercent = {50, 40};
		ctx.False(LF_RaceCourse.IsValid(bad), "a payout split that is not 100 is invalid");
		bad = BuildCourse(1);
		bad.m_iMinEntrants = 0;
		ctx.False(LF_RaceCourse.IsValid(bad), "no minimum entrants is invalid");
		ctx.False(LF_RaceCourse.IsValid(null), "no course is invalid");
	}
}
