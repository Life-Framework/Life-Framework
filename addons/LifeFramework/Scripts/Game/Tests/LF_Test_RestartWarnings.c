// red-proof: Observed 2026-11-15 before LF_RestartSchedule existed: `tools\cli test --no-build --only LF_Test_RestartWarnings` failed to compile ("Unknown type 'LF_RestartSchedule'"), so it could not pass. Batched `--red` run 2026-11-15: `RED-OK ops/restart-warnings (fails with the hook on, 32 assertions)`; green without it (`--only`, 1 of 1).
// tier: LOGIC
//! The restart schedule (#1264): from a clock (seconds since local midnight) the schedule answers the
//! seconds to the next restart and every warning time still ahead, the restart itself last. Times are
//! `restarts.times`, leads `restarts.warningMinutes`. Pure: the clock is an argument.
class LF_Test_RestartWarnings : LF_Test
{
	override string GetName()
	{
		return "ops/restart-warnings";
	}

	//! A config with up to two restart times and up to four leads; "" and 0 leave a slot out.
	protected LF_RestartConfig Make(string t1, string t2, int l1, int l2, int l3, int l4 = 0)
	{
		LF_RestartConfig config = LF_RestartConfig.CreateDefault();
		config.m_aTimes.Clear();
		config.m_aWarningMinutes.Clear();
		if (!t1.IsEmpty())
			config.m_aTimes.Insert(t1);
		if (!t2.IsEmpty())
			config.m_aTimes.Insert(t2);
		if (l1 != 0)
			config.m_aWarningMinutes.Insert(l1);
		if (l2 != 0)
			config.m_aWarningMinutes.Insert(l2);
		if (l3 != 0)
			config.m_aWarningMinutes.Insert(l3);
		if (l4 != 0)
			config.m_aWarningMinutes.Insert(l4);
		return config;
	}

	protected static int Clock(int hh, int mm, int ss = 0)
	{
		return hh * 3600 + mm * 60 + ss;
	}

