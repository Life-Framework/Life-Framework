//------------------------------------------------------------------------------------------------
//! The server's list of live farm plots, their growth clock, and the bridge to the save. A plot
//! registers itself when it exists; a saved crop that arrives before its plot waits here and is
//! applied when the plot registers, and the other way round, so load order never matters.
class LF_FarmPlots
{
	static const string FEATURE = "Farming";
	//! The system id (LF_Systems, #960). Off: plots offer nothing, refuse, and stop growing; saved crops stay.
	static const string SYSTEM = "Farming";
	static const int TICK_MS = 1000;

	protected static ref map<string, ref LF_FarmPlotRow> s_mPending;
	protected static bool s_bTicking;
	protected static float s_fLastTickMs = -1;

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref LF_FarmPlotRow> Pending()
	{
		if (!s_mPending)
			s_mPending = new map<string, ref LF_FarmPlotRow>();

		return s_mPending;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		StopTicking();
		LF_FarmPlotComponent.ResetLive();
		s_mPending = null;
	}

	//------------------------------------------------------------------------------------------------
	//! The growth clock stops when no plot is left (the world is unloading, or a reset).
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
	//! Server: a plot is gone. The clock stops with the last one.
	static void Unregister(LF_FarmPlotComponent plot)
	{
		map<string, LF_FarmPlotComponent> live = LF_FarmPlotComponent.Live();
		if (plot && live.Get(plot.GetPlotKey()) == plot)
			live.Remove(plot.GetPlotKey());

		if (live.IsEmpty())
			StopTicking();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a plot appeared. A saved crop that arrived first is applied; the growth clock starts.
	static void Register(LF_FarmPlotComponent plot)
	{
		if (!plot)
			return;

		string key = plot.GetPlotKey();
		if (key.IsEmpty())
		{
			LF_Debug.Error(FEATURE, "a farm plot has no plot key; it grows but its crop is not saved");
			return;
		}

		LF_FarmPlotComponent known = LF_FarmPlotComponent.Live().Get(key);
		if (known && known != plot)
		{
			LF_Debug.Error(FEATURE, string.Format("plot key %1 is used twice; the second plot is not registered", key));
			return;
		}

		LF_FarmPlotComponent.Live().Set(key, plot);

		LF_FarmPlotRow row = Pending().Get(key);
		if (row)
		{
			plot.Import(row);
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
	static LF_FarmPlotComponent Find(string plotKey)
	{
		return LF_FarmPlotComponent.Live().Get(plotKey);
	}

	//------------------------------------------------------------------------------------------------
	static int Count()
	{
		return LF_FarmPlotComponent.Live().Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: every planted plot grows by the world time since the last tick, so a late tick
	//! never loses growth.
	static void Tick()
	{
		if (!Replication.IsServer())
			return;

		BaseWorld world = GetGame().GetWorld();
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
		AdvanceAll(seconds);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: every planted plot grows by seconds. While the Farming system is off nothing grows,
	//! so a crop comes back exactly as it was when the system is turned on again (#960).
	//! eturn How many plots grew.
	static int AdvanceAll(float seconds)
	{
		if (!LF_Systems.IsEnabled(SYSTEM))
			return 0;

		int grew;
		foreach (string key, LF_FarmPlotComponent plot : LF_FarmPlotComponent.Live())
		{
			if (!plot || plot.IsEmpty())
				continue;

			plot.ServerAdvance(seconds);
			grew++;
		}

		return grew;
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: every planted plot, plus saved crops whose plot has not appeared.
	static array<ref LF_FarmPlotRow> ExportAll()
	{
		array<ref LF_FarmPlotRow> result = new array<ref LF_FarmPlotRow>();
		foreach (string key, LF_FarmPlotComponent plot : LF_FarmPlotComponent.Live())
		{
			if (!plot)
				continue;

			LF_FarmPlotRow row = plot.Export();
			if (row)
				result.Insert(row);
		}

		foreach (string key, LF_FarmPlotRow pending : Pending())
			result.Insert(pending.Copy());

		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: the saved crops replace the live ones. A live plot with no row is emptied;
	//! a row with no live plot waits for it. Idempotent: a second load converges.
	static void ApplyAll(array<ref LF_FarmPlotRow> records)
	{
		s_mPending = new map<string, ref LF_FarmPlotRow>();
		if (records)
		{
			foreach (LF_FarmPlotRow record : records)
			{
				if (!record || record.m_sPlotKey.IsEmpty())
				{
					LF_Debug.Warn(FEATURE, "load skipped a farm plot row with no key");
					continue;
				}

				s_mPending.Set(record.m_sPlotKey, record.Copy());
			}
		}

		foreach (string key, LF_FarmPlotComponent plot : LF_FarmPlotComponent.Live())
		{
			if (!plot)
				continue;

			LF_FarmPlotRow row = s_mPending.Get(key);
			plot.Import(row);
			if (row)
				s_mPending.Remove(key);
		}
	}
}
