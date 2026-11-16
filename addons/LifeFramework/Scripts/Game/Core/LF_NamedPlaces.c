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
	//! \return The editable location entity nearest to pos within maxRadius, or null.
	protected static SCR_EditableEntityComponent FindNearest(vector pos, float maxRadius)
	{
		SCR_EditableEntityCore core = SCR_EditableEntityCore.Cast(SCR_EditableEntityCore.GetInstance(SCR_EditableEntityCore));
		if (!core)
		{
			LF_Debug.Warn(FEATURE, "no editable entity core in this world; no place names");
			return null;
		}

		SCR_EditableEntityComponent nearest = core.FindNearestEntity(pos, EEditableEntityType.COMMENT, EEditableEntityFlag.LOCAL);
		if (!nearest)
			return null;

		vector at;
		if (!nearest.GetPos(at))
			return null;

		if (vector.DistanceXZ(at, pos) > maxRadius)
			return null;

		return nearest;
	}
}