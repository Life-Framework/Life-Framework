//------------------------------------------------------------------------------------------------
//! The server's own once-a-minute performance line (Ops, M4, #1452):
//!   [LFDebug:Perf] players=N fps=F frameMs=M entities=E
//! so a 40-player session can be judged from the log afterwards. `host perf` (#1406) samples the
//! process from outside; this line is the game's view. Server only, off nothing: one line a minute
//! is the whole cost. Self-started by the modded game mode at the bottom of this file.
class LF_ServerPerfLog
{
	static const string FEATURE = "Perf";
	//! Seconds of fed time between two lines.
	static const float INTERVAL_S = 60; // lf-fixed: the line is one a minute by contract (#1452), not a tuning knob
	//! How often the poll runs; the gate decides whether a line is due.
	static const int POLL_MS = 5000; // lf-fixed: how often the gate is fed, not a gameplay value

	protected static ref LF_ServerPerfLogGate s_Gate;
	protected static int s_iLastTickMs;
	protected static bool s_bStarted;
	protected static string s_sLastLine;

	//------------------------------------------------------------------------------------------------
	//! The line body for one sample. frameMs is shown to one decimal; a bad sample clamps to zero.
	static string Format(int players, int fps, float frameMs, int entities)
	{
		if (players < 0)
			players = 0;
		if (fps < 0)
			fps = 0;
		if (entities < 0)
			entities = 0;
		if (frameMs < 0)
			frameMs = 0;

		int tenths = Math.Round(frameMs * 10);
		return string.Format("players=%1 fps=%2 frameMs=%3.%4 entities=%5", players, fps, tenths / 10, tenths % 10, entities);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: starts the minute poll once. A second call does nothing.
	static void Start()
	{
		if (s_bStarted)
			return;
		if (!Replication.IsServer())
			return;

		s_bStarted = true;
		s_Gate = new LF_ServerPerfLogGate();
		s_iLastTickMs = System.GetTickCount();
		GetGame().GetCallqueue().CallLater(Poll, POLL_MS, true);
		LF_Debug.Info(FEATURE, "server perf line on: one per 60 s");
	}

	//------------------------------------------------------------------------------------------------
	//! Stops the poll; Start() may run again afterwards.
	static void Stop()
	{
		if (!s_bStarted)
			return;

		s_bStarted = false;
		GetGame().GetCallqueue().Remove(Poll);
	}

	//------------------------------------------------------------------------------------------------
	//! The line the last Log() printed; tests read it.
	static string GetLastLine()
	{
		return s_sLastLine;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Poll()
	{
		int now = System.GetTickCount();
		float elapsedS = (now - s_iLastTickMs) / 1000.0;
		s_iLastTickMs = now;
		if (s_Gate && s_Gate.Feed(elapsedS))
			Log();
	}

	//------------------------------------------------------------------------------------------------
	//! Samples the game and prints one line.
	static void Log()
	{
		int players = 0;
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (playerManager)
			players = playerManager.GetPlayerCount();

		int entities = 0;
		BaseWorld world = GetGame().GetWorld();
		if (world)
			entities = world.GetNumEntities(0);

		s_sLastLine = Format(players, System.GetFPS(), System.GetFrameTimeS() * 1000, entities);
		LF_Debug.Info(FEATURE, s_sLastLine);
	}
}

//------------------------------------------------------------------------------------------------
//! Fires once per INTERVAL_S of fed time, however finely or coarsely the time arrives. A stall
//! longer than one interval fires once, then keeps the remainder, so the log never bursts.
class LF_ServerPerfLogGate
{
	protected float m_fAccumS;

	//------------------------------------------------------------------------------------------------
	//! Adds elapsed seconds; true when a line is due. A zero or negative feed adds nothing.
	bool Feed(float seconds)
	{
		if (seconds <= 0)
			return false;

		m_fAccumS += seconds;
		if (m_fAccumS < LF_ServerPerfLog.INTERVAL_S)
			return false;

		while (m_fAccumS >= LF_ServerPerfLog.INTERVAL_S)
		{
			m_fAccumS -= LF_ServerPerfLog.INTERVAL_S;
		}
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Starts the perf line once per world, on the server.
modded class LF_GameModeRoleplay
{
	//------------------------------------------------------------------------------------------------
	protected override void OnGameStart()
	{
		super.OnGameStart();
		LF_ServerPerfLog.Start();
	}
}
