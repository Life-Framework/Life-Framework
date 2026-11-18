// LF_ResidentMemory.c - how each resident feels about each character (#2859, epic #991 step 6).
// Server side, pure rules: help raises a score, a crime seen lowers it, -100..100, no decay. The
// game reads a word (Tier), never the number (M6 decision 7). A crime one resident saw reaches the
// residents they know, one hop, at the config share. Keyed by resident id and character uid, never
// an EntityID. Not saved here; the payload is #2861.

//------------------------------------------------------------------------------------------------
//! One resident's feeling about one character.
class LF_ResidentMemoryRow
{
	int m_iScore;
	ref map<string, int> m_mLastSeconds = new map<string, int>();
}

//------------------------------------------------------------------------------------------------
class LF_ResidentMemory
{
	static const string FEATURE = "Residents";
	static const int SCORE_MIN = -100;	// lf-fixed: the documented scale
	static const int SCORE_MAX = 100;	// lf-fixed: the documented scale
	static const string TIER_REFUSES = "refuses";
	static const string TIER_WARY = "wary";
	static const string TIER_NEUTRAL = "neutral";
	static const string TIER_FRIENDLY = "friendly";
	static const string TIER_REGULAR = "regular";

	protected static ref map<string, ref LF_ResidentMemoryRow> s_mRows;

	//------------------------------------------------------------------------------------------------
	protected static string KeyOf(string residentId, string uid)
	{
		return residentId + "|" + uid;
	}

	//------------------------------------------------------------------------------------------------
	//! Applies `reason` for `residentId` about `uid` at unix time `nowSeconds`. A known reason inside
	//! the cooldown of the same pair is refused; a reason marked gossip also moves the residents
	//! `residentId` knows (once, no further).
	//! \return True when the direct change was made.
	static bool Apply(string residentId, string uid, string reason, int nowSeconds)
	{
		if (residentId.IsEmpty() || uid.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, "memory: a resident id and a character uid are both needed");
			return false;
		}

		LF_ResidentMemoryConfig config = LF_ResidentMemoryConfig.Get();
		if (!config)
			return false;

		LF_ResidentMemoryReason def = config.FindReason(reason);
		if (!def)
		{
			LF_Debug.Warn(FEATURE, string.Format("memory: unknown reason '%1' refused for %2", reason, residentId));
			return false;
		}

		LF_ResidentMemoryRow row = RowFor(residentId, uid, true);
		int last;
		if (row.m_mLastSeconds.Find(reason, last) && nowSeconds - last < config.m_iCooldownSeconds)
		{
			LF_Debug.Info(FEATURE, string.Format("memory: %1 for %2 inside the cooldown, ignored", reason, residentId));
			return false;
		}

		row.m_mLastSeconds.Set(reason, nowSeconds);
		Shift(row, def.m_iDelta);
		LF_Debug.Info(FEATURE, string.Format("memory: %1 about %2 is now %3 (%4)", residentId, uid, Tier(residentId, uid), reason));

		if (def.m_bGossip)
		{
			array<string> known = {};
			config.GetKnown(residentId, known);
			int share = def.m_iDelta * config.m_iGossipPercent / 100;	// integer division truncates toward zero
			foreach (string other : known)
			{
				if (other == residentId)
					continue;

				Shift(RowFor(other, uid, true), share);
				LF_Debug.Info(FEATURE, string.Format("memory: %1 heard of it, now %2", other, Tier(other, uid)));
			}
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Shift(LF_ResidentMemoryRow row, int delta)
	{
		row.m_iScore = LF_Utils.ClampInt(row.m_iScore + delta, SCORE_MIN, SCORE_MAX);
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_ResidentMemoryRow RowFor(string residentId, string uid, bool create)
	{
		if (!s_mRows)
			s_mRows = new map<string, ref LF_ResidentMemoryRow>();

		string key = KeyOf(residentId, uid);
		LF_ResidentMemoryRow row = s_mRows.Get(key);
		if (!row && create)
		{
			row = new LF_ResidentMemoryRow();
			s_mRows.Set(key, row);
		}

		return row;
	}

	//------------------------------------------------------------------------------------------------
	//! The score for tests and the save; the game reads Tier.
	static int Score(string residentId, string uid)
	{
		LF_ResidentMemoryRow row = RowFor(residentId, uid, false);
		if (!row)
			return 0;

		return row.m_iScore;
	}

	//------------------------------------------------------------------------------------------------
	//! The word for how `residentId` feels about `uid`: refuses, wary, neutral, friendly or regular.
	static string Tier(string residentId, string uid)
	{
		LF_ResidentMemoryConfig config = LF_ResidentMemoryConfig.Get();
		if (!config)
			return TIER_NEUTRAL;

		return TierOf(Score(residentId, uid), config);
	}

	//------------------------------------------------------------------------------------------------
	static string TierOf(int score, notnull LF_ResidentMemoryConfig config)
	{
		if (score < config.m_iRefusesBelow)
			return TIER_REFUSES;

		if (score < config.m_iWaryBelow)
			return TIER_WARY;

		if (score >= config.m_iRegularFrom)
			return TIER_REGULAR;

		if (score >= config.m_iFriendlyFrom)
			return TIER_FRIENDLY;

		return TIER_NEUTRAL;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mRows = null;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_ResidentMemory's statics (LF_StaticReset finds it by reflection).
class LF_ResidentMemory_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_ResidentMemory";
	}

	override void Apply()
	{
		LF_ResidentMemory.ResetStatics();
		LF_ResidentMemoryConfig.ResetStatics();
	}
}
