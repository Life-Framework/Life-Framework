//------------------------------------------------------------------------------------------------
//! Where a save's time goes (#2945). The engine spreads a save point over a few server frames, and
//! the LF store write runs inside one of them. The store write times each legacy payload and each
//! section; the manager times the whole engine save (OnBeforeSave to OnAfterSave) and ticks once a
//! frame meanwhile, so the longest frame (the hitch players feel) is measured, not guessed. Two
//! `[LFDebug:Persistence] save timing` lines per save: the store total with its slowest parts, then
//! the engine total, frame count, longest frame and the store's share. A slow save on a live server
//! says what to fix without a profiler (docs/persistence.md "Save timing").
//!
//! System.GetTickCount is whole milliseconds: a payload under 1 ms reads 0.
class LF_SaveTiming
{
	//! Parts at or above this many ms are named in the summary line.
	static const int REPORT_MS = 2;
	//! At most this many parts are named.
	static const int REPORT_TOP = 6;

	protected static ref array<string> s_aNames;
	protected static array<string> LzNames()
	{
		if (!s_aNames)
			s_aNames = new array<string>();
		return s_aNames;
	}
	protected static ref array<int> s_aMs;
	protected static array<int> LzMs()
	{
		if (!s_aMs)
			s_aMs = new array<int>();
		return s_aMs;
	}
	protected static int s_iStoreStart;
	protected static int s_iStoreMs;
	protected static int s_iEngineStart;
	protected static bool s_bEngineRunning;
	protected static int s_iLastTick;
	protected static int s_iLongestFrame;
	protected static int s_iFrames;

	//------------------------------------------------------------------------------------------------
	//! World end: stops a tick left running by a save the world outlived.
	static void ResetStatics()
	{
		if (s_bEngineRunning && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Tick);

		s_bEngineRunning = false;
		LzNames();
		s_aNames.Clear();
		LzMs();
		s_aMs.Clear();
		s_iStoreMs = 0;
		s_iLongestFrame = 0;
		s_iFrames = 0;
	}

	//------------------------------------------------------------------------------------------------
	static void BeginStore()
	{
		LzNames();
		s_aNames.Clear();
		LzMs();
		s_aMs.Clear();
		s_iStoreStart = System.GetTickCount();
	}

	//------------------------------------------------------------------------------------------------
	//! \return A start token for Part.
	static int Now()
	{
		return System.GetTickCount();
	}

	//------------------------------------------------------------------------------------------------
	//! Records one payload or section that started at the token.
	static void Part(string name, int startMs)
	{
		Record(name, System.GetTickCount() - startMs);
	}

	//------------------------------------------------------------------------------------------------
	//! Records one part that took ms.
	static void Record(string name, int ms)
	{
		LzNames();
		s_aNames.Insert(name);
		LzMs();
		s_aMs.Insert(ms);
	}

	//------------------------------------------------------------------------------------------------
	static void EndStore()
	{
		s_iStoreMs = System.GetTickCount() - s_iStoreStart;
		LF_Debug.Info("Persistence", string.Format("save timing: store %1 ms (%2)", s_iStoreMs, Slowest()));
	}

	//------------------------------------------------------------------------------------------------
	//! \return The slowest parts of the last store write, "name=ms" joined by spaces, or "none slow".
	static string Slowest()
	{
		array<int> order = {};
		LzMs();
		for (int i = 0; i < s_aMs.Count(); i++)
		{
			if (LzMs()[i] < REPORT_MS)
				continue;

			int at = 0;
			LzMs();
			while (at < order.Count() && s_aMs[order[at]] >= s_aMs[i])
				at++;

			order.InsertAt(i, at);
		}

		string text;
		int shown = LF_Utils.MinInt(order.Count(), REPORT_TOP);
		for (int j = 0; j < shown; j++)
		{
			if (j > 0)
				text += " ";

			LzNames();
			LzMs();
			text += s_aNames[order[j]] + "=" + s_aMs[order[j]].ToString();
		}

		if (text.IsEmpty())
			return "none slow";

		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! The engine save starts: times it, and ticks every frame until it ends to find the longest
	//! frame, the hitch players feel.
	static void BeginEngine()
	{
		s_iEngineStart = System.GetTickCount();
		StartFrames(s_iEngineStart);
		if (!s_bEngineRunning && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().CallLater(Tick, 0, true);

		s_bEngineRunning = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Once a frame while the engine save runs.
	static void Tick()
	{
		NoteFrame(System.GetTickCount());
	}

	//------------------------------------------------------------------------------------------------
	//! Starts a frame count at nowMs: no frame yet, nothing longest.
	static void StartFrames(int nowMs)
	{
		s_iLastTick = nowMs;
		s_iLongestFrame = 0;
		s_iFrames = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Frames counted since StartFrames.
	static int GetFrames()
	{
		return s_iFrames;
	}

	//------------------------------------------------------------------------------------------------
	//! Records a frame boundary at nowMs: the gap since the last one is a frame.
	static void NoteFrame(int nowMs)
	{
		int frame = nowMs - s_iLastTick;
		if (frame > s_iLongestFrame)
			s_iLongestFrame = frame;

		s_iLastTick = nowMs;
		s_iFrames++;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The longest frame of the last engine save, in ms.
	static int GetLongestFrameMs()
	{
		return s_iLongestFrame;
	}

	//------------------------------------------------------------------------------------------------
	//! Logs the engine save's total, the longest frame inside it and the store's share.
	static void EndEngine(string saveType)
	{
		if (!s_bEngineRunning)
			return;

		s_bEngineRunning = false;
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Tick);

		int now = System.GetTickCount();
		NoteFrame(now);
		int total = now - s_iEngineStart;
		LF_Debug.Info("Persistence", string.Format("save timing: engine save %1 %2 ms over %3 frame(s), longest frame %4 ms, LF store %5 ms", saveType, total, s_iFrames, s_iLongestFrame, s_iStoreMs));
	}
}

//------------------------------------------------------------------------------------------------
class LF_SaveTiming_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_SaveTiming";
	}

	override void Apply()
	{
		LF_SaveTiming.ResetStatics();
	}
}
