//------------------------------------------------------------------------------------------------
//! Server-side membership offers (M1-17). A leader facing someone makes an offer; the invitee
//! joins only by facing that leader and choosing Join. The offer is session state: it is not
//! saved, and it lapses after LF_OrgRules.OFFER_MS. Joining goes through
//! LF_OrganizationManager.Invite, which checks the leader's rank again at that moment.
//------------------------------------------------------------------------------------------------
enum LF_EOrgOfferResult
{
	OFFERED,
	JOINED,
	REFUSED_NO_ORG,
	REFUSED_RANK,
	REFUSED_SELF,
	REFUSED_ALREADY_MEMBER,
	REFUSED_NO_OFFER,
	REFUSED_WRONG_INVITER,
	REFUSED_EXPIRED
}

//------------------------------------------------------------------------------------------------
class LF_OrgOffer : Managed
{
	string m_sOrgId;
	string m_sInviterCharacterId;
	int m_iInviterPlayerId;
	float m_fExpiresMs;
}

//------------------------------------------------------------------------------------------------
class LF_OrgInvites
{
	//! Invitee character id -> the one offer they hold (a newer offer replaces it).
	protected static ref map<string, ref LF_OrgOffer> s_mOffers;

	//------------------------------------------------------------------------------------------------
	static void Reset()
	{
		s_mOffers = null;
	}

