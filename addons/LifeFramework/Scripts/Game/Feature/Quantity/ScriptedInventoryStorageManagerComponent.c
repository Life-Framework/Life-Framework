modded class ScriptedInventoryStorageManagerComponent
{
	protected ref map<ResourceName, int> m_mELAmountCache;
	protected ref map<ResourceName, ref ScriptInvoker> m_mELItemCountInvokers;
	protected ref set<ResourceName> m_aELPendingItemRecountTypes;

	//------------------------------------------------------------------------------------------------
	//! Event invoker args (ResourceName prefab, int newAmount, int oldAmount)
	ScriptInvoker LF_GetOnAmountChanged(ResourceName prefab)
	{
		if (!prefab) return null;

		if (!m_mELAmountCache) m_mELAmountCache = new map<ResourceName, int>();
		if (!m_mELItemCountInvokers) m_mELItemCountInvokers = new map<ResourceName, ref ScriptInvoker>();

		ScriptInvoker invoker = m_mELItemCountInvokers.Get(prefab);
		if (!invoker)
		{
			invoker = new ScriptInvoker();
			m_mELItemCountInvokers.Set(prefab, invoker)
		}

		return invoker;
	}

	//------------------------------------------------------------------------------------------------
	//! The name a listener subscribed with for this item's prefab, or empty when nobody listens. A
	//! moved resource reports a new path with the same GUID, so the subscription is found by GUID.
	protected ResourceName LF_SubscribedPrefab(ResourceName prefab)
	{
		if (!m_mELItemCountInvokers || prefab.IsEmpty())
			return ResourceName.Empty;

		if (m_mELItemCountInvokers.Contains(prefab))
			return prefab;

		for (int i = 0; i < m_mELItemCountInvokers.Count(); i++)
		{
			ResourceName subscribed = m_mELItemCountInvokers.GetKey(i);
			if (LF_Utils.SamePrefab(subscribed, prefab))
				return subscribed;
		}

		return ResourceName.Empty;
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_RecountItemsDebounced(ResourceName prefab)
	{
		if (!m_aELPendingItemRecountTypes) m_aELPendingItemRecountTypes = new set<ResourceName>();
		m_aELPendingItemRecountTypes.Insert(prefab);

		ScriptCallQueue callQueue = GetGame().GetCallqueue();
		if (callQueue.GetRemainingTime(LF_FlushRecountItems) == -1)
			callQueue.Call(LF_FlushRecountItems);
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_FlushRecountItems()
	{
		foreach (ResourceName prefab : m_aELPendingItemRecountTypes)
		{
			int newTotal = LF_InventoryUtils.GetAmount(this, prefab);
			if (newTotal == -1) continue;

			int previous = m_mELAmountCache.Get(prefab);
			m_mELAmountCache.Set(prefab, newTotal);

			ScriptInvoker invoker = m_mELItemCountInvokers.Get(prefab);
			if (invoker && newTotal != previous) invoker.Invoke(prefab, newTotal, previous);
		}

		m_aELPendingItemRecountTypes = null;
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnItemAdded(BaseInventoryStorageComponent storageOwner, IEntity item)
	{
		super.OnItemAdded(storageOwner, item);

		ResourceName prefab = LF_SubscribedPrefab(LF_Utils.GetPrefabName(item));
		if (!prefab.IsEmpty())
		{
			LF_RecountItemsDebounced(prefab);

			LF_QuantityComponent quantity = LF_Component<LF_QuantityComponent>.Find(item);
			if (quantity) quantity.GetOnQuantityChanged().Insert(LF_HandleOnQuantityChanged);
		}
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnItemRemoved(BaseInventoryStorageComponent storageOwner, IEntity item)
	{
		super.OnItemRemoved(storageOwner, item);

		ResourceName prefab = LF_SubscribedPrefab(LF_Utils.GetPrefabName(item));
		if (!prefab.IsEmpty())
		{
			LF_RecountItemsDebounced(prefab);

			LF_QuantityComponent quantity = LF_Component<LF_QuantityComponent>.Find(item);
			if (quantity) quantity.GetOnQuantityChanged().Remove(LF_HandleOnQuantityChanged);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected event void LF_HandleOnQuantityChanged(LF_QuantityComponent quantityComponent)
	{
		ResourceName prefab = LF_SubscribedPrefab(LF_Utils.GetPrefabName(quantityComponent.GetOwner()));
		if (!prefab.IsEmpty())
			LF_RecountItemsDebounced(prefab);
	}

	//------------------------------------------------------------------------------------------------
	void LF_RequestQuantitySplit(notnull IEntity sourceEntity, int splitSize = -1)
	{
		Rpc(RPC_LF_RequestQuantitySplit, LF_NetworkUtils.GetRplId(sourceEntity), splitSize);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RPC_LF_RequestQuantitySplit(RplId quantitySourceRplId, int splitSize)
	{
		LF_QuantityComponent quantitySource = LF_Component<LF_QuantityComponent>.Find(LF_NetworkUtils.FindEntityByRplId(quantitySourceRplId));
		if (!quantitySource || !LF_CanManipulate(quantitySource.GetOwner()))
		{
			LF_Debug.Warn("Quantity", "Quantity split refused: source not found or out of range");
			return;
		}
		if (!LF_CanManipulateOwned(quantitySource.GetOwner()))
		{
			LF_Debug.Warn("Quantity", "Quantity split refused: source not owned");
			return;
		}

		int sourceQuantity = quantitySource.GetQuantity();
		if (splitSize == -1) splitSize = sourceQuantity / 2;
		if (splitSize < 1 || splitSize >= sourceQuantity)
		{
			LF_Debug.Warn("Quantity", string.Format("Quantity split refused: splitSize %1 out of range for quantity %2", splitSize, sourceQuantity));
			return;
		}

		quantitySource.Split(splitSize);
	}

	//------------------------------------------------------------------------------------------------
	void LF_RequestQuantityTransfer(notnull IEntity sourceEntity, notnull IEntity destinationEntity, int amount = -1)
	{
		Rpc(RPC_LF_QuantityTransfer, LF_NetworkUtils.GetRplId(sourceEntity), LF_NetworkUtils.GetRplId(destinationEntity), amount);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RPC_LF_QuantityTransfer(RplId quantitySourceRplId, RplId quantityDestinationRplId, int amount)
	{
		LF_QuantityComponent quantitySource = LF_Component<LF_QuantityComponent>.Find(LF_NetworkUtils.FindEntityByRplId(quantitySourceRplId));
		if (!quantitySource || !LF_CanManipulate(quantitySource.GetOwner()))
		{
			LF_Debug.Warn("Quantity", "Quantity transfer refused: source not found or out of range");
			return;
		}
		if (!LF_CanManipulateOwned(quantitySource.GetOwner()))
		{
			LF_Debug.Warn("Quantity", "Quantity transfer refused: source not owned");
			return;
		}

		LF_QuantityComponent quantityDestination = LF_Component<LF_QuantityComponent>.Find(LF_NetworkUtils.FindEntityByRplId(quantityDestinationRplId));
		if (!quantityDestination || !LF_CanManipulate(quantityDestination.GetOwner()))
		{
			LF_Debug.Warn("Quantity", "Quantity transfer refused: destination not found or out of range");
			return;
		}
		if (!LF_CanManipulateOwned(quantityDestination.GetOwner()))
		{
			LF_Debug.Warn("Quantity", "Quantity transfer refused: destination not owned");
			return;
		}

		int sourceQuantity = quantitySource.GetQuantity();
		if (amount == -1) amount = sourceQuantity;
		if (amount < 1 || amount > sourceQuantity)
		{
			LF_Debug.Warn("Quantity", string.Format("Quantity transfer refused: amount %1 out of range for quantity %2", amount, sourceQuantity));
			return;
		}

		quantityDestination.Combine(quantitySource, amount);
	}

	//------------------------------------------------------------------------------------------------
	void LF_SetTransferIntent(notnull IEntity sourceEntity, bool keepSeperate)
	{
		Rpc(RPC_LF_SetTransferIntent, LF_NetworkUtils.GetRplId(sourceEntity), keepSeperate);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RPC_LF_SetTransferIntent(RplId sourceRplId, bool keepSeperate)
	{
		IEntity sourceEntity = LF_NetworkUtils.FindEntityByRplId(sourceRplId);
		if (!sourceEntity || !LF_CanManipulate(sourceEntity)) return;
		if (!LF_CanManipulateOwned(sourceEntity))
		{
			LF_Debug.Warn("Quantity", "Transfer intent refused: source not owned");
			return;
		}
		LF_QuantityComponent.SetTransferIntent(sourceEntity, keepSeperate);
	}

	//------------------------------------------------------------------------------------------------
	protected bool LF_CanManipulate(IEntity entity)
	{
		const int maxManipulationRange = 10;
		return (vector.Distance(entity.GetOrigin(), GetOwner().GetOrigin()) < maxManipulationRange);
	}

	//------------------------------------------------------------------------------------------------
	//! Server-side ownership gate. The requesting player may manipulate items in
	//! their own storage or in storages not held by another living player (loot
	//! containers, vehicles, ground items). Rejects another player's carried items.
	protected bool LF_CanManipulateOwned(IEntity entity)
	{
		if (!entity) return false;

		IEntity storageOwner = LF_GetStorageOwner(entity);
		if (storageOwner == GetOwner()) return true;
		if (!storageOwner) return false;

		// A locked vehicle or house storage is closed, same vehicle-wide rule as
		// LF_VehicleStorageAccessControl. Range alone would let anyone within 10 m in.
		if (LF_IsStorageOwnerLocked(storageOwner))
		{
			LF_Debug.Warn("Quantity", string.Format("manipulation refused: storage owner %1 is locked", storageOwner));
			return false;
		}

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager) return false;

		array<int> playerIds();
		if (playerManager.GetPlayers(playerIds) <= 0) return false;
		foreach (int playerId : playerIds)
		{
			if (playerManager.GetPlayerControlledEntity(playerId) == storageOwner)
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! True when the storage owner carries a locked LF_VehicleLockComponent or a locked
	//! LF_LockComponent. Null is not locked.
	static bool LF_IsStorageOwnerLocked(IEntity storageOwner)
	{
		if (!storageOwner)
			return false;

		LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(storageOwner);
		if (vehicleLock && vehicleLock.IsVehicleLocked())
			return true;

		LF_LockComponent houseLock = LF_Component<LF_LockComponent>.Find(storageOwner);
		if (houseLock && houseLock.IsLocked())
			return true;

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Topmost entity owning the storage that holds the given item. Ground items
	//! resolve to themselves.
	protected IEntity LF_GetStorageOwner(IEntity entity)
	{
		InventoryItemComponent itemComp = LF_Component<InventoryItemComponent>.Find(entity);
		if (itemComp)
		{
			InventoryStorageSlot parentSlot = itemComp.GetParentSlot();
			if (parentSlot)
			{
				BaseInventoryStorageComponent storage = parentSlot.GetStorage();
				if (storage && storage.GetOwner())
					entity = storage.GetOwner();
			}
		}

		IEntity parent = entity.GetParent();
		while (parent)
		{
			entity = parent;
			parent = entity.GetParent();
		}

		return entity;
	}
};
