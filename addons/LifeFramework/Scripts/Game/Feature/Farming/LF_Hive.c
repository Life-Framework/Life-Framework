//------------------------------------------------------------------------------------------------
//! One saved hive (the "hives" save section, #3447): its key, the combs inside and the game minutes of
//! work toward the next comb. Field order is the binary save order; append only.
class LF_HiveRow : Managed
{
	static const int ROW_VERSION = 1;

	int m_iVersion;
	string m_sHiveKey;
	int m_iCombs;
	int m_iProgressMinutes;

	//------------------------------------------------------------------------------------------------
	static LF_HiveRow Create(string key, int combs, int progressMinutes)
	{
		LF_HiveRow row = new LF_HiveRow();
		row.m_iVersion = ROW_VERSION;
		row.m_sHiveKey = key;
		row.m_iCombs = combs;
		row.m_iProgressMinutes = progressMinutes;
		return row;
	}

	//------------------------------------------------------------------------------------------------
	LF_HiveRow Copy()
	{
		return Create(m_sHiveKey, m_iCombs, m_iProgressMinutes);
	}
}

//------------------------------------------------------------------------------------------------
//! The server's live beehives by key, their clock and the bridge to the save (#3447). A hive registers
//! itself when it exists; a saved hive that arrives before its entity waits here and is applied when the
//! entity registers, so load order never matters (the LF_FarmPlots pattern, its own section).
class LF_Hives
{
	static const string FEATURE = "Farming";
	//! Tick cadence only: work is computed from the elapsed world time, so this changes no balance.
	static const int TICK_MS = 5000; // lf-fixed: tick cadence, the work comes from elapsed game time

	protected static ref map<string, ref LF_HiveRow> s_mPending;
	protected static bool s_bTicking;
	protected static float s_fLastTickMs = -1;

	//------------------------------------------------------------------------------------------------
	static map<string, LF_HiveComponent> Live()
	{
		return LF_HiveComponent.Live();
	}

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref LF_HiveRow> Pending()
	{
		if (!s_mPending)
			s_mPending = new map<string, ref LF_HiveRow>();

		return s_mPending;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		StopTicking();
		LF_HiveComponent.ResetLive();
		s_mPending = null;
	}

