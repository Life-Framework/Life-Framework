// red-proof: PENDING
// tier: LOGIC
//! Race panel (#1924): the panel model for a race state names the right lap, next checkpoint and
//! place. No world: a course built in script, progress set by the course rules.
class LF_Test_RaceHud : LF_Test
{
	override string GetName()
	{
		return "racing/race-hud";
	}

	protected LF_RaceCourseRecord BuildCourse()
	{
		LF_RaceCourseRecord course = new LF_RaceCourseRecord();
		course.m_sId = "hud_loop";
		course.m_aCheckpoints = {"cp_a", "cp_b", "cp_c"};
		course.m_iLaps = 2;
		course.m_iEntryFee = 0;
		course.m_iMinEntrants = 1;
		course.m_iLobbySeconds = 10;
		course.m_aPayoutPercent = {100};
		return course;
	}

	override void Run(LF_TestContext ctx)
	{
		LF_RaceCourseRecord course = BuildCourse();

		// At the start: lap 1, checkpoint 1 of 3 next.
		LF_RaceProgress fresh = new LF_RaceProgress();
		LF_RaceHudState start = LF_RaceHud.Build(course, fresh, 1, 2);
		ctx.True(start != null, "a panel builds for a fresh racer");
		ctx.True(start.m_iLap == 1 && start.m_iLaps == 2, "the start is lap 1 of 2");
		ctx.True(start.m_iCheckpoint == 1 && start.m_iCheckpoints == 3, "checkpoint 1 of 3 is next at the start");
		ctx.EqualStr("cp_a", start.m_sNext, "the next checkpoint id is the first");

		// A racer who passed a full lap and one more: lap 2, checkpoint 2 next.
		LF_RaceProgress ahead = new LF_RaceProgress();
		if (!RedProof())
		{
			LF_RaceCourse.Pass(course, ahead, "cp_a", 1000);
			LF_RaceCourse.Pass(course, ahead, "cp_b", 2000);
			LF_RaceCourse.Pass(course, ahead, "cp_c", 3000);
			LF_RaceCourse.Pass(course, ahead, "cp_a", 4000);
		}

		LF_RaceHudState lap2 = LF_RaceHud.Build(course, ahead, 1, 2);
		ctx.True(lap2.m_iLap == 2, "after a full lap the panel says lap 2");
		ctx.True(lap2.m_iCheckpoint == 2, "after the lap's first checkpoint, checkpoint 2 is next");
		ctx.EqualStr("cp_b", lap2.m_sNext, "the next checkpoint id is cp_b");
		ctx.True(LF_RaceHud.PassesOf(course, ahead) == 4, "four checkpoints passed over the race");

		// Place: most checkpoints first, a tie goes to who got there first, finishers above everyone.
		array<ref LF_RaceHudRacer> racers = {};
		racers.Insert(LF_RaceHudRacer.Create("slow", 1, 900, false, -1));
		racers.Insert(LF_RaceHudRacer.Create("fast", LF_RaceHud.PassesOf(course, ahead), 4000, false, -1));
		racers.Insert(LF_RaceHudRacer.Create("tied", 1, 1200, false, -1));
		ctx.True(LF_RaceHud.PlaceOf(racers, "fast") == 1, "the racer with most checkpoints leads");
		ctx.True(LF_RaceHud.PlaceOf(racers, "slow") == 2, "on a tie the earlier pass ranks higher");
		ctx.True(LF_RaceHud.PlaceOf(racers, "tied") == 3, "the later pass on a tie is behind");
		ctx.True(LF_RaceHud.PlaceOf(racers, "nobody") == 0, "a uid not in the race has no place");

		racers.Insert(LF_RaceHudRacer.Create("done", 6, 9000, true, 9000));
		ctx.True(LF_RaceHud.PlaceOf(racers, "done") == 1, "a finisher ranks above every racer still out");
		ctx.True(LF_RaceHud.PlaceOf(racers, "fast") == 2, "the leader drops a place behind the finisher");

		// The signature that decides a resend changes with the checkpoint, not with the clock.
		ctx.True(start.Signature() != lap2.Signature(), "a new checkpoint changes the signature");
		LF_RaceHudState again = LF_RaceHud.Build(course, fresh, 1, 2);
		ctx.EqualStr(start.Signature(), again.Signature(), "the same state gives the same signature");

		// Finished: last lap, no next checkpoint.
		LF_RaceProgress finished = new LF_RaceProgress();
		finished.m_bFinished = true;
		finished.m_iLapsDone = 2;
		finished.m_iFinishMs = 61500;
		LF_RaceHudState end = LF_RaceHud.Build(course, finished, 1, 2);
		ctx.True(end.m_bFinished && end.m_iLap == 2 && end.m_iCheckpoint == 0, "a finisher shows the last lap and no checkpoint");
		ctx.EqualStr("", end.m_sNext, "a finisher has no next checkpoint");
		ctx.EqualStr("1:01.5", LF_RaceHud.Clock(61500), "the clock reads m:ss.t");
		ctx.True(LF_RaceHud.Build(null, fresh, 1, 1) == null, "no course builds no panel");
	}
}
