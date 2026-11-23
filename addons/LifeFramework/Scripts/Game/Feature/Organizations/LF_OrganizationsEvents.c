//------------------------------------------------------------------------------------------------
//! Organizations events on the server bus (docs/architecture/events.md). Fields are plain values:
//! uids and ints, never IEntity or EntityID. Published on the server only, after the state change
//! happened. A new event of this feature is appended to this file, not to a central list.

//! An organization lost its last member (#2571): the last one left (LF_OrganizationManager.Leave,
//! LeaveDirect) or was kicked. There is no disband verb (#2415), so this is the disband: what an
//! organization holds on to for its members (a hideout lease) ends with it. The organization record
//! itself stays on the register. One event per emptying, never per member.
class LF_OnOrganizationEmptied : LF_Event
{
	string m_sOrgId;
	string m_sReason;

	static LF_OnOrganizationEmptied Create(string orgId, string reason)
	{
		LF_OnOrganizationEmptied e = new LF_OnOrganizationEmptied();
		e.m_sOrgId = orgId;
		e.m_sReason = reason;
		return e;
	}
}

//! A member left an organization (#4221): left on their own (Leave, LeaveDirect) or was kicked.
//! Published before LF_OnOrganizationEmptied when they were the last one. The gang radio draws its
//! band again on it, so the one who left stops hearing the gang.
class LF_OnOrganizationMemberLeft : LF_Event
{
	string m_sOrgId;
	string m_sCharacterId;
	string m_sReason;

	static LF_OnOrganizationMemberLeft Create(string orgId, string characterId, string reason)
	{
		LF_OnOrganizationMemberLeft e = new LF_OnOrganizationMemberLeft();
		e.m_sOrgId = orgId;
		e.m_sCharacterId = characterId;
		e.m_sReason = reason;
		return e;
	}
}
