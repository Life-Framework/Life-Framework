//------------------------------------------------------------------------------------------------
//! An oriented box in world space: the clearance box of a candidate spot, or a door leaf's. Just
//! enough for the door check (#2445): the separating-axis depth two boxes overlap by, and a leaf's
//! swing zone. The same maths as the offline audit's obbPenetration and doorZone
//! (agent-box/place-audit.mjs), which stays the reference.
//------------------------------------------------------------------------------------------------
class LF_PlacementBox
{
	vector m_vCentre;
	//! Half extents along m_aAxes.
	vector m_vHalf;
	//! The three unit axes.
	ref array<vector> m_aAxes = {};

	//------------------------------------------------------------------------------------------------
	//! The box `mins`..`maxs` in `transform`'s frame (what a placement's footprint is).
	static LF_PlacementBox FromFrame(vector transform[4], vector mins, vector maxs)
	{
		LF_PlacementBox box = new LF_PlacementBox();
		box.m_aAxes.Insert(transform[0].Normalized());
		box.m_aAxes.Insert(transform[1].Normalized());
		box.m_aAxes.Insert(transform[2].Normalized());
		vector mid = (mins + maxs) * 0.5;
		box.m_vHalf = (maxs - mins) * 0.5;
		box.m_vCentre = transform[3] + box.m_aAxes[0] * mid[0] + box.m_aAxes[1] * mid[1] + box.m_aAxes[2] * mid[2];
		return box;
	}

	//------------------------------------------------------------------------------------------------
	//! An entity's local bounds through its world transform, scale applied once (the audit's OrientedBox).
	static LF_PlacementBox FromEntity(notnull IEntity entity)
	{
		vector lmin;
		vector lmax;
		entity.GetBounds(lmin, lmax);
		vector mat[4];
		entity.GetWorldTransform(mat);
		float scale = entity.GetScale();
		LF_PlacementBox box = new LF_PlacementBox();
		box.m_aAxes.Insert(mat[0].Normalized());
		box.m_aAxes.Insert(mat[1].Normalized());
		box.m_aAxes.Insert(mat[2].Normalized());
		vector mid = (lmin + lmax) * 0.5;
		box.m_vHalf = (lmax - lmin) * 0.5 * scale;
		box.m_vCentre = mat[3] + box.m_aAxes[0] * (mid[0] * scale) + box.m_aAxes[1] * (mid[1] * scale) + box.m_aAxes[2] * (mid[2] * scale);
		return box;
	}

