//------------------------------------------------------------------------------------------------
//! Where the ore that breaks off a rock lies. Pure arithmetic apart from the world's footprint reading,
//! so a LOGIC test drives it (`mining/ore-drop-spot`). The spot is a ground position beside the rock,
//! outside its footprint box, in a fan around the side the miner stands on; the height comes from the
//! shared runtime probe (LF_PlaceSnap.Settle), never from math here.
class LF_OreDrop
{
	static const string FEATURE = "Mining";

	//! A piece lies this far (metres) beyond the rock's footprint edge: clear of the mesh and in reach of a pickup.
	static const float MARGIN = 0.3;
	//! Closest two pieces of one rock may lie to each other (metres, on the ground plane).
	static const float SPACING = 0.35;
	//! Angle between neighbouring fan slots, radians (about 34 degrees).
	static const float FAN_STEP = 0.6;
	//! Slots in the fan before it wraps to the first one again: 0, then three steps to either side (103 degrees),
	//! so no piece lies behind the rock from the miner.
	static const int SLOTS = 7;
	//! A spot pushed out for spacing moves this far each try (metres), at most PUSH_TRIES times.
	protected static const float PUSH = 0.1;
	protected static const int PUSH_TRIES = 30;

	//------------------------------------------------------------------------------------------------
	//! Distance from the box centre to its edge along the unit direction (dirX, dirZ).
	static float ExitDistance(float halfX, float halfZ, float dirX, float dirZ)
	{
		float tx = 1e9;
		float tz = 1e9;
		if (Math.AbsFloat(dirX) > 0.0001)
			tx = halfX / Math.AbsFloat(dirX);

		if (Math.AbsFloat(dirZ) > 0.0001)
			tz = halfZ / Math.AbsFloat(dirZ);

		return Math.Min(tx, tz);
	}

	//------------------------------------------------------------------------------------------------
	//! True when an offset from the box centre lies clear of the box grown by `margin`.
	static bool OutsideBox(float offX, float offZ, float halfX, float halfZ, float margin)
	{
		return Math.AbsFloat(offX) >= halfX + margin - 0.0005 || Math.AbsFloat(offZ) >= halfZ + margin - 0.0005;
	}

	//------------------------------------------------------------------------------------------------
	//! The offset (x, 0, z) from the rock's centre where fan slot `slot` lies. Slot 0 is straight toward the
	//! miner (`away`, horizontal), then alternating to either side. Each slot lies on the edge of the box grown
	//! by `margin`, along its ray, pushed outward until it is `spacing` from every lower slot, so a rock mined
	//! from one spot never stacks its pieces. The same inputs give the same spot.
	static vector Spot(float halfX, float halfZ, vector away, int slot, float margin = MARGIN, float spacing = SPACING)
	{
		away[1] = 0;
		if (away.LengthSq() < 0.0001)
			away = vector.Forward;
		away.Normalize();

		slot = slot % SLOTS;
		float baseAngle = Math.Atan2(away[0], away[2]);
		array<vector> placed = {};
		vector spot = vector.Zero;
		for (int s = 0; s <= slot; s++)
		{
			int ring = (s + 1) / 2;
			float angle = baseAngle;
			if (s % 2 == 1)
				angle = baseAngle + ring * FAN_STEP;
			else if (s > 0)
				angle = baseAngle - ring * FAN_STEP;

			float dx = Math.Sin(angle);
			float dz = Math.Cos(angle);
			// Where the ray from the centre leaves the box grown by the margin: clear of the mesh on every side.
			float dist = ExitDistance(halfX + margin, halfZ + margin, dx, dz);
			spot = Vector(dx * dist, 0, dz * dist);
			for (int tries = 0; tries < PUSH_TRIES && TooClose(spot, placed, spacing); tries++)
			{
				dist += PUSH;
				spot = Vector(dx * dist, 0, dz * dist);
			}

			placed.Insert(spot);
		}

		return spot;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool TooClose(vector spot, array<vector> placed, float spacing)
	{
		foreach (vector other : placed)
		{
			if (vector.Distance(spot, other) < spacing)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Half extents on the ground plane of the world box round an entity's local bounds: the eight corners
	//! through its own transform (scale, yaw, tilt), so the box covers the mesh however the rock stands.
	//! \param[out] centre The box centre in world space.
	//! \param[out] halfX Half the box's x extent. \param[out] halfZ Half its z extent.
	static void Footprint(notnull IEntity rock, out vector centre, out float halfX, out float halfZ)
	{
		vector mins;
		vector maxs;
		rock.GetBounds(mins, maxs);
		vector lo = Vector(1e9, 1e9, 1e9);
		vector hi = Vector(-1e9, -1e9, -1e9);
		for (int i = 0; i < 8; i++)
		{
			vector corner = Vector(mins[0], mins[1], mins[2]);
			if (i & 1)
				corner[0] = maxs[0];

			if (i & 2)
				corner[1] = maxs[1];

			if (i & 4)
				corner[2] = maxs[2];

			vector world = rock.CoordToParent(corner);
			for (int a = 0; a < 3; a++)
			{
				if (world[a] < lo[a])
					lo[a] = world[a];

				if (world[a] > hi[a])
					hi[a] = world[a];
			}
		}

		centre = (lo + hi) * 0.5;
		halfX = (hi[0] - lo[0]) * 0.5;
		halfZ = (hi[2] - lo[2]) * 0.5;
	}
}
