//------------------------------------------------------------------------------------------------
//! Lumber events on the server bus (docs/architecture/events.md). Fields are plain values: uids, keys
//! and ints, never IEntity or EntityID. Published on the server only, after the state change happened.
//! A new event of this feature is appended to this file, not to a central list.

//! A map tree was felled (#3260). m_sTreeKey is LF_TreeFelling.KeyOf, the stable key the ledger holds.
class LF_OnTreeFelled : LF_Event
{
	string m_sPlayerUid;
	string m_sTreeKey;
	LF_TreeWoodClass m_eWoodClass;
	int m_iSize;
	int m_iLogs;

	static LF_OnTreeFelled Create(string playerUid, string treeKey, LF_TreeWoodClass woodClass, int size, int logs)
	{
		LF_OnTreeFelled e = new LF_OnTreeFelled();
		e.m_sPlayerUid = playerUid;
		e.m_sTreeKey = treeKey;
		e.m_eWoodClass = woodClass;
		e.m_iSize = size;
		e.m_iLogs = logs;
		return e;
	}
}