	//------------------------------------------------------------------------------------------------
	static void StopTicking()
	{
		if (!s_bTicking)
			return;

		s_bTicking = false;
		ArmaReforgerScripted game = GetGame();
		if (game && game.GetCallqueue())
			game.GetCallqueue().Remove(Tick);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a hive appeared. A saved hive that arrived first is applied; the clock starts.
	static void Register(LF_HiveComponent hive)
	{
		if (!hive)
			return;

		string key = hive.GetHiveKey();
		if (key.IsEmpty())
		{
			LF_Debug.Error(FEATURE, "a beehive has no key; it works but is not saved");
			return;
		}

		LF_HiveComponent known = Live().Get(key);
		if (known && known != hive)
		{
			LF_Debug.Error(FEATURE, string.Format("hive key %1 is used twice; the second hive is not registered", key));
			return;
		}

		Live().Set(key, hive);
		LF_HiveRow row = Pending().Get(key);
		if (row)
		{
			hive.Import(row);
			Pending().Remove(key);
		}

		if (!s_bTicking)
		{
			s_bTicking = true;
			s_fLastTickMs = -1;
			GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a hive is gone (deleted or destroyed) and its combs with it. The clock stops with the last hive.
	static void Unregister(LF_HiveComponent hive)
	{
		if (hive && Live().Get(hive.GetHiveKey()) == hive)
			Live().Remove(hive.GetHiveKey());

		if (Live().IsEmpty())
			StopTicking();
	}

	//------------------------------------------------------------------------------------------------
	static LF_HiveComponent Find(string key)
	{
		return Live().Get(key);
	}

	//------------------------------------------------------------------------------------------------
	//! Combs a saved hive waits with until its entity registers; -1 when nothing waits.
	static int PendingCombs(string key)
	{
		LF_HiveRow row = Pending().Get(key);
		if (!row)
			return -1;

		return row.m_iCombs;
	}

	//------------------------------------------------------------------------------------------------
	//! Is it raining now: the weather state's name says rain or storm.
	static bool IsRaining()
	{
		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		if (!world)
			return false;

		TimeAndWeatherManagerEntity clock = world.GetTimeAndWeatherManager();
		if (!clock)
			return false;

		WeatherState state = clock.GetCurrentWeatherState();
		if (!state)
			return false;

		string name = state.GetStateName();
		name.ToLower();
		return name.Contains("rain") || name.Contains("storm");
	}

	//------------------------------------------------------------------------------------------------
	//! Server: every hive works by the game hours since the last tick, so a late tick never loses work.
	static void Tick()
	{
		if (!Replication.IsServer())
			return;

		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		if (!world)
			return;

		float nowMs = world.GetWorldTime();
		if (s_fLastTickMs < 0 || nowMs < s_fLastTickMs)
		{
			s_fLastTickMs = nowMs;
			return;
		}

		float seconds = (nowMs - s_fLastTickMs) / 1000;
		s_fLastTickMs = nowMs;

		// Game hours: a game day of 24 h lasts GetDayDuration() real seconds. No clock: one hour an hour.
		float gameHours = seconds / 3600;
		float hourOfDay = 12;
		TimeAndWeatherManagerEntity clock = world.GetTimeAndWeatherManager();
		if (clock)
		{
			if (clock.GetDayDuration() > 0)
				gameHours = seconds * 24 / clock.GetDayDuration();

			hourOfDay = clock.GetTimeOfTheDay();
		}

		AdvanceAll(gameHours, IsRaining(), hourOfDay);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: every live hive works for `hours` game hours at this hour of the day. Nothing works while
	//! the Farming system is off (#960). \return Combs gained across all hives.
	static int AdvanceAll(float hours, bool raining, float hourOfDay)
	{
		if (!LF_Systems.IsEnabled(LF_FarmPlots.SYSTEM))
			return 0;

		LF_HiveConfig config = LF_HiveConfig.Get();
		if (!config)
			return 0;

		bool night = LF_HiveRules.IsNight(hourOfDay, config.m_fNightStartHour, config.m_fNightEndHour);
		int gained;
		foreach (string key, LF_HiveComponent hive : Live())
		{
			if (hive)
				gained += hive.ServerAdvance(hours, raining, night);
		}

		return gained;
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: every live hive, plus saved hives whose entity has not appeared.
	static array<ref LF_HiveRow> ExportAll()
	{
		array<ref LF_HiveRow> rows = new array<ref LF_HiveRow>();
		foreach (string key, LF_HiveComponent hive : Live())
		{
			if (!hive)
				continue;

			LF_HiveRow row = hive.Export();
			if (row)
				rows.Insert(row);
		}

		foreach (string key, LF_HiveRow pending : Pending())
		{
			if (!Live().Contains(key))
				rows.Insert(pending.Copy());
		}

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: the saved hives replace the live ones. A live hive with no row is emptied; a row
	//! with no live hive waits for it. Idempotent: a second load converges. \return Rows kept.
	static int ApplyAll(array<ref LF_HiveRow> rows)
	{
		s_mPending = new map<string, ref LF_HiveRow>();
		int kept;
		if (rows)
		{
			foreach (LF_HiveRow row : rows)
			{
				if (!row || row.m_sHiveKey.IsEmpty())
				{
					LF_Debug.Warn(FEATURE, "load skipped a hive row with no key");
					continue;
				}

				s_mPending.Set(row.m_sHiveKey, row.Copy());
				kept++;
			}
		}

		foreach (string key, LF_HiveComponent hive : Live())
		{
			if (!hive)
				continue;

			LF_HiveRow row = s_mPending.Get(key);
			hive.Import(row);
			if (row)
				s_mPending.Remove(key);
		}

		return kept;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_Hives's statics (LF_StaticReset finds it by reflection).
class LF_Hives_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_Hives";
	}

	override void Apply()
	{
		LF_Hives.ResetStatics();
	}
}
