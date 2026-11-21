//! Hides vanilla clutter a building prefab brings with it (card #3882, docs/architecture/base-world-clear.md).
//! A stamped shop kit lists the vanilla props it replaces (the kit's clear list); this takes those prop prefabs out of
//! one building as a player sees and bumps into them: not drawn, not traced, no collision. It looks only inside that
//! building's own box, so nothing outside the building and no other copy of the prefab is touched.
//! Base-map entities load the same way on the server and on every client, so each machine runs the hide on its own
//! copy at load: nothing goes over the network, a late joiner hides at its own load, and a restart hides again.
class LF_BaseWorldClear
{
	static const string FEATURE = "Placement";

	//------------------------------------------------------------------------------------------------
	//! The path part of a resource name, lower case: "{GUID}Prefabs/A.et" and "Prefabs/a.et" match.
	static string PathKey(string resource)
	{
		string key = resource;
		int close = key.IndexOf("}");
		if (key.StartsWith("{") && close > 0)
			key = key.Substring(close + 1, key.Length() - close - 1); // lf-bounded: a resource path, far under 8 KB
		key.ToLower();
		return key;
	}

	//------------------------------------------------------------------------------------------------
	//! The prefab an entity comes from. A building furniture prop is an inline child of the furniture prefab, so its own
	//! GetPrefabName() is empty; the first ancestor container with a path names the real prop (StepladderWooden_01.et).
	static string PrefabOf(notnull IEntity entity)
	{
		EntityPrefabData data = entity.GetPrefabData();
		if (!data)
			return string.Empty;

		return SCR_BaseContainerTools.GetPrefabResourceName(data.GetPrefab());
	}

	//------------------------------------------------------------------------------------------------
	//! Hides every entity of the building whose prefab is on the list.
	//! \return how many entities were hidden now (0 when the list names nothing the building has).
	static int HideInBuilding(notnull IEntity building, notnull array<string> prefabs)
	{
		set<string> keys = new set<string>();
		foreach (string prefab : prefabs)
		{
			if (!prefab.IsEmpty())
				keys.Insert(PathKey(prefab));
		}

		if (keys.IsEmpty())
			return 0;

		int hidden = HideUnder(building, keys);
		string name = PrefabOf(building);

		if (hidden == 0)
			LF_Debug.Warn(FEATURE, string.Format("base-world clear: nothing left to hide (%1 prefabs listed) in %2 at %3", keys.Count(), name, building.GetOrigin()));
		else
			LF_Debug.Info(FEATURE, string.Format("base-world clear: hid %1 vanilla props in %2 at %3", hidden, name, building.GetOrigin()));

		return hidden;
	}

	//------------------------------------------------------------------------------------------------
	//! Hides every entity of the list inside an oriented box that is not a building (outdoor clutter: crates and pallets
	//! of the base map in a yard). mins and maxs are local to mat; the same hide and the same prefab match as a building.
	//! eturn how many entities were hidden now.
	static int HideInBox(notnull BaseWorld world, vector mins, vector maxs, vector mat[4], notnull array<string> prefabs, string label)
	{
		set<string> keys = new set<string>();
		foreach (string prefab : prefabs)
		{
			if (!prefab.IsEmpty())
				keys.Insert(PathKey(prefab));
		}

		if (keys.IsEmpty())
			return 0;

		LF_BaseWorldClearQuery query = new LF_BaseWorldClearQuery();
		query.m_Keys = keys;
		world.QueryEntitiesByOBB(mins, maxs, mat, query.Collect, null, EQueryEntitiesFlags.ALL);

		int hidden = HideFound(query);
		if (hidden == 0)
			LF_Debug.Warn(FEATURE, string.Format("base-world clear: nothing left to hide (%1 prefabs listed) in the box %2 at %3", keys.Count(), label, mat[3]));
		else
			LF_Debug.Info(FEATURE, string.Format("base-world clear: hid %1 vanilla props in the box %2 at %3", hidden, label, mat[3]));

		return hidden;
	}

	//------------------------------------------------------------------------------------------------
	protected static int HideFound(notnull LF_BaseWorldClearQuery query)
	{
		int hidden = 0;
		foreach (IEntity prop : query.m_aFound)
		{
			if (!LF_FelledTreesReplication.IsHidden(prop))
			{
				LF_FelledTreesReplication.HideTree(prop);
				hidden++;
			}
		}

		return hidden;
	}

	//------------------------------------------------------------------------------------------------
	//! Building furniture is not parented to the building at run time (a prefab child with no Hierarchy component
	//! spawns as its own world entity), so the hide matches by prefab inside the building's own box.
	protected static int HideUnder(IEntity building, set<string> keys)
	{
		vector mins, maxs;
		building.GetBounds(mins, maxs);
		vector mat[4];
		building.GetWorldTransform(mat);
		LF_BaseWorldClearQuery query = new LF_BaseWorldClearQuery();
		query.m_Keys = keys;
		query.m_Building = building;
		building.GetWorld().QueryEntitiesByOBB(mins, maxs, mat, query.Collect, null, EQueryEntitiesFlags.ALL);

		return HideFound(query);
	}
}

