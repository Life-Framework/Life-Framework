//------------------------------------------------------------------------------------------------
//! The `org_invite` slot of the player interaction menu (M1-12 seam, filled by M1-17). One entry,
//! read per pair (LF_OrgRules.ModeFor):
//!   - a leader facing someone with no organization: "Invite to <org>" makes an offer;
//!   - that person facing the leader: "Join <org>" takes it (the invitee's own in-world step);
//!   - a leader facing a member: "Remove from <org>".
//! The entry is offered from the replicated copies (LF_OrgReplication); Perform re-reads the
//! registry on the server, which checks rank, range (LF_PlayerInteraction.PerformRole), and
//! membership again.
//------------------------------------------------------------------------------------------------
class LF_OrgInviteRole : LF_PlayerRoleHandler
{
	static const string SLOT = "org_invite";

	//------------------------------------------------------------------------------------------------
	//! Every machine, once per world (LF_GameModeRoleplay.OnGameStart). Idempotent.
	static void Register()
	{
		// The rank entries (#750a) share this registration so the game mode keeps one call.
		LF_OrgRankRole.Register();
		if (LF_OrgInviteRole.Cast(LF_PlayerInteraction.GetRole(SLOT)))
			return;

		LF_PlayerInteraction.RegisterRole(SLOT, new LF_OrgInviteRole());
	}

	//------------------------------------------------------------------------------------------------
	static LF_EOrgRoleMode ModeOf(IEntity user, IEntity target)
	{
		if (!user || !target || user == target)
			return LF_EOrgRoleMode.NONE;

		return LF_OrgRules.ModeFor(LF_OrgReplication.ViewOf(user), LF_OrgReplication.ViewOf(target), LF_PlayerInteraction.PlayerIdOf(target));
	}

	//------------------------------------------------------------------------------------------------
	override string GetActionName()
	{
		return "#LF-Org_InviteGeneric";
	}

