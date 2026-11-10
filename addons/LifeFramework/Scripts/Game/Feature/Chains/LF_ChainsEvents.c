//------------------------------------------------------------------------------------------------
//! Chains events on the server bus (docs/architecture/events.md). Fields are plain values: uids and
//! ints, never IEntity or EntityID. Published on the server only, after the state change happened.
//! A new event of this feature is appended to this file, not to a central list.

//! A hands-on chain station changed state (#417): m_sKind is "load" (a worker put a batch in),
//! "ready" (the last unit of work was done) or "collect" (the product was taken out). One event per
//! state change, never per work tick. m_fHeat is the site heat after the change.
class LF_OnChainStep : LF_Event
{
	string m_sWorkerUid;
	string m_sSiteId;
	string m_sChainId;
	string m_sStepId;
	string m_sStationKey;
	string m_sKind;
	float m_fHeat;
	vector m_vPosition;

	static LF_OnChainStep Create(string workerUid, string siteId, string chainId, string stepId, string stationKey, string kind, float heat, vector position)
	{
		LF_OnChainStep e = new LF_OnChainStep();
		e.m_sWorkerUid = workerUid;
		e.m_sSiteId = siteId;
		e.m_sChainId = chainId;
		e.m_sStepId = stepId;
		e.m_sStationKey = stationKey;
		e.m_sKind = kind;
		e.m_fHeat = heat;
		e.m_vPosition = position;
		return e;
	}
}

//! One accepted piece of hands-on work at a chain station (#417), a held action finished by a
//! worker (about one per worker per couple of seconds while they work). m_fHeat is the site heat
//! after the work, m_fRangeM the distance it can be smelled from; the smell and smoke trace (#418)
//! reads these. The site's heat is also readable any time from LF_ChainSites.
class LF_OnChainWork : LF_Event
{
	string m_sWorkerUid;
	string m_sSiteId;
	string m_sChainId;
	string m_sStationKey;
	int m_iPercent;
	float m_fHeat;
	float m_fRangeM;
	vector m_vPosition;

	static LF_OnChainWork Create(string workerUid, string siteId, string chainId, string stationKey, int percent, float heat, float rangeM, vector position)
	{
		LF_OnChainWork e = new LF_OnChainWork();
		e.m_sWorkerUid = workerUid;
		e.m_sSiteId = siteId;
		e.m_sChainId = chainId;
		e.m_sStationKey = stationKey;
		e.m_iPercent = percent;
		e.m_fHeat = heat;
		e.m_fRangeM = rangeM;
		e.m_vPosition = position;
		return e;
	}
}

//! A player stood inside the smell range of a running chain site (#418). Published by
//! LF_ChainTrace at most once per listener and site per cooldown, never per frame. The lead
//! subscriber turns it into a police service call; m_bOfficer is true for an on-duty officer.
class LF_OnChainSmell : LF_Event
{
	string m_sListenerUid;
	bool m_bOfficer;
	string m_sSiteId;
	string m_sChainId;
	float m_fDistance;
	float m_fHeat;
	vector m_vPosition;

	static LF_OnChainSmell Create(string listenerUid, bool officer, string siteId, string chainId, float distance, float heat, vector position)
	{
		LF_OnChainSmell e = new LF_OnChainSmell();
		e.m_sListenerUid = listenerUid;
		e.m_bOfficer = officer;
		e.m_sSiteId = siteId;
		e.m_sChainId = chainId;
		e.m_fDistance = distance;
		e.m_fHeat = heat;
		e.m_vPosition = position;
		return e;
	}
}
