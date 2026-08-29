class EL_InventoryUtils
{
	//------------------------------------------------------------------------------------------------
	//! Inserta un ítem por prefab en el inventario dado. Devuelve true si se insertó correctamente.
	static bool AddItem(notnull InventoryStorageManagerComponent storageManager, ResourceName prefab)
	{
		if (!storageManager || prefab == ResourceName.Empty)
			return false;

		IEntity item = GetGame().SpawnEntityPrefab(Resource.Load(prefab));
		if (!item)
			return false;

		// Intenta insertar el ítem en el inventario principal
		if (storageManager.TryInsertItem(item))
			return true;

		// Si falla, elimina el ítem para evitar leaks
		SCR_EntityHelper.DeleteEntityAndChildren(item);
		return false;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsStorageHierachyRoot(IEntity item)
	{
		if (!item) return false;
		InventoryItemComponent itemComponent = InventoryItemComponent.Cast(item.FindComponent(InventoryItemComponent));
		return !itemComponent.GetParentSlot();
	}

	//------------------------------------------------------------------------------------------------
	static IEntity GetStorageHierachyRoot(IEntity item)
	{
		while (item)
		{
			InventoryItemComponent itemComponent = InventoryItemComponent.Cast(item.FindComponent(InventoryItemComponent));
			if (!itemComponent || !itemComponent.GetParentSlot()) break;
			item = itemComponent.GetParentSlot().GetStorage().GetOwner();
		}
		return item;
	}

	//------------------------------------------------------------------------------------------------
	static InventoryStorageManagerComponent GetResponsibleStorageManager(IEntity item)
	{
		while (item)
		{
			InventoryStorageManagerComponent manager = EL_Component<InventoryStorageManagerComponent>.Find(item);
			if (manager) return manager;
			InventoryItemComponent itemComponent = InventoryItemComponent.Cast(item.FindComponent(InventoryItemComponent));
			if (!itemComponent || !itemComponent.GetParentSlot()) break;
			item = itemComponent.GetParentSlot().GetStorage().GetOwner();
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Returns the Owner of the storage the item is in
	//! \param item Item Entity to get Owner from
	//! \return Entity Storage Owner
	static IEntity GetStorageOwner(notnull IEntity item)
	{
		InventoryItemComponent itemComponent = InventoryItemComponent.Cast(item.FindComponent(InventoryItemComponent));
		InventoryStorageSlot slot = itemComponent.GetParentSlot();
		if (!slot) return null;

		BaseInventoryStorageComponent itemParentStorage = slot.GetStorage();
		if (!itemParentStorage) return null;

		InventoryStorageSlot parentSlot = itemParentStorage.GetParentSlot();
		if (!parentSlot) return null;

		return parentSlot.GetStorage().GetOwner();
	}

	//------------------------------------------------------------------------------------------------
	static bool DropItem(notnull IEntity character, notnull IEntity item)
	{
		InventoryItemComponent itemComponent = EL_Component<InventoryItemComponent>.Find(item);
		if (!itemComponent) return false;

		InventoryStorageSlot parentSlot = itemComponent.GetParentSlot();
		if (!parentSlot) return false;

		InventoryStorageManagerComponent inventoryManager = EL_Component<InventoryStorageManagerComponent>.Find(character);
		BaseInventoryStorageComponent storage = parentSlot.GetStorage();
		if (!storage || storage.GetOwner() != character) return false;
		return inventoryManager.TryRemoveItemFromStorage(item, storage);
	}

	//------------------------------------------------------------------------------------------------
	static array<IEntity> FindItemsByPrefab(notnull InventoryStorageManagerComponent storageManager, ResourceName prefab)
	{
		array<IEntity> foundItems();
		SCR_PrefabNamePredicate prefabNamePredicate();
		prefabNamePredicate.prefabName = prefab;
		storageManager.FindItems(foundItems, prefabNamePredicate);
		return foundItems;
	}
}
