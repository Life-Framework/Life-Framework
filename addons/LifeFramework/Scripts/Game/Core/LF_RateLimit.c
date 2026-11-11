//------------------------------------------------------------------------------------------------
//! Server-side per-player request rate limit (#693b). A client RPC that buys, travels, posts to
//! comms, or spawns or moves an entity asks LF_RateLimit before acting. Each player has one bucket
//! per request kind: a fixed window that admits a small burst. A refused request is never silent:
//! the first refusal in a window logs LF_Debug.Warn and (unless the caller replies itself) toasts
//! the player; further refusals in the same window are counted and logged as one line when the
//! window rolls over, so a spamming client cannot flood the server log.
//!
//! The limits are named constants, not config: the #278 config service has not landed, and these
//! are anti-spam floors that sit above anything a person clicking a button can reach.
//------------------------------------------------------------------------------------------------
enum LF_ERequestKind
{
	PURCHASE,	//!< Shop buy and sell, vehicle lot, organization upgrade and founding
	TRAVEL,		//!< Spawn hub travel globe
	COMMS,		//!< Service board and other comms reads or posts
	SPAWN,		//!< Building placement, give item or cash, arrest: spawns or moves an entity
	INVENTORY	//!< Quantity split and transfer from the inventory screen
}

//------------------------------------------------------------------------------------------------
enum LF_ERateVerdict
{
	ALLOW,
	REJECT,			//!< First refusal in this window: log it and tell the player
	REJECT_REPEAT	//!< A further refusal in the same window: counted, logged at rollover
}

//------------------------------------------------------------------------------------------------
class LF_RateBucket
{
	int m_iWindowStartMs;
	int m_iCount;
	int m_iRefused;
}

//------------------------------------------------------------------------------------------------
//! The pure limiter: no clock, no players, no logging. LOGIC-tested by LF_Test_RateLimit.
class LF_RateLimiter
{
	//! Above this many buckets, Take drops the ones whose window has ended.
	static const int PRUNE_ABOVE = 512; // lf-fixed: memory bound on a structural table, not a gameplay knob
	//! A bucket idle this long is dropped by a prune; longer than any window in LF_RateLimit.
	static const int PRUNE_IDLE_MS = 60000; // lf-fixed: must exceed the longest window, not a gameplay knob

	protected ref map<int, ref LF_RateBucket> m_mBuckets = new map<int, ref LF_RateBucket>();

	//------------------------------------------------------------------------------------------------
	static int Key(int playerId, LF_ERequestKind kind)
	{
		return playerId * 8 + kind;
	}

	//------------------------------------------------------------------------------------------------
	//! A clock that went backwards (tick wrap) also ends the window.
	static bool IsWindowOver(int startMs, int nowMs, int windowMs)
	{
		return nowMs < startMs || nowMs - startMs >= windowMs;
	}

