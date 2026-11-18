//------------------------------------------------------------------------------------------------
//! Picking up a permanent placement (#1032, slice #2653): the placer, standing within reach, takes
//! it back. The entity goes, its row is forgotten (so the next save drops it and it stays gone after
//! a restart), a refundable cost is given back once (LF_PlacementCost.OnPickedUp) and the carried item
//! it came from goes into the player's inventory, or at their feet when it does not fit.
//! Server only; nothing comes from the client but which entity.
//------------------------------------------------------------------------------------------------
class LF_PlacementPickup
{
	static const string FEATURE = "Placement";
	static const float REACH_METRES = 4; // lf-fixed: the composition pick-up's reach (LF_PlacementCompositions.PickUp)

	protected static string s_sLastRefusal;

	//------------------------------------------------------------------------------------------------
	static string LastRefusal()
	{
		return s_sLastRefusal;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_sLastRefusal = "";
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: why `pickerKey` may not pick up a placement owned by `ownerKey` at `distance` metres.
	static string Judge(string pickerKey, string ownerKey, float distance)
	{
		if (pickerKey.IsEmpty() || pickerKey != ownerKey)
			return "only the owner may pick it up";

		if (distance > REACH_METRES)
			return string.Format("too far (%1 m, most %2 m)", Math.Round(distance * 10) / 10, REACH_METRES);

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Server: `player` picks up the permanent placement `placed`. \return true when it was taken.
	static bool PickUp(IEntity player, IEntity placed)
	{
		s_sLastRefusal = "";
		if (!Replication.IsServer())
			return Refuse("refused: not the server");

		if (!player || !placed)
			return Refuse("refused: no player or no placement");

		string id = LF_PlacementSave.IdOf(placed);
		LF_PlacementRow row = LF_PlacementSave.Find(id);
		if (!row)
			return Refuse("refused: not a permanent placement");

		string pickerKey = LF_Houses.CharacterKeyOf(player);
		string why = Judge(pickerKey, row.m_sOwnerKey, vector.Distance(player.GetOrigin(), placed.GetOrigin()));
		if (!why.IsEmpty())
			return Refuse(string.Format("refused: %1 picking up %2: %3", pickerKey, id, why));

		// Storage furniture with something in it stays: the pick-up path has no inventory to put it in.
		if (LF_PlacedStorage.HasContents(placed, id))
		{
			LF_PlacedStorage.NotifyEmptyFirst(player);
			return Refuse(string.Format("refused: %1 picking up %2: it still holds items, empty it first", pickerKey, id));
		}

		EntityID placedId = placed.GetID();
		LF_PlacementSave.Forget(id);
		LF_PlacedStorage.Release(id);
		SCR_EntityHelper.DeleteEntityAndChildren(placed);
		LF_PlacementCost.OnPickedUp(player, placedId);
		GiveItemBack(player, row.m_sItemPrefab);
		LF_Debug.Info(FEATURE, string.Format("picked up permanent placement %1 (%2) by %3, %4 owned", id, row.m_sKind, pickerKey, LF_PlacementSave.CountOwned(pickerKey)));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void GiveItemBack(notnull IEntity player, string itemPrefab)
	{
		if (itemPrefab.IsEmpty())
			return;

		InventoryStorageManagerComponent storage = InventoryStorageManagerComponent.Cast(player.FindComponent(InventoryStorageManagerComponent));
		if (storage && LF_InventoryUtils.AddItem(storage, itemPrefab))
			return;

		// Does not fit: at the player's feet, so the pick-up never loses it.
		if (!LF_Utils.SpawnEntityPrefab(itemPrefab, player.GetOrigin()))
			LF_Debug.Error(FEATURE, string.Format("picked-up item could not be given back: %1", itemPrefab));
	}

	//------------------------------------------------------------------------------------------------
	protected static bool Refuse(string message)
	{
		s_sLastRefusal = message;
		LF_Debug.Warn(FEATURE, message);
		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_PlacementPickup's statics (LF_StaticReset finds it by reflection).
class LF_PlacementPickup_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_PlacementPickup";
	}

	override void Apply()
	{
		LF_PlacementPickup.ResetStatics();
	}
}
