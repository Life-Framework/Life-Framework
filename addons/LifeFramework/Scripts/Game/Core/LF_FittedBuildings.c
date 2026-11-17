//------------------------------------------------------------------------------------------------
//! Fitted buildings are indestructible (D15, docs/architecture/everon-world.md). A vanilla building
//! the town pipeline fits stands in the base map, not in our layers, so no layer can switch its
//! destruction off. The fit layer's place marker carries LF_FittedBuildingComponent instead; on the
//! server it asks this class to find the building around the marker and turn its damage handling off.
//!
//! How the base game breaks a building (game build 26266485): SCR_DestructibleBuildingComponent is a
//! damage manager; every hit (shell, explosive, vehicle contact in OnFilteredContact, a parent's
//! DestroyChildDestructibles) goes through HandleDamage, and only the server's DESTROYED state change
//! sends RPC_GoToDestroyedState and the JIP OnRplSave bit. DamageManagerComponent.EnableDamageHandling
//! (false), called on the server, stops HandleDamage from touching the hit zones, so the building never
//! reaches DESTROYED and has nothing to replicate: clients and late joiners see it standing.
//!
//! The lookup is one sphere query per fitted place, bounded by the marker's radius. No coordinates
//! live here: the marker's position is the pack's data.
//------------------------------------------------------------------------------------------------
class LF_FittedBuildings
{
	static const string FEATURE = "FittedBuildings";
	//! A marker just outside the building's bounds (a doorstep) still finds it.
	static const float BOUNDS_SLACK = 3;

	//! Building EntityID -> the place id that fitted it.
	protected static ref map<EntityID, string> s_mFitted = new map<EntityID, string>();

	//------------------------------------------------------------------------------------------------
	//! Server: finds the vanilla building around `at` and makes it immune to damage.
	//! \param at The fitted place marker (inside the building, or on its doorstep).
	//! \param radius How far to look; the building's origin must lie inside it.
	//! \param placeId For the log and IsFitted.
	//! \return The building made indestructible, or null (logged).
	static IEntity Fit(vector at, float radius, string placeId)
	{
		if (!Replication.IsServer())
			return null;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
		{
			LF_Debug.Error(FEATURE, string.Format("%1: no world, the building stays destructible", placeId));
			return null;
		}

		IEntity building = FindBuilding(world, at, radius);
		if (!building)
		{
			LF_Debug.Error(FEATURE, string.Format("%1: no destructible building within %2 m of %3; it stays destructible", placeId, radius, at));
			return null;
		}

		DamageManagerComponent damage = DamageManagerComponent.Cast(building.FindComponent(SCR_DestructibleBuildingComponent));
		if (damage.IsDestroyed())
			LF_Debug.Warn(FEATURE, string.Format("%1: building %2 is already destroyed; fitting it anyway", placeId, building.GetPrefabData().GetPrefabName()));

		int parts = Shield(building);
		s_mFitted.Set(building.GetID(), placeId);
		LF_Debug.Info(FEATURE, string.Format("%1: building %2 at %3 is indestructible (%4 damage managers off)", placeId, building.GetPrefabData().GetPrefabName(), building.GetOrigin(), parts));
		return building;
	}

	//------------------------------------------------------------------------------------------------
	//! \return true when Fit made this building indestructible.
	static bool IsFitted(IEntity building)
	{
		return building && s_mFitted.Contains(building.GetID());
	}

	//------------------------------------------------------------------------------------------------
	//! Server: gives a fitted building its damage back (tests clean up with it).
	static void Release(IEntity building)
	{
		if (!building || !s_mFitted.Contains(building.GetID()))
			return;

		s_mFitted.Remove(building.GetID());
		if (Replication.IsServer())
			SetHandling(building, true);
	}

	//------------------------------------------------------------------------------------------------
	static int Count()
	{
		return s_mFitted.Count();
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mFitted.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! The building whose bounds hold `at` (the smallest footprint when several do), else the one whose
	//! bounds come closest within BOUNDS_SLACK.
	protected static IEntity FindBuilding(BaseWorld world, vector at, float radius)
	{
		LF_FittedBuildingQuery query = new LF_FittedBuildingQuery();
		world.QueryEntitiesBySphere(at, radius, query.Collect, null, EQueryEntitiesFlags.STATIC);

		IEntity best;
		float bestArea = float.MAX;
		IEntity near;
		float nearGap = BOUNDS_SLACK;
		foreach (IEntity e : query.m_aHits)
		{
			if (!e)
				continue;

			vector mins;
			vector maxs;
			e.GetWorldBounds(mins, maxs);
			float gap = GapXZ(at, mins, maxs);
			bool inside = gap <= 0 && at[1] >= mins[1] - 2 && at[1] <= maxs[1];
			if (inside)
			{
				float area = (maxs[0] - mins[0]) * (maxs[2] - mins[2]);
				if (area < bestArea)
				{
					bestArea = area;
					best = e;
				}
			}
			else if (gap < nearGap)
			{
				nearGap = gap;
				near = e;
			}
		}

		if (best)
			return best;

		return near;
	}

	//------------------------------------------------------------------------------------------------
	//! Horizontal distance from p to the box; 0 or less when p is over it.
	protected static float GapXZ(vector p, vector mins, vector maxs)
	{
		float dx = Math.Max(mins[0] - p[0], p[0] - maxs[0]);
		float dz = Math.Max(mins[2] - p[2], p[2] - maxs[2]);
		if (dx <= 0 && dz <= 0)
			return Math.Max(dx, dz);

		return Math.Sqrt(Math.Max(dx, 0) * Math.Max(dx, 0) + Math.Max(dz, 0) * Math.Max(dz, 0));
	}

	//------------------------------------------------------------------------------------------------
	//! Turns damage off on the building and on its child destructibles (a parent's destruction breaks them through HandleDamage).
	protected static int Shield(IEntity building)
	{
		return SetHandling(building, false);
	}

	//------------------------------------------------------------------------------------------------
	protected static int SetHandling(IEntity entity, bool enable)
	{
		int count = 0;
		DamageManagerComponent damage = DamageManagerComponent.Cast(entity.FindComponent(SCR_DestructibleBuildingComponent));
		if (damage)
		{
			damage.EnableDamageHandling(enable);
			count++;
		}

		IEntity child = entity.GetChildren();
		while (child)
		{
			count += SetHandling(child, enable);
			child = child.GetSibling();
		}

		return count;
	}
}

//------------------------------------------------------------------------------------------------
//! One sphere query's destructible buildings; lives only for the query (no static entity list).
class LF_FittedBuildingQuery
{
	ref array<IEntity> m_aHits = {};

	bool Collect(IEntity entity)
	{
		if (entity && entity.FindComponent(SCR_DestructibleBuildingComponent))
			m_aHits.Insert(entity);

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_FittedBuildings's statics (registered at script load; Core/LF_StaticReset.c).
class LF_FittedBuildings_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_FittedBuildings_StaticResetHook());

	override string Owner()
	{
		return "LF_FittedBuildings";
	}

	override void Apply()
	{
		LF_FittedBuildings.ResetStatics();
	}
}