	//------------------------------------------------------------------------------------------------
	override string GetActionNameFor(IEntity user, IEntity target)
	{
		LF_OrgView view = LF_OrgReplication.ViewOf(user);
		LF_EOrgRoleMode mode = ModeOf(user, target);
		if (mode == LF_EOrgRoleMode.INVITE)
			return WidgetManager.Translate("#LF-Org_InviteTo", view.m_sOrgName);

		if (mode == LF_EOrgRoleMode.REMOVE)
			return WidgetManager.Translate("#LF-Org_RemoveFrom", view.m_sOrgName);

		if (mode == LF_EOrgRoleMode.JOIN)
			return WidgetManager.Translate("#LF-Org_Join", view.m_sOfferOrgName);

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanOffer(IEntity user, IEntity target)
	{
		return LF_Systems.IsEnabled(LF_OrgRules.SYSTEM) && ModeOf(user, target) != LF_EOrgRoleMode.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! Server only, after LF_PlayerInteraction.PerformRole checked reach and CanOffer.
	override void Perform(IEntity user, IEntity target)
	{
		if (!Replication.IsServer())
			return;

		if (LF_Systems.Refuses(LF_OrgRules.SYSTEM, "organization role"))
			return;

		string userCharacter = LF_Hideout.CharacterIdOf(user);
		string targetCharacter = LF_Hideout.CharacterIdOf(target);
		int userId = LF_PlayerInteraction.PlayerIdOf(user);
		int targetId = LF_PlayerInteraction.PlayerIdOf(target);
		if (userCharacter.IsEmpty() || targetCharacter.IsEmpty())
		{
			LF_Debug.Warn("Organizations", string.Format("org_invite refused: player %1 or %2 has no character", userId, targetId));
			return;
		}

		LF_EOrgRoleMode mode = ModeOf(user, target);
		if (mode == LF_EOrgRoleMode.INVITE)
			PerformInvite(user, target, userCharacter, targetCharacter, userId, targetId);
		else if (mode == LF_EOrgRoleMode.JOIN)
			PerformJoin(user, target, userCharacter, targetCharacter, userId, targetId);
		else if (mode == LF_EOrgRoleMode.REMOVE)
			PerformRemove(user, userCharacter, targetCharacter, userId, targetId);
		else
			LF_Debug.Info("Organizations", string.Format("org_invite refused: nothing to offer player %1 on player %2", userId, targetId));
	}

	//------------------------------------------------------------------------------------------------
	protected void PerformInvite(IEntity user, IEntity target, string userCharacter, string targetCharacter, int userId, int targetId)
	{
		LF_EOrgOfferResult result = LF_OrgInvites.Offer(userCharacter, userId, targetCharacter, LF_OrgInvites.Now());
		if (result != LF_EOrgOfferResult.OFFERED)
		{
			LF_NotificationManagerComponent.NotifyPlayer(userId, "#LF-Org_Title", "#LF-Org_OfferRefused", 4.0, LF_ENotificationType.WARNING);
			return;
		}

		LF_Organization org = LF_OrganizationManager.GetByCharacter(userCharacter);
		string orgName;
		if (org)
			orgName = org.GetName();

		LF_PlayerInteraction.PlayGesture(user, LF_PlayerInteraction.GESTURE_POINT);
		LF_OrgReplication.Publish(target);
		LF_NotificationManagerComponent.NotifyPlayer(userId, "#LF-Org_Title", "#LF-Org_OfferMade", 4.0, LF_ENotificationType.INFO);
		LF_NotificationManagerComponent.NotifyPlayer(targetId, "#LF-Org_Title", "#LF-Org_OfferReceived", 8.0, LF_ENotificationType.INFO, orgName);
		LF_Debug.Info("Organizations", string.Format("player %1 offered player %2 a place in %3", userId, targetId, orgName));
	}

	//------------------------------------------------------------------------------------------------
	protected void PerformJoin(IEntity user, IEntity target, string userCharacter, string targetCharacter, int userId, int targetId)
	{
		LF_EOrgOfferResult result = LF_OrgInvites.Accept(userCharacter, targetCharacter, LF_OrgInvites.Now());
		LF_OrgReplication.Publish(user);
		if (result != LF_EOrgOfferResult.JOINED)
		{
			LF_NotificationManagerComponent.NotifyPlayer(userId, "#LF-Org_Title", "#LF-Org_JoinRefused", 4.0, LF_ENotificationType.WARNING);
			return;
		}

		LF_Organization org = LF_OrganizationManager.GetByCharacter(userCharacter);
		string orgName;
		if (org)
			orgName = org.GetName();

		// A raised hand to take the place, and the card-in-hand sound at the new member.
		LF_PlayerInteraction.PlayGesture(user, LF_PlayerInteraction.GESTURE_HALT);
		LF_PlayerInteraction.PlayHandoverSound(user, LF_PlayerInteraction.SOUND_HANDOVER);
		LF_OrgReplication.Publish(target);
		LF_NotificationManagerComponent.NotifyPlayer(userId, "#LF-Org_Title", "#LF-Org_Joined", 5.0, LF_ENotificationType.SUCCESS, orgName);
		LF_NotificationManagerComponent.NotifyPlayer(targetId, "#LF-Org_Title", "#LF-Org_NewMember", 5.0, LF_ENotificationType.SUCCESS, orgName);
		LF_Debug.Info("Organizations", string.Format("player %1 joined %2 from player %3", userId, orgName, targetId));
	}

	//------------------------------------------------------------------------------------------------
	protected void PerformRemove(IEntity user, string userCharacter, string targetCharacter, int userId, int targetId)
	{
		LF_Organization org = LF_OrganizationManager.GetByCharacter(userCharacter);
		if (!org || !LF_OrganizationManager.Kick(org.GetId(), userCharacter, targetCharacter))
		{
			LF_NotificationManagerComponent.NotifyPlayer(userId, "#LF-Org_Title", "#LF-Org_RemoveRefused", 4.0, LF_ENotificationType.WARNING);
			return;
		}

		LF_PlayerInteraction.PlayGesture(user, LF_PlayerInteraction.GESTURE_POINT);
		LF_OrgReplication.PublishCharacterId(targetCharacter);
		LF_NotificationManagerComponent.NotifyPlayer(userId, "#LF-Org_Title", "#LF-Org_Removed", 4.0, LF_ENotificationType.INFO);
		LF_NotificationManagerComponent.NotifyPlayer(targetId, "#LF-Org_Title", "#LF-Org_YouWereRemoved", 6.0, LF_ENotificationType.WARNING, org.GetName());
		LF_Debug.Info("Organizations", string.Format("player %1 removed player %2 from %3", userId, targetId, org.GetName()));
	}
}
