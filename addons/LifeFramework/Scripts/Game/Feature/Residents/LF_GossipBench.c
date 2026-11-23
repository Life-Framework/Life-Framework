// LF_GossipBench.c - "Sit and listen" on the gossip bench (#4462, slice of #4170, plan W5.3).
// A player who sits on the bench while a gossip resident is within earshot and stays seated for the
// listen time overhears one rumor: it goes through LF_RumorPool.Tell, so its place is marked heard of
// on their own map. Standing up early or no gossip near gives nothing, and one bench tells one rumor
// per game hour. The bench is an action on the object (the base Sit action), never a menu. The rules
// here are pure; LF_GossipBenchComponent feeds them from the world once a second, server only.

//------------------------------------------------------------------------------------------------
//! The numbers of the bench.
class LF_GossipBenchRules
{
	static const string GIVER = "gossip";		// the rumor giver id (LF_RumorDef.m_aGivers)
	static const string RESIDENT_PREFIX = "gossip_";	// a resident whose id starts so counts as a gossip
	static const int LISTEN_SECONDS = 20;		// lf-fixed: the plan's listen time ("after 20 s")
	static const float RANGE_M = 6;			// lf-fixed: how near a gossip must be to be overheard
	static const int HOUR_MINUTES = 60;		// lf-fixed: one rumor per bench per game hour
	static const int MINUTES_PER_DAY = 1440;	// lf-fixed: game minutes in a day

	//------------------------------------------------------------------------------------------------
	//! True when this resident id is a gossip.
	static bool IsGossipId(string residentId)
	{
		return residentId.Length() >= RESIDENT_PREFIX.Length() && residentId.Substring(0, RESIDENT_PREFIX.Length()) == RESIDENT_PREFIX;
	}
}

//------------------------------------------------------------------------------------------------
//! One bench's listening state: how long each character has sat and listened, and when it last
//! told a rumor. `stamp` is a running game minute (day * 1440 + minute of day), never wrapped.
class LF_GossipBenchState
{
	protected ref map<string, float> m_mListen = new map<string, float>();
	protected int m_iLastHeard = -1000000;	// lf-fixed: "never", far enough back that the hour is always free

	//------------------------------------------------------------------------------------------------
	//! True when the bench's hour is free to tell another rumor at `stamp`.
	bool HourFree(int stamp)
	{
		return stamp - m_iLastHeard >= LF_GossipBenchRules.HOUR_MINUTES;
	}

	//------------------------------------------------------------------------------------------------
	//! Seconds `characterId` has listened so far.
	float ListenedSeconds(string characterId)
	{
		float seconds;
		if (m_mListen.Find(characterId, seconds))
			return seconds;

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! One step of `dt` seconds for one character. Listening counts only while they sit with a gossip
	//! near and the bench's hour is free; standing up or the gossips leaving starts the listen over.
	//! \return True on the step the listen time is reached (the listen starts over at once).
	bool Step(string characterId, bool sitting, bool gossipNear, float dt, int stamp)
	{
		if (characterId.IsEmpty())
			return false;

		if (!sitting || !gossipNear || !HourFree(stamp))
		{
			m_mListen.Remove(characterId);
			return false;
		}

		float seconds = ListenedSeconds(characterId) + dt;
		if (seconds < LF_GossipBenchRules.LISTEN_SECONDS)
		{
			m_mListen.Set(characterId, seconds);
			return false;
		}

		m_mListen.Remove(characterId);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The bench told a rumor at `stamp`: its hour is spent.
	void MarkHeard(int stamp)
	{
		m_iLastHeard = stamp;
		m_mListen.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Drop the listen of every character not in `keep` (someone who left the server).
	void KeepOnly(notnull array<string> keep)
	{
		array<string> drop = {};
		foreach (string characterId, float seconds : m_mListen)
		{
			if (keep.Find(characterId) == -1)
				drop.Insert(characterId);
		}

		foreach (string gone : drop)
			m_mListen.Remove(gone);
	}
}

//------------------------------------------------------------------------------------------------
class LF_GossipBench
{
	static const string FEATURE = "Residents";

	//------------------------------------------------------------------------------------------------
	//! Server: the character overhears one rumor a gossip may tell them: `Offers` for the giver
	//! "gossip", `Pick` by `seed`, `Tell`. Gossip is never a crime rumor (the giver is not a crime
	//! giver), so the tier is neutral.
	//! \return The rumor told, or null when none is on offer or its place is already known.
	static LF_RumorDef Overhear(string characterId, notnull LF_RumorState state, int seed)
	{
		array<ref LF_RumorDef> offers = LF_RumorPool.Offers(characterId, LF_GossipBenchRules.GIVER, LF_ResidentMemory.TIER_NEUTRAL, state);
		LF_RumorDef def = LF_RumorPool.Pick(offers, seed);
		if (!def)
		{
			LF_Debug.Info(FEATURE, string.Format("gossip bench: nothing new for %1 to overhear", characterId));
			return null;
		}

		if (!LF_RumorPool.Tell(characterId, def))
			return null;

		LF_Debug.Info(FEATURE, string.Format("gossip bench: %1 overheard rumor '%2' about %3", characterId, def.m_sId, def.m_sPlaceId));
		return def;
	}
}
