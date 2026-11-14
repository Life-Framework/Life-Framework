class LF_OpenTraderAction : SCR_InventoryAction
{
	//! The system id the Trader switch answers to (#1074).
	static const string SYSTEM = "Trader";

	//------------------------------------------------------------------------------------------------
	//! Hidden while the Trader system is off.
	override bool CanBeShownScript(IEntity user)
	{
		return LF_Systems.IsEnabled(SYSTEM);
	}

	override protected void PerformActionInternal(SCR_InventoryStorageManagerComponent manager, IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (LF_Systems.Refuses(SYSTEM, "trader storage"))
			return;

		manager.SetStorageToOpen(pOwnerEntity);
		manager.OpenInventory();
	}
}
