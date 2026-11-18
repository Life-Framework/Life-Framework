//------------------------------------------------------------------------------------------------
//! Fishing events on the server bus (docs/architecture/events.md). Fields are plain values: uids, ids
//! and ints, never IEntity or EntityID. Published on the server only, after the state change happened.
//! A new event of this feature is appended to this file, not to a central list.

//! One fish was landed and is in the angler's hands (#2995): the strike was inside the window, the bait
//! is spent, the stock is thinned and the FISHER XP is booked. m_sPlayerUid is the character record key
//! (the key the job levels use). m_iSize is 0 small, 1 medium, 2 large. m_sSpotTable is the table of the
//! spot the cast landed in, empty for open water.
class LF_OnFishCaught : LF_Event
{
	string m_sPlayerUid;
	string m_sSpeciesId;
	int m_iSize;
	string m_sFishPrefab;
	string m_sWater;
	string m_sSpotTable;

	static LF_OnFishCaught Create(string playerUid, string speciesId, int size, string fishPrefab, string water, string spotTable)
	{
		LF_OnFishCaught e = new LF_OnFishCaught();
		e.m_sPlayerUid = playerUid;
		e.m_sSpeciesId = speciesId;
		e.m_iSize = size;
		e.m_sFishPrefab = fishPrefab;
		e.m_sWater = water;
		e.m_sSpotTable = spotTable;
		return e;
	}
}
