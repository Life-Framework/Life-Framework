// LF_ResidentAnchors.c - turns a timetable line (place kind + anchor id) into a world position and
// facing (#2857, epic #991 design step 3). A map maker drops a resident anchor (id `counter`, `bench`,
// `door`) inside a place; a system asks here by kind and anchor id and never by name or coordinate
// (rule 9). Server side. Entries hold EntityID, never IEntity.

//------------------------------------------------------------------------------------------------
//! Where a resident stands: a world position and the angles (yaw, pitch, roll in degrees) it faces.
class LF_ResidentSpot
{
	vector m_vOrigin;
	vector m_vAngles;
	//! True when the anchor was found; false when the place's own origin stood in for it.
	bool m_bAnchored;
}

//------------------------------------------------------------------------------------------------
class LF_ResidentAnchors
{
	static const string FEATURE = "Residents";
	//! An anchor not parented to its place belongs to the nearest place of the kind within this many metres.
	static const float OWN_RADIUS = 30;	// lf-fixed: how close a loose anchor must sit to count as inside a place

	protected static ref array<EntityID> s_aAnchors;
	protected static ref set<string> s_mWarned;
	protected static int s_iWarnCount;

	//------------------------------------------------------------------------------------------------
	//! A placed anchor announces itself. The same entity again is a no-op.
	static void Register(IEntity anchor)
	{
		if (!anchor)
			return;

		if (!s_aAnchors)
			s_aAnchors = {};

		if (s_aAnchors.Find(anchor.GetID()) < 0)
			s_aAnchors.Insert(anchor.GetID());
	}

	//------------------------------------------------------------------------------------------------
	//! Where a resident of a timetable line stands. The place is the nearest of `kind` to `near`. An
	//! empty anchor id is the place's own origin; an unknown anchor id falls back to it and logs one
	//! warning per kind and id pair.
	//! \return false only when no place of the kind exists (spot is then untouched).
	static bool Resolve(string kind, string anchorId, vector near, notnull LF_ResidentSpot spot)
	{
		IEntity place = NearestPlace(kind, near);
		if (!place)
		{
			WarnOnce(kind, anchorId, string.Format("no place of kind '%1' is placed", kind));
			return false;
		}

		IEntity anchor;
		if (!anchorId.IsEmpty())
		{
			anchor = FindAnchor(place, kind, anchorId);
			if (!anchor)
				WarnOnce(kind, anchorId, string.Format("place '%1' has no anchor '%2'; the resident stands at the place origin", kind, anchorId));
		}

		spot.m_bAnchored = anchor != null;
		if (!anchor)
			anchor = place;

		vector transform[4];
		anchor.GetWorldTransform(transform);
		spot.m_vOrigin = transform[3];
		spot.m_vAngles = Math3D.MatrixToAngles(transform);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! How many anchor warnings were logged since world start (tests read it).
	static int WarnCount()
	{
		return s_iWarnCount;
	}

	//------------------------------------------------------------------------------------------------
	//! World end: the next world places its own anchors.
	static void ResetStatics()
	{
		s_aAnchors = null;
		s_mWarned = null;
		s_iWarnCount = 0;
	}

	//------------------------------------------------------------------------------------------------
	protected static void WarnOnce(string kind, string anchorId, string message)
	{
		if (!s_mWarned)
			s_mWarned = new set<string>();

		string key = kind + "|" + anchorId;
		if (s_mWarned.Contains(key))
			return;

		s_mWarned.Insert(key);
		s_iWarnCount++;
		LF_Debug.Warn(FEATURE, message);
	}

	//------------------------------------------------------------------------------------------------
	protected static IEntity NearestPlace(string kind, vector near)
	{
		array<IEntity> places = {};
		LF_Places.FindAll(kind, places);
		IEntity best;
		float bestDist = 0;
		foreach (IEntity place : places)
		{
			float dist = vector.DistanceSq(place.GetOrigin(), near);
			if (!best || dist < bestDist)
			{
				best = place;
				bestDist = dist;
			}
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! The anchor `anchorId` that belongs to place: nested under it, else loose within OWN_RADIUS and
	//! closer to this place than to any other of the kind. A nested anchor beats a loose one, then the
	//! nearest wins.
	protected static IEntity FindAnchor(IEntity place, string kind, string anchorId)
	{
		if (!s_aAnchors || !GetGame() || !GetGame().GetWorld())
			return null;

		IEntity best;
		float bestDist = 0;
		bool bestNested;
		array<EntityID> stale = {};
		foreach (EntityID id : s_aAnchors)
		{
			IEntity anchor = GetGame().GetWorld().FindEntityByID(id);
			if (!anchor)
			{
				stale.Insert(id);
				continue;
			}

			LF_ResidentAnchorComponent comp = LF_ResidentAnchorComponent.Cast(anchor.FindComponent(LF_ResidentAnchorComponent));
			if (!comp || comp.GetAnchorId() != anchorId)
				continue;

			bool nested = IsUnder(anchor, place);
			float dist = vector.DistanceSq(anchor.GetOrigin(), place.GetOrigin());
			if (!nested)
			{
				if (dist > OWN_RADIUS * OWN_RADIUS)
					continue;

				IEntity owner = NearestPlace(kind, anchor.GetOrigin());
				if (owner != place)
					continue;
			}

			if (!best || (nested && !bestNested) || (nested == bestNested && dist < bestDist))
			{
				best = anchor;
				bestDist = dist;
				bestNested = nested;
			}
		}

		foreach (EntityID gone : stale)
			s_aAnchors.RemoveItem(gone);

		return best;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsUnder(IEntity entity, IEntity ancestor)
	{
		IEntity parent = entity.GetParent();
		while (parent)
		{
			if (parent == ancestor)
				return true;

			parent = parent.GetParent();
		}

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of the resident anchor statics (LF_StaticReset finds it by reflection).
class LF_ResidentAnchors_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_ResidentAnchors";
	}

	override void Apply()
	{
		LF_ResidentAnchors.ResetStatics();
	}
}
