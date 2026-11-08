// LF_FilmRecorder.c - in-engine recording for the filming harness.
//
// Screen capture cannot see the hidden LF-Agents desktop, and an unfocused
// game window stops presenting to the desktop, so tools/film recorded one
// frozen frame (#91). This records from inside the engine instead: a
// per-frame callqueue tick calls System.MakeScreenshot at a fixed rate, which
// writes the window's backbuffer (3D view, HUD, menus, toasts) as a BMP under
// $logs:el-film/<name>/. Stop writes index.txt: one line per shot with its
// real time, so tools/film encodes the sequence at true speed even when the
// engine missed a slot.
//
// LF_FilmPacer is the pure half (when to shoot, frame-time stats) and is
// covered by the fast suite. Recording needs a renderer: a dedicated server is
// refused with error:no_renderer. Driven by the harness verb `film`.

class LF_FilmPacer : Managed
{
	//! A frame slower than this counts as a stall.
	static const int STALL_MS = 100;

	protected int m_iIntervalMs;
	protected int m_iStartMs;
	protected int m_iLastFrameMs;
	protected int m_iNextShotMs;
	protected bool m_bShotLastFrame;
	protected int m_iFrames;
	protected int m_iShots;
	protected int m_iMaxGapMs;
	protected int m_iStalls;
	protected int m_iShotGapSumMs;
	protected int m_iShotGapCount;
	protected int m_iPlainGapSumMs;
	protected int m_iPlainGapCount;

