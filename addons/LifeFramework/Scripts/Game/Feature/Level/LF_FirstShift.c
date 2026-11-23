//------------------------------------------------------------------------------------------------
//! First shift of the day (0.3.0 W5, decision N11): the first 30 minutes a character earns XP in a
//! trade each day pay double. The next shift that day pays the normal rate; the next day opens it
//! again, and so does the morning whistle (#4497) through ReopenAll.
//!
//! The 30 minutes start at the first real award, never at clock-in: XP comes from outcomes, not
//! time on duty. A trade has its own window per character, so a second trade gets its own first
//! shift. Server-only session state, keyed by uid and job: nothing is saved, so a restart re-opens
//! every window.
//!
//! The day is the same server-time day the daily XP cap uses (86400 s since boot,
//! LF_JobLevels.DAY_SECONDS). LF_JobLevels.AddXp calls Apply before the cap, so the cap still holds
//! on the doubled amount.
//------------------------------------------------------------------------------------------------
class LF_FirstShift
{
	static const string FEATURE = "FirstShift";

	//! Length of the double-XP window in seconds (N11: 30 minutes).
	static const float WINDOW_SECONDS = 1800;	// lf-fixed: decision N11
	//! XP multiplier inside the window.
	static const int MULTIPLIER = 2;	// lf-fixed: decision N11, "double XP"

	//! uid|job -> world time (s) of the award that opened today's window.
	protected static ref map<string, float> s_mOpenedAt;
	//! uid|job -> game day the window belongs to.
	protected static ref map<string, int> s_mDay;
	//! Off: no window opens and no XP doubles. A test of unrelated XP maths turns it off.
	protected static bool s_bEnabled = true;
	//! On: only a character a connected player plays can earn the bonus.
	protected static bool s_bRequireLive = true;

	//------------------------------------------------------------------------------------------------
	//! World end (LF_StaticReset). Lives in this file; see LF_FirstShift_StaticResetHook.
	static void ResetStatics()
	{
		s_mOpenedAt = null;
		s_mDay = null;
		s_bEnabled = true;
		s_bRequireLive = true;
	}

	//------------------------------------------------------------------------------------------------
	static void SetEnabled(bool enabled)
	{
		s_bEnabled = enabled;
	}

	//------------------------------------------------------------------------------------------------
	//! Test seam: false lets a made-up uid earn a first shift (a test has no connected player).
	static void SetRequireLive(bool required)
	{
		s_bRequireLive = required;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when a connected player plays the character with this record key.
	protected static bool IsLive(string playerUid)
	{
		if (!GetGame())
			return false;

		PlayerManager players = GetGame().GetPlayerManager();
		if (!players)
			return false;

		array<int> ids = {};
		players.GetPlayers(ids);
		foreach (int playerId : ids)
		{
			if (LF_Utils.GetPlayerUID(playerId) == playerUid)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsEnabled()
	{
		return s_bEnabled;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The game day a world time falls on.
	static int DayOf(float now)
	{
		return Math.Floor(now / LF_JobLevels.DAY_SECONDS);
	}

	//------------------------------------------------------------------------------------------------
	protected static string Key(string playerUid, string job)
	{
		return playerUid + "|" + job;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: books one XP award against the first-shift window and returns the amount to grant.
	//! The first award of a game day in a trade opens its 30 minutes and doubles; an award inside the
	//! window doubles; one after it, or on a later shift that day, is returned as given.
	//! \param[in] now World time in seconds (LF_JobGuards.Now()).
	//! \return amount, doubled when the first shift is open.
	static int Apply(string playerUid, string job, int amount, float now)
	{
		if (!s_bEnabled || amount <= 0 || playerUid.IsEmpty() || job.IsEmpty())
			return amount;

		// The bonus is for a player at work: a grant to a character nobody plays (a test uid, a
		// payout after a disconnect) never opens a window.
		if (s_bRequireLive && !IsLive(playerUid))
			return amount;

		if (!s_mOpenedAt)
			s_mOpenedAt = new map<string, float>();
		if (!s_mDay)
			s_mDay = new map<string, int>();

		string key = Key(playerUid, job);
		int day = DayOf(now);
		if (!s_mDay.Contains(key) || s_mDay.Get(key) != day)
		{
			s_mDay.Set(key, day);
			s_mOpenedAt.Set(key, now);
			LF_Debug.Info(FEATURE, string.Format("first shift opened: %1 %2 (double xp for %3 s)", playerUid, job, WINDOW_SECONDS));
			return amount * MULTIPLIER;
		}

		if (now - s_mOpenedAt.Get(key) < WINDOW_SECONDS)
			return amount * MULTIPLIER;

		return amount;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True while a character's first shift in a trade is open at `now`. Does not open one.
	static bool IsOpen(string playerUid, string job, float now)
	{
		if (!s_bEnabled || !s_mDay || !s_mOpenedAt)
			return false;

		string key = Key(playerUid, job);
		if (!s_mDay.Contains(key) || s_mDay.Get(key) != DayOf(now))
			return false;

		return now - s_mOpenedAt.Get(key) < WINDOW_SECONDS;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the morning whistle (#4497) calls this; every trade's first shift opens again.
	static void ReopenAll(string reason)
	{
		s_mOpenedAt = null;
		s_mDay = null;
		LF_Debug.Info(FEATURE, "first shifts reopened: " + reason);
	}

	//------------------------------------------------------------------------------------------------
	//! Drops one character's windows (test cleanup, admin wipe).
	static void Forget(string playerUid)
	{
		if (!s_mDay || !s_mOpenedAt)
			return;

		array<string> drop = {};
		string prefix = playerUid + "|";
		foreach (string key, int day : s_mDay)
		{
			if (key.StartsWith(prefix))
				drop.Insert(key);
		}

		foreach (string gone : drop)
		{
			s_mDay.Remove(gone);
			s_mOpenedAt.Remove(gone);
		}
	}
}

//------------------------------------------------------------------------------------------------
class LF_FirstShift_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_FirstShift";
	}

	override void Apply()
	{
		LF_FirstShift.ResetStatics();
	}
}