	//------------------------------------------------------------------------------------------------
	//! World time in ms on this machine, 0 without a world (a LOGIC test passes its own clock).
	static float Now()
	{
		if (!GetGame() || !GetGame().GetWorld())
			return 0;

		return GetGame().GetWorld().GetWorldTime();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: inviter offers targetCharacterId a place in the inviter's organization.
	static LF_EOrgOfferResult Offer(string inviterCharacterId, int inviterPlayerId, string targetCharacterId, float nowMs)
	{
		LF_Organization org = LF_OrganizationManager.GetByCharacter(inviterCharacterId);
		if (!org)
			return Refused("offer", LF_EOrgOfferResult.REFUSED_NO_ORG, inviterCharacterId, targetCharacterId);

		if (targetCharacterId.IsEmpty() || targetCharacterId == inviterCharacterId)
			return Refused("offer", LF_EOrgOfferResult.REFUSED_SELF, inviterCharacterId, targetCharacterId);

		if (!org.Allows(inviterCharacterId, LF_EOrganizationPermission.INVITE))
			return Refused("offer", LF_EOrgOfferResult.REFUSED_RANK, inviterCharacterId, targetCharacterId);

		if (LF_OrganizationManager.GetByCharacter(targetCharacterId))
			return Refused("offer", LF_EOrgOfferResult.REFUSED_ALREADY_MEMBER, inviterCharacterId, targetCharacterId);

		LF_OrgOffer offer = new LF_OrgOffer();
		offer.m_sOrgId = org.GetId();
		offer.m_sInviterCharacterId = inviterCharacterId;
		offer.m_iInviterPlayerId = inviterPlayerId;
		offer.m_fExpiresMs = nowMs + LF_OrgRules.OFFER_MS;
		if (!s_mOffers)
			s_mOffers = new map<string, ref LF_OrgOffer>();

		s_mOffers.Set(targetCharacterId, offer);
		LF_Debug.Info("Organizations", string.Format("offer org=%1 from=%2 to=%3", org.GetId(), inviterCharacterId, targetCharacterId));
		return LF_EOrgOfferResult.OFFERED;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: targetCharacterId takes the offer inviterCharacterId made. Joins at member rank.
	static LF_EOrgOfferResult Accept(string targetCharacterId, string inviterCharacterId, float nowMs)
	{
		LF_OrgOffer offer = Pending(targetCharacterId, nowMs);
		if (!offer)
			return Refused("join", LF_EOrgOfferResult.REFUSED_NO_OFFER, inviterCharacterId, targetCharacterId);

		if (offer.m_sInviterCharacterId != inviterCharacterId)
			return Refused("join", LF_EOrgOfferResult.REFUSED_WRONG_INVITER, inviterCharacterId, targetCharacterId);

		s_mOffers.Remove(targetCharacterId);
		if (LF_OrganizationManager.GetByCharacter(targetCharacterId))
			return Refused("join", LF_EOrgOfferResult.REFUSED_ALREADY_MEMBER, inviterCharacterId, targetCharacterId);

		if (!LF_OrganizationManager.Invite(offer.m_sOrgId, inviterCharacterId, targetCharacterId))
			return Refused("join", LF_EOrgOfferResult.REFUSED_RANK, inviterCharacterId, targetCharacterId);

		LF_Debug.Info("Organizations", string.Format("joined org=%1 member=%2 invited by %3", offer.m_sOrgId, targetCharacterId, inviterCharacterId));
		return LF_EOrgOfferResult.JOINED;
	}

	//------------------------------------------------------------------------------------------------
	//! The live offer a character holds, or null. A lapsed offer is dropped here.
	static LF_OrgOffer Pending(string targetCharacterId, float nowMs)
	{
		if (!s_mOffers || targetCharacterId.IsEmpty())
			return null;

		LF_OrgOffer offer = s_mOffers.Get(targetCharacterId);
		if (!offer)
			return null;

		if (nowMs > offer.m_fExpiresMs || !LF_OrganizationManager.Get(offer.m_sOrgId))
		{
			s_mOffers.Remove(targetCharacterId);
			LF_Debug.Info("Organizations", string.Format("offer to %1 lapsed", targetCharacterId));
			return null;
		}

		return offer;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_EOrgOfferResult Refused(string what, LF_EOrgOfferResult result, string inviterCharacterId, string targetCharacterId)
	{
		LF_Debug.Info("Organizations", string.Format("%1 refused: %2 (inviter %3, invitee %4)", what, typename.EnumToString(LF_EOrgOfferResult, result), inviterCharacterId, targetCharacterId));
		return result;
	}
}

//------------------------------------------------------------------------------------------------
//! The hideout yard off the registry alley is leased at the ledger. One organization holds it;
//! its members open the gate (LF_Hideout.AllowsUnlock). Taking the lease locks the gate. The lease
//! costs nothing in M1.
enum LF_EOrgLease
{
	FREE,
	OURS,
	TAKEN
}

//------------------------------------------------------------------------------------------------
class LF_OrgHideoutLease
{
	//------------------------------------------------------------------------------------------------
	static LF_EOrgLease StateFor(string orgId, string hideoutId)
	{
		string owner = LF_Hideout.GetOwner(hideoutId);
		if (owner.IsEmpty() || !LF_OrganizationManager.Get(owner))
			return LF_EOrgLease.FREE;

		if (owner == orgId)
			return LF_EOrgLease.OURS;

		return LF_EOrgLease.TAKEN;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a character with the buy right takes the free hideout for their organization.
	static bool Lease(string characterId, string hideoutId)
	{
		LF_Organization org = LF_OrganizationManager.GetByCharacter(characterId);
		if (!org)
		{
			LF_Debug.Info("Organizations", "lease refused: no organization");
			return false;
		}

		if (!org.Allows(characterId, LF_EOrganizationPermission.BUY_UPGRADE))
		{
			LF_Debug.Info("Organizations", string.Format("lease refused: %1 lacks the rank", characterId));
			return false;
		}

		if (StateFor(org.GetId(), hideoutId) != LF_EOrgLease.FREE)
		{
			LF_Debug.Info("Organizations", string.Format("lease refused: %1 is not free", hideoutId));
			return false;
		}

		if (!LF_Hideout.Assign(hideoutId, org.GetId()))
			return false;

		// A new lease starts with the gate shut: only members open it from here on.
		LF_Hideout.LockDoor(characterId, hideoutId);

		LF_Debug.Info("Organizations", string.Format("lease org=%1 hideout=%2", org.GetId(), hideoutId));
		return true;
	}
}
