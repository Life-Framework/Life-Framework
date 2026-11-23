// red-proof: observed 2026-11-23 with `tools cli test --no-build --red LF_Test_IslandCalendar`: RED-OK (`RedProof()` skips every calendar tick, so no row fires: `it acts on the third try (expected 3, got 0)` and the fire counts fail). Green with `--only`.
//! events/island-calendar (#4238): rows fire at their weekday and hour, once. Operations are fakes
//! registered on the event shell under test ids; the clock is passed in as a day number and an hour,
//! so no world and no real clock. Covers the date maths, the grace window, the once-per-slot rule,
//! weekday gating, a row waiting for an unregistered operation, bad rows and the date-rollover fix;
//! and that the shipped Configs/Events/LF_IslandCalendar.conf loads with every row valid.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_IslandCalendar : LF_Test
{
	static const string OP_BELL = "test_cal_bell";
	static const string OP_SIREN = "test_cal_siren";
	static const string OP_MARKET = "test_cal_market";
	static const string OP_LATE = "test_cal_late";

	// 1989-06-03 was a Saturday; the days after it are Sunday and Monday.
	protected int m_iSaturday;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "events/island-calendar";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_IslandCalendarConfig Config()
	{
		LF_IslandCalendarConfig config = new LF_IslandCalendarConfig();
		config.m_aRows = {};
		config.m_aRows.Insert(LF_IslandCalendarRow.Make("bell", 0, 7, 21, 1, OP_BELL));
		config.m_aRows.Insert(LF_IslandCalendarRow.Make("siren", 0, 6, 22, 8, OP_SIREN));
		config.m_aRows.Insert(LF_IslandCalendarRow.Make("market", 6, 8, 8, 1, OP_MARKET));
		config.m_aRows.Insert(LF_IslandCalendarRow.Make("late", 0, 12, 12, 1, OP_LATE));
		return config;
	}

	//------------------------------------------------------------------------------------------------
	protected void Tick(float now, int day, float hour)
	{
		if (!RedProof())
			LF_IslandCalendar.Tick(now, day, hour);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		m_iSaturday = LF_IslandCalendarRules.DayNumber(1989, 6, 3);
		int sunday = m_iSaturday + 1;
		int monday = m_iSaturday + 2;

		// The date maths.
		ctx.Equal(0, LF_IslandCalendarRules.DayNumber(1970, 1, 1), "the epoch is day 0");
		ctx.Equal(4, LF_IslandCalendarRules.Weekday(0), "1970-01-01 was a Thursday");
		ctx.Equal(6, LF_IslandCalendarRules.Weekday(m_iSaturday), "1989-06-03 was a Saturday");
		ctx.Equal(7, LF_IslandCalendarRules.Weekday(sunday), "the next day is a Sunday");
		ctx.Equal(1, LF_IslandCalendarRules.Weekday(monday), "then Monday");
		ctx.Equal(m_iSaturday + 28, LF_IslandCalendarRules.DayNumber(1989, 7, 1), "the month rolls: 1989-07-01 is 28 days on");
		ctx.Equal(m_iSaturday + 1, LF_IslandCalendarRules.LocalDayNumber(sunday, 3, 4), "same side of midnight: the clock's date");
		ctx.Equal(m_iSaturday, LF_IslandCalendarRules.LocalDayNumber(sunday, 1, 23.5), "solar hour still on yesterday's evening: yesterday");
		ctx.Equal(monday, LF_IslandCalendarRules.LocalDayNumber(sunday, 23.5, 1), "solar hour already past midnight: tomorrow");

		// Fakes on the event shell.
		LF_Test_IslandCalendarOp bell = new LF_Test_IslandCalendarOp();
		LF_Test_IslandCalendarOp siren = new LF_Test_IslandCalendarOp();
		LF_Test_IslandCalendarOp market = new LF_Test_IslandCalendarOp();
		LF_Test_IslandCalendarOp late = new LF_Test_IslandCalendarOp();
		LF_EventShell.RegisterOperation(OP_BELL, bell);
		LF_EventShell.RegisterOperation(OP_SIREN, siren);
		LF_EventShell.RegisterOperation(OP_MARKET, market);
		LF_IslandCalendar.Use(Config());
		ctx.Equal(4, LF_IslandCalendar.RowCount(), "four rows loaded");

		// Sunday 07:06: the bell is due, the siren is not (06, 14, 22), the market is a Saturday row.
		Tick(100, sunday, 7.1);
		ctx.Equal(1, bell.m_iRuns, "the bell rings at 07:00 on a Sunday");
		ctx.Equal(0, siren.m_iRuns, "the siren is not due at 07");
		ctx.Equal(0, market.m_iRuns, "the market row does not fire on a Sunday");

		// Once: more ticks in the same hour do nothing, and so does the second half of the hour.
		Tick(101, sunday, 7.1);
		Tick(102, sunday, 7.4);
		Tick(103, sunday, 7.9);
		ctx.Equal(1, bell.m_iRuns, "the bell rings once in the hour");
		ctx.EqualStr("bell", bell.m_sLastPhase, "the operation is run under the row id");
		ctx.EqualStr(LF_IslandCalendar.EVENT_ID, bell.m_sLastEvent, "under the calendar's event id");
		ctx.EqualFloat(100, bell.m_fLastPhaseStart, 0.01, "with a phase start of the first firing, so a scene counts it as one run");

		// Next hour fires again; hours outside 07 to 21 do not.
		Tick(200, sunday, 8.0);
		ctx.Equal(2, bell.m_iRuns, "08:00 rings again");
		Tick(300, sunday, 22.1);
		ctx.Equal(2, bell.m_iRuns, "22:00 is past the last bell hour");
		ctx.Equal(1, siren.m_iRuns, "but the 22:00 siren runs (06, 14, 22 from 6 every 8)");
		Tick(400, sunday, 6.2);
		ctx.Equal(2, siren.m_iRuns, "06:00 runs the siren");
		Tick(500, sunday, 14.0);
		ctx.Equal(3, siren.m_iRuns, "14:00 runs the siren");
		ctx.Equal(3, bell.m_iRuns, "and rings the bell");
		Tick(600, sunday, 10.0);
		ctx.Equal(3, siren.m_iRuns, "10:00 is not a siren hour");
		ctx.Equal(4, bell.m_iRuns, "10:00 rings the bell");

		// The same hour on the next day is a new slot.
		Tick(86500, monday, 7.1);
		ctx.Equal(5, bell.m_iRuns, "the same hour a day later fires again");

		// Weekday gate: Saturday 08:00 runs the market row, once.
		Tick(700, m_iSaturday, 8.2);
		ctx.Equal(1, market.m_iRuns, "the market row fires on a Saturday at 08");
		Tick(701, m_iSaturday, 8.3);
		ctx.Equal(1, market.m_iRuns, "once");
		Tick(800, monday, 8.0);
		ctx.Equal(1, market.m_iRuns, "not on a Monday");

		// A row whose operation is not registered waits, then runs in the same slot once it is.
		LF_IslandCalendarConfig lateConfig = new LF_IslandCalendarConfig();
		lateConfig.m_aRows = {};
		lateConfig.m_aRows.Insert(LF_IslandCalendarRow.Make("late", 0, 12, 12, 1, OP_LATE));
		LF_IslandCalendar.Use(lateConfig);
		Tick(900, sunday, 12.2);
		ctx.Equal(0, late.m_iRuns, "an unregistered operation does not run");
		ctx.Equal(0, LF_IslandCalendar.FiredCount(), "an unregistered row is not counted as fired");
		LF_EventShell.RegisterOperation(OP_LATE, late);
		Tick(901, sunday, 12.3);
		ctx.Equal(1, late.m_iRuns, "the operation registered in the same hour runs then");
		Tick(902, sunday, 12.4);
		ctx.Equal(1, late.m_iRuns, "and only once");
		ctx.Equal(1, LF_IslandCalendar.FiredCount(), "counted once");

		// A row whose operation did not act is tried again, and counted once it does.
		LF_Test_IslandCalendarOp stubborn = new LF_Test_IslandCalendarOp();
		stubborn.m_bAct = false;
		LF_EventShell.RegisterOperation("test_cal_stubborn", stubborn);
		LF_IslandCalendarConfig stubbornConfig = new LF_IslandCalendarConfig();
		stubbornConfig.m_aRows = {};
		stubbornConfig.m_aRows.Insert(LF_IslandCalendarRow.Make("stubborn", 0, 9, 9, 1, "test_cal_stubborn"));
		LF_IslandCalendar.Use(stubbornConfig);
		Tick(1000, sunday, 9.1);
		Tick(1001, sunday, 9.2);
		ctx.Equal(2, stubborn.m_iRuns, "an operation that did not act is asked again on the next tick");
		ctx.Equal(0, LF_IslandCalendar.FiredCount(), "and is not counted");
		stubborn.m_bAct = true;
		Tick(1002, sunday, 9.3);
		ctx.Equal(3, stubborn.m_iRuns, "it acts on the third try");
		Tick(1003, sunday, 9.4);
		ctx.Equal(3, stubborn.m_iRuns, "then it is done for the hour");
		Tick(1004, sunday, 9.7);
		ctx.Equal(3, stubborn.m_iRuns, "a tick past the half hour does nothing either");

		// A boot late in the hour never rings it.
		LF_IslandCalendar.Use(Config());
		int bellBefore = bell.m_iRuns;
		Tick(2000, sunday, 15.8);
		ctx.Equal(bellBefore, bell.m_iRuns, "a boot at 15:48 does not ring the 15:00 bell");
		ctx.Equal(0, LF_IslandCalendar.FiredCount(), "nothing fired");

		// Bad rows are refused, the rest load.
		LF_IslandCalendarConfig bad = new LF_IslandCalendarConfig();
		bad.m_aRows = {};
		bad.m_aRows.Insert(LF_IslandCalendarRow.Make("", 0, 7, 7, 1, OP_BELL));
		bad.m_aRows.Insert(LF_IslandCalendarRow.Make("no_op", 0, 7, 7, 1, ""));
		bad.m_aRows.Insert(LF_IslandCalendarRow.Make("backwards", 0, 9, 7, 1, OP_BELL));
		bad.m_aRows.Insert(LF_IslandCalendarRow.Make("step0", 0, 7, 9, 0, OP_BELL));
		bad.m_aRows.Insert(LF_IslandCalendarRow.Make("day9", 9, 7, 7, 1, OP_BELL));
		bad.m_aRows.Insert(LF_IslandCalendarRow.Make("hour25", 0, 7, 25, 1, OP_BELL));
		bad.m_aRows.Insert(LF_IslandCalendarRow.Make("good", 0, 7, 7, 1, OP_BELL));
		bad.m_aRows.Insert(LF_IslandCalendarRow.Make("good", 0, 8, 8, 1, OP_BELL));
		LF_IslandCalendar.Use(bad);
		ctx.Equal(1, LF_IslandCalendar.RowCount(), "six bad rows and a repeated id are left out, the good one stays");

		// Off row.
		LF_IslandCalendarRow off = LF_IslandCalendarRow.Make("off", 0, 7, 7, 1, OP_BELL);
		off.m_bEnabled = false;
		ctx.Equal(-1, LF_IslandCalendarRules.DueSlot(off, sunday, 7.1), "a row switched off is never due");

		// The shipped core file: loads, every row valid, the bell is hourly 07 to 21, the market is Saturday.
		LF_IslandCalendarLoad.ResetStatics();
		LF_IslandCalendarConfig shipped = LF_IslandCalendarLoad.Get();
		ctx.NotNull(shipped, "the core calendar conf loads");
		if (shipped && shipped.m_aRows)
		{
			ctx.True(shipped.m_aRows.Count() >= 8, "the core file has the eight weekly rows");
			bool allValid = true;
			foreach (LF_IslandCalendarRow shippedRow : shipped.m_aRows)
			{
				if (!LF_IslandCalendarRules.Validate(shippedRow).IsEmpty())
					allValid = false;
			}
			ctx.True(allValid, "every shipped row is valid");

			LF_IslandCalendar.Use(shipped);
			ctx.Equal(shipped.m_aRows.Count(), LF_IslandCalendar.RowCount(), "no shipped row is refused");
			int bellSlots;
			foreach (LF_IslandCalendarRow shippedRow2 : shipped.m_aRows)
			{
				if (shippedRow2.m_sId == "calendar_hour_bell" && LF_IslandCalendarRules.DueSlot(shippedRow2, sunday, 7.1) >= 0 && LF_IslandCalendarRules.DueSlot(shippedRow2, sunday, 21.1) >= 0 && LF_IslandCalendarRules.DueSlot(shippedRow2, sunday, 22.1) < 0)
					bellSlots++;
				if (shippedRow2.m_sId == "calendar_market_day" && LF_IslandCalendarRules.DueSlot(shippedRow2, m_iSaturday, 8.1) >= 0 && LF_IslandCalendarRules.DueSlot(shippedRow2, sunday, 8.1) < 0)
					bellSlots++;
			}
			ctx.Equal(2, bellSlots, "the shipped bell is hourly 07 to 21 and the market row is Saturday only");
		}
		LF_IslandCalendarLoad.ResetStatics();

		// Cleanup: the fakes leave the shell, the calendar forgets its rows.
		LF_EventShell.Unregister(OP_BELL);
		LF_EventShell.Unregister(OP_SIREN);
		LF_EventShell.Unregister(OP_MARKET);
		LF_EventShell.Unregister(OP_LATE);
		LF_EventShell.Unregister("test_cal_stubborn");
		LF_IslandCalendar.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! A fake operation module: counts its runs and keeps what it was asked.
class LF_Test_IslandCalendarOp : LF_EventOperation
{
	int m_iRuns;
	bool m_bAct = true;
	string m_sLastEvent;
	string m_sLastPhase;
	float m_fLastPhaseStart;

	//------------------------------------------------------------------------------------------------
	override bool Run(string eventId, string phaseName, LF_EventContext ctx)
	{
		m_iRuns++;
		m_sLastEvent = eventId;
		m_sLastPhase = phaseName;
		if (ctx)
			m_fLastPhaseStart = ctx.m_fPhaseStartedAt;
		return m_bAct;
	}
}
