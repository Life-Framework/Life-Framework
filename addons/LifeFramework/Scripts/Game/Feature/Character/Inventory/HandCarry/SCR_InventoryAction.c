modded class SCR_InventoryAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
 	{
		CharacterControllerComponent characterController = LF_Component<CharacterControllerComponent>.Find(user);
		if (characterController && LF_Component<LF_HandCarryComponent>.Find(characterController.GetAttachedGadgetAtLeftHandSlot()))
		{
			//SetCannotPerformReason("Hands obstructed"); // TODO localize and uncomment then
			return false;
		}

		return super.CanBePerformedScript(user);
	}
}
