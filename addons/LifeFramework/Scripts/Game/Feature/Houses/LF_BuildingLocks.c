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
	protected static ref map<EntityID, EntityID> s_mMarkerBuilding;
	protected static map<EntityID, EntityID> LzMarkerBuilding()
	{
		if (!s_mMarkerBuilding)
			s_mMarkerBuilding = new map<EntityID, EntityID>();
		return s_mMarkerBuilding;
	}
	//! Building -> the sign key of the lock marker that locked it last.
	protected static ref map<EntityID, string> s_mBuildingKey;
	protected static map<EntityID, string> LzBuildingKey()
	{
		if (!s_mBuildingKey)
			s_mBuildingKey = new map<EntityID, string>();
		return s_mBuildingKey;
	}
	//! Building -> the house marker (an LF_HouseManagerComponent on the lock marker) whose owner and key holders pass (#3584).
	protected static ref map<EntityID, EntityID> s_mBuildingHouse;
	protected static map<EntityID, EntityID> LzBuildingHouse()
	{
		if (!s_mBuildingHouse)
			s_mBuildingHouse = new map<EntityID, EntityID>();
		return s_mBuildingHouse;
	}
	//! Building -> the sign key its door shows a stranger once its house has an owner.
	protected static ref map<EntityID, string> s_mBuildingOwnedKey;
	protected static map<EntityID, string> LzBuildingOwnedKey()
	{
		if (!s_mBuildingOwnedKey)
			s_mBuildingOwnedKey = new map<EntityID, string>();
		return s_mBuildingOwnedKey;
	}
	//! Open marker id (a fitted place's position) -> the building it keeps open.
	protected static ref map<string, EntityID> s_mOpen = new map<string, EntityID>();

	//------------------------------------------------------------------------------------------------
	//! Finds the building around the marker and locks it with `signKey`.
	//! A marker that also carries an LF_HouseManagerComponent makes the building a house (#3584): its owner and anyone
	//! holding its key pass, and once it has an owner a stranger's prompt shows `ownedSignKey` instead of `signKey`.
	//! \return true when a building was found and locked (an already registered marker counts).
	static bool Register(IEntity marker, float radius, string signKey, string ownedSignKey = "")
	{
		if (!marker)
			return false;

		LzMarkerBuilding();
		if (s_mMarkerBuilding.Contains(marker.GetID()))
			return true;

		IEntity building = FindIn(marker.GetOrigin(), radius);
		if (!building)
		{
			LF_Debug.Error(FEATURE, string.Format("lock marker %1: no building within %2 m of %3; nothing is locked", marker.GetName(), radius, marker.GetOrigin()));
			return false;
		}

		LzMarkerBuilding();
		s_mMarkerBuilding.Set(marker.GetID(), building.GetID());
		LzBuildingKey();
		s_mBuildingKey.Set(building.GetID(), signKey);
		if (LF_Component<LF_HouseManagerComponent>.Find(marker))
		{
			LzBuildingHouse();
			s_mBuildingHouse.Set(building.GetID(), marker.GetID());
			LzBuildingOwnedKey();
			s_mBuildingOwnedKey.Set(building.GetID(), ownedSignKey);
		}

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
		LzMarkerBuilding();
		if (!s_mMarkerBuilding.Find(marker.GetID(), building))
			return;

		LzMarkerBuilding();
		s_mMarkerBuilding.Remove(marker.GetID());
		foreach (EntityID other, EntityID otherBuilding : LzMarkerBuilding())
		{
			if (otherBuilding == building)
				return;
		}

		LzBuildingKey();
		s_mBuildingKey.Remove(building);
		LzBuildingHouse();
		s_mBuildingHouse.Remove(building);
		LzBuildingOwnedKey();
		s_mBuildingOwnedKey.Remove(building);
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
	//! the first locked building up the chain decides. A house marker's building (#3584) is free for `user` when they
	//! own the house or hold its key; with no user it stays locked.
	static string ReasonFor(IEntity door, IEntity user = null)
	{
		LzBuildingKey();
		if (s_mBuildingKey.IsEmpty())
			return "";

		// A resident on a walk goes in and out of its own home through the locked door: the lock keeps players out.
		if (user && LF_ResidentWalk.IsWalkingBody(user))
			return "";

		IEntity node = door;
		while (node)
		{
			string key;
			LzBuildingKey();
			if (s_mBuildingKey.Find(node.GetID(), key))
			{
				if (IsKeptOpen(node.GetID()))
					return "";

				return HouseReason(node.GetID(), key, user);
			}

			node = node.GetParent();
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! \return The reason a lock holds `user` out of `building`: `key` for a plain lock or an unowned house, the owned
	//! key for an owned house, "" when the user owns the house or holds its key. Reads the marker's replicated owner
	//! key and house id, so the client's prompt and the server's refusal agree.
	protected static string HouseReason(EntityID building, string key, IEntity user)
	{
		EntityID markerId;
		LzBuildingHouse();
		if (!s_mBuildingHouse.Find(building, markerId))
			return key;

		BaseWorld world = GetGame().GetWorld();
		IEntity marker;
		if (world)
			marker = world.FindEntityByID(markerId);

		LF_HouseManagerComponent house;
		if (marker)
			house = LF_Component<LF_HouseManagerComponent>.Find(marker);

		if (!house)
			return key;

		if (user && LF_BuildSiteComponent.UserHoldsKey(user, house.GetHouseIdentifier()))
			return "";

		string ownerKey = house.GetOwnerKey();
		if (ownerKey.IsEmpty())
			return key;

		if (user && LF_VehicleLockComponent.IdentifiersMatch(LF_Houses.CharacterKeyOf(user), ownerKey))
			return "";

		string ownedKey;
		LzBuildingOwnedKey();
		s_mBuildingOwnedKey.Find(building, ownedKey);
		if (ownedKey.IsEmpty())
			return key;

		return ownedKey;
	}

	//------------------------------------------------------------------------------------------------
	static int Count()
	{
		LzBuildingKey();
		return s_mBuildingKey.Count();
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		LzMarkerBuilding();
		s_mMarkerBuilding.Clear();
		LzBuildingKey();
		s_mBuildingKey.Clear();
		LzBuildingHouse();
		s_mBuildingHouse.Clear();
		LzBuildingOwnedKey();
		s_mBuildingOwnedKey.Clear();
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
