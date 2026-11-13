//------------------------------------------------------------------------------------------------
//! "Show ID" on another character: the user holds up the ID card they carry, and the character
//! they face (the action's owner) sees the card (LF_IdCardView on that player's machine). The
//! card is the item in the user's inventory, not an account lookup, so a player without a card
//! has nothing to show. The server reads the card and sends the printed fields and the photo to
//! the viewer (LF_IdCheck.ShowTo, #902); the viewer's screen shows it for a few seconds.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true)]
class LF_ShowIDAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		LF_IdCheck.ShowTo(pUserEntity, pOwnerEntity);
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
