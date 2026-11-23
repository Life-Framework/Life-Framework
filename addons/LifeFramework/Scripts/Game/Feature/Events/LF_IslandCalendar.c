//------------------------------------------------------------------------------------------------
//! One row of the island calendar (#4238, WO-W6.1): "on this weekday, at these hours, run this
//! operation module of the event shell". The row says WHEN and the operation id says WHAT: the
//! church bell, the ship's horn, the market-day scene are operation modules a pack registers
//! through the event module list (LF_EventModuleLoader). The core names no place or sound.
//! Weekdays count 1 Monday to 7 Sunday; 0 is every day. Hours are whole in-game hours (0 to 23):
//! the row fires at `m_iFromHour`, then every `m_iEveryHours`, up to and including `m_iToHour`.
//! A pack overlay of rows is a later card (the pack-layer tooling must know the list first).
[BaseContainerProps()]
class LF_IslandCalendarRow
{
	[Attribute("", UIWidgets.EditBox, "Row id, unique across the calendar. Never rename a shipped id")]
	string m_sId;

	[Attribute("0", UIWidgets.Auto, "Weekday it fires on: 1 Monday to 7 Sunday, 0 every day", params: "0 7")]
	int m_iWeekday;

	[Attribute("7", UIWidgets.Auto, "The first in-game hour it fires at (0 to 23)", params: "0 23")]
	int m_iFromHour;

	[Attribute("7", UIWidgets.Auto, "The last in-game hour it may fire at (0 to 23, not before the first)", params: "0 23")]
	int m_iToHour;

	[Attribute("1", UIWidgets.Auto, "Hours between firings from the first hour: 1 every hour, 8 for 06, 14, 22", params: "1 24")]
	int m_iEveryHours;

	[Attribute("", UIWidgets.EditBox, "The event shell operation module this row runs (a scene, a bell, a horn), registered by id from a module list")]
	string m_sOperationId;

	[Attribute("1", UIWidgets.CheckBox, "Off switch: a pack patch can silence one row")]
	bool m_bEnabled;

	//------------------------------------------------------------------------------------------------
	static LF_IslandCalendarRow Make(string id, int weekday, int fromHour, int toHour, int everyHours, string operationId)
	{
		LF_IslandCalendarRow row = new LF_IslandCalendarRow();
		row.m_sId = id;
		row.m_iWeekday = weekday;
		row.m_iFromHour = fromHour;
		row.m_iToHour = toHour;
		row.m_iEveryHours = everyHours;
		row.m_sOperationId = operationId;
		row.m_bEnabled = true;
		return row;
	}
}

//------------------------------------------------------------------------------------------------
//! The island calendar file root (Configs/Events/LF_IslandCalendar.conf).
[BaseContainerProps(configRoot: true)]
class LF_IslandCalendarConfig
{
	[Attribute("", UIWidgets.Object, "Every recurring scene, by weekday and hour")]
	ref array<ref LF_IslandCalendarRow> m_aRows;
}

//------------------------------------------------------------------------------------------------
//! The calendar's pure rules: which game day it is, whether a row is due, whether a row is usable.
//! No world, no clock: the caller passes the numbers.
class LF_IslandCalendarRules
{
	//! A row fires in the first half hour of its hour; a later start (a boot at 07:45) does not ring
	//! the 07:00 bell, and a tick lost to a hitch still catches the slot.
	static const float GRACE_HOURS = 0.5;

	//------------------------------------------------------------------------------------------------
	//! Days since 1970-01-01 of a calendar date (proleptic Gregorian), an int that stays exact.
	static int DayNumber(int year, int month, int day)
	{
		int y = year;
		if (month <= 2)
			y--;

		int era = y / 400;
		int yearOfEra = y - era * 400;
		int monthFromMarch = (month + 9) % 12;
		int dayOfYear = (153 * monthFromMarch + 2) / 5 + day - 1;
		int dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
		return era * 146097 + dayOfEra - 719468;
	}

	//------------------------------------------------------------------------------------------------
	//! 1 Monday to 7 Sunday for a day number (1970-01-01 was a Thursday).
	static int Weekday(int dayNumber)
	{
		int shifted = (dayNumber + 3) % 7;
		if (shifted < 0)
			shifted += 7;
		return shifted + 1;
	}

