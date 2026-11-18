//------------------------------------------------------------------------------------------------
//! Locked vanilla buildings (#3203, "every building has a purpose"). A map maker drops
//! LF_BuildingLock.et inside a base-map house; its LF_BuildingLockComponent asks this class to find the
//! building (LF_FittedBuildings.FindBuilding: one bounded sphere query, the building whose bounds hold
//! the marker) and records the building's EntityID with the marker's sign key. LF_DoorUserAction walks a
//! door's parents and asks IsLocked, so every door of the building refuses to open with the key as the
//! reason, and the server refuses a forged open the same way.
//!
//! The registry is static data filled by the markers on EVERY machine (the prompt is right on a client,
//! a late joiner loads the same markers, and a restart loads them again): nothing replicates and
//! nothing is saved. A building that also holds an open marker (a fitted place, later a house marker)
//! ignores the private lock, so cards that make a building a home or a shop never edit the lock layer.
//! Only EntityIDs are stored, never entities, and nothing leaves this machine.
//------------------------------------------------------------------------------------------------
class LF_BuildingLocks
{
	static const string FEATURE = "BuildingLock";
	//! Neutral core sign keys; a pack rewords them by overriding the same Ids.
	static const string KEY_PRIVATE = "#LF-BuildingLock_Private";
	static const string KEY_STAFF_ONLY = "#LF-BuildingLock_StaffOnly";
	static const string KEY_FOR_SALE_SOON = "#LF-BuildingLock_ForSaleSoon";
	static const string KEY_OPENING_SOON = "#LF-BuildingLock_OpeningSoon";
	static const string KEY_CLOSED = "#LF-BuildingLock_Closed";

	//! Lock marker -> the building it locks.
	protected static ref map<EntityID, EntityID> s_mMarkerBuilding = new map<EntityID, EntityID>();
	//! Building -> the sign key of the lock marker that locked it last.
	protected static ref map<EntityID, string> s_mBuildingKey = new map<EntityID, string>();
	//! Open marker id (a fitted place's position) -> the building it keeps open.
	protected static ref map<string, EntityID> s_mOpen = new map<string, EntityID>();

	//------------------------------------------------------------------------------------------------
	//! Finds the building around the marker and locks it with `signKey`.
	//! \return true when a building was found and locked (an already registered marker counts).
	static bool Register(IEntity marker, float radius, string signKey)
	{
		if (!marker)
			return false;

		if (s_mMarkerBuilding.Contains(marker.GetID()))
			return true;

		IEntity building = FindIn(marker.GetOrigin(), radius);
		if (!building)
		{
			LF_Debug.Error(FEATURE, string.Format("lock marker %1: no building within %2 m of %3; nothing is locked", marker.GetName(), radius, marker.GetOrigin()));
			return false;
		}

		s_mMarkerBuilding.Set(marker.GetID(), building.GetID());
		s_mBuildingKey.Set(building.GetID(), signKey);
		LF_Debug.Info(FEATURE, string.Format("building %1 at %2 is locked: %3", building.GetPrefabData().GetPrefabName(), building.GetOrigin(), signKey));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The marker left the world: its building opens again unless another lock marker still holds it.
	static void Unregister(IEntity marker)
	{
		if (!marker)
			return;

		EntityID building;
		if (!s_mMarkerBuilding.Find(marker.GetID(), building))
			return;

		s_mMarkerBuilding.Remove(marker.GetID());
		foreach (EntityID other, EntityID otherBuilding : s_mMarkerBuilding)
		{
			if (otherBuilding == building)
				return;
		}

		s_mBuildingKey.Remove(building);
		LF_Debug.Info(FEATURE, string.Format("lock marker %1 removed: its building opens again", marker.GetName()));
	}

	//------------------------------------------------------------------------------------------------
	//! An open marker (a fitted place's marker; a house marker) keeps the building around `at` open.
	//! Every machine calls it, so the client's prompt matches the server's decision.
	//! \param id Unique per marker (the marker's position works); UnregisterOpen takes the same id.
	static bool RegisterOpenAt(vector at, float radius, string id)
	{
		if (s_mOpen.Contains(id))
			return true;

		IEntity building = FindIn(at, radius);
		if (!building)
			return false;

		s_mOpen.Set(id, building.GetID());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static void UnregisterOpen(string id)
	{
		s_mOpen.Remove(id);
	}

	//------------------------------------------------------------------------------------------------
	//! The sign key that locks `door`'s building, or "" when the door is free. Walks the door's parents;
	//! the first locked building up the chain decides.
	static string ReasonFor(IEntity door)
	{
		if (s_mBuildingKey.IsEmpty())
			return "";

		IEntity node = door;
		while (node)
		{
			string key;
			if (s_mBuildingKey.Find(node.GetID(), key))
			{
				if (IsKeptOpen(node.GetID()))
					return "";

				return key;
			}

			node = node.GetParent();
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	static int Count()
	{
		return s_mBuildingKey.Count();
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mMarkerBuilding.Clear();
		s_mBuildingKey.Clear();
		s_mOpen.Clear();
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsKeptOpen(EntityID building)
	{
		foreach (string id, EntityID kept : s_mOpen)
		{
			if (kept == building)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static IEntity FindIn(vector at, float radius)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return null;

		return LF_FittedBuildings.FindBuilding(world, at, radius);
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_BuildingLocks's statics (LF_StaticReset finds it by reflection).
class LF_BuildingLocks_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_BuildingLocks";
	}

	override void Apply()
	{
		LF_BuildingLocks.ResetStatics();
	}
}
