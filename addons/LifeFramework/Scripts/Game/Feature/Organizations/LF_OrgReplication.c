//------------------------------------------------------------------------------------------------
//! Server -> every client: each player's organization (id, name, rights bits) and the offer they
//! hold, as plain RplProps on their character. The offer side (interaction menu entry name, the
//! hideout gate prompt, the ledger's first paint) reads them; the server decides from
//! LF_OrganizationManager and never from these copies. Same shape as LF_AccountReplication: written
//! on the server only, bumped only on a change, refreshed on spawn and by the periodic pass, so a
//! late joiner gets the values with the entity snapshot.
//------------------------------------------------------------------------------------------------
class LF_OrgReplication
{
	//------------------------------------------------------------------------------------------------
	//! Server: what this character's view is right now.
	static LF_OrgView ServerView(IEntity character)
	{
		string characterId = LF_Hideout.CharacterIdOf(character);
		string orgId;
		string orgName;
		int rights;
		string offerName;
		int offerFrom;

		LF_Organization org = LF_OrganizationManager.GetByCharacter(characterId);
		if (org)
		{
			orgId = org.GetId();
			orgName = org.GetName();
			rights = LF_OrgRules.RightsOf(org, characterId);
		}

		LF_OrgOffer offer = LF_OrgInvites.Pending(characterId, LF_OrgInvites.Now());
		if (offer)
		{
			LF_Organization offerOrg = LF_OrganizationManager.Get(offer.m_sOrgId);
			if (offerOrg)
			{
				offerName = offerOrg.GetName();
				offerFrom = offer.m_iInviterPlayerId;
			}
		}

		return LF_OrgView.Create(orgId, orgName, rights, offerName, offerFrom);
	}

	//------------------------------------------------------------------------------------------------
	//! Offer-side read. The server (dedicated or listen host) answers from the registry; a client
	//! answers from the replicated copy on the character.
	static LF_OrgView ViewOf(IEntity character)
	{
		if (!character)
			return LF_OrgView.Create("", "", 0, "", 0);

		if (Replication.IsServer())
			return ServerView(character);

		SCR_ChimeraCharacter chimera = SCR_ChimeraCharacter.Cast(character);
		if (!chimera)
			return LF_OrgView.Create("", "", 0, "", 0);

		return chimera.LF_GetOrgView();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: writes the view onto the character (bumps only on a change).
	static void Publish(IEntity character)
	{
		if (!Replication.IsServer())
			return;

		SCR_ChimeraCharacter chimera = SCR_ChimeraCharacter.Cast(character);
		if (chimera)
			chimera.LF_SetOrgView(ServerView(chimera));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: republishes the online player playing characterId, if any.
	static void PublishCharacterId(string characterId)
	{
		IEntity character = FindOnline(characterId);
		if (character)
			Publish(character);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: republishes every online member of an organization (after a name or rank change).
	static void PublishAll()
	{
		if (!Replication.IsServer() || !GetGame() || !GetGame().GetPlayerManager())
			return;

		PlayerManager players = GetGame().GetPlayerManager();
		array<int> ids = {};
		players.GetPlayers(ids);
		foreach (int playerId : ids)
		{
			Publish(players.GetPlayerControlledEntity(playerId));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the controlled entity of the online player whose active character is characterId.
	static IEntity FindOnline(string characterId)
	{
		if (characterId.IsEmpty() || !GetGame() || !GetGame().GetPlayerManager())
			return null;

		PlayerManager players = GetGame().GetPlayerManager();
		array<int> ids = {};
		players.GetPlayers(ids);
		foreach (int playerId : ids)
		{
			IEntity controlled = players.GetPlayerControlledEntity(playerId);
			if (controlled && LF_Hideout.CharacterIdOf(controlled) == characterId)
				return controlled;
		}

		return null;
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_ChimeraCharacter
{
	[RplProp()]
	protected string m_sLF_OrgId;

	[RplProp()]
	protected string m_sLF_OrgName;

	[RplProp()]
	protected int m_iLF_OrgRights;

	[RplProp()]
	protected string m_sLF_OrgOffer;

	[RplProp()]
	protected int m_iLF_OrgOfferFrom;

	//------------------------------------------------------------------------------------------------
	void LF_SetOrgView(LF_OrgView view)
	{
		if (!Replication.IsServer() || !view)
			return;

		if (view.m_sOrgId == m_sLF_OrgId && view.m_sOrgName == m_sLF_OrgName && view.m_iRights == m_iLF_OrgRights
			&& view.m_sOfferOrgName == m_sLF_OrgOffer && view.m_iOfferFromPlayerId == m_iLF_OrgOfferFrom)
			return;

		m_sLF_OrgId = view.m_sOrgId;
		m_sLF_OrgName = view.m_sOrgName;
		m_iLF_OrgRights = view.m_iRights;
		m_sLF_OrgOffer = view.m_sOfferOrgName;
		m_iLF_OrgOfferFrom = view.m_iOfferFromPlayerId;
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	LF_OrgView LF_GetOrgView()
	{
		return LF_OrgView.Create(m_sLF_OrgId, m_sLF_OrgName, m_iLF_OrgRights, m_sLF_OrgOffer, m_iLF_OrgOfferFrom);
	}
}