	//------------------------------------------------------------------------------------------------
	//! The day number the island lives on. The date comes from the world clock, the hour from the
	//! solar hour (LF_Atmosphere.HourNow); they roll over at different moments, so when the two
	//! clocks disagree by half a day the date follows the solar hour.
	static int LocalDayNumber(int clockDayNumber, float clockHour, float solarHour)
	{
		if (solarHour - clockHour > 12)
			return clockDayNumber - 1;
		if (clockHour - solarHour > 12)
			return clockDayNumber + 1;
		return clockDayNumber;
	}

	//------------------------------------------------------------------------------------------------
	//! \return an empty string when the row is usable, else why not.
	static string Validate(LF_IslandCalendarRow row)
	{
		if (!row)
			return "null row";
		if (row.m_sId.IsEmpty())
			return "no id";
		if (row.m_sOperationId.IsEmpty())
			return "no operation id";
		if (row.m_iWeekday < 0 || row.m_iWeekday > 7)
			return "weekday outside 0 to 7";
		if (row.m_iFromHour < 0 || row.m_iFromHour > 23 || row.m_iToHour < 0 || row.m_iToHour > 23)
			return "hour outside 0 to 23";
		if (row.m_iToHour < row.m_iFromHour)
			return "last hour before the first";
		if (row.m_iEveryHours < 1)
			return "every-hours below 1";
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! \return the slot key (day number * 24 + hour) when `row` is due at this weekday and hour, else -1.
	//! Two ticks in the same slot give the same key, which is how the runner fires once.
	static int DueSlot(LF_IslandCalendarRow row, int dayNumber, float hour)
	{
		if (!row || !row.m_bEnabled || dayNumber < 0 || !Validate(row).IsEmpty())
			return -1;

		if (row.m_iWeekday != 0 && row.m_iWeekday != Weekday(dayNumber))
			return -1;

		int whole = hour;
		if (hour - whole > GRACE_HOURS)
			return -1;
		if (whole < row.m_iFromHour || whole > row.m_iToHour)
			return -1;
		if ((whole - row.m_iFromHour) % row.m_iEveryHours != 0)
			return -1;

		return dayNumber * 24 + whole;
	}
}

//------------------------------------------------------------------------------------------------
//! The island calendar (#4238): once a second it reads the game day and hour and runs, once per
//! slot, the event shell operation each due row names (LF_EventShell.FindOperation). A slot that
//! was run is remembered for this boot, so a row fires once per day and hour; a restart forgets it
//! (the grace window keeps a late boot from ringing an old hour). A row whose operation is not
//! registered, is busy or did not act is tried again on the next tick until the grace window closes;
//! each is logged once. A scene operation waits while another scene is on stage. Server only.
class LF_IslandCalendar
{
	static const string FEATURE = "Events";
	static const string EVENT_ID = "island_calendar";
	static const int TICK_MS = 1000;	// lf-fixed: once a second, as the event shell

	protected static ref array<ref LF_IslandCalendarRow> s_aRows;
	protected static ref map<string, int> s_mFiredSlot;
	protected static ref set<string> s_aLogged;
	protected static bool s_bRunning;
	protected static int s_iFired;

	//------------------------------------------------------------------------------------------------
	//! Server, game start: loads the rows and starts the ticker.
	static void StartIfServer()
	{
		if (!Replication.IsServer() || s_bRunning || !GetGame())
			return;

		if (!s_aRows)
			Use(LF_IslandCalendarLoad.Get());

		s_bRunning = true;
		GetGame().GetCallqueue().CallLater(TickNow, TICK_MS, true);
		LF_Debug.Info(FEATURE, string.Format("island calendar started: %1 row(s)", RowCount()));
	}

