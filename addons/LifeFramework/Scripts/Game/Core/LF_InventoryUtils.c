class LF_InventoryUtils
{
	//------------------------------------------------------------------------------------------------
	//! Inserta un ítem por prefab en el inventario dado. Devuelve true si se insertó correctamente.
	static bool AddItem(notnull InventoryStorageManagerComponent storageManager, ResourceName prefab)
	{
		if (!storageManager || prefab == ResourceName.Empty)
			return false;

		// The helper logs LF_Debug.Error and returns null when the prefab does not resolve.
		vector origin = "0 0 0";
		if (storageManager.GetOwner())
			origin = storageManager.GetOwner().GetOrigin();

		IEntity item = LF_Utils.SpawnEntityPrefab(prefab, origin);
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
		if (!itemComponent)
			return true; // Not an inventory item, so it cannot sit inside a storage.

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
			InventoryStorageManagerComponent manager = LF_Component<InventoryStorageManagerComponent>.Find(item);
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
		if (!itemComponent) return null;

		InventoryStorageSlot slot = itemComponent.GetParentSlot();
		if (!slot) return null;

		BaseInventoryStorageComponent itemParentStorage = slot.GetStorage();
		if (!itemParentStorage) return null;

		InventoryStorageSlot parentSlot = itemParentStorage.GetParentSlot();
		if (!parentSlot || !parentSlot.GetStorage()) return null;

		return parentSlot.GetStorage().GetOwner();
	}

	//------------------------------------------------------------------------------------------------
	static bool DropItem(notnull IEntity character, notnull IEntity item)
	{
		InventoryItemComponent itemComponent = LF_Component<InventoryItemComponent>.Find(item);
		if (!itemComponent) return false;

		InventoryStorageSlot parentSlot = itemComponent.GetParentSlot();
		if (!parentSlot) return false;

		InventoryStorageManagerComponent inventoryManager = LF_Component<InventoryStorageManagerComponent>.Find(character);
		if (!inventoryManager)
		{
			LF_Debug.Error("Inventory", "drop refused: character has no InventoryStorageManagerComponent");
			return false;
		}

		BaseInventoryStorageComponent storage = parentSlot.GetStorage();
		if (!storage || storage.GetOwner() != character) return false;
		return inventoryManager.TryRemoveItemFromStorage(item, storage);
	}

	//------------------------------------------------------------------------------------------------
	//! Items of one prefab, matched by GUID (LF_Utils.SamePrefab), so a name saved or configured
	//! before the resource moved still finds the items spawned after.
	static array<IEntity> FindItemsByPrefab(notnull InventoryStorageManagerComponent storageManager, ResourceName prefab)
	{
		array<IEntity> foundItems();
		LF_SamePrefabPredicate predicate();
		predicate.m_sPrefab = prefab;
		storageManager.FindItems(foundItems, predicate);
		return foundItems;
	}
}

//------------------------------------------------------------------------------------------------
//! SCR_PrefabNamePredicate compares the path text, which a moved resource changes. This one
//! compares the GUID.
class LF_SamePrefabPredicate : InventorySearchPredicate
{
	string m_sPrefab;

	//------------------------------------------------------------------------------------------------
	override protected bool IsMatch(BaseInventoryStorageComponent storage, IEntity item, array<GenericComponent> queriedComponents, array<BaseItemAttributeData> queriedAttributes)
	{
		EntityPrefabData data = item.GetPrefabData();
		if (!data)
			return false;

		return LF_Utils.SamePrefab(data.GetPrefabName(), m_sPrefab);
	}
}
