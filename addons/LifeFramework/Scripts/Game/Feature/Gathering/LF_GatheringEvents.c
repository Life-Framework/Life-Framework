//------------------------------------------------------------------------------------------------
//! Gathering events on the server bus (docs/architecture/events.md). Fields are plain values: uids and
//! ints, never IEntity or EntityID. Published on the server only, after the state change happened.
//! A new event of this feature is appended to this file, not to a central list.

//! One gather finished and its items were granted (#2706). m_sJobFamily is the node's family
//! ("MINING", "DIGGING", "FORAGING" ...), m_fToolSpeed the factor of the tool that did the work.
class LF_OnGathered : LF_Event
{
	string m_sPlayerUid;
	string m_sNodeId;
	string m_sJobFamily;
	string m_sItemPrefab;
	int m_iAmount;
	float m_fToolSpeed;

	static LF_OnGathered Create(string playerUid, string nodeId, string jobFamily, string itemPrefab, int amount, float toolSpeed)
	{
		LF_OnGathered e = new LF_OnGathered();
		e.m_sPlayerUid = playerUid;
		e.m_sNodeId = nodeId;
		e.m_sJobFamily = jobFamily;
		e.m_sItemPrefab = itemPrefab;
		e.m_iAmount = amount;
		e.m_fToolSpeed = toolSpeed;
		return e;
	}
}
