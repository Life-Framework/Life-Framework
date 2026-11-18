//------------------------------------------------------------------------------------------------
//! Sites events on the server bus (docs/architecture/events.md). Fields are plain values: hideout
//! and organisation ids, never IEntity or EntityID. Published on the server only, after the state
//! change happened. A new event of this feature is appended to this file, not to a central list.

//! A contested site changed hands (#2680): the old lease was released and the capturer leases the
//! hideout. `m_sFromOrgId` is "" when nobody leased it.
class LF_OnSiteCaptured : LF_Event
{
	string m_sHideoutId;
	string m_sFromOrgId;
	string m_sToOrgId;

	static LF_OnSiteCaptured Create(string hideoutId, string fromOrgId, string toOrgId)
	{
		LF_OnSiteCaptured e = new LF_OnSiteCaptured();
		e.m_sHideoutId = hideoutId;
		e.m_sFromOrgId = fromOrgId;
		e.m_sToOrgId = toOrgId;
		return e;
	}
}

//! A site became contested (#2680): two or more organisations stand on it after a sweep in which
//! fewer did. Published once per contest, not every sweep.
class LF_OnSiteContested : LF_Event
{
	string m_sHideoutId;
	string m_sHolderOrgId;
	int m_iOrganisations;

	static LF_OnSiteContested Create(string hideoutId, string holderOrgId, int organisations)
	{
		LF_OnSiteContested e = new LF_OnSiteContested();
		e.m_sHideoutId = hideoutId;
		e.m_sHolderOrgId = holderOrgId;
		e.m_iOrganisations = organisations;
		return e;
	}
}
