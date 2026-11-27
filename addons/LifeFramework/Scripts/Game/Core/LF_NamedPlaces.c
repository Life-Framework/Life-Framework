//------------------------------------------------------------------------------------------------
//! The name of the nearest town, village or landmark to a position (#1943), read from the map's
//! own location entities (the editor comment entities the base game places for every map label,
//! `Prefabs/World/Locations/...`). The core holds no list of places: a pack on another map, or a
//! pack that wants a name the map lacks, places a location entity and Nearest finds it.
//!
//! The answer is the entity's display name as the map stores it, usually a `#` localization key:
//! pass it to a text widget's SetText and it localizes there.
//------------------------------------------------------------------------------------------------
class LF_NamedPlaces
{
	static const string FEATURE = "NamedPlaces";

	//! How far from a position a location still names it, metres on the ground plane.
	static const float DEFAULT_RADIUS = 600;

	//! How far a settlement label still names a place, metres: a town is wider than one building.
	static const float SETTLEMENT_RADIUS = 1500;

	//------------------------------------------------------------------------------------------------
	//! \param pos World position to name
	//! \param maxRadius Farthest a location may be from pos and still count, metres
	//! \return The display name of the nearest map location within maxRadius, or empty when none is
	//! that close (open country, a map with no labels, no editable-entity core in this world).
	static string Nearest(vector pos, float maxRadius = DEFAULT_RADIUS)
	{
		SCR_EditableEntityComponent location = FindNearest(pos, maxRadius);
		if (!location)
			return "";

		return location.GetDisplayName();
	}

	//------------------------------------------------------------------------------------------------
	//! The name of the nearest settlement (city, town, village or settlement label), skipping the
	//! labels of a factory, farm, harbour or other single site. A place's town on the map reads from
	//! this, so a clerk beside a factory still sits in its settlement.
	//! \param pos World position to name
	//! \param maxRadius Farthest a settlement may be from pos, metres on the ground plane
	//! \return The settlement's display name (usually a `#` key), or empty when none is that close.
	static string NearestSettlement(vector pos, float maxRadius = SETTLEMENT_RADIUS)
	{
		SCR_EditableEntityComponent location = FindNearest(pos, maxRadius, true);
		if (!location)
			return "";

		return location.GetDisplayName();
	}

	//------------------------------------------------------------------------------------------------
	//! True for the descriptor types that name a settlement: not a generic site label, not a local one.
	static bool IsSettlementType(int type)
	{
		switch (type)
		{
			case EMapDescriptorType.MDT_NAME_CITY:
			case EMapDescriptorType.MDT_NAME_VILLAGE:
			case EMapDescriptorType.MDT_NAME_TOWN:
			case EMapDescriptorType.MDT_NAME_SETTLEMENT:
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsSettlementLabel(SCR_EditableEntityComponent location)
	{
		IEntity owner = location.GetOwner();
		if (!owner)
			return false;

		MapDescriptorComponent descriptor = MapDescriptorComponent.Cast(owner.FindComponent(MapDescriptorComponent));
		if (!descriptor)
			return false;

		return IsSettlementType(descriptor.GetBaseType());
	}

	//------------------------------------------------------------------------------------------------
	//! \return The editable location entity nearest to pos within maxRadius, or null. settlementOnly
	//! keeps only the labels whose map descriptor is a settlement type.
	protected static SCR_EditableEntityComponent FindNearest(vector pos, float maxRadius, bool settlementOnly = false)
	{
		SCR_EditableEntityCore core = SCR_EditableEntityCore.Cast(SCR_EditableEntityCore.GetInstance(SCR_EditableEntityCore));
		if (!core)
		{
			LF_Debug.Warn(FEATURE, "no editable entity core in this world; no place names");
			return null;
		}

		// Not core.FindNearestEntity: it iterates the core's private entity set without a null check, and
		// that set stays null until the first editable entity registers (a world with no labels, such as
		// TestLogic, VMEs inside it). GetAllEntities guards the null and hands back an empty set.
		set<SCR_EditableEntityComponent> entities = new set<SCR_EditableEntityComponent>();
		core.GetAllEntities(entities, true);

		SCR_EditableEntityComponent nearest;
		float nearestDistSq = float.MAX;
		vector nearestAt;
		foreach (SCR_EditableEntityComponent entity : entities)
		{
			if (!entity || entity.GetEntityType() != EEditableEntityType.COMMENT || !entity.HasEntityFlag(EEditableEntityFlag.LOCAL))
				continue;

			if (settlementOnly && !IsSettlementLabel(entity))
				continue;

			vector at;
			if (!entity.GetPos(at))
				continue;

			float distSq = vector.DistanceSqXZ(at, pos);
			if (distSq >= nearestDistSq)
				continue;

			nearestDistSq = distSq;
			nearest = entity;
			nearestAt = at;
		}

		if (!nearest)
			return null;

		if (vector.DistanceXZ(nearestAt, pos) > maxRadius)
			return null;

		return nearest;
	}
}