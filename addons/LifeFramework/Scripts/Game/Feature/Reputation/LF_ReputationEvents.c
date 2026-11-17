//------------------------------------------------------------------------------------------------
//! Reputation (player-facing: standing) events on the server bus (docs/architecture/events.md).
//! Fields are plain values: uids, faction ids and ints, never IEntity or EntityID. Published on the
//! server only, after the state change happened. A new event of this feature is appended to this
//! file, not to a central list.

//! A character's tier with one faction changed (LF_Reputation.Add / Award). Points moving inside a
//! tier publish nothing.
class LF_OnReputationChanged : LF_Event
{
	string m_sPlayerUid;
	string m_sFaction;
	int m_iTierBefore;
	int m_iTier;

	static LF_OnReputationChanged Create(string playerUid, string faction, int tierBefore, int tier)
	{
		LF_OnReputationChanged e = new LF_OnReputationChanged();
		e.m_sPlayerUid = playerUid;
		e.m_sFaction = faction;
		e.m_iTierBefore = tierBefore;
		e.m_iTier = tier;
		return e;
	}
}
