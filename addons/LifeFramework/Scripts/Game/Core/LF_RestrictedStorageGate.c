//------------------------------------------------------------------------------------------------
//! The hook a quantity transfer or split asks before it changes a stack in place (#2585). Such a
//! change moves no entity, so a storage's own CanStoreItem and HandleAccessValidation never run.
//! A feature whose storage has its own access rule (hideout stash, trader crate, downed body)
//! overrides LF_AllowsRestrictedStorage in its own file. Declared in Core so it compiles before
//! every feature that overrides it.
modded class ScriptedInventoryStorageManagerComponent
{
	//------------------------------------------------------------------------------------------------
	//! Server: whether this manager's owner may change a stack held (at any depth) by a storage
	//! with its own access rule. Features that own such a storage override it, call super first,
	//! and log their refusal. True here: no rule.
	protected bool LF_AllowsRestrictedStorage(notnull IEntity item)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The entity holding the given one: the owner of the storage it sits in, else its parent.
	//! Null at the top. Walk it to find every container around an item.
	static IEntity LF_HolderOf(IEntity entity)
	{
		if (!entity)
			return null;

		InventoryItemComponent itemComp = LF_Component<InventoryItemComponent>.Find(entity);
		if (itemComp)
		{
			InventoryStorageSlot parentSlot = itemComp.GetParentSlot();
			if (parentSlot)
			{
				BaseInventoryStorageComponent storage = parentSlot.GetStorage();
				if (storage && storage.GetOwner() && storage.GetOwner() != entity)
					return storage.GetOwner();
			}
		}

		return entity.GetParent();
	}
}