	//------------------------------------------------------------------------------------------------
	//! How far this box and `other` overlap along the axis they overlap least on; negative when they
	//! are apart (the depth a door swing is judged by).
	float Penetration(notnull LF_PlacementBox other)
	{
		array<vector> axes = {};
		axes.InsertAll(m_aAxes);
		axes.InsertAll(other.m_aAxes);
		foreach (vector p : m_aAxes)
		{
			foreach (vector q : other.m_aAxes)
			{
				vector cross = LF_PlacementGeometry.Cross(p, q);
				float length = cross.Length();
				if (length > 0.0001)
					axes.Insert(cross / length);
			}
		}

		vector between = other.m_vCentre - m_vCentre;
		float best = 1000000;
		foreach (vector axis : axes)
		{
			float overlap = Radius(axis) + other.Radius(axis) - Math.AbsFloat(vector.Dot(between, axis));
			if (overlap < best)
				best = overlap;

			if (best < 0)
				return best;
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Half the box's extent along a unit `axis`.
	protected float Radius(vector axis)
	{
		return m_vHalf[0] * Math.AbsFloat(vector.Dot(m_aAxes[0], axis)) + m_vHalf[1] * Math.AbsFloat(vector.Dot(m_aAxes[1], axis)) + m_vHalf[2] * Math.AbsFloat(vector.Dot(m_aAxes[2], axis));
	}

	//------------------------------------------------------------------------------------------------
	//! This door leaf's zone: its thin side pushed out to `clear` metres, its height capped at 1.1 m
	//! half-extent. Null when the leaf does not stand upright (two horizontal axes needed).
	LF_PlacementBox DoorZone(float clear)
	{
		array<int> horizontal = {};
		int up = 1;
		for (int i = 0; i < 3; i++)
		{
			if (Math.AbsFloat(m_aAxes[i][1]) < 0.7)
				horizontal.Insert(i);
			else
				up = i;
		}

		if (horizontal.Count() < 2)
			return null;

		int thin = horizontal[1];
		if (m_vHalf[horizontal[0]] <= m_vHalf[horizontal[1]])
			thin = horizontal[0];

		LF_PlacementBox zone = new LF_PlacementBox();
		zone.m_vCentre = m_vCentre;
		zone.m_vHalf = m_vHalf;
		zone.m_aAxes.InsertAll(m_aAxes);
		zone.m_vHalf[thin] = Math.Max(zone.m_vHalf[thin], clear);
		zone.m_vHalf[up] = Math.Min(zone.m_vHalf[up], 1.1);
		return zone;
	}
}

//------------------------------------------------------------------------------------------------
//! Collects the door leaves a sphere query finds.
class LF_PlacementDoorQuery
{
	ref array<IEntity> m_aDoors = {};

	//------------------------------------------------------------------------------------------------
	bool Visit(IEntity entity)
	{
		if (entity && entity.FindComponent(DoorComponent))
			m_aDoors.Insert(entity);

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Server and client: whether a candidate spot stands in a doorway. A door leaf's swing (1 m both
//! ways of the leaf) and, for a ground-floor door, the 3 m walk-up in front of it are kept clear, so
//! a placed thing never walls a door in (#2445, the door half of #1029). The same rule the offline
//! audit applies to the town (`tools\cli place`, docs/placement-tool.md "in the swing of a door"),
//! run live from LF_PlacementRules.FindBlocker so the single, aimed and composition placements all
//! get it and the ghost turns red in a doorway.
//------------------------------------------------------------------------------------------------
class LF_PlacementDoors
{
	//! What the refusal calls the blocker: "a door is in the way".
	static const string REASON = "a door";

	static const float SWING_CLEAR_M = 1.0; // lf-fixed: the audit's LIMITS.doorClear, how far a leaf swings both ways
	static const float WALK_UP_M = 3; // lf-fixed: the audit's LIMITS.doorWalk, the way in front of a door
	static const float OVERLAP_M = 0.1; // lf-fixed: the audit's LIMITS.door, a graze is not in the way
	static const float GROUND_FLOOR_M = 1.2; // lf-fixed: a door whose foot is this near the ground is a street door
	static const float MIN_HEIGHT_M = 0.25; // lf-fixed: the audit steps over a lower thing (a kerb, a mat)
	static const float MAX_VOLUME_M3 = 40; // lf-fixed: the audit treats a bigger box as a building, not a prop

	//------------------------------------------------------------------------------------------------
	//! "a door" when the box `mins`..`maxs` in `transform`'s frame is in a door's swing or walk-up,
	//! empty when it is clear. `exclude` entities (the player, the thing being moved) are skipped.
	static string Find(vector transform[4], vector mins, vector maxs, array<IEntity> exclude)
	{
		vector size = maxs - mins;
		if (size[1] < MIN_HEIGHT_M * 2 || size[0] * size[1] * size[2] > MAX_VOLUME_M3)
			return "";

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return "";

		LF_PlacementBox box = LF_PlacementBox.FromFrame(transform, mins, maxs);
		LF_PlacementDoorQuery query = new LF_PlacementDoorQuery();
		world.QueryEntitiesBySphere(box.m_vCentre, box.m_vHalf.Length() + WALK_UP_M + 2, query.Visit, null, EQueryEntitiesFlags.ALL);
		foreach (IEntity door : query.m_aDoors)
		{
			if (exclude && exclude.Find(door) != -1)
				continue;

			if (Blocks(door, box))
				return REASON;
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Whether `box` is in `door`'s swing, or in its walk-up (a ground-floor door, from outside its own building).
	static bool Blocks(notnull IEntity door, notnull LF_PlacementBox box)
	{
		LF_PlacementBox leaf = LF_PlacementBox.FromEntity(door);
		LF_PlacementBox swing = leaf.DoorZone(SWING_CLEAR_M);
		if (!swing)
			return false;

		if (swing.Penetration(box) > OVERLAP_M)
			return true;

		float surfaceY = GetGame().GetWorld().GetSurfaceY(leaf.m_vCentre[0], leaf.m_vCentre[2]);
		float foot = leaf.m_vCentre[1] - leaf.m_vHalf[1];
		if (foot - surfaceY >= GROUND_FLOOR_M)
			return false;

		if (Indoors(door, box.m_vCentre))
			return false;

		return leaf.DoorZone(WALK_UP_M).Penetration(box) > OVERLAP_M;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether `point` is inside the footprint of the building the door belongs to (its root entity),
	//! where furniture stands behind the door rather than in front of it.
	protected static bool Indoors(notnull IEntity door, vector point)
	{
		IEntity root = door.GetRootParent();
		if (!root || root == door)
			return false;

		vector mins;
		vector maxs;
		root.GetWorldBounds(mins, maxs);
		return point[0] > mins[0] && point[0] < maxs[0] && point[2] > mins[2] && point[2] < maxs[2];
	}
}
