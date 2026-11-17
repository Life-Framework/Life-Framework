//------------------------------------------------------------------------------------------------
//! A quantity transfer or split changes a stack in place, so neither the stash's
//! HandleAccessValidation nor LF_HideoutStashStorageComponent.CanStoreItem runs. The quantity
//! gate asks this instead: a stack anywhere inside a hideout stash is the stash's, and only a
//! member of the owning organization changes it (#2585).
modded class ScriptedInventoryStorageManagerComponent
{
	//------------------------------------------------------------------------------------------------
	override protected bool LF_AllowsRestrictedStorage(notnull IEntity item)
	{
		if (!super.LF_AllowsRestrictedStorage(item))
			return false;

		IEntity holder = LF_HolderOf(item);
		while (holder)
		{
			LF_HideoutStashStorageManagerComponent stash = LF_Component<LF_HideoutStashStorageManagerComponent>.Find(holder);
			if (stash && stash != this)
			{
				if (stash.AllowsUser(GetOwner(), true))
					return true;

				LF_Debug.Warn("Hideout", string.Format("stash %1 kept %2: a quantity change by a non-member", stash.GetHideoutId(), LF_Utils.GetPrefabName(item)));
				return false;
			}

			holder = LF_HolderOf(holder);
		}

		return true;
	}
}
