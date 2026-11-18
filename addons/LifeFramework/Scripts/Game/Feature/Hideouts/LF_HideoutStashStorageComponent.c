//------------------------------------------------------------------------------------------------
//! The stash's storage. A normal universal storage, except that the server only takes an item in
//! from the stash itself, from a server spawn (restore, seeding: no carrier, inside
//! LF_ServerInsertScope; a ground item has no carrier either and is refused, #2584), or from a living
//! member's own inventory. CanStoreItem has no instigator, so the carrier stands in for it only
//! where the engine makes them the same person: a living character's inventory is moved by its
//! own player alone. A vehicle, crate or body is open to anyone, so an item there is refused and
//! a member carries it first (#2574).
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Hideouts", description: "Storage of a hideout stash. Takes items only from a living member's own inventory or a server spawn.")]
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

		LF_HideoutStashStorageManagerComponent stash = LF_HideoutStashStorageManagerComponent.Cast(GetOwner().FindComponent(LF_HideoutStashStorageManagerComponent));
		if (!stash)
			return true;

		return AllowsFromCarrier(stash, item, LF_InventoryUtils.GetResponsibleStorageManager(item));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: whether an item held by this carrier manager may go into the stash.
	//! No carrier is a server spawn (restore, seeding) or an item on the ground that a client asks
	//! to insert; only the first runs inside LF_ServerInsertScope (#2584).
	protected bool AllowsFromCarrier(LF_HideoutStashStorageManagerComponent stash, IEntity item, InventoryStorageManagerComponent carrierManager)
	{
		if (carrierManager == stash)
			return true;

		if (!carrierManager)
		{
			if (LF_ServerInsertScope.IsOpenFor(item))
				return true;

			LF_Debug.Warn("Hideout", string.Format("stash %1 refused %2 from the ground: a member carries it first", stash.GetHideoutId(), LF_Utils.GetPrefabName(item)));
			return false;
		}

		IEntity carrier = carrierManager.GetOwner();
		ChimeraCharacter character = ChimeraCharacter.Cast(carrier);
		if (!character)
		{
			LF_Debug.Warn("Hideout", string.Format("stash %1 refused %2 from %3: only a member's own inventory fills the stash", stash.GetHideoutId(), LF_Utils.GetPrefabName(item), LF_Utils.GetPrefabName(carrier)));
			return false;
		}

		// Anyone can loot a body, so a dead or unconscious carrier is not the one moving the item.
		if (!LF_Utils.IsAliveState(character))
		{
			LF_Debug.Warn("Hideout", string.Format("stash %1 refused %2 from a body that is not alive", stash.GetHideoutId(), LF_Utils.GetPrefabName(item)));
			return false;
		}

		if (stash.AllowsUser(carrier, true))
			return true;

		LF_Debug.Warn("Hideout", string.Format("stash %1 refused %2 from a non-member", stash.GetHideoutId(), LF_Utils.GetPrefabName(item)));
		return false;
	}
}
