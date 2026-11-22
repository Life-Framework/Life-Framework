//------------------------------------------------------------------------------------------------
//! Recruiting from the action list (GA-C5, #4214). Invite, Join and Remove are the `org_invite`
//! entry on the character's action list (LF_OrgInviteRole). This file adds the fourth entry:
//! "Decline <org>", shown to the invitee facing the one who offered, beside Join. It drops the
//! offer on the server and tells both players; the invitee's offer copy clears at once, so Join
//! and Decline both leave the list.
//------------------------------------------------------------------------------------------------
modded class LF_OrgInvites
{
	//------------------------------------------------------------------------------------------------
	//! Server: targetCharacterId turns down the offer inviterCharacterId made. False (and a log
	//! line) when there is no live offer or it came from someone else; the offer is then kept.
	static bool Decline(string targetCharacterId, string inviterCharacterId, float nowMs)
	{
		LF_OrgOffer offer = Pending(targetCharacterId, nowMs);
		if (!offer)
		{
			Refused("decline", LF_EOrgOfferResult.REFUSED_NO_OFFER, inviterCharacterId, targetCharacterId);
			return false;
		}

		if (offer.m_sInviterCharacterId != inviterCharacterId)
		{
			Refused("decline", LF_EOrgOfferResult.REFUSED_WRONG_INVITER, inviterCharacterId, targetCharacterId);
			return false;
		}

		s_mOffers.Remove(targetCharacterId);
		LF_Debug.Info("Organizations", string.Format("declined org=%1 invitee=%2 inviter=%3", offer.m_sOrgId, targetCharacterId, inviterCharacterId));
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The `org_decline` slot: offered exactly when `org_invite` reads Join (same pair rule).
class LF_OrgDeclineRole : LF_PlayerRoleHandler
{
	static const string SLOT = "org_decline";

	//------------------------------------------------------------------------------------------------
	//! Every machine, once per world. Idempotent.
	static void Register()
	{
		if (LF_OrgDeclineRole.Cast(LF_PlayerInteraction.GetRole(SLOT)))
			return;

		LF_PlayerInteraction.RegisterRole(SLOT, new LF_OrgDeclineRole());
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the Decline entry follows the Join entry.
	static bool OffersDecline(LF_EOrgRoleMode inviteMode)
	{
		return inviteMode == LF_EOrgRoleMode.JOIN;
	}

	//------------------------------------------------------------------------------------------------
	override string GetActionName()
	{
		return "#LF-Org_DeclineGeneric";
	}

	//------------------------------------------------------------------------------------------------
	override string GetActionNameFor(IEntity user, IEntity target)
	{
		if (!OffersDecline(LF_OrgInviteRole.ModeOf(user, target)))
			return string.Empty;

		LF_OrgView view = LF_OrgReplication.ViewOf(user);
		if (!view)
			return string.Empty;

		return WidgetManager.Translate("#LF-Org_Decline", view.m_sOfferOrgName);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanOffer(IEntity user, IEntity target)
	{
		return LF_Systems.IsEnabled(LF_OrgRules.SYSTEM) && OffersDecline(LF_OrgInviteRole.ModeOf(user, target));
	}

	//------------------------------------------------------------------------------------------------
	//! Server only, after LF_PlayerInteraction.PerformRole checked reach and CanOffer.
	override void Perform(IEntity user, IEntity target)
	{
		if (!Replication.IsServer())
			return;

		if (LF_Systems.Refuses(LF_OrgRules.SYSTEM, "organization decline"))
			return;

		string userCharacter = LF_Hideout.CharacterIdOf(user);
		string targetCharacter = LF_Hideout.CharacterIdOf(target);
		int userId = LF_PlayerInteraction.PlayerIdOf(user);
		int targetId = LF_PlayerInteraction.PlayerIdOf(target);
		if (userCharacter.IsEmpty() || targetCharacter.IsEmpty())
		{
			LF_Debug.Warn("Organizations", string.Format("org_decline refused: player %1 or %2 has no character", userId, targetId));
			return;
		}

		string orgName;
		LF_Organization org = LF_OrganizationManager.GetByCharacter(targetCharacter);
		if (org)
			orgName = org.GetName();

		bool declined = LF_OrgInvites.Decline(userCharacter, targetCharacter, LF_OrgInvites.Now());
		// Either way the invitee's copy is refreshed, so a stale Join / Decline leaves the list.
		LF_OrgReplication.Publish(user);
		if (!declined)
		{
			LF_NotificationManagerComponent.NotifyPlayer(userId, "#LF-Org_Title", "#LF-Org_JoinRefused", 4.0, LF_ENotificationType.WARNING);
			return;
		}

		LF_PlayerInteraction.PlayGesture(user, LF_PlayerInteraction.GESTURE_HALT);
		LF_NotificationManagerComponent.NotifyPlayer(userId, "#LF-Org_Title", "#LF-Org_Declined", 4.0, LF_ENotificationType.INFO, orgName);
		LF_NotificationManagerComponent.NotifyPlayer(targetId, "#LF-Org_Title", "#LF-Org_OfferDeclined", 5.0, LF_ENotificationType.INFO);
		LF_Debug.Info("Organizations", string.Format("player %1 declined %2 from player %3", userId, orgName, targetId));
	}
}

//------------------------------------------------------------------------------------------------
modded class LF_GameModeRoleplay
{
	//------------------------------------------------------------------------------------------------
	//! The decline slot registers on every machine, beside the other role slots.
	protected override void OnGameStart()
	{
		super.OnGameStart();
		LF_OrgDeclineRole.Register();
	}
}
