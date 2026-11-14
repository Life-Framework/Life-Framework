// LF_ClipStamp.c - the on-screen clip stamp (#1173).
// A small corner stamp (server name, the tail of the player's id, real server time in UTC) so a
// clip can be matched to an LF_IncidentLog row, which stamps System.GetUnixTime on the server.
// Client: the player toggles it with the LF_ClipStamp key (off by default). Server: answers one
// RPC with the real time, the name and the id tail; the client keeps the offset and ticks the
// seconds itself, so nothing is replicated per frame and no RplProp is bumped.

//------------------------------------------------------------------------------------------------
//! Pure rules: time and text. No world, no widgets.
class LF_ClipStamp
{
	static const string FEATURE = "ClipStamp";
	static const int SUFFIX_LENGTH = 6;
	static const string DEFAULT_SERVER = "LF";
	static const int RESYNC_MS = 300000; // lf-fixed: how often the visible stamp asks the server for the time again

	protected static bool s_bShown;
	protected static bool s_bAllowed = true;
	protected static string s_sServer;
	protected static string s_sSuffix;
	//! The server's unix seconds and the client tick (ms) at the moment of its answer. Kept apart:
	//! unix seconds times 1000 does not fit in an int.
	protected static int s_iSyncUnix;
	protected static int s_iSyncTickMs;
	protected static bool s_bSynced;

	//------------------------------------------------------------------------------------------------
	//! Pure: "2026-11-14 21:40:07" from seconds since 1970, UTC.
	static string FormatTime(int unix)
	{
		int days = unix / 86400;
		int secs = unix - days * 86400;
		if (secs < 0)
		{
			secs += 86400;
			days -= 1;
		}

		// Civil date from days since 1970-01-01 (Hinnant's algorithm).
		int z = days + 719468;
		int era = z / 146097;
		if (z < 0)
			era = (z - 146096) / 146097;

		int doe = z - era * 146097;
		int yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
		int year = yoe + era * 400;
		int doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
		int mp = (5 * doy + 2) / 153;
		int day = doy - (153 * mp + 2) / 5 + 1;
		int month = mp + 3;
		if (mp >= 10)
			month = mp - 9;

		if (month <= 2)
			year += 1;

		return string.Format("%1-%2-%3 %4:%5:%6", year, Pad2(month), Pad2(day), Pad2(secs / 3600), Pad2((secs / 60) % 60), Pad2(secs % 60));
	}

	//------------------------------------------------------------------------------------------------
	protected static string Pad2(int value)
	{
		if (value < 10)
			return "0" + value.ToString();

		return value.ToString();
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the last SUFFIX_LENGTH characters of an id, upper case. A short id is shown whole; an
	//! empty one shows dashes, so the stamp never reads blank.
	static string IdTail(string uid)
	{
		if (uid.IsEmpty())
			return "------";

		int length = uid.Length();
		if (length > SUFFIX_LENGTH)
			uid = uid.Substring(length - SUFFIX_LENGTH, SUFFIX_LENGTH);

		uid.ToUpper();
		return uid;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the server name as shown; an empty setting falls back to DEFAULT_SERVER.
	static string ServerName(string configured)
	{
		configured = configured.Trim();
		if (configured.IsEmpty())
			return DEFAULT_SERVER;

		return configured;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: server unix seconds now, from the answer (its unix seconds and the tick it landed on)
	//! and the client tick now.
	static int NowUnix(int syncUnix, int syncTickMs, int tickMs)
	{
		return syncUnix + (tickMs - syncTickMs) / 1000;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the server answered with its clock, name, id tail and whether the owner allows it.
	static void Sync(int serverUnix, string server, string suffix, bool allowed, int tickMs)
	{
		s_iSyncUnix = serverUnix;
		s_iSyncTickMs = tickMs;
		s_sServer = server;
		s_sSuffix = suffix;
		s_bAllowed = allowed;
		s_bSynced = true;
		if (!allowed)
			s_bShown = false;

		LF_Debug.Info(FEATURE, string.Format("clip stamp synced: server %1, id %2, owner allows %3", server, suffix, allowed));
	}

	//------------------------------------------------------------------------------------------------
	static bool IsShown()
	{
		return s_bShown && s_bAllowed;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsAllowed()
	{
		return s_bAllowed;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsSynced()
	{
		return s_bSynced;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the player's toggle. Returns the new state; the server's answer can still switch it
	//! off when the owner disallows the stamp.
	static bool Toggle()
	{
		s_bShown = !s_bShown;
		LF_Debug.Info(FEATURE, string.Format("clip stamp toggled %1", s_bShown));
		return s_bShown;
	}

	//------------------------------------------------------------------------------------------------
	//! The stamp's text for this tick, e.g. "LF-1 / A1B2C3 / 2026-11-14 21:40:07 UTC".
	static string Text(int tickMs)
	{
		return WidgetManager.Translate("#LF-ClipStamp_Line", s_sServer, s_sSuffix, FormatTime(NowUnix(s_iSyncUnix, s_iSyncTickMs, tickMs)));
	}

	//------------------------------------------------------------------------------------------------
	//! Called from LF_StaticReset.ResetAll when a world ends.
	static void ResetStatics()
	{
		s_bShown = false;
		s_bAllowed = true;
		s_sServer = "";
		s_sSuffix = "";
		s_iSyncUnix = 0;
		s_iSyncTickMs = 0;
		s_bSynced = false;
		LF_ClipStampHud.Remove();
	}
}