	//------------------------------------------------------------------------------------------------
	//! Start pacing at nowMs. fps 0 measures frame times without shooting (the baseline).
	void Begin(int fps, int nowMs)
	{
		m_iIntervalMs = 0;
		if (fps > 0)
		{
			float interval = 1000;
			m_iIntervalMs = Math.Round(interval / fps);
		}
		m_iStartMs = nowMs;
		m_iLastFrameMs = nowMs;
		m_iNextShotMs = nowMs;
		m_bShotLastFrame = false;
		m_iFrames = 0;
		m_iShots = 0;
		m_iMaxGapMs = 0;
		m_iStalls = 0;
		m_iShotGapSumMs = 0;
		m_iShotGapCount = 0;
		m_iPlainGapSumMs = 0;
		m_iPlainGapCount = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! One rendered frame at nowMs. Returns true when a shot is due. A late
	//! frame takes one shot and the schedule moves on from now, so a stall
	//! never produces a burst of catch-up shots.
	bool OnFrame(int nowMs)
	{
		int gap = nowMs - m_iLastFrameMs;
		m_iLastFrameMs = nowMs;
		m_iFrames++;
		if (gap > m_iMaxGapMs)
			m_iMaxGapMs = gap;
		if (gap > STALL_MS)
			m_iStalls++;
		// The frame after a shot carries the shot's cost.
		if (m_bShotLastFrame)
		{
			m_iShotGapSumMs += gap;
			m_iShotGapCount++;
		}
		else if (m_iFrames > 1)
		{
			m_iPlainGapSumMs += gap;
			m_iPlainGapCount++;
		}

		m_bShotLastFrame = false;
		if (m_iIntervalMs <= 0 || nowMs < m_iNextShotMs)
			return false;

		m_iNextShotMs += m_iIntervalMs;
		if (m_iNextShotMs <= nowMs)
			m_iNextShotMs = nowMs + m_iIntervalMs;
		m_iShots++;
		m_bShotLastFrame = true;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	int GetShots()
	{
		return m_iShots;
	}

	//------------------------------------------------------------------------------------------------
	int GetFrames()
	{
		return m_iFrames;
	}

	//------------------------------------------------------------------------------------------------
	int GetStalls()
	{
		return m_iStalls;
	}

	//------------------------------------------------------------------------------------------------
	int GetMaxGapMs()
	{
		return m_iMaxGapMs;
	}

	//------------------------------------------------------------------------------------------------
	int ElapsedMs(int nowMs)
	{
		return nowMs - m_iStartMs;
	}

	//------------------------------------------------------------------------------------------------
	//! Mean frame time of the frames right after a shot, and of every other frame.
	float ShotFrameMs()
	{
		if (m_iShotGapCount <= 0)
			return 0;
		float sum = m_iShotGapSumMs;
		return sum / m_iShotGapCount;
	}

	//------------------------------------------------------------------------------------------------
	float PlainFrameMs()
	{
		if (m_iPlainGapCount <= 0)
			return 0;
		float sum = m_iPlainGapSumMs;
		return sum / m_iPlainGapCount;
	}

	//------------------------------------------------------------------------------------------------
	//! "shots=120 seconds=10.0 fps=12.0 gameFps=58.1 frameMs=17.2 shotFrameMs=31.0 maxGapMs=64 stalls=0"
	string Summary(int nowMs)
	{
		float seconds = ElapsedMs(nowMs);
		seconds = seconds / 1000;
		float fps = 0;
		float gameFps = 0;
		if (seconds > 0)
		{
			fps = m_iShots;
			fps = fps / seconds;
			gameFps = m_iFrames;
			gameFps = gameFps / seconds;
		}
		return string.Format("shots=%1 seconds=%2 fps=%3 gameFps=%4 frameMs=%5 shotFrameMs=%6 maxGapMs=%7 stalls=%8",
			m_iShots, Round1(seconds), Round1(fps), Round1(gameFps), Round1(PlainFrameMs()), Round1(ShotFrameMs()), m_iMaxGapMs, m_iStalls);
	}

	//------------------------------------------------------------------------------------------------
	protected static string Round1(float value)
	{
		float tenths = Math.Round(value * 10);
		return (tenths / 10).ToString();
	}
}

class LF_FilmRecorder
{
	protected static const string FEATURE = "Film";
	static const string ROOT = "$logs:el-film";
	static const int MAX_FPS = 30;
	//! Default and ceiling for one recording. A 1280x720 BMP is about 3.7 MB, so
	//! 12 fps for 300 s is about 13 GB on disk until tools/film encodes it.
	static const int DEFAULT_MAX_SECONDS = 300;

	protected static ref LF_FilmPacer s_Pacer;
	protected static ref array<int> s_aShotMs;
	protected static string s_sDir;
	protected static int s_iFps;
	protected static int s_iMaxMs;
	protected static int s_iFailed;
	protected static bool s_bOn;
	protected static string s_sLast = "error:not_recording";

	//------------------------------------------------------------------------------------------------
	static bool IsRecording()
	{
		return s_bOn;
	}

	//------------------------------------------------------------------------------------------------
	//! Start recording into $logs:el-film/<name>. Returns "ok <dir>" or "error:<reason>".
	static string Start(string name, int fps, int maxSeconds)
	{
		if (s_bOn)
			return Refuse("already_recording", name);
		if (!GetGame() || !GetGame().GetWorld())
			return Refuse("no_world", name);
		if (RplSession.Mode() == RplMode.Dedicated)
			return Refuse("no_renderer", name);
		if (!EL_AgentCommand.IsClassToken(name))
			return Refuse("bad_name", name);
		if (fps < 0 || fps > MAX_FPS)
			return Refuse("bad_fps", name);
		if (maxSeconds <= 0 || maxSeconds > DEFAULT_MAX_SECONDS)
			maxSeconds = DEFAULT_MAX_SECONDS;

		s_sDir = ROOT + "/" + name;
		FileIO.MakeDirectory(ROOT);
		FileIO.MakeDirectory(s_sDir);
		s_iFps = fps;
		s_iMaxMs = maxSeconds * 1000;
		s_iFailed = 0;
		s_aShotMs = {};
		s_Pacer = new LF_FilmPacer();
		s_Pacer.Begin(fps, System.GetTickCount());
		s_bOn = true;
		GetGame().GetCallqueue().CallLater(Tick, 0, true);
		EL_Debug.Info(FEATURE, string.Format("recording %1 at %2 fps (max %3 s)", s_sDir, fps, maxSeconds));
		return "ok " + s_sDir;
	}

	//------------------------------------------------------------------------------------------------
	//! Stop, write index.txt, and return "ok <dir> <summary>". When the recording
	//! already ended (max length), returns that result again.
	static string Stop()
	{
		if (!s_bOn)
			return s_sLast;

		s_bOn = false;
		if (GetGame())
			GetGame().GetCallqueue().Remove(Tick);

		int now = System.GetTickCount();
		string summary = s_Pacer.Summary(now) + string.Format(" failed=%1", s_iFailed);
		if (!WriteIndex(summary))
		{
			s_sLast = "error:index_not_written " + s_sDir;
			return s_sLast;
		}

		EL_Debug.Info(FEATURE, string.Format("stopped %1 %2", s_sDir, summary));
		s_sLast = string.Format("ok %1 %2", s_sDir, summary);
		return s_sLast;
	}

	//------------------------------------------------------------------------------------------------
	static string Status()
	{
		if (!s_bOn)
			return "idle";
		return string.Format("recording %1 %2", s_sDir, s_Pacer.Summary(System.GetTickCount()));
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tick()
	{
		if (!s_bOn || !s_Pacer)
			return;

		int now = System.GetTickCount();
		if (s_Pacer.OnFrame(now))
		{
			int n = s_Pacer.GetShots();
			if (System.MakeScreenshot(string.Format("%1/f%2", s_sDir, n)))
				s_aShotMs.Insert(s_Pacer.ElapsedMs(now));
			else
			{
				s_iFailed++;
				s_aShotMs.Insert(-1);
			}
		}

		if (s_Pacer.ElapsedMs(now) >= s_iMaxMs)
		{
			EL_Debug.Warn(FEATURE, string.Format("max length reached; stopping %1", s_sDir));
			Stop();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! "# fps=12" then "f<n> <ms since start>" per shot (failed shots skipped), then "# <summary>".
	protected static bool WriteIndex(string summary)
	{
		FileHandle handle = FileIO.OpenFile(s_sDir + "/index.txt", FileMode.WRITE);
		if (!handle)
		{
			EL_Debug.Error(FEATURE, string.Format("cannot write %1/index.txt", s_sDir));
			return false;
		}

		handle.WriteLine(string.Format("# fps=%1", s_iFps));
		for (int i = 0; i < s_aShotMs.Count(); i++)
		{
			if (s_aShotMs[i] >= 0)
				handle.WriteLine(string.Format("f%1 %2", i + 1, s_aShotMs[i]));
		}
		handle.WriteLine("# " + summary);
		handle.Close();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static string Refuse(string reason, string name)
	{
		EL_Debug.Error(FEATURE, string.Format("recording %1 refused: %2", name, reason));
		return "error:" + reason;
	}
}
