//------------------------------------------------------------------------------------------------
//! The player interaction menu's own entries on Character_Roleplay.et (context EL_ID). Each one
//! offers on the client through LF_PlayerInteraction.CanOffer and decides on the server.
//! Show ID (EL_ShowIDAction), search (EL_ConfiscateAction), and arrest (LF_JailArrestAction)
//! sit in the same list and use the same reach rule.
//------------------------------------------------------------------------------------------------
class LF_IntroduceAction : ScriptedUserAction
{
	[Attribute(defvalue: "2", desc: "Base-game gesture id played by the one introducing themselves (Configs/Commanding/Gestures; 2 = Gesture_Halt, a raised open hand). 0 plays none.")]
	protected int m_iGestureId;

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return LF_PlayerInteraction.Offers(LF_EPlayerAction.INTRODUCE, false) && LF_PlayerInteraction.CanOffer(user, GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		LF_PlayerInteraction.Introduce(pUserEntity, pOwnerEntity, m_iGestureId);
	}
}

//------------------------------------------------------------------------------------------------
//! Opens the amount sheet on the giver's machine; the sheet sends the amount to the server.
class LF_GiveCashAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return LF_PlayerInteraction.Offers(LF_EPlayerAction.GIVE_CASH, false) && LF_PlayerInteraction.CanOffer(user, GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (EL_MoneyUtils.GetCash(user) > 0)
			return true;

		SetCannotPerformReason("#LF-Interact_NoCash");
		return false;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		LF_PlayerInteraction.OpenGiveCash(pUserEntity, pOwnerEntity);
	}
}

//------------------------------------------------------------------------------------------------
//! Hands over what the giver holds. The giver's machine names the item; the server checks it is
//! really theirs and moves it.
class LF_GiveItemAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return LF_PlayerInteraction.Offers(LF_EPlayerAction.GIVE_ITEM, false) && LF_PlayerInteraction.CanOffer(user, GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	//! Only the giver's own machine knows its hands; elsewhere the server decides after the offer.
	override bool CanBePerformedScript(IEntity user)
	{
		if (user != EL_Utils.GetLocalCharacter())
			return true;

		if (LF_PlayerInteraction.ItemInHand(user))
			return true;

		SetCannotPerformReason("#LF-Interact_NothingInHand");
		return false;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		LF_PlayerInteraction.OfferItemInHand(pUserEntity, pOwnerEntity);
	}
}

//------------------------------------------------------------------------------------------------
//! A slot a later scope fills (fine, org_invite) through LF_PlayerInteraction.RegisterRole.
//! Hidden while no handler is registered for the slot.
class LF_PlayerRoleAction : ScriptedUserAction
{
	[Attribute(defvalue: "", desc: "Role slot name a handler registers for (fine, org_invite)")]
	protected string m_sSlot;

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		LF_PlayerRoleHandler handler = LF_PlayerInteraction.GetRole(m_sSlot);
		if (!handler)
			return false;

		if (!LF_PlayerInteraction.CanOffer(user, GetOwner()))
			return false;

		return handler.CanOffer(user, GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		LF_PlayerRoleHandler handler = LF_PlayerInteraction.GetRole(m_sSlot);
		if (!handler)
			return false;

		outName = handler.GetActionName();
		return !outName.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		LF_PlayerInteraction.PerformRole(m_sSlot, pUserEntity, pOwnerEntity);
	}

	//------------------------------------------------------------------------------------------------
	string GetSlot()
	{
		return m_sSlot;
	}
}
