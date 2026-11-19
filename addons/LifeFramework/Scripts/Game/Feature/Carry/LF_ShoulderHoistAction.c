//------------------------------------------------------------------------------------------------
//! Hoist: the F-key action on a ground load with LF_ShoulderCarryComponent (#3263). The vanilla pick-up
//! would put the item wherever the inventory finds room; this one puts it on the shoulder through the
//! hand-carry storage and refuses a second load, a down carrier or an item already held.
class LF_ShoulderHoistAction : SCR_PickUpItemAction
{
	//------------------------------------------------------------------------------------------------
	override protected void PerformActionInternal(SCR_InventoryStorageManagerComponent manager, IEntity pOwnerEntity, IEntity pUserEntity)
	{
		LF_ShoulderCarry.Hoist(pUserEntity, pOwnerEntity);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!user || !GetOwner())
			return false;

		if (LF_ShoulderCarry.Carried(user))
		{
			SetCannotPerformReason("#LF-ShoulderCarry_Denied");
			return false;
		}

		return super.CanBePerformedScript(user);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-ShoulderCarry_Action";
		return true;
	}
}
