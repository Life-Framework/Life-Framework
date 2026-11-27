//------------------------------------------------------------------------------------------------
//! Keeps a store's shelf stock away from the vanilla garbage system (#3117). The engine tracks a loose
//! world item and deletes it when its lifetime runs out (Configs/Systems/GarbageSystem/Default.conf:
//! 1200 s). On an idle dedicated server every shelf copy was deleted in batches 20 minutes after boot
//! and the store opened empty (0grind, 2026-11-18). A shelf copy is blacklisted instead: the system
//! withdraws it now and refuses to insert it again (GarbageSystem.UpdateBlacklist). A unit a shopper
//! takes off the shelf is released back to the normal rules.
class LF_StoreGarbage
{
	protected static bool s_bWarned;

	//------------------------------------------------------------------------------------------------
	protected static GarbageSystem Find(IEntity item)
	{
		ChimeraWorld world = ChimeraWorld.CastFrom(item.GetWorld());
		if (!world)
			return null;

		GarbageSystem system = world.GetGarbageSystem();
		if (!system && !s_bWarned)
		{
			s_bWarned = true;
			LF_Debug.Warn("Store", "no garbage system in this world; shelf items cannot be excluded from it");
		}

		return system;
	}

	//------------------------------------------------------------------------------------------------
	//! A shelf copy leaves the garbage system and stays out of it.
	//! \return True when the item is blacklisted.
	static bool Hold(IEntity item)
	{
		if (!item)
			return false;

		GarbageSystem system = Find(item);
		if (!system)
			return false;

		return system.UpdateBlacklist(item, true);
	}

	//------------------------------------------------------------------------------------------------
	//! A unit that left the shelf goes back under the garbage rules. An item in an inventory is tracked by
	//! the engine when it is dropped; one lying loose is inserted now.
	static void Release(IEntity item)
	{
		if (!item)
			return;

		GarbageSystem system = Find(item);
		if (!system)
			return;

		system.UpdateBlacklist(item, false);
		if (LF_InventoryUtils.GetStorageHierachyRoot(item) == item)
			system.Insert(item);
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the item is tracked by the garbage system.
	static bool IsTracked(IEntity item)
	{
		if (!item)
			return false;

		GarbageSystem system = Find(item);
		if (!system)
			return false;

		return system.IsInserted(item);
	}

	//------------------------------------------------------------------------------------------------
	//! Test seam: asks the system to track the item (no lifetime argument: the config rules).
	//! \return True when the system took it.
	static bool TryInsert(IEntity item)
	{
		if (!item)
			return false;

		GarbageSystem system = Find(item);
		if (!system)
			return false;

		return system.Insert(item);
	}
}
