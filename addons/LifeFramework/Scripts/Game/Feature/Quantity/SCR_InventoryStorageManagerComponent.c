modded class SCR_InventoryStorageManagerComponent
{
	//------------------------------------------------------------------------------------------------
	override protected void OnItemAdded(BaseInventoryStorageComponent storageOwner, IEntity item)
	{
		// Interrupt SCR super call chain if item gets deleted during quantity combine
		if (LF_QuantityComponent.HandleOnItemAdded(this, storageOwner, item)) return;
		super.OnItemAdded(storageOwner, item);
	}
}