//! Entities inside one building's box whose prefab is on a clear list.
class LF_BaseWorldClearQuery : Managed
{
	ref set<string> m_Keys;
	IEntity m_Building;
	ref array<IEntity> m_aFound = {};

	//------------------------------------------------------------------------------------------------
	bool Collect(IEntity entity)
	{
		if (entity == m_Building)
			return true;

		string prefab = LF_BaseWorldClear.PrefabOf(entity);
		if (!prefab.IsEmpty() && m_Keys.Contains(LF_BaseWorldClear.PathKey(prefab)))
			m_aFound.Insert(entity);

		return true;
	}
}

class LF_BaseWorldClearComponentClass : ScriptComponentClass
{
}

//! The stamp's carrier for a clear list: an entity in the island layer at a stamped building's origin. At load it finds
//! the building of m_sBuilding nearest to itself (within m_fRadiusM) and hides the listed vanilla props inside it.
//! Runs on the server and on every client, since the layer and the base map load on each.
class LF_BaseWorldClearComponent : ScriptComponent
{
	[Attribute(desc: "Building prefab whose own furniture is cleared", params: "et")]
	protected ResourceName m_sBuilding;

	[Attribute(desc: "Vanilla prop prefabs to hide inside that building", params: "et")]
	protected ref array<ResourceName> m_aPrefabs;

	[Attribute("0 0 0", desc: "Box mode (no building): half size on x and z in metres, centred on this entity and turned by its yaw. y is unused; the box stands on the ground under the carrier. Zero = building mode")]
	protected vector m_vBoxHalfM;

	[Attribute("4", desc: "Box mode: how tall the box is above the ground under the carrier, metres (it also reaches 1 m below)")]
	protected float m_fBoxHeightM;

	[Attribute("3", desc: "How far from this entity the building origin may be on x and z, metres")]
	protected float m_fRadiusM;

	//! Extra sphere radius so a carrier at another height still reaches the building origin.
	protected static const float SEARCH_UP_M = 30;

	protected IEntity m_Found;
	protected float m_fFoundM;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		// The base map may finish its own init after this layer entity; hide on the next frame.
		GetGame().GetCallqueue().CallLater(Apply, 0, false, owner);
	}

	//------------------------------------------------------------------------------------------------
	protected bool Collect(IEntity entity)
	{
		if (LF_BaseWorldClear.PathKey(LF_BaseWorldClear.PrefabOf(entity)) != LF_BaseWorldClear.PathKey(m_sBuilding))
			return true;

		// Horizontal distance only: the carrier stores no height, the building supplies it.
		vector a = entity.GetOrigin();
		vector b = GetOwner().GetOrigin();
		float gap = Math.Sqrt((a[0] - b[0]) * (a[0] - b[0]) + (a[2] - b[2]) * (a[2] - b[2]));
		if (gap <= m_fRadiusM && (!m_Found || gap < m_fFoundM))
		{
			m_Found = entity;
			m_fFoundM = gap;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void Apply(IEntity owner)
	{
		if (!owner || !m_aPrefabs || m_aPrefabs.IsEmpty())
		{
			LF_Debug.Error(LF_BaseWorldClear.FEATURE, "base-world clear: a carrier has no clear list");
			return;
		}

		if (m_sBuilding.IsEmpty())
		{
			ApplyBox(owner);
			return;
		}

		m_Found = null;
		owner.GetWorld().QueryEntitiesBySphere(owner.GetOrigin(), m_fRadiusM + SEARCH_UP_M, Collect, null, EQueryEntitiesFlags.ALL);
		if (!m_Found)
		{
			LF_Debug.Error(LF_BaseWorldClear.FEATURE, string.Format("base-world clear: no %1 within %2 m of %3", m_sBuilding, m_fRadiusM, owner.GetOrigin()));
			return;
		}

		array<string> prefabs = {};
		foreach (ResourceName prefab : m_aPrefabs)
		{
			prefabs.Insert(prefab);
		}

		LF_BaseWorldClear.HideInBuilding(m_Found, prefabs);
		m_Found = null;
	}

	//------------------------------------------------------------------------------------------------
	//! No building: the box stands on the ground under the carrier, so the layer stores no height.
	protected void ApplyBox(IEntity owner)
	{
		if (m_vBoxHalfM[0] <= 0 || m_vBoxHalfM[2] <= 0)
		{
			LF_Debug.Error(LF_BaseWorldClear.FEATURE, string.Format("base-world clear: a carrier at %1 has neither a building nor a box", owner.GetOrigin()));
			return;
		}

		vector mat[4];
		owner.GetWorldTransform(mat);
		mat[3][1] = owner.GetWorld().GetSurfaceY(mat[3][0], mat[3][2]);
		array<string> prefabs = {};
		foreach (ResourceName prefab : m_aPrefabs)
		{
			prefabs.Insert(prefab);
		}

		LF_BaseWorldClear.HideInBox(owner.GetWorld(), Vector(-m_vBoxHalfM[0], -1, -m_vBoxHalfM[2]), Vector(m_vBoxHalfM[0], m_fBoxHeightM, m_vBoxHalfM[2]), mat, prefabs, owner.GetName());
	}
}