	//------------------------------------------------------------------------------------------------
	//! Counts one request against the player's bucket for this kind.
	//! \param nowMs A millisecond clock (System.GetTickCount on the server).
	//! \param windowMs Length of the fixed window.
	//! \param burst Requests admitted per window; at least 1.
	//! \param[out] refusedBefore When this request opens a new window, how many requests the old
	//!        window refused after the first; 0 otherwise. The caller logs it.
	LF_ERateVerdict Take(int playerId, LF_ERequestKind kind, int nowMs, int windowMs, int burst, out int refusedBefore)
	{
		refusedBefore = 0;
		if (burst < 1)
			burst = 1;

		int key = Key(playerId, kind);
		LF_RateBucket bucket = m_mBuckets.Get(key);
		if (!bucket)
		{
			if (m_mBuckets.Count() >= PRUNE_ABOVE)
				Prune(nowMs, PRUNE_IDLE_MS);

			bucket = new LF_RateBucket();
			bucket.m_iWindowStartMs = nowMs;
			m_mBuckets.Insert(key, bucket);
		}
		else if (IsWindowOver(bucket.m_iWindowStartMs, nowMs, windowMs))
		{
			if (bucket.m_iRefused > 1)
				refusedBefore = bucket.m_iRefused - 1;

			bucket.m_iWindowStartMs = nowMs;
			bucket.m_iCount = 0;
			bucket.m_iRefused = 0;
		}

		if (bucket.m_iCount < burst)
		{
			bucket.m_iCount++;
			return LF_ERateVerdict.ALLOW;
		}

		bucket.m_iRefused++;
		if (bucket.m_iRefused == 1)
			return LF_ERateVerdict.REJECT;

		return LF_ERateVerdict.REJECT_REPEAT;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops buckets idle for longer than idleMs (a disconnected player's, mostly).
	void Prune(int nowMs, int idleMs)
	{
		array<int> gone = {};
		foreach (int key, LF_RateBucket bucket : m_mBuckets)
		{
			if (!bucket || IsWindowOver(bucket.m_iWindowStartMs, nowMs, idleMs))
				gone.Insert(key);
		}

		foreach (int key : gone)
		{
			m_mBuckets.Remove(key);
		}
	}

	//------------------------------------------------------------------------------------------------
	int Count()
	{
		return m_mBuckets.Count();
	}
}

//------------------------------------------------------------------------------------------------
//! The server's limiter and the limits per kind.
class LF_RateLimit
{
	static const string FEATURE = "RateLimit";

	// lf-fixed: anti-spam floor; four buys in two seconds is faster than the order slip can be clicked
	static const int PURCHASE_WINDOW_MS = 2000;
	static const int PURCHASE_BURST = 4; // lf-fixed: see PURCHASE_WINDOW_MS
	// lf-fixed: anti-spam floor; a travel takes a loading moment, two per ten seconds is a playtester's ceiling
	static const int TRAVEL_WINDOW_MS = 10000;
	static const int TRAVEL_BURST = 2; // lf-fixed: see TRAVEL_WINDOW_MS
	// lf-fixed: anti-spam floor for comms reads and posts
	static const int COMMS_WINDOW_MS = 5000;
	static const int COMMS_BURST = 3; // lf-fixed: see COMMS_WINDOW_MS
	// lf-fixed: anti-spam floor for requests that spawn or move an entity
	static const int SPAWN_WINDOW_MS = 1000;
	static const int SPAWN_BURST = 4; // lf-fixed: see SPAWN_WINDOW_MS
	// lf-fixed: anti-spam floor; dragging stacks around the inventory stays well under it
	static const int INVENTORY_WINDOW_MS = 1000;
	static const int INVENTORY_BURST = 8; // lf-fixed: see INVENTORY_WINDOW_MS

	protected static ref LF_RateLimiter s_Limiter;

	//------------------------------------------------------------------------------------------------
	static int WindowMs(LF_ERequestKind kind)
	{
		switch (kind)
		{
			case LF_ERequestKind.PURCHASE: return PURCHASE_WINDOW_MS;
			case LF_ERequestKind.TRAVEL: return TRAVEL_WINDOW_MS;
			case LF_ERequestKind.COMMS: return COMMS_WINDOW_MS;
			case LF_ERequestKind.SPAWN: return SPAWN_WINDOW_MS;
			case LF_ERequestKind.INVENTORY: return INVENTORY_WINDOW_MS;
		}

		return SPAWN_WINDOW_MS;
	}

	//------------------------------------------------------------------------------------------------
	static int Burst(LF_ERequestKind kind)
	{
		switch (kind)
		{
			case LF_ERequestKind.PURCHASE: return PURCHASE_BURST;
			case LF_ERequestKind.TRAVEL: return TRAVEL_BURST;
			case LF_ERequestKind.COMMS: return COMMS_BURST;
			case LF_ERequestKind.SPAWN: return SPAWN_BURST;
			case LF_ERequestKind.INVENTORY: return INVENTORY_BURST;
		}

		return SPAWN_BURST;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: counts one request from playerId. A refusal logs a warning; the first refusal in a
	//! window also toasts the player unless notify is false (the caller sends its own reply).
	//! \param request Short request name for the log line, e.g. "shop buy".
	//! \return true when the request may proceed.
	static bool Allow(int playerId, LF_ERequestKind kind, string request, bool notify = true)
	{
		// A client can only RPC on what it owns (its controller or controlled character), so a
		// request with no player is server-local. Counting it would pool every such caller into
		// one bucket and refuse them for each other.
		if (playerId <= 0)
			return true;

		if (!s_Limiter)
			s_Limiter = new LF_RateLimiter();

		int refusedBefore;
		LF_ERateVerdict verdict = s_Limiter.Take(playerId, kind, System.GetTickCount(), WindowMs(kind), Burst(kind), refusedBefore);
		string kindName = typename.EnumToString(LF_ERequestKind, kind);
		if (refusedBefore > 0)
			LF_Debug.Warn(FEATURE, string.Format("player %1 %2: %3 more request(s) refused in the last window", playerId, kindName, refusedBefore));

		if (verdict == LF_ERateVerdict.ALLOW)
			return true;

		if (verdict == LF_ERateVerdict.REJECT)
		{
			LF_Debug.Warn(FEATURE, string.Format("%1 refused: player %2 is over the %3 limit (%4 per %5 ms)", request, playerId, kindName, Burst(kind), WindowMs(kind)));
			if (notify)
				LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-RateLimit_Title", "#LF-RateLimit_Slow", 3.0, LF_ENotificationType.WARNING);
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: Allow for a request that arrived on a character. The player controlling it sent it.
	static bool AllowCharacter(IEntity character, LF_ERequestKind kind, string request, bool notify = true)
	{
		int playerId = 0;
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (playerManager && character)
			playerId = playerManager.GetPlayerIdFromControlledEntity(character);

		return Allow(playerId, kind, request, notify);
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Limiter = null;
	}
}
