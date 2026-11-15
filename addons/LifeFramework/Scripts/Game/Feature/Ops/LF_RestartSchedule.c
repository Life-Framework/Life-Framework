//------------------------------------------------------------------------------------------------
//! The restart schedule (Ops, M4, #1264): from a clock, the next restart and the warning times still
//! ahead. Pure: every function takes the clock as an argument (seconds since local midnight), so tests
//! are deterministic; the ticker (#1348) reads NowSeconds() and the config.
//!
//! Reads LF_RestartConfig (`restarts.times`, `restarts.warningMinutes`) and does not change it. An
//! empty list means no scheduled restarts. A time that is not HH:MM is skipped (the config already
//! refuses such a list whole; this guards a hand-built one). Nothing persists: it is all the clock.
//------------------------------------------------------------------------------------------------
class LF_RestartSchedule
{
	static const int DAY_SECONDS = 86400;	// lf-fixed: seconds in a day

	//------------------------------------------------------------------------------------------------
	//! \return Seconds since local midnight of the server's wall clock.
	static int NowSeconds()
	{
		int hour, minute, second;
		System.GetHourMinuteSecond(hour, minute, second);
		return hour * 3600 + minute * 60 + second;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Seconds since midnight for an HH:MM time, or -1 when the text is not a 24-hour HH:MM.
	static int ParseTime(string time)
	{
		if (!LF_RestartConfig.IsValidTime(time))
			return -1;

		return time.Substring(0, 2).ToInt() * 3600 + time.Substring(3, 2).ToInt() * 60;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Seconds from `nowSeconds` to the next restart, 0 when the clock is on one, wrapping past
	//! midnight; -1 when no restart is scheduled.
	static int SecondsToNext(LF_RestartConfig config, int nowSeconds)
	{
		if (!config || !config.m_aTimes)
			return -1;

		int best = -1;
		foreach (string time : config.m_aTimes)
		{
			int at = ParseTime(time);
			if (at < 0)
				continue;

			int delta = (at - nowSeconds + DAY_SECONDS) % DAY_SECONDS;
			if (best < 0 || delta < best)
				best = delta;
		}
		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! The next restart and every warning still ahead of `nowSeconds`, soonest first, the restart last.
	//! `clockSeconds` gets each step's time of day (seconds since midnight) and `minutesLeft` its lead
	//! (0 for the restart itself); both are cleared first. A warning whose time has already passed is
	//! left out; a repeated lead counts once.
	//! \return Seconds to the next restart, or -1 (both lists empty) when none is scheduled.
	static int Plan(LF_RestartConfig config, int nowSeconds, notnull array<int> clockSeconds, notnull array<int> minutesLeft)
	{
		clockSeconds.Clear();
		minutesLeft.Clear();

		int untilRestart = SecondsToNext(config, nowSeconds);
		if (untilRestart < 0)
			return -1;

		// Insertion sort by seconds from now; the lists stay a handful of entries.
		array<int> ahead = {};
		array<int> leads = {};
		if (config.m_aWarningMinutes)
		{
			foreach (int lead : config.m_aWarningMinutes)
			{
				if (lead < 1 || leads.Find(lead) != -1)
					continue;

				int delay = untilRestart - lead * 60;
				if (delay < 0)
					continue;

				int at = 0;
				while (at < ahead.Count() && ahead[at] < delay)
					at++;
				ahead.InsertAt(delay, at);
				leads.InsertAt(lead, at);
			}
		}

		for (int i = 0; i < ahead.Count(); i++)
		{
			clockSeconds.Insert((nowSeconds + ahead[i]) % DAY_SECONDS);
			minutesLeft.Insert(leads[i]);
		}

		clockSeconds.Insert((nowSeconds + untilRestart) % DAY_SECONDS);
		minutesLeft.Insert(0);
		return untilRestart;
	}
}
