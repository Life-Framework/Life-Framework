modded class SCR_InventoryStorageManagerComponent
{
	//------------------------------------------------------------------------------------------------
	override void InsertItem(IEntity pItem, BaseInventoryStorageComponent pStorageTo = null, BaseInventoryStorageComponent pStorageFrom = null, SCR_InvCallBack cb = null)
	{
		// Stop pickup animations on hand carry items
		if (m_CharacterController && LF_Component<LF_HandCarryComponent>.Find(pItem))
		{
			m_CharacterController.TryPlayItemGesture(EItemGesture.EItemGestureNone);
		}

		super.InsertItem(pItem, pStorageTo, pStorageFrom, cb);
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnItemAdded(BaseInventoryStorageComponent storageOwner, IEntity item)
	{
		super.OnItemAdded(storageOwner, item);
		LF_RefreshHandCarryMenu(item);
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnItemRemoved(BaseInventoryStorageComponent storageOwner, IEntity item)
	{
		super.OnItemRemoved(storageOwner, item);
		LF_RefreshHandCarryMenu(item);
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_RefreshHandCarryMenu(IEntity item)
	{
		if (!LF_Component<LF_HandCarryComponent>.Find(item)) return;

		SCR_InventoryMenuUI inventoryMenu = SCR_InventoryMenuUI.LF_GetCurrentInstance();
		if (!inventoryMenu) return;

		SCR_InventoryStoragesListUI storageList = SCR_InventoryStoragesListUI.Cast(inventoryMenu.GetStorageList());
		if (storageList) storageList.LF_UpdateHandSlot();
	}
}
