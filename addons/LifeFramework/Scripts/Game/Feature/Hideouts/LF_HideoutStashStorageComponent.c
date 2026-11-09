//------------------------------------------------------------------------------------------------
//! The stash's storage. A normal universal storage, except that the server refuses an item a
//! non-member carries. An item with no carrier (a restored item the server spawns) is accepted.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Hideouts", description: "Storage of a hideout stash. Refuses items carried by non-members.")]
class LF_HideoutStashStorageComponentClass : SCR_UniversalInventoryStorageComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_HideoutStashStorageComponent : SCR_UniversalInventoryStorageComponent
{
	//------------------------------------------------------------------------------------------------
	override bool CanStoreItem(IEntity item, int slotID)
	{
		if (!super.CanStoreItem(item, slotID))
			return false;

		// The client has no organization data; the server decides.
		if (!Replication.IsServer() || !item)
			return true;

		InventoryStorageManagerComponent carrierManager = LF_InventoryUtils.GetResponsibleStorageManager(item);
		if (!carrierManager)
			return true;

		LF_HideoutStashStorageManagerComponent stash = LF_HideoutStashStorageManagerComponent.Cast(GetOwner().FindComponent(LF_HideoutStashStorageManagerComponent));
		if (!stash || carrierManager == stash)
			return true;

		IEntity carrier = carrierManager.GetOwner();
		if (!ChimeraCharacter.Cast(carrier))
			return true;

		if (stash.AllowsUser(carrier, true))
			return true;

		LF_Debug.Warn("Hideout", string.Format("stash %1 refused %2 from a non-member", stash.GetHideoutId(), LF_Utils.GetPrefabName(item)));
		return false;
	}
}
