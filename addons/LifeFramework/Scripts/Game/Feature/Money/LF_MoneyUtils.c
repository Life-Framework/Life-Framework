class LF_MoneyUtils
{
	//! The prefab slot (#964) of the cash item: Configs/PrefabSlots/LF_PrefabSlots.conf, a pack swaps the look.
	static const string SLOT_CASH = "cash";
	//! The ledger reason of a cash change whose caller named none.
	static const string REASON_CASH = "cash";

	//------------------------------------------------------------------------------------------------
	//! The cash item prefab (slot "cash"), or empty when no usable prefab resolves (logged).
	static ResourceName CashPrefab()
	{
		return LF_PrefabSlots.Get(SLOT_CASH);
	}

	//------------------------------------------------------------------------------------------------
	//! Prefabs that still count as cash though they are not the slot's prefab now (#1130): the core
	//! cash when a pack swapped the slot after a server's first save, and every prefab the save
	//! alias table leads to it. Stacks of these are counted and spent first; new cash is always the
	//! slot prefab, so they drain away. The save format holds the prefab and is untouched.
	static array<ResourceName> LegacyCashPrefabs()
	{
		array<ResourceName> legacy = {};
		LF_PrefabSlots.LegacyPrefabs(SLOT_CASH, legacy);
		return legacy;
	}

	//------------------------------------------------------------------------------------------------
	//! Get the total amount of cash the target has across all storages
	//! \return the total cash, or -1 when the target has no resolvable storage
	static int GetCash(InventoryStorageManagerComponent target)
	{
		int total = LF_InventoryUtils.GetAmount(target, CashPrefab());
		if (total < 0)
			return total;

		array<ResourceName> legacy = LegacyCashPrefabs();
		foreach (ResourceName prefab : legacy)
		{
			int amount = LF_InventoryUtils.GetAmount(target, prefab);
			if (amount > 0)
				total += amount;
		}

		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! \ref GetCash(InventoryStorageManagerComponent)
	static int GetCash(IEntity target)
	{
		return GetCash(LF_InventoryUtils.GetResponsibleStorageManager(target));
	}

	//------------------------------------------------------------------------------------------------
	//! Add cash to the target. A player target publishes LF_OnMoneyMoved (the ledger records it).
	//! \param amount Cash to add
	//! \param reason The reason key the ledger records for this change
	//! \param counterpartyUid The other character of a hand-over, empty for a system source
	//! \return the amount of cash actually added, or 0 on faulty operations
	static int AddCash(InventoryStorageManagerComponent target, int amount, string reason = REASON_CASH, string counterpartyUid = "")
	{
		int added = LF_InventoryUtils.AddAmount(target, CashPrefab(), amount);
		if (target)
			PublishMoved(target.GetOwner(), added, true, reason, counterpartyUid);

		return added;
	}

	//------------------------------------------------------------------------------------------------
	//! \ref AddCash(InventoryStorageManagerComponent, int, string, string)
	static int AddCash(IEntity target, int amount, string reason = REASON_CASH, string counterpartyUid = "")
	{
		int added = LF_InventoryUtils.AddAmount(target, CashPrefab(), amount);
		PublishMoved(target, added, true, reason, counterpartyUid);
		return added;
	}

	//------------------------------------------------------------------------------------------------
	//! Remove cash from the target. A player target publishes LF_OnMoneyMoved.
	//! \param amount Cash to remove
	//! \param reason The reason key the ledger records for this change
	//! \param counterpartyUid The other character of a hand-over, empty for a system sink
	//! \return the amount of cash actually removed, or 0 on faulty operations.
	//! A partial removal returns the removed part, never the full request.
	static int RemoveCash(InventoryStorageManagerComponent target, int amount, string reason = REASON_CASH, string counterpartyUid = "")
	{
		int removed = RemoveCounted(target, amount);
		if (target)
			PublishMoved(target.GetOwner(), removed, false, reason, counterpartyUid);

		return removed;
	}

	//------------------------------------------------------------------------------------------------
	//! \ref RemoveCash(InventoryStorageManagerComponent, int, string, string)
	static int RemoveCash(IEntity target, int amount, string reason = REASON_CASH, string counterpartyUid = "")
	{
		int removed = RemoveCounted(LF_InventoryUtils.GetResponsibleStorageManager(target), amount);
		PublishMoved(target, removed, false, reason, counterpartyUid);
		return removed;
	}

	//------------------------------------------------------------------------------------------------
	//! Takes `amount` from the legacy stacks first, then the slot's prefab (the same set GetCash counts).
	//! eturn what was removed in all
	protected static int RemoveCounted(InventoryStorageManagerComponent target, int amount)
	{
		if (!target || amount <= 0)
			return 0;

		int removed;
		array<ResourceName> legacy = LegacyCashPrefabs();
		foreach (ResourceName prefab : legacy)
		{
			if (removed >= amount)
				break;

			removed += LF_InventoryUtils.RemoveAmount(target, prefab, amount - removed);
		}

		if (removed < amount)
			removed += LF_InventoryUtils.RemoveAmount(target, CashPrefab(), amount - removed);

		return removed;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: announces a cash change of a player on the bus. A target that is not a player (a bag,
	//! an AI) has no ledger and publishes nothing.
	protected static void PublishMoved(IEntity owner, int moved, bool credit, string reason, string counterpartyUid)
	{
		if (moved <= 0 || !owner || !Replication.IsServer())
			return;

		string uid = LF_Utils.GetPlayerUID(owner);
		if (uid.IsEmpty())
			return;

		int balance = Math.Max(GetCash(owner), 0);
		LF_EventBus.Publish(LF_OnMoneyMoved.CreateFull(uid, LF_Ledger.ACCOUNT_CASH, moved, credit, reason, balance, counterpartyUid));
	}
	
	//------------------------------------------------------------------------------------------------
	//! Alias for AddCash - Give cash to the target
	//! \param amount Cash to give
	//! \return the amount of cash given or 0 on faulty operations.
	static int GiveCash(InventoryStorageManagerComponent target, int amount, string reason = REASON_CASH, string counterpartyUid = "")
	{
		return AddCash(target, amount, reason, counterpartyUid);
	}

	//------------------------------------------------------------------------------------------------
	//! \ref GiveCash(InventoryStorageManagerComponent, int)
	static int GiveCash(IEntity target, int amount, string reason = REASON_CASH, string counterpartyUid = "")
	{
		return AddCash(target, amount, reason, counterpartyUid);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Alias for RemoveCash - Take cash from the target
	//! \param amount Cash to take
	//! \return the amount of cash taken or 0 on faulty operations.
	static int TakeCash(InventoryStorageManagerComponent target, int amount, string reason = REASON_CASH, string counterpartyUid = "")
	{
		return RemoveCash(target, amount, reason, counterpartyUid);
	}

	//------------------------------------------------------------------------------------------------
	//! \ref TakeCash(InventoryStorageManagerComponent, int)
	static int TakeCash(IEntity target, int amount, string reason = REASON_CASH, string counterpartyUid = "")
	{
		return RemoveCash(target, amount, reason, counterpartyUid);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Backwards-compatible aliases used across the codebase.
	//!
	//! RemoveAmount reports the amount actually removed: 0 means nothing was
	//! removed, a non-zero value below the request means a partial removal.
	//! Callers must compare the return against the requested amount instead of
	//! treating any non-zero value as full success.
	//------------------------------------------------------------------------------------------------
	
	static int RemoveAmount(InventoryStorageManagerComponent target, int amount, string reason = REASON_CASH, string counterpartyUid = "")
	{
		return RemoveCash(target, amount, reason, counterpartyUid);
	}
	
	static int RemoveAmount(IEntity target, int amount, string reason = REASON_CASH, string counterpartyUid = "")
	{
		return RemoveCash(target, amount, reason, counterpartyUid);
	}
	
	static int AddAmount(InventoryStorageManagerComponent target, int amount, string reason = REASON_CASH, string counterpartyUid = "")
	{
		return AddCash(target, amount, reason, counterpartyUid);
	}
	
	static int AddAmount(IEntity target, int amount, string reason = REASON_CASH, string counterpartyUid = "")
	{
		return AddCash(target, amount, reason, counterpartyUid);
	}
	
	static int GetAmount(InventoryStorageManagerComponent target)
	{
		return GetCash(target);
	}
	
	static int GetAmount(IEntity target)
	{
		return GetCash(target);
	}
}
