//------------------------------------------------------------------------------------------------
//! Where a system finds its places (#961). A placed component registers its owner here under a
//! kind (what the place is: a breach target, a bank vault, a hospital till) and an id (which one:
//! the stable key the feature saves). A system then asks LF_Places for the place by kind and id,
//! never by world entity name, layer name or coordinate, so a pack on another map works by
//! placing the components.
//!
//! Components register in OnPostInit, which runs before the game mode starts, so the table is
//! dropped only at world end (LF_StaticReset). Entries hold EntityID, never IEntity: a read
//! resolves the id and drops an entry whose entity is gone.
//------------------------------------------------------------------------------------------------
class LF_Places
{
	static const string FEATURE = "Places";

	//! kind -> (id -> entity)
	protected static ref map<string, ref map<string, EntityID>> s_mPlaces;

	//------------------------------------------------------------------------------------------------
	//! Registers entity as the place kind/id. The same entity again is a no-op. A second live entity
	//! under a taken key is refused: the id is a persistence key and two places cannot share it.
	//! \return true when entity now holds the key.
	static bool Register(string kind, string id, IEntity entity)
	{
		if (kind.IsEmpty() || id.IsEmpty() || !entity)
		{
			LF_Debug.Error(FEATURE, string.Format("register refused: kind '%1' id '%2' entity %3", kind, id, entity != null));
			return false;
		}

		map<string, EntityID> ids = Kind(kind, true);
		EntityID held;
		if (ids.Find(id, held))
		{
			if (held == entity.GetID())
				return true;

			if (Resolve(held))
			{
				LF_Debug.Error(FEATURE, string.Format("register refused: %1 '%2' is already placed; ids must be unique", kind, id));
				return false;
			}
		}

		ids.Set(id, entity.GetID());
		LF_Debug.Log(FEATURE, string.Format("placed %1 '%2'", kind, id));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The live entity placed as kind/id, or null when none is placed (logic tests, a map
	//! without that place). A stale entry is dropped.
	static IEntity Find(string kind, string id)
	{
		map<string, EntityID> ids = Kind(kind, false);
		if (!ids)
			return null;

		EntityID held;
		if (!ids.Find(id, held))
			return null;

		IEntity entity = Resolve(held);
		if (!entity)
			ids.Remove(id);

		return entity;
	}

	//------------------------------------------------------------------------------------------------
	//! World position of kind/id; false (and origin untouched) when it is not placed. Uses the world
	//! transform, so a place that is a child (the cash van's box) reports where it really is.
	static bool FindOrigin(string kind, string id, out vector origin)
	{
		IEntity entity = Find(kind, id);
		if (!entity)
			return false;

		vector transform[4];
		entity.GetWorldTransform(transform);
		origin = transform[3];
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Every live place of a kind, appended to into. \return how many were added.
	static int FindAll(string kind, notnull array<IEntity> into)
	{
		map<string, EntityID> ids = Kind(kind, false);
		if (!ids)
			return 0;

		int added = 0;
		array<string> stale = {};
		foreach (string id, EntityID held : ids)
		{
			IEntity entity = Resolve(held);
			if (!entity)
			{
				stale.Insert(id);
				continue;
			}

			into.Insert(entity);
			added++;
		}

		foreach (string id : stale)
			ids.Remove(id);

		return added;
	}

	//------------------------------------------------------------------------------------------------
	//! The id of the order-th place of a series that shares an id (the stops of one route). A
	//! negative order is not a series: the id itself.
	static string OrderedId(string id, int order)
	{
		if (order < 0)
			return id;

		return string.Format("%1#%2", id, order);
	}

	//------------------------------------------------------------------------------------------------
	//! World end: the next world registers its own places.
	static void ResetStatics()
	{
		s_mPlaces = null;
	}

	//------------------------------------------------------------------------------------------------
	protected static map<string, EntityID> Kind(string kind, bool create)
	{
		if (!s_mPlaces)
		{
			if (!create)
				return null;

			s_mPlaces = new map<string, ref map<string, EntityID>>();
		}

		map<string, EntityID> ids = s_mPlaces.Get(kind);
		if (!ids && create)
		{
			ids = new map<string, EntityID>();
			s_mPlaces.Set(kind, ids);
		}

		return ids;
	}

	//------------------------------------------------------------------------------------------------
	protected static IEntity Resolve(EntityID id)
	{
		if (id == EntityID.INVALID || !GetGame() || !GetGame().GetWorld())
			return null;

		return GetGame().GetWorld().FindEntityByID(id);
	}
}
