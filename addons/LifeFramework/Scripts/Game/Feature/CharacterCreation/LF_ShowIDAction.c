//------------------------------------------------------------------------------------------------
//! "Show ID" on another character: the user holds up the ID card they carry, and the character
//! they face (the action's owner) sees the card (LF_IdCardView on that player's machine). The
//! card is the item in the user's inventory, not an account lookup, so a player without a card
//! has nothing to show. The server reads the card and sends the printed fields to the viewer.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true)]
class LF_ShowIDAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		int showerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(pUserEntity);
		if (!pOwnerEntity || !pUserEntity || pOwnerEntity == pUserEntity)
		{
			LF_Debug.Warn("IdCard", "show refused: no one to show the card to");
			return;
		}

		LF_EInteractionRefusal refusal = LF_PlayerInteraction.CheckPair(pUserEntity, pOwnerEntity);
		if (refusal != LF_EInteractionRefusal.NONE)
		{
			LF_Debug.Info("IdCard", "show refused: " + typename.EnumToString(LF_EInteractionRefusal, refusal));
			LF_NotificationManagerComponent.NotifyPlayer(showerId, "#EL-ID_Information", LF_PlayerInteraction.RefusalKey(refusal), 4.0, LF_ENotificationType.WARNING);
			return;
		}

		LF_IdCardComponent card = LF_IdCard.FindShown(pUserEntity, OwnHolderId(showerId));
		if (!card)
		{
			LF_Debug.Info("IdCard", "show refused: player " + showerId + " carries no ID card");
			LF_NotificationManagerComponent.NotifyPlayer(showerId, "#EL-ID_Information", "#LF-ID_NoCard", 5.0, LF_ENotificationType.WARNING);
			return;
		}

		int viewerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(pOwnerEntity);
		SCR_PlayerController viewer = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(viewerId));
		if (!viewer)
		{
			LF_Debug.Warn("IdCard", "show refused: the character facing player " + showerId + " is not a player");
			return;
		}

		viewer.LF_ShowIdCardView(card.GetFirstName(), card.GetLastName(), card.GetAge(), LF_NetworkUtils.GetRplId(pUserEntity));
		LF_PlayerInteraction.PlayGesture(pUserEntity, LF_PlayerInteraction.GESTURE_POINT);
		LF_PlayerInteraction.PlayHandoverSound(pUserEntity, LF_PlayerInteraction.SOUND_HANDOVER);

		// An ID check: an on-duty officer who reads a wanted player's own card now knows who they
		// are, and the wanted posters name them. Someone else's card identifies nobody.
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		string ownHolderId = OwnHolderId(showerId);
		if (accounts && !ownHolderId.IsEmpty() && card.GetHolderId() == ownHolderId && LF_PoliceUtils.IsOnDutyOfficer(accounts.GetFromCache(viewerId)))
			LF_WantedReports.Identify(LF_Utils.GetPlayerUID(showerId), "ID check");
		LF_NotificationManagerComponent.NotifyPlayer(showerId, "#EL-ID_Information", "#LF-ID_Shown", 4.0);
		LF_Debug.Info("IdCard", string.Format("player %1 showed their ID card to player %2", showerId, viewerId));
	}

	//------------------------------------------------------------------------------------------------
	protected string OwnHolderId(int playerId)
	{
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		if (!accounts)
			return string.Empty;

		LF_PlayerAccount account = accounts.GetFromCache(playerId);
		if (!account || !account.GetActiveCharacter())
			return string.Empty;

		return account.GetActiveCharacter().GetId();
	}

	//------------------------------------------------------------------------------------------------
	//! The user's own client sees its replicated inventory, so a player with no card is told why
	//! instead of offered a dead action.
	override bool CanBePerformedScript(IEntity user)
	{
		if (LF_IdCard.FindShown(user, string.Empty))
			return true;

		SetCannotPerformReason("#LF-ID_NoCard");
		return false;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		// One reach rule for the whole player interaction menu (LF_PlayerInteraction.REACH).
		// Looking at the person is the action context's job; they need not face you back.
		return LF_PlayerInteraction.Offers(LF_EPlayerAction.SHOW_ID, false) && LF_PlayerInteraction.CanOffer(user, GetOwner());
	}
};
