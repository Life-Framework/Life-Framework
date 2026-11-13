modded class SCR_CharacterControllerComponent
{
	//------------------------------------------------------------------------------------------------
	override void OnGadgetStateChanged(IEntity gadget, bool isInHand, bool isOnGround)
	{
		super.OnGadgetStateChanged(gadget, isInHand, isOnGround);
		LF_HandInventoryStorageComponent handStorage = LF_Component<LF_HandInventoryStorageComponent>.Find(GetOwner());
		if (handStorage) handStorage.OnGadgetStateChanged(gadget, isInHand, isOnGround);
	}

	//------------------------------------------------------------------------------------------------
	//! The engine asks to drop a hand item on some movement changes (sprint, stance). A hand-carried
	//! item stays in the hands of a living carrier; only a drop action or a downed carrier lets go.
	override bool ShouldGadgetBeDropped(IEntity gadget)
	{
		if (gadget && LF_Component<LF_HandCarryComponent>.Find(gadget) && !LF_ParcelComponent.CarrierIsDown(GetOwner()))
			return false;

		return super.ShouldGadgetBeDropped(gadget);
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_OnWeaponChanged(BaseWeaponComponent weapon, BaseWeaponComponent prevWeapon)
	{
		LF_HandInventoryStorageComponent handStorage = LF_Component<LF_HandInventoryStorageComponent>.Find(GetOwner());
		if (handStorage) handStorage.OnWeaponChanged(weapon, prevWeapon);
	}

	//------------------------------------------------------------------------------------------------
	override void OnControlledByPlayer(IEntity owner, bool controlled)
	{
		super.OnControlledByPlayer(owner, controlled);

		// We need the eventhandler only locally on the players machine
		PlayerController playerController = GetGame().GetPlayerController();
		if (!controlled || !playerController || owner != playerController.GetControlledEntity()) return;

		EventHandlerManagerComponent eventHandler = LF_Component<EventHandlerManagerComponent>.Find(owner);
		if (eventHandler) eventHandler.RegisterScriptHandler("OnWeaponChanged", this, LF_OnWeaponChanged, false);
	}

	//------------------------------------------------------------------------------------------------
	void ~SCR_CharacterControllerComponent()
	{
		EventHandlerManagerComponent eventHandler = LF_Component<EventHandlerManagerComponent>.Find(GetOwner());
		if (eventHandler) eventHandler.RemoveScriptHandler("OnWeaponChanged", this, LF_OnWeaponChanged, false);
	}
}