	//------------------------------------------------------------------------------------------------
	static void Stop()
	{
		if (s_bRunning && GetGame())
			GetGame().GetCallqueue().Remove(TickNow);
		s_bRunning = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the rows (game start, tests). A bad row or a repeated id is logged and skipped; the rest still run.
	static void Use(LF_IslandCalendarConfig config)
	{
		s_aRows = {};
		s_mFiredSlot = new map<string, int>();
		s_aLogged = null;
		s_iFired = 0;
		if (!config || !config.m_aRows)
			return;

		set<string> seen = new set<string>();
		foreach (LF_IslandCalendarRow row : config.m_aRows)
		{
			string reason = LF_IslandCalendarRules.Validate(row);
			if (reason.IsEmpty() && seen.Contains(row.m_sId))
				reason = "duplicate row id";
			if (!reason.IsEmpty())
			{
				string id = "?";
				if (row)
					id = row.m_sId;
				LF_Debug.Error(FEATURE, string.Format("calendar row %1 skipped: %2", id, reason));
				continue;
			}

			seen.Insert(row.m_sId);
			s_aRows.Insert(row);
		}
	}

	//------------------------------------------------------------------------------------------------
	static int RowCount()
	{
		if (!s_aRows)
			return 0;
		return s_aRows.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Slots run since the rows were last loaded. Tests read it.
	static int FiredCount()
	{
		return s_iFired;
	}

	//------------------------------------------------------------------------------------------------
	protected static void TickNow()
	{
		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		if (!world)
			return;

		TimeAndWeatherManagerEntity clock = world.GetTimeAndWeatherManager();
		if (!clock)
			return;

		float solarHour = LF_Atmosphere.HourNow();
		int day = LF_IslandCalendarRules.LocalDayNumber(LF_IslandCalendarRules.DayNumber(clock.GetYear(), clock.GetMonth(), clock.GetDay()), clock.GetTimeOfTheDay(), solarHour);
		Tick(world.GetWorldTime() / 1000, day, solarHour);
	}

	//------------------------------------------------------------------------------------------------
	//! One pass over the rows at server second `now`, game day number `dayNumber` and in-game hour `hour`.
	//! The ticker calls it; tests drive it. \return how many rows ran their operation.
	static int Tick(float now, int dayNumber, float hour)
	{
		if (!s_aRows)
			return 0;

		int ran;
		foreach (LF_IslandCalendarRow row : s_aRows)
		{
			int slot = LF_IslandCalendarRules.DueSlot(row, dayNumber, hour);
			if (slot < 0)
				continue;

			int last;
			if (s_mFiredSlot.Find(row.m_sId, last) && last == slot)
				continue;

			if (Fire(row, slot, now, hour))
			{
				s_mFiredSlot.Set(row.m_sId, slot);
				s_iFired++;
				ran++;
			}
		}

		return ran;
	}

	//------------------------------------------------------------------------------------------------
	//! Runs the row's operation for this slot. \return true when it acted; false (logged once) to try again.
	protected static bool Fire(notnull LF_IslandCalendarRow row, int slot, float now, float hour)
	{
		LF_EventOperation operation = LF_EventShell.FindOperation(row.m_sOperationId);
		if (!operation)
		{
			LogOnce(row.m_sId + ":missing", string.Format("calendar row %1: operation %2 is not registered; the row waits for it", row.m_sId, row.m_sOperationId));
			return false;
		}

		// One scene on stage at a time: a scene operation would only queue behind it and lose the slot.
		if (LF_EventSceneOperation.Cast(operation) && LF_EventScenePlanner.RunningScene() != "")
		{
			LogOnce(string.Format("%1:%2", row.m_sId, slot), string.Format("calendar row %1: another scene is on stage; tried again until the hour is half over", row.m_sId));
			return false;
		}

		// A fresh phase start per slot, so a scene operation counts each firing as its own phase run.
		LF_EventContext ctx = LF_EventContext.Make(hour, 0, 0, now);
		if (!operation.Run(EVENT_ID, row.m_sId, ctx))
		{
			LogOnce(string.Format("%1:%2", row.m_sId, slot), string.Format("calendar row %1: operation %2 did not act; tried again until the hour is half over", row.m_sId, row.m_sOperationId));
			return false;
		}

		LF_Debug.Info(FEATURE, string.Format("calendar row %1 ran %2 (slot %3)", row.m_sId, row.m_sOperationId, slot));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void LogOnce(string key, string message)
	{
		if (!s_aLogged)
			s_aLogged = new set<string>();
		if (s_aLogged.Contains(key))
			return;

		s_aLogged.Insert(key);
		LF_Debug.Warn(FEATURE, message);
	}

	//------------------------------------------------------------------------------------------------
	//! World end (LF_StaticReset) and tests.
	static void ResetStatics()
	{
		Stop();
		s_aRows = null;
		s_mFiredSlot = null;
		s_aLogged = null;
		s_iFired = 0;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_IslandCalendar's statics (Core/LF_StaticReset.c).
class LF_IslandCalendar_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_IslandCalendar";
	}

	override void Apply()
	{
		LF_IslandCalendar.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! Starts the calendar at game start, without editing LF_GameModeRoleplay.c (M4 rule).
modded class LF_GameModeRoleplay
{
	//------------------------------------------------------------------------------------------------
	protected override void OnGameStart()
	{
		super.OnGameStart();
		LF_IslandCalendar.StartIfServer();
	}
}
