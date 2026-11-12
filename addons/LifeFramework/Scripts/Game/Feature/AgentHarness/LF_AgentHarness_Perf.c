// LF_AgentHarness_Perf.c - `perf <seconds> [label]`: the client's frame time and memory over a window.
//
// Samples World.GetTimeSlice() once a frame (a per-frame call queue entry, the way the harness's
// HoldInput runs) and reports the frame count, mean, p50, p95, p99 and worst frame in ms, the mean
// fps, and System.MemoryAllocationKB() at the start and the end. Client-side only: what a player's
// frame costs, which is what a render target, a preview or a prop in view adds to. Read the number
// against the same window with the thing out of view; a session's -maxFPS cap hides frames faster
// than the cap (`session up --levers uncapped`).

class LF_HarnessPerf
{
	static const int MAX_SECONDS = 60; // lf-fixed: harness probe limit, not gameplay
	static const int MAX_SAMPLES = 20000; // lf-fixed: harness probe memory bound

	protected static int s_iId;
	protected static string s_sLabel;
	protected static int s_iEndTick;
	protected static int s_iStartKB;
	protected static ref array<float> s_aFrames;
	protected static string s_sResult;
	protected static int s_iResultId;

	//------------------------------------------------------------------------------------------------
	//! Start a window of `seconds`; false when one is running or nothing renders here.
	static bool Start(int id, float seconds, string label, out string error)
	{
		if (s_iId > 0)
		{
			error = "perf_busy";
			return false;
		}

		if (!GetGame() || !GetGame().GetWorld() || !GetGame().GetCallqueue())
		{
			error = "no_world";
			return false;
		}

		if (seconds <= 0 || seconds > MAX_SECONDS)
		{
			error = "bad_seconds";
			return false;
		}

		s_iId = id;
		s_sLabel = label;
		s_aFrames = {};
		s_iStartKB = System.MemoryAllocationKB();
		s_iEndTick = System.GetTickCount() + Math.Round(seconds * 1000);
		GetGame().GetCallqueue().CallLater(Sample, 0, true);
		LF_Debug.Info("Perf", string.Format("client window %1 started: %2 s", Label(), seconds));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static string Label()
	{
		if (s_sLabel.IsEmpty())
			return "-";

		return s_sLabel;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Sample()
	{
		BaseWorld world;
		if (GetGame())
			world = GetGame().GetWorld();

		if (world && s_aFrames && s_aFrames.Count() < MAX_SAMPLES)
			s_aFrames.Insert(world.GetTimeSlice() * 1000);

		if (System.GetTickCount() < s_iEndTick)
			return;

		GetGame().GetCallqueue().Remove(Sample);
		s_sResult = Summary(s_aFrames, s_iStartKB, System.MemoryAllocationKB(), Label());
		s_iResultId = s_iId;
		s_iId = 0;
		s_aFrames = null;
		LF_Debug.Info("Perf", "client window " + s_sResult);
	}

	//------------------------------------------------------------------------------------------------
	//! "label=x frames=N meanMs=.. p50Ms=.. p95Ms=.. p99Ms=.. maxMs=.. fps=.. memKB=start->end (+delta)".
	//! Pure, so a LOGIC test drives it.
	static string Summary(array<float> frames, int startKB, int endKB, string label)
	{
		int count;
		if (frames)
			count = frames.Count();

		if (count == 0)
			return string.Format("label=%1 frames=0 memKB=%2->%3 (%4)", label, startKB, endKB, endKB - startKB);

		array<float> sorted = {};
		sorted.Copy(frames);
		sorted.Sort();
		float total;
		foreach (float ms : sorted)
			total += ms;

		float mean = total / count;
		float fps;
		if (mean > 0)
			fps = 1000 / mean;

		return string.Format("label=%1 frames=%2 meanMs=%3 p50Ms=%4 p95Ms=%5 p99Ms=%6 maxMs=%7 fps=%8 memKB=%9", label, count, Fixed(mean), Fixed(Percentile(sorted, 50)), Fixed(Percentile(sorted, 95)), Fixed(Percentile(sorted, 99)), Fixed(sorted[count - 1]), Fixed(fps), string.Format("%1->%2 (%3)", startKB, endKB, endKB - startKB));
	}

	//------------------------------------------------------------------------------------------------
	//! Nearest-rank percentile of an ascending array.
	static float Percentile(array<float> sorted, int pct)
	{
		if (!sorted || sorted.IsEmpty())
			return 0;

		int rank = Math.Ceil(pct / 100.0 * sorted.Count());
		rank = Math.Clamp(rank, 1, sorted.Count());
		return sorted[rank - 1];
	}

	//------------------------------------------------------------------------------------------------
	protected static string Fixed(float value)
	{
		return (Math.Round(value * 100) / 100).ToString();
	}

	//------------------------------------------------------------------------------------------------
	//! The finished window's result, once; 0 while none is waiting.
	static int TakeResult(out string detail)
	{
		int id = s_iResultId;
		detail = s_sResult;
		s_iResultId = 0;
		s_sResult = string.Empty;
		return id;
	}
}
