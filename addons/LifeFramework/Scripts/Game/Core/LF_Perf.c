//------------------------------------------------------------------------------------------------
//! Server cost counters (#335). A system times its periodic pass with Begin/End and counts a
//! replication bump or an RPC with Count. Everything is a no-op until the flag is on
//! (`-lfPerf` on the server command line, or SetEnabled in a test or the harness), so shipped
//! play pays one bool check per call. Readers: Get/Report here, the LF_Test_PerfBudget gate, and
//! `tools\cli perf`, which parses the `[LFDebug:Perf]` lines Report prints.
//!
//! Timers use System.GetTickCount, which is whole milliseconds: a pass under 1 ms adds 0 to the
//! total and still counts as a call, so read `calls` and `maxMs` beside `totalMs`. A benchmark
//! that needs finer numbers times a batch of passes and divides (LF_Test_PerfBudget does).
//!
//! Keys are "<System>.<name>", for example "Account.refreshAll" or "Replication.account". Names
//! are free text; a budget row names the keys it limits (Configs/Perf/LF_PerfBudget.conf).
class LF_Perf
{
	static const string FEATURE = "Perf";
	static const string CLI_FLAG = "lfPerf";

	protected static bool s_bFlagRead;
	protected static bool s_bEnabled;
	protected static ref map<string, int> s_mCount = new map<string, int>();
	protected static ref map<string, int> s_mTotalMs = new map<string, int>();
	protected static ref map<string, int> s_mMaxMs = new map<string, int>();
	protected static ref map<string, int> s_mCalls = new map<string, int>();
	protected static ref map<string, int> s_mGauge = new map<string, int>();

	//------------------------------------------------------------------------------------------------
	//! True when counters record. Reads the command line once.
	static bool IsEnabled()
	{
		if (!s_bFlagRead)
		{
			s_bFlagRead = true;
			if (System.IsCLIParam(CLI_FLAG))
				s_bEnabled = true;
		}

		return s_bEnabled;
	}

	//------------------------------------------------------------------------------------------------
	static void SetEnabled(bool enabled)
	{
		s_bFlagRead = true;
		s_bEnabled = enabled;
	}

	//------------------------------------------------------------------------------------------------
	//! Forgets every counter, timer and gauge. The flag stays.
	static void Reset()
	{
		s_mCount.Clear();
		s_mTotalMs.Clear();
		s_mMaxMs.Clear();
		s_mCalls.Clear();
		s_mGauge.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Adds to an event counter (a replication bump, an RPC, an entity spawned).
	static void Count(string key, int amount = 1)
	{
		if (!IsEnabled())
			return;

		int value;
		s_mCount.Find(key, value);
		s_mCount.Set(key, value + amount);
	}

	//------------------------------------------------------------------------------------------------
	//! Latest value of a level (entities alive, rows held). Overwrites.
	static void Gauge(string key, int value)
	{
		if (!IsEnabled())
			return;

		s_mGauge.Set(key, value);
	}

	//------------------------------------------------------------------------------------------------
	//! Start of a timed pass.
	//! \return A token for End, or -1 when the flag is off.
	static int Begin()
	{
		if (!IsEnabled())
			return -1;

		return System.GetTickCount();
	}

	//------------------------------------------------------------------------------------------------
	//! End of a timed pass. A token of -1 (flag was off at Begin) is ignored.
	static void End(string key, int token)
	{
		if (token < 0)
			return;

		AddTime(key, System.GetTickCount(token), 1);
	}

	//------------------------------------------------------------------------------------------------
	//! Adds a measured duration directly (a benchmark that timed a batch).
	static void AddTime(string key, int ms, int calls)
	{
		int total;
		s_mTotalMs.Find(key, total);
		s_mTotalMs.Set(key, total + ms);

		int calledBefore;
		s_mCalls.Find(key, calledBefore);
		s_mCalls.Set(key, calledBefore + calls);

		int worst;
		s_mMaxMs.Find(key, worst);
		if (ms > worst)
			s_mMaxMs.Set(key, ms);
	}

	//------------------------------------------------------------------------------------------------
	static int GetCount(string key)
	{
		int value;
		s_mCount.Find(key, value);
		return value;
	}

	//------------------------------------------------------------------------------------------------
	static int GetGauge(string key)
	{
		int value;
		s_mGauge.Find(key, value);
		return value;
	}

	//------------------------------------------------------------------------------------------------
	static int GetTotalMs(string key)
	{
		int value;
		s_mTotalMs.Find(key, value);
		return value;
	}

	//------------------------------------------------------------------------------------------------
	static int GetCalls(string key)
	{
		int value;
		s_mCalls.Find(key, value);
		return value;
	}

	//------------------------------------------------------------------------------------------------
	static int GetMaxMs(string key)
	{
		int value;
		s_mMaxMs.Find(key, value);
		return value;
	}

	//------------------------------------------------------------------------------------------------
	//! Prints every counter as a greppable line: `[LFDebug:Perf] count <key> <n>`,
	//! `time <key> calls=<n> totalMs=<n> maxMs=<n>`, `gauge <key> <n>`. Adds the live gauges
	//! the server can read for free (players, accounts, ledger rows).
	static void Report()
	{
		if (!IsEnabled())
		{
			LF_Debug.Warn(FEATURE, "report requested but counters are off (start the server with -lfPerf)");
			return;
		}

		PlayerManager players = GetGame().GetPlayerManager();
		if (players)
			Gauge("Server.players", players.GetPlayerCount());

		Gauge("Ledger.rows", LF_Ledger.Count());

		foreach (string key, int count : s_mCount)
		{
			LF_Debug.Info(FEATURE, string.Format("count %1 %2", key, count));
		}

		foreach (string timed, int calls : s_mCalls)
		{
			LF_Debug.Info(FEATURE, string.Format("time %1 calls=%2 totalMs=%3 maxMs=%4", timed, calls, GetTotalMs(timed), GetMaxMs(timed)));
		}

		foreach (string level, int value : s_mGauge)
		{
			LF_Debug.Info(FEATURE, string.Format("gauge %1 %2", level, value));
		}
	}
}
