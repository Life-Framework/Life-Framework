modded class EL_InventoryUtils
{
	//------------------------------------------------------------------------------------------------
	//! Returns the total amount or -1 on faulty opertations.
	static int GetAmount(InventoryStorageManagerComponent target, ResourceName prefab)
	{
		if (!target) return -1;

		// No quantity used, so default engine count function
		if (!SCR_BaseContainerTools.FindComponentSource(Resource.Load(prefab), EL_QuantityComponent))
		{
			return target.GetDepositItemCountByResource(target.GetOwner(), prefab) + CarriedAmount(target, prefab, null);
		}

		array<IEntity> foundItems = FindItemsByPrefab(target, prefab);
		int totalAmount;
		foreach (IEntity item : foundItems)
		{
			int amount = 1;
			EL_QuantityComponent quantity = EL_Component<EL_QuantityComponent>.Find(item);
			if (quantity) amount = quantity.GetQuantity();
			totalAmount += amount;
		}

		// FindItems misses a carried stack. The deposit count still sees it.
		if (totalAmount == 0 && target.GetOwner())
			totalAmount = target.GetDepositItemCountByResource(target.GetOwner(), prefab);

		return totalAmount + CarriedAmount(target, prefab, foundItems);
	}

	//------------------------------------------------------------------------------------------------
	//! \ref GetAmount(InventoryStorageManagerComponent, ResourceName)
	static int GetAmount(IEntity target, ResourceName prefab)
	{
		InventoryStorageManagerComponent invManager = EL_InventoryUtils.GetResponsibleStorageManager(target);
		return GetAmount(invManager, prefab);
	}

	//------------------------------------------------------------------------------------------------
	//! Returns actual amount added or 0 on faulty operations.
	static int AddAmount(InventoryStorageManagerComponent storageManager, ResourceName prefab, int amount, bool dropOverflow = false)
	{
		if (!storageManager || amount <= 0) return 0;
		if (!Replication.IsServer()) return 0;

		int remainingAmount = amount;

		bool isQuantityPrefab = SCR_BaseContainerTools.FindComponentSource(Resource.Load(prefab), EL_QuantityComponent) != null;
		if (isQuantityPrefab)
		{
			// Try fill up existing quantity components first
			array<IEntity> prefabItems = FindItemsByPrefab(storageManager, prefab);
			array<EL_QuantityComponent> quantityComponents = EL_QuantityComponent.ExtractQuantityComponents(prefabItems);
			array<EL_QuantityComponent> sortedQuantityComponents = EL_QuantityComponent.SortByQuantity(quantityComponents);
			foreach (EL_QuantityComponent quantityComponent : sortedQuantityComponents)
			{
				int added;
				quantityComponent.AddQuantity(remainingAmount, true, added);
				remainingAmount -= added;
				if (remainingAmount <= 0) return amount;
			}
		}

		while (remainingAmount > 0)
		{
			IEntity item = EL_Utils.SpawnEntityPrefab(prefab, storageManager.GetOwner().GetOrigin());
			if (!item)
			{
				EL_Debug.Error("Quantity", string.Format("add failed: spawn returned nothing for %1", prefab));
				break;
			}

			if (!storageManager.TryInsertItem(item) && !dropOverflow)
			{
				EL_Debug.Error("Quantity", string.Format("add failed: inventory rejected %1", prefab));
				SCR_EntityHelper.DeleteEntityAndChildren(item);
				break;
			}

			if (isQuantityPrefab)
			{
				EL_QuantityComponent quantityComponent = EL_Component<EL_QuantityComponent>.Find(item);
				if (!quantityComponent)
				{
					remainingAmount--;
				}
				else
				{
					// A fresh item already holds its configured init quantity, not 1, so its
					// spawn counts towards the total before anything is added on top.
					int spawnedQuantity = quantityComponent.GetQuantity();
					if (remainingAmount > spawnedQuantity)
					{
						int added;
						quantityComponent.AddQuantity(remainingAmount - spawnedQuantity, true, added);
						remainingAmount -= added + spawnedQuantity;
					}
					else
					{
						quantityComponent.SetQuantity(remainingAmount);
						remainingAmount = 0;
					}
				}
			}
			else
			{
				remainingAmount--;
			}
		}

		return amount - remainingAmount;
	}

	//------------------------------------------------------------------------------------------------
	//! \ref AddAmount(InventoryStorageManagerComponent, ResourceName, int, bool)
	static int AddAmount(IEntity target, ResourceName prefab, int amount, bool dropOverflow = false)
	{
		InventoryStorageManagerComponent invManager = EL_InventoryUtils.GetResponsibleStorageManager(target);
		return AddAmount(invManager, prefab, amount, dropOverflow);
	}

	//------------------------------------------------------------------------------------------------
	//! Returns acutal amount removed or 0 on faulty opertations.
	static int RemoveAmount(InventoryStorageManagerComponent storageManager, ResourceName prefab, int amount)
	{
		if (!storageManager || amount <= 0) return 0;
		if (!Replication.IsServer()) return 0;

		int remainingAmount = amount;

		array<IEntity> prefabItems = FindItemsByPrefab(storageManager, prefab);

		if (SCR_BaseContainerTools.FindComponentSource(Resource.Load(prefab), EL_QuantityComponent))
		{
			// Remove lowest quantity first
			array<EL_QuantityComponent> quantityComponents = EL_QuantityComponent.ExtractQuantityComponents(prefabItems);
			array<EL_QuantityComponent> sortedQuantityComponents = EL_QuantityComponent.SortByQuantity(quantityComponents, false);
			foreach (EL_QuantityComponent quantityComponent : sortedQuantityComponents)
			{
				if (remainingAmount <= 0) return amount;
				if (!quantityComponent) continue;

				int quantityChange;
				quantityComponent.AddQuantity(-remainingAmount, true, quantityChange);
				remainingAmount += quantityChange;
				if (remainingAmount <= 0) return amount;
			}
		}
		else
		{
			foreach (IEntity item : prefabItems)
			{
				SCR_EntityHelper.DeleteEntityAndChildren(item);
				if (--remainingAmount <= 0) return amount;
			}
		}

		if (remainingAmount > 0)
			remainingAmount = ConsumeCarried(storageManager, prefab, remainingAmount);

		return amount - remainingAmount;
	}
	
	//------------------------------------------------------------------------------------------------
	//! \ref RemoveAmount(InventoryStorageManagerComponent, ResourceName, int)
	static int RemoveAmount(IEntity target, ResourceName prefab, int amount)
	{
		InventoryStorageManagerComponent invManager = EL_InventoryUtils.GetResponsibleStorageManager(target);
		return RemoveAmount(invManager, prefab, amount);
	}

	//------------------------------------------------------------------------------------------------
	//! Items in the hands are not always in the storage FindItems walks. A log
	//! the player is carrying still counts as an input for a processor.
	static int CarriedAmount(InventoryStorageManagerComponent storageManager, ResourceName prefab, array<IEntity> alreadyFound)
	{
		if (!storageManager || prefab.IsEmpty())
			return 0;

		IEntity owner = storageManager.GetOwner();
		if (!owner)
			return 0;

		CharacterControllerComponent controller = EL_Component<CharacterControllerComponent>.Find(owner);
		if (!controller)
			return 0;

		int total = AmountOfCarried(controller.GetRightHandItem(), prefab, alreadyFound);
		IEntity left = controller.GetAttachedGadgetAtLeftHandSlot();
		if (left != controller.GetRightHandItem())
			total += AmountOfCarried(left, prefab, alreadyFound);

		EL_HandInventoryStorageComponent hands = EL_Component<EL_HandInventoryStorageComponent>.Find(owner);
		if (hands)
		{
			array<IEntity> stored();
			hands.GetAll(stored);
			foreach (IEntity storedItem : stored)
				total += AmountOfCarried(storedItem, prefab, alreadyFound);
		}

		return total;
	}

	//------------------------------------------------------------------------------------------------
	static int AmountOfCarried(IEntity item, ResourceName prefab, array<IEntity> alreadyFound)
	{
		if (!item || EL_Utils.GetPrefabName(item) != prefab)
			return 0;

		if (alreadyFound)
		{
			foreach (IEntity found : alreadyFound)
			{
				if (found == item)
					return 0;
			}
		}

		EL_QuantityComponent quantity = EL_Component<EL_QuantityComponent>.Find(item);
		if (quantity)
			return quantity.GetQuantity();
		return 1;
	}

	//------------------------------------------------------------------------------------------------
	static int ConsumeCarried(InventoryStorageManagerComponent storageManager, ResourceName prefab, int remainingAmount)
	{
		if (!storageManager || remainingAmount <= 0)
			return remainingAmount;

		IEntity owner = storageManager.GetOwner();
		if (!owner)
			return remainingAmount;

		CharacterControllerComponent controller = EL_Component<CharacterControllerComponent>.Find(owner);
		if (!controller)
			return remainingAmount;

		remainingAmount = ConsumeOneCarried(controller, controller.GetRightHandItem(), prefab, remainingAmount);
		if (remainingAmount <= 0)
			return 0;

		IEntity left = controller.GetAttachedGadgetAtLeftHandSlot();
		if (left != controller.GetRightHandItem())
			remainingAmount = ConsumeOneCarried(controller, left, prefab, remainingAmount);
		if (remainingAmount <= 0)
			return 0;

		EL_HandInventoryStorageComponent hands = EL_Component<EL_HandInventoryStorageComponent>.Find(owner);
		if (!hands)
			return remainingAmount;

		array<IEntity> stored();
		hands.GetAll(stored);
		foreach (IEntity storedItem : stored)
		{
			if (storedItem == controller.GetRightHandItem() || storedItem == left)
				continue;
			remainingAmount = ConsumeOneCarried(controller, storedItem, prefab, remainingAmount);
			if (remainingAmount <= 0)
				return 0;
		}

		return remainingAmount;
	}

	//------------------------------------------------------------------------------------------------
	static int ConsumeOneCarried(CharacterControllerComponent controller, IEntity item, ResourceName prefab, int remainingAmount)
	{
		if (!item || remainingAmount <= 0 || EL_Utils.GetPrefabName(item) != prefab)
			return remainingAmount;

		EL_QuantityComponent quantity = EL_Component<EL_QuantityComponent>.Find(item);
		if (quantity)
		{
			int quantityChange;
			quantity.AddQuantity(-remainingAmount, true, quantityChange);
			remainingAmount += quantityChange;
			return remainingAmount;
		}

		if (controller.GetAttachedGadgetAtLeftHandSlot() == item)
			controller.RemoveGadgetFromHand(true);

		SCR_EntityHelper.DeleteEntityAndChildren(item);
		remainingAmount--;
		return remainingAmount;
	}
}
