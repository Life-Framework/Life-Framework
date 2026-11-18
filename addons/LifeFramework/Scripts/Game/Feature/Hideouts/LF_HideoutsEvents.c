//------------------------------------------------------------------------------------------------
//! Hideout events on the server bus (docs/architecture/events.md). Fields are plain values: hideout
//! and organisation ids, never IEntity or EntityID. Published on the server only, after the state
//! change happened. A new event of this feature is appended to this file, not to a central list.

//! The lease of a site changed (#2834): an organisation took it (`m_bLeased` true) or lost it (false:
//! given up, lapsed idle, captured away, organisation emptied). A capture publishes the loss for the
//! old lessee and the lease for the new one. The org ledger reads it to refresh an open page.
class LF_OnHideoutLeaseChanged : LF_Event
{
	string m_sHideoutId;
	string m_sOrgId;
	bool m_bLeased;

	static LF_OnHideoutLeaseChanged Create(string hideoutId, string orgId, bool leased)
	{
		LF_OnHideoutLeaseChanged e = new LF_OnHideoutLeaseChanged();
		e.m_sHideoutId = hideoutId;
		e.m_sOrgId = orgId;
		e.m_bLeased = leased;
		return e;
	}
}
