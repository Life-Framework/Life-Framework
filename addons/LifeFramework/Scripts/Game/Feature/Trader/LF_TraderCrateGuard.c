//------------------------------------------------------------------------------------------------
//! A trader's crate never keeps an item. The one way goods reach a trader is LF_TraderSale.Sell,
//! which pays and deletes the item without inserting it anywhere. Every other path that would
//! land an item in the crate (an inventory vicinity move, a direct storage insert, a swap, a
//! prefab spawned into the storage) skips the sale, so the server's storage refuses it and the
//! item stays where it was.
//! Server only: a client's own check feeds the inventory UI's drop highlight, and its drag still
//! has to reach SCR_InventoryStorageManagerComponent.InsertItem, which forwards the sale request.
modded class LF_RestrictedInventoryStorageComponent
{
	//------------------------------------------------------------------------------------------------
	override bool CanStoreItem(IEntity item, int slotID)
	{
		if (LF_TraderCrateGuard.Refuses(this, LF_Utils.GetPrefabName(item)))
			return false;

		return super.CanStoreItem(item, slotID);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanStoreResource(ResourceName resourceName, int slotID)
	{
		if (LF_TraderCrateGuard.Refuses(this, resourceName))
			return false;

		return super.CanStoreResource(resourceName, slotID);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanReplaceItem(IEntity nextItem, int slotID)
	{
		if (LF_TraderCrateGuard.Refuses(this, LF_Utils.GetPrefabName(nextItem)))
			return false;

		return super.CanReplaceItem(nextItem, slotID);
	}
}

//------------------------------------------------------------------------------------------------
class LF_TraderCrateGuard
{
	//------------------------------------------------------------------------------------------------
	//! \return true when `storage` belongs to a trader and this machine is the server: the insert
	//! skipped the sale and is refused.
	static bool Refuses(BaseInventoryStorageComponent storage, ResourceName what)
	{
		if (!storage || !Replication.IsServer())
			return false;

		IEntity owner = storage.GetOwner();
		if (!owner || !owner.FindComponent(LF_TraderManagerComponent))
			return false;

		LF_Debug.Warn("Trader", string.Format("crate refused %1: only the server sale takes goods (LF_TraderSale)", what));
		return true;
	}
}
