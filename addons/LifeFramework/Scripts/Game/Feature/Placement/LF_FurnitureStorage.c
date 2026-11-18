//------------------------------------------------------------------------------------------------
//! Storage furniture (wardrobe, dresser, cupboard, chest, crate, fridge, kitchen unit): a placed
//! permanent prop with a real inventory whose contents persist with the placement.
//!
//!  - The placed prefab carries a vanilla SCR_UniversalInventoryStorageComponent (the capacity), an
//!    LF_FurnitureStorageManagerComponent (what LF_PersistentStorageComponent inserts through), an
//!    LF_PersistentStorageComponent with `m_bKeyAssignedLater` and an LF_FurnitureStorageOpenAction.
//!  - LF_PlacementSave calls LF_PlacedStorage.Attach on a fresh placement and again when it spawns a saved
//!    row: the storage key is `furniture:<placement id>`, stable across restarts, and the contents ride the
//!    existing "carried" payload as a placed-slot LF_StorageSnapshot record (no new save section).
//!  - A placement with items in it cannot be picked up ("Empty it first"), so items are never lost into the
//!    inventory-less pick-up path. An empty one is picked up and its record cleared.
//!  - Only the player who placed it is offered "Open" (the same owner stamp as "Pick up").
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "EveronLife/Feature/Placement", description: "Inventory manager of a placed storage furniture; LF_PersistentStorageComponent inserts through it.")]
class LF_FurnitureStorageManagerComponentClass : ScriptedInventoryStorageManagerComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_FurnitureStorageManagerComponent : ScriptedInventoryStorageManagerComponent
{
	//------------------------------------------------------------------------------------------------
	//! The engine asks this when a client's inventory request moves or removes an item the furniture holds.
	//! Only the player who placed it may (the "Open" action is only offered to them, this is the server's
	//! check behind it); a server-side request with no instigator (the restore, a script) passes.
	override protected bool HandleAccessValidation(InventoryStorageManagerComponent instigatorManager, IEntity item, EInventoryManagerOperation operation)
	{
		if (!Replication.IsServer() || !instigatorManager || instigatorManager == this)
			return true;

		if (LF_PlacedOwnerComponent.IsOwnedBy(GetOwner(), instigatorManager.GetOwner()))
			return true;

		LF_Debug.Warn(LF_PlacedStorage.FEATURE, string.Format("furniture storage kept %1: the taker is not the placer", LF_Utils.GetPrefabName(item)));
		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! "Open" on a placed storage furniture: the vanilla open-storage action, offered to the placer only.
class LF_FurnitureStorageOpenAction : SCR_OpenStorageAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!LF_PlacedOwnerComponent.IsOwnedBy(GetOwner(), user))
			return false;

		return super.CanBeShownScript(user);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!LF_PlacedOwnerComponent.IsOwnedBy(GetOwner(), user))
			return false;

		return super.CanBePerformedScript(user);
	}
}

//------------------------------------------------------------------------------------------------
class LF_PlacedStorage
{
	static const string FEATURE = "PlacedStorage";
	static const string KEY_PREFIX = "furniture:";

	//------------------------------------------------------------------------------------------------
	//! The save key of the storage of placement `id`.
	static string KeyFor(string placementId)
	{
		return KEY_PREFIX + placementId;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The persistent storage component of a placed prop, null for a prop with no inventory.
	static LF_PersistentStorageComponent ComponentOf(IEntity placed)
	{
		return LF_Component<LF_PersistentStorageComponent>.Find(placed);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a placed prop that has an inventory gets its stable key (and its saved contents back). A prop
	//! with none (a chair) is left alone, silently. \return true when a storage was attached.
	static bool Attach(IEntity placed, string placementId)
	{
		LF_PersistentStorageComponent storage = ComponentOf(placed);
		if (!storage || placementId.IsEmpty())
			return false;

		return storage.AssignKey(KeyFor(placementId));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: whether the storage of a placed prop holds anything.
	static bool HasContents(IEntity placed, string placementId)
	{
		if (!ComponentOf(placed))
			return false;

		LF_StorageSnapshotRecord record = LF_StorageSnapshot.Capture(KeyFor(placementId), LF_PersistentStorageComponent.SLOT_PLACED, placed);
		return record && record.m_aItems && record.m_aItems.Count() > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: forgets the saved record of a placement that is gone for good (picked up empty).
	static void Release(string placementId)
	{
		LF_StorageSnapshot.Clear(KeyFor(placementId), LF_PersistentStorageComponent.SLOT_PLACED);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: tells `player` why a pick-up was refused.
	static void NotifyEmptyFirst(IEntity player)
	{
		if (!player || !GetGame() || !GetGame().GetPlayerManager())
			return;

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(player);
		if (playerId > 0)
			LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-Placement_Title", "#LF-Placement_EmptyFirst", 3.0, LF_ENotificationType.WARNING);
	}
}