	override void Run(LF_TestContext ctx)
	{
		array<int> clocks = {};
		array<int> minutes = {};
		int untilRestart = -1;

		// The flagship: 06:00 and 18:00, leads 15, 5, 1.
		LF_RestartConfig flagship = Make("06:00", "18:00", 15, 5, 1);
		if (!RedProof())
			untilRestart = LF_RestartSchedule.Plan(flagship, Clock(17, 40), clocks, minutes);

		ctx.Equal(1200, untilRestart, "17:40 is 20 minutes before 18:00");
		ctx.Equal(4, clocks.Count(), "three warnings and the restart");
		if (clocks.Count() == 4)
		{
			ctx.Equal(Clock(17, 45), clocks[0], "first warning at 17:45");
			ctx.Equal(Clock(17, 55), clocks[1], "second warning at 17:55");
			ctx.Equal(Clock(17, 59), clocks[2], "third warning at 17:59");
			ctx.Equal(Clock(18, 0), clocks[3], "the restart at 18:00");
			ctx.Equal(15, minutes[0], "first warning has 15 minutes left");
			ctx.Equal(5, minutes[1], "second warning has 5 minutes left");
			ctx.Equal(1, minutes[2], "third warning has 1 minute left");
			ctx.Equal(0, minutes[3], "the restart has 0 minutes left");
		}

		// A warning already passed is not listed again.
		clocks.Clear();
		minutes.Clear();
		untilRestart = LF_RestartSchedule.Plan(flagship, Clock(17, 50), clocks, minutes);
		ctx.Equal(600, untilRestart, "17:50 is 10 minutes before 18:00");
		ctx.Equal(3, clocks.Count(), "the 17:45 warning has passed");
		if (clocks.Count() == 3)
		{
			ctx.Equal(Clock(17, 55), clocks[0], "next warning at 17:55");
			ctx.Equal(5, minutes[0], "next warning has 5 minutes left");
		}

		// Just past the last warning: only the restart is left.
		clocks.Clear();
		minutes.Clear();
		untilRestart = LF_RestartSchedule.Plan(flagship, Clock(17, 59, 30), clocks, minutes);
		ctx.Equal(30, untilRestart, "17:59:30 is 30 seconds before 18:00");
		ctx.Equal(1, clocks.Count(), "only the restart is left");

		// The nearer of two times wins, and the day wraps: 18:30 looks ahead to 06:00 tomorrow.
		ctx.Equal(Clock(0, 30), LF_RestartSchedule.SecondsToNext(flagship, Clock(5, 30)), "05:30 is 30 minutes before 06:00");
		ctx.Equal(Clock(11, 30), LF_RestartSchedule.SecondsToNext(flagship, Clock(18, 30)), "18:30 wraps to 06:00 tomorrow");
		ctx.Equal(0, LF_RestartSchedule.SecondsToNext(flagship, Clock(18, 0)), "the clock on the restart is due now");

		// Midnight wrap: a 00:05 restart warned from the evening before.
		LF_RestartConfig midnight = Make("00:05", "", 15, 5, 1);
		clocks.Clear();
		minutes.Clear();
		untilRestart = LF_RestartSchedule.Plan(midnight, Clock(23, 50), clocks, minutes);
		ctx.Equal(900, untilRestart, "23:50 is 15 minutes before 00:05");
		ctx.Equal(4, clocks.Count(), "the midnight restart has four steps");
		if (clocks.Count() == 4)
		{
			ctx.Equal(Clock(23, 50), clocks[0], "the 15 minute warning is the clock itself");
			ctx.Equal(Clock(0, 0), clocks[1], "the 5 minute warning wraps to 00:00");
			ctx.Equal(Clock(0, 4), clocks[2], "the 1 minute warning is 00:04");
			ctx.Equal(Clock(0, 5), clocks[3], "the restart is 00:05");
		}
		ctx.Equal(1, LF_RestartSchedule.SecondsToNext(Make("00:00", "", 1, 0, 0), Clock(23, 59, 59)), "a second before midnight");

		// Leads in any order and repeated give the same steps; a bad time entry is skipped.
		LF_RestartConfig messy = Make("nonsense", "18:00", 1, 15, 5, 5);
		clocks.Clear();
		minutes.Clear();
		untilRestart = LF_RestartSchedule.Plan(messy, Clock(17, 40), clocks, minutes);
		ctx.Equal(1200, untilRestart, "the bad time is skipped");
		ctx.Equal(4, clocks.Count(), "unsorted and repeated leads give one step each");
		if (clocks.Count() == 4)
		{
			ctx.Equal(Clock(17, 45), clocks[0], "steps are in order");
			ctx.Equal(Clock(17, 59), clocks[2], "steps are in order, third");
		}

		// Nothing configured: no restart, no warnings.
		clocks.Clear();
		minutes.Clear();
		LF_RestartConfig none = LF_RestartConfig.CreateDefault();
		untilRestart = LF_RestartSchedule.Plan(none, Clock(12, 0), clocks, minutes);
		ctx.Equal(-1, untilRestart, "an empty list schedules no restart");
		ctx.Equal(0, clocks.Count(), "an empty list has no warnings");
		ctx.Equal(-1, LF_RestartSchedule.SecondsToNext(null, Clock(12, 0)), "a null config schedules no restart");

		// A time is HH:MM.
		ctx.Equal(Clock(6, 0), LF_RestartSchedule.ParseTime("06:00"), "06:00 parses");
		ctx.Equal(Clock(23, 59), LF_RestartSchedule.ParseTime("23:59"), "23:59 parses");
		ctx.Equal(-1, LF_RestartSchedule.ParseTime("24:00"), "24:00 is not a time");
		ctx.Equal(-1, LF_RestartSchedule.ParseTime("6:00"), "6:00 is not HH:MM");

		// The live clock is a second of the day.
		int now = LF_RestartSchedule.NowSeconds();
		ctx.True(now >= 0 && now < LF_RestartSchedule.DAY_SECONDS, "the wall clock is within the day");
	}
}
