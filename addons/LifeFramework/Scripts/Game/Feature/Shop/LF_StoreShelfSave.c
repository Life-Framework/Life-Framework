//------------------------------------------------------------------------------------------------
//! Keeps a store's shelf copies out of the world save (#3127). A shelf copy is a plain world item, so
//! the engine's PersistenceSystem tracked it and wrote it; at boot it restored every copy beside the
//! ones the store spawns from its saved stock (the "storestock" payload), and AdoptStrays deleted the
//! restored ones again (0grind: hundreds of `a restored copy of X sat on a shelf point already filled;
//! deleted` at every boot). The stock count is the shelf's only save. A copy the store spawns or adopts
//! stops being tracked, and its old record is dropped at the next save; a unit a shopper takes off the
//! shelf is tracked again, so a carried or dropped item is saved as before.
class LF_StoreShelfSave
{
	protected static bool s_bWarned;

	//------------------------------------------------------------------------------------------------
	protected static PersistenceSystem Find()
	{
		PersistenceSystem system = PersistenceSystem.GetInstance();
		if (!system && !s_bWarned && Replication.IsServer())
		{
			s_bWarned = true;
			LF_Debug.Warn("Store", "no persistence system in this world; shelf items cannot be left out of the save");
		}

		return system;
	}

	//------------------------------------------------------------------------------------------------
	//! A shelf copy leaves the world save; a record it had is removed at the next save.
	//! \return True when the item is no longer tracked.
	static bool Hold(IEntity item)
	{
		if (!item)
			return false;

		PersistenceSystem system = Find();
		if (!system)
			return false;

		if (system.IsTracked(item))
			system.StopTracking(item, true);

		return !system.IsTracked(item);
	}

	//------------------------------------------------------------------------------------------------
	//! A unit that left the shelf is saved again, in a pocket or on the ground.
	static void Release(IEntity item)
	{
		if (!item)
			return;

		PersistenceSystem system = Find();
		if (!system || system.IsTracked(item))
			return;

		if (!system.StartTracking(item))
			LF_Debug.Warn("Store", string.Format("a unit taken off the shelf (%1) could not be tracked for the save", LF_Utils.GetPrefabName(item)));
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the item is tracked, so the world save writes it.
	static bool IsSaved(IEntity item)
	{
		if (!item)
			return false;

		PersistenceSystem system = Find();
		if (!system)
			return false;

		return system.IsTracked(item);
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the world has a persistence system to ask.
	static bool HasSystem()
	{
		return PersistenceSystem.GetInstance() != null;
	}
}
