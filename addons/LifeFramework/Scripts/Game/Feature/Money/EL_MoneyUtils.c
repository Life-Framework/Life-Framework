class EL_MoneyUtils
{
	static const ResourceName PREFAB_CASH = "{5439738849229352}Prefabs/Items/Currencies/MoneyStack.et";

	//------------------------------------------------------------------------------------------------
	//! Get the total amount of cash the target has across all storages
	//! \return the total cash or -1 on faulty opertations.
	static int GetCash(InventoryStorageManagerComponent target)
	{
		return EL_InventoryUtils.GetAmount(target, PREFAB_CASH);
	}

	//------------------------------------------------------------------------------------------------
	//! \ref GetCash(InventoryStorageManagerComponent)
	static int GetCash(IEntity target)
	{
		return EL_InventoryUtils.GetAmount(target, PREFAB_CASH);
	}

	//------------------------------------------------------------------------------------------------
	//! Add cash to the target
	//! \param amount Cash to add
	//! \return the amount of cash added or 0 on faulty opertations.
	static int AddCash(InventoryStorageManagerComponent target, int amount)
	{
		return EL_InventoryUtils.AddAmount(target, PREFAB_CASH, amount);
	}

	//------------------------------------------------------------------------------------------------
	//! \ref AddCash(InventoryStorageManagerComponent, int)
	static int AddCash(IEntity target, int amount)
	{
		return EL_InventoryUtils.AddAmount(target, PREFAB_CASH, amount);
	}

	//------------------------------------------------------------------------------------------------
	//! Remove cash from the target
	//! \param amount Cash to remove
	//! \return the amount of cash added or 0 on faulty opertations.
	static int RemoveCash(InventoryStorageManagerComponent target, int amount)
	{
		return EL_InventoryUtils.RemoveAmount(target, PREFAB_CASH, amount);
	}

	//------------------------------------------------------------------------------------------------
	//! \ref RemoveCash(InventoryStorageManagerComponent, int)
	static int RemoveCash(IEntity target, int amount)
	{
		return EL_InventoryUtils.RemoveAmount(target, PREFAB_CASH, amount);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Alias for AddCash - Give cash to the target
	//! \param amount Cash to give
	//! \return the amount of cash given or 0 on faulty operations.
	static int GiveCash(InventoryStorageManagerComponent target, int amount)
	{
		return AddCash(target, amount);
	}

	//------------------------------------------------------------------------------------------------
	//! \ref GiveCash(InventoryStorageManagerComponent, int)
	static int GiveCash(IEntity target, int amount)
	{
		return AddCash(target, amount);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Alias for RemoveCash - Take cash from the target
	//! \param amount Cash to take
	//! \return the amount of cash taken or 0 on faulty operations.
	static int TakeCash(InventoryStorageManagerComponent target, int amount)
	{
		return RemoveCash(target, amount);
	}

	//------------------------------------------------------------------------------------------------
	//! \ref TakeCash(InventoryStorageManagerComponent, int)
	static int TakeCash(IEntity target, int amount)
	{
		return RemoveCash(target, amount);
	}
	
	//------------------------------------------------------------------------------------------------
	// Backwards-compatible aliases used across the codebase
	//------------------------------------------------------------------------------------------------
	
	static int RemoveAmount(InventoryStorageManagerComponent target, int amount)
	{
		return RemoveCash(target, amount);
	}
	
	static int RemoveAmount(IEntity target, int amount)
	{
		return RemoveCash(target, amount);
	}
	
	static int AddAmount(InventoryStorageManagerComponent target, int amount)
	{
		return AddCash(target, amount);
	}
	
	static int AddAmount(IEntity target, int amount)
	{
		return AddCash(target, amount);
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
