//------------------------------------------------------------------------------------------------
//! The `org_promote` and `org_demote` slots of the player interaction menu (#750a). A member with
//! the manage-ranks right faces a member below them and moves them one rung: "Promote in <org>"
//! when the rung above is still below the user, "Demote in <org>" when there is a rung below.
//! Rank is given face to face, like an invite (decision "In-world first").
//! The entry is offered from the replicated copies (LF_OrgReplication); Perform asks
//! LF_OrganizationManager.Promote/Demote on the server, which re-checks the right and the ladder.
//------------------------------------------------------------------------------------------------
class LF_OrgRankRole : LF_PlayerRoleHandler
{
	static const string SLOT_PROMOTE = "org_promote";
	static const string SLOT_DEMOTE = "org_demote";

	protected bool m_bPromote;

	//------------------------------------------------------------------------------------------------
	//! Every machine, once per world (from LF_OrgInviteRole.Register). Idempotent.
	static void Register()
	{
		if (!LF_OrgRankRole.Cast(LF_PlayerInteraction.GetRole(SLOT_PROMOTE)))
			LF_PlayerInteraction.RegisterRole(SLOT_PROMOTE, new LF_OrgRankRole(true));

		if (!LF_OrgRankRole.Cast(LF_PlayerInteraction.GetRole(SLOT_DEMOTE)))
			LF_PlayerInteraction.RegisterRole(SLOT_DEMOTE, new LF_OrgRankRole(false));
	}

	//------------------------------------------------------------------------------------------------
	void LF_OrgRankRole(bool promote)
	{
		m_bPromote = promote;
	}

	//------------------------------------------------------------------------------------------------
	bool IsPromote()
	{
		return m_bPromote;
	}

	//------------------------------------------------------------------------------------------------
	LF_EOrgRankMode ModeOf(IEntity user, IEntity target)
	{
		if (!user || !target || user == target)
			return LF_EOrgRankMode.NONE;

		return LF_OrgRules.RankModeFor(LF_OrgReplication.ViewOf(user), LF_OrgReplication.ViewOf(target), m_bPromote);
	}

	//------------------------------------------------------------------------------------------------
	override string GetActionName()
	{
		if (m_bPromote)
			return "#LF-Org_PromoteGeneric";

		return "#LF-Org_DemoteGeneric";
	}

	//------------------------------------------------------------------------------------------------
	override string GetActionNameFor(IEntity user, IEntity target)
	{
		LF_EOrgRankMode mode = ModeOf(user, target);
		if (mode == LF_EOrgRankMode.NONE)
			return string.Empty;

		LF_OrgView view = LF_OrgReplication.ViewOf(user);
		if (mode == LF_EOrgRankMode.PROMOTE)
			return WidgetManager.Translate("#LF-Org_PromoteIn", view.m_sOrgName);

		return WidgetManager.Translate("#LF-Org_DemoteIn", view.m_sOrgName);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanOffer(IEntity user, IEntity target)
	{
		return ModeOf(user, target) != LF_EOrgRankMode.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! Server only, after LF_PlayerInteraction.PerformRole checked reach and CanOffer.
	override void Perform(IEntity user, IEntity target)
	{
		if (!Replication.IsServer())
			return;

		string userCharacter = LF_Hideout.CharacterIdOf(user);
		string targetCharacter = LF_Hideout.CharacterIdOf(target);
		int userId = LF_PlayerInteraction.PlayerIdOf(user);
		int targetId = LF_PlayerInteraction.PlayerIdOf(target);
		LF_Organization org = LF_OrganizationManager.GetByCharacter(userCharacter);
		bool moved = false;
		if (org && !targetCharacter.IsEmpty())
		{
			if (m_bPromote)
				moved = LF_OrganizationManager.Promote(org.GetId(), userCharacter, targetCharacter);
			else
				moved = LF_OrganizationManager.Demote(org.GetId(), userCharacter, targetCharacter);
		}

		if (!moved)
		{
			LF_Debug.Warn("Organizations", string.Format("%1 refused for player %2 on player %3", SlotName(), userId, targetId));
			LF_NotificationManagerComponent.NotifyPlayer(userId, "#LF-Org_Title", "#LF-Org_RankRefused", 4.0, LF_ENotificationType.WARNING);
			return;
		}

		string rankName = org.GetRankName(targetCharacter);
		LF_PlayerInteraction.PlayGesture(user, LF_PlayerInteraction.GESTURE_POINT);
		LF_OrgReplication.Publish(target);
		LF_NotificationManagerComponent.NotifyPlayer(userId, "#LF-Org_Title", "#LF-Org_RankGiven", 4.0, LF_ENotificationType.INFO, rankName);
		LF_NotificationManagerComponent.NotifyPlayer(targetId, "#LF-Org_Title", "#LF-Org_YourRank", 6.0, LF_ENotificationType.SUCCESS, rankName);
		LF_Debug.Info("Organizations", string.Format("player %1 moved player %2 to %3 in %4", userId, targetId, rankName, org.GetName()));
	}

	//------------------------------------------------------------------------------------------------
	protected string SlotName()
	{
		if (m_bPromote)
			return SLOT_PROMOTE;

		return SLOT_DEMOTE;
	}
}
