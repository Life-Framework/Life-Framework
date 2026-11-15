//------------------------------------------------------------------------------------------------
//! Goods flagged stolen (fence buyback). The server marks an item when a crime hands it over (a
//! weapon robbery, the bank's loot) and a stocked shop whose buyback takes stolen goods buys it.
//! Server only and not saved: the flag lives as long as the item entity does this session, so a
//! restart launders an item (a known gap; the configured buyback list does not depend on it).
class LF_StolenGoods
{
	static const string FEATURE = "Stolen";

	protected static ref set<EntityID> s_Flagged;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Flagged = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: flag one item stolen.
	static void Mark(IEntity item)
	{
		if (!item || !Replication.IsServer())
			return;

		if (!s_Flagged)
			s_Flagged = new set<EntityID>();

		s_Flagged.Insert(item.GetID());
		LF_Debug.Info(FEATURE, "flagged stolen: " + LF_Utils.GetPrefabName(item));
	}

	//------------------------------------------------------------------------------------------------
	static bool IsStolen(IEntity item)
	{
		if (!item || !s_Flagged)
			return false;

		return s_Flagged.Contains(item.GetID());
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the flagged items of one prefab \p owner carries, in storage or in hand. A stack
	//! (LF_QuantityComponent) is never counted: one entity would sell as one unit and lose the rest.
	static void Carried(IEntity owner, ResourceName prefab, notnull array<IEntity> flagged)
	{
		flagged.Clear();
		if (!owner || prefab.IsEmpty() || !s_Flagged)
			return;

		array<IEntity> found = {};
		InventoryStorageManagerComponent storage = LF_InventoryUtils.GetResponsibleStorageManager(owner);
		if (storage)
			found.Copy(LF_InventoryUtils.FindItemsByPrefab(storage, prefab));

		foreach (IEntity held : LF_InventoryUtils.CollectCarried(owner))
		{
			if (held && found.Find(held) < 0 && LF_Utils.SamePrefab(LF_Utils.GetPrefabName(held), prefab))
				found.Insert(held);
		}

		foreach (IEntity item : found)
		{
			if (IsStolen(item) && !LF_Component<LF_QuantityComponent>.Find(item))
				flagged.Insert(item);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: delete up to \p units flagged items of a prefab from \p owner.
	//! \return How many were taken.
	static int Take(IEntity owner, ResourceName prefab, int units)
	{
		array<IEntity> flagged = {};
		Carried(owner, prefab, flagged);
		int taken;
		CharacterControllerComponent controller = LF_Component<CharacterControllerComponent>.Find(owner);
		foreach (IEntity item : flagged)
		{
			if (taken >= units)
				break;

			if (controller && controller.GetAttachedGadgetAtLeftHandSlot() == item)
				controller.RemoveGadgetFromHand(true);

			s_Flagged.RemoveItem(item.GetID());
			SCR_EntityHelper.DeleteEntityAndChildren(item);
			taken++;
		}

		return taken;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: flag every item of \p prefab \p owner carries that is not in \p before (what a crime
	//! just handed over, when the grant does not return the entity).
	static void MarkNew(IEntity owner, ResourceName prefab, array<IEntity> before)
	{
		InventoryStorageManagerComponent storage = LF_InventoryUtils.GetResponsibleStorageManager(owner);
		if (!storage)
			return;

		foreach (IEntity item : LF_InventoryUtils.FindItemsByPrefab(storage, prefab))
		{
			if (!before || before.Find(item) < 0)
				Mark(item);
		}
	}
}
