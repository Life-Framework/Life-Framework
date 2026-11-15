//------------------------------------------------------------------------------------------------
//! Scheduled-restart ticker (Ops O6T, #1348). The game mode calls StartIfConfigured() on the server
//! beside LF_HostStop.StartIfRequested(). With restart times configured it subscribes the toast
//! (LF_RestartToast, #1265) and the close (LF_RestartClose, #1267), then ticks once a second on the
//! call queue (never per frame). Each tick reads LF_RestartSchedule for the next restart and
//! publishes LF_OnRestartWarning when the clock crosses a lead and LF_OnRestartDue at the restart
//! time, each once per restart. A tick that skips the restart second (a slow frame) still publishes
//! the due event, up to MISSED_DUE_SECONDS late. A lead whose time passed before the ticker started
//! is not announced late. Nothing persists: the state is the clock.
class LF_RestartWarner
{
	static const string FEATURE = "Ops";
	static const int TICK_MS = 1000;	// lf-fixed: once a second, the brief's cadence
	static const int MISSED_DUE_SECONDS = 60;	// lf-fixed: a due second skipped by at most this still fires

	protected static string s_sLastLog;
	protected static bool s_bRunning;
	protected static ref LF_RestartConfig s_Config;

	//! Clock second of the restart the fired leads belong to; -1 before the first tick.
	protected static int s_iTarget = -1;
	//! Seconds to that restart on the previous tick.
	protected static int s_iPrevUntil = -1;
	protected static bool s_bDueFired;
	protected static ref array<int> s_aFiredLeads = {};

	//------------------------------------------------------------------------------------------------
	//! Server: starts the warning ticker when the owner configured a restart schedule.
	static void StartIfConfigured()
	{
		if (!Replication.IsServer())
			return;

		if (!StartWith(LF_RestartConfig.Get()))
			return;

		if (s_bRunning || !GetGame())
			return;

		s_bRunning = true;
		GetGame().GetCallqueue().CallLater(TickNow, TICK_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Uses `config` and hooks the toast and the close; no call queue (tests drive Tick themselves).
	//! \return false when the config schedules no restart.
	static bool StartWith(LF_RestartConfig config)
	{
		Use(config);
		int next = LF_RestartSchedule.SecondsToNext(config, LF_RestartSchedule.NowSeconds());
		if (next < 0)
		{
			s_sLastLog = "restart warner: no restart times configured, ticker off";
			LF_Debug.Info(FEATURE, s_sLastLog);
			return false;
		}

		LF_RestartToast.Subscribe();
		LF_RestartClose.Register();

		s_sLastLog = string.Format("restart warner: ticking once a second, restarts at %1, warnings %2 min before, next in %3 s", JoinTimes(config), JoinLeads(config), next);
		LF_Debug.Info(FEATURE, s_sLastLog);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: the schedule Tick reads, without hooking anything; clears the per-restart state.
	static void Use(LF_RestartConfig config)
	{
		s_Config = config;
		s_iTarget = -1;
		s_iPrevUntil = -1;
		s_bDueFired = false;
		s_aFiredLeads.Clear();
	}

	//------------------------------------------------------------------------------------------------
	protected static void TickNow()
	{
		Tick(LF_RestartSchedule.NowSeconds());
	}

	//------------------------------------------------------------------------------------------------
	//! One step at `nowSeconds` (seconds since local midnight). Public so tests inject the clock.
	static void Tick(int nowSeconds)
	{
		int until = LF_RestartSchedule.SecondsToNext(s_Config, nowSeconds);
		if (until < 0)
			return;

		int target = (nowSeconds + until) % LF_RestartSchedule.DAY_SECONDS;
		if (target != s_iTarget)
		{
			// The restart we were counting down to has passed without its exact second being ticked.
			if (s_iTarget >= 0 && !s_bDueFired && s_iPrevUntil >= 0 && s_iPrevUntil <= MISSED_DUE_SECONDS)
				PublishDue(nowSeconds);

			// A new restart: on the first tick a lead counts only when the clock sits exactly on it.
			s_iTarget = target;
			s_iPrevUntil = until + 1;
			s_bDueFired = false;
			s_aFiredLeads.Clear();
		}

		if (s_Config.m_aWarningMinutes)
		{
			foreach (int lead : s_Config.m_aWarningMinutes)
			{
				int leadSeconds = lead * 60;
				if (lead < 1 || until > leadSeconds || leadSeconds >= s_iPrevUntil)
					continue;
				if (s_aFiredLeads.Find(lead) != -1)
					continue;

				s_aFiredLeads.Insert(lead);
				LF_EventBus.Publish(LF_OnRestartWarning.Create(lead, until));
				LF_Debug.Info(FEATURE, string.Format("restart warning: %1 minute(s) to the restart at %2 (%3 s)", lead, FormatClock(target), until));
			}
		}

		if (until == 0 && !s_bDueFired)
			PublishDue(nowSeconds);

		s_iPrevUntil = until;
	}

	//------------------------------------------------------------------------------------------------
	protected static void PublishDue(int nowSeconds)
	{
		s_bDueFired = true;
		LF_Debug.Info(FEATURE, string.Format("restart due at %1 (clock %2), publishing LF_OnRestartDue", FormatClock(s_iTarget), FormatClock(nowSeconds)));
		LF_EventBus.Publish(LF_OnRestartDue.Create());
	}

	//------------------------------------------------------------------------------------------------
	protected static string FormatClock(int seconds)
	{
		int h = seconds / 3600;
		int m = (seconds / 60) % 60;
		int s = seconds % 60;
		return string.Format("%1:%2:%3", h.ToString(2), m.ToString(2), s.ToString(2));
	}

	//------------------------------------------------------------------------------------------------
	protected static string JoinTimes(LF_RestartConfig config)
	{
		string text;
		foreach (string time : config.m_aTimes)
		{
			if (!text.IsEmpty())
				text += ", ";
			text += time;
		}
		return text;
	}

	//------------------------------------------------------------------------------------------------
	protected static string JoinLeads(LF_RestartConfig config)
	{
		string text;
		if (!config.m_aWarningMinutes)
			return text;

		foreach (int lead : config.m_aWarningMinutes)
		{
			if (!text.IsEmpty())
				text += ", ";
			text += lead.ToString();
		}
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: stops the ticker and clears the schedule state.
	static void ResetStatics()
	{
		if (s_bRunning && GetGame())
			GetGame().GetCallqueue().Remove(TickNow);
		s_bRunning = false;
		s_sLastLog = "";
		Use(null);
	}

	//------------------------------------------------------------------------------------------------
	//! The line the last StartIfConfigured() logged; tests read it.
	static string GetLastLog()
	{
		return s_sLastLog;
	}
}
