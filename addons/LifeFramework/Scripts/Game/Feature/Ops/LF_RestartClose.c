//------------------------------------------------------------------------------------------------
//! Scheduled-restart close (Ops O8, #1267). Subscribes to LF_OnRestartDue; when the restart time
//! comes it writes MARKER_FILE into the server profile, then asks the engine for a normal close
//! like LF_HostStop, so the SHUTDOWN save runs and the host watchdog (the marker says "restart me",
//! not "stay stopped") boots the server again. Server only; nothing persists here.
//! Register() is called by whoever starts the schedule (LF_RestartWarner, O6T); it is idempotent.
class LF_RestartClose
{
	static const string FEATURE = "Ops";
	//! Read by the host watchdog (docs/roadmap/m4-slices/plan-1006.md, O8 and the watchdog card).
	static const string MARKER_FILE = "$profile:lf-host-restart";

	protected static ref LF_RestartCloseSubscriber s_Subscriber;
	protected static bool s_bClosing;

	// Test seam: record the close instead of closing, and write the marker to a test path.
	protected static bool s_bSeam;
	protected static string s_sSeamMarker;
	protected static int s_iCloseRequests;
	protected static bool s_bMarkerAtClose;

	//------------------------------------------------------------------------------------------------
	//! Server: subscribes the close to LF_OnRestartDue. \return false when already subscribed.
	static bool Register()
	{
		if (!Replication.IsServer())
			return false;

		if (!s_Subscriber)
			s_Subscriber = new LF_RestartCloseSubscriber();

		if (!LF_EventBus.Subscribe(LF_OnRestartDue, s_Subscriber))
			return false;

		// A fresh bus is a fresh world: a close flag from an earlier world in this process is stale.
		s_bClosing = false;

		// A marker left from a crashed run would make a later host stop look like a scheduled restart.
		string marker = MarkerPath();
		if (FileIO.FileExists(marker))
		{
			FileIO.DeleteFile(marker);
			LF_Debug.Warn(FEATURE, string.Format("removed a stale %1 from an earlier run", marker));
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The restart time came: marker first, then the normal close. \return false on a broken marker.
	static bool OnRestartDue()
	{
		if (s_bClosing)
		{
			LF_Debug.Info(FEATURE, "restart due again while closing; ignored");
			return true;
		}

		string marker = MarkerPath();
		FileHandle handle = FileIO.OpenFile(marker, FileMode.WRITE);
		if (!handle)
		{
			// Without the marker the watchdog would leave the server down; staying up is safer.
			LF_Debug.Error(FEATURE, string.Format("cannot write %1; not closing, the server runs on past the scheduled restart", marker));
			return false;
		}
		handle.WriteLine("scheduled restart");
		handle.Close();

		s_bClosing = true;
		LF_Debug.Info(FEATURE, string.Format("restart due: wrote %1, closing the game (engine shutdown save follows)", marker));
		RequestClose();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void RequestClose()
	{
		if (s_bSeam)
		{
			s_iCloseRequests++;
			s_bMarkerAtClose = FileIO.FileExists(s_sSeamMarker);
			return;
		}

		if (GetGame())
			GetGame().RequestClose();
	}

	//------------------------------------------------------------------------------------------------
	protected static string MarkerPath()
	{
		if (s_bSeam)
			return s_sSeamMarker;

		return MARKER_FILE;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: record close requests instead of closing, and write the marker to markerPath.
	static void SetTestSeam(string markerPath)
	{
		s_bSeam = true;
		s_sSeamMarker = markerPath;
	}

	//------------------------------------------------------------------------------------------------
	static int GetCloseRequests()
	{
		return s_iCloseRequests;
	}

	//------------------------------------------------------------------------------------------------
	static bool WasMarkerPresentAtClose()
	{
		return s_bMarkerAtClose;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: drops the seam, the counters and the subscriber (call LF_EventBus.ResetStatics too).
	static void ResetStatics()
	{
		s_Subscriber = null;
		s_bClosing = false;
		s_bSeam = false;
		s_sSeamMarker = "";
		s_iCloseRequests = 0;
		s_bMarkerAtClose = false;
	}
}

//------------------------------------------------------------------------------------------------
class LF_RestartCloseSubscriber : LF_EventSubscriber
{
	override string GetName()
	{
		return "RestartClose";
	}

	override bool Handle(notnull LF_Event busEvent)
	{
		if (!LF_OnRestartDue.Cast(busEvent))
			return true;

		return LF_RestartClose.OnRestartDue();
	}
}
