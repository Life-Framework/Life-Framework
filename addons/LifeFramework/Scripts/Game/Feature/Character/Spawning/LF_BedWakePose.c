// LF_BedWakePose.c — where a body that wakes in a bed is put, for the first-spawn beds (#1965) and
// the hospital beds. The wake plays the base game's bed wake loiter (type 7, the tutorial intro's):
// the body sits on the bed with its feet on the floor, then stands up beside it. The animation
// expects the character origin on the floor beside the bed, facing the mattress: the sitting hips
// land about 0.7 m in front of the feet and 0.6 m up. A body placed on the bed origin instead was
// pushed up onto the mattress by physics, so the whole pose played half a metre too high: sitting,
// then standing, in the air above the bed (measured on the island hostel beds, 2026-11-19).

class LF_BedWakePose
{
	//! Metres past the mattress side where the feet stand: a character capsule clear of the bed (0.2
	//! overlapped it and physics popped the body onto the mattress corner), and the sitting hips,
	//! 0.7 m in front of the feet, still land on the mattress.
	static const float SIT_OUT = 0.4;
	//! The sit point stays this far from either end of the bed.
	static const float END_MARGIN = 0.45;
	//! A floor probe searches from this far above the bed origin (a bed stands on its floor, so a
	//! higher start found desk and windowsill tops beside the bed: a body woke on them, 0.9 m up)...
	static const float FLOOR_SEARCH_UP = 0.3;
	//! ... down to this far below it.
	static const float FLOOR_SEARCH = 1.0;
	//! A body this close above the floor stands on it.
	static const float ON_FLOOR = 0.1;
	//! Sit points tried along each side of the bed: this many steps of SIT_STEP metres each way.
	static const int SIT_STEPS = 4;
	static const float SIT_STEP = 0.2;
	//! Wider rings tried after the nearest spots (a wall, desk or second bed beside this one).
	static const int RING_COUNT = 3;
	static const float RING_STEP = 0.35;
	//! A patient laid on the bed lies this far above the mattress top; the probe starts this far above it.
	static const float LAY_CLEARANCE = 0.03;
	static const float LAY_PROBE_UP = 0.5;
	//! The room a standing body needs (metres): half its width, its height, and the gap above the
	//! floor the room test starts at, so the floor itself never counts as in the way.
	static const float BODY_HALF_WIDTH = 0.3;
	static const float BODY_HEIGHT = 1.7;
	static const float FEET_CLEARANCE = 0.15;

	//------------------------------------------------------------------------------------------------
	//! Bed space: the feet of the sitting body, on the bed side the stand offset points to.
	//! \param mins Bed local bounds minimum.
	//! \param maxs Bed local bounds maximum.
	//! \param standOffset The bed's stand offset (bed space): its X sign picks the side, its Z the spot along the bed.
	static vector SitLocal(vector mins, vector maxs, vector standOffset)
	{
		float x = mins[0] - SIT_OUT;
		if (standOffset[0] >= 0)
			x = maxs[0] + SIT_OUT;

		float low = mins[2] + END_MARGIN;
		float high = maxs[2] - END_MARGIN;
		float z = (mins[2] + maxs[2]) * 0.5;
		if (low <= high)
			z = Math.Clamp(standOffset[2], low, high);

		return Vector(x, 0, z);
	}

	//------------------------------------------------------------------------------------------------
	//! World transform of the waking body: feet on the probed floor at the sit point, facing the
	//! bed (the bed wake animation sits forward onto the mattress).
	static void SitTransform(IEntity bed, vector standOffset, out vector transform[4])
	{
		vector bedTransform[4];
		bed.GetWorldTransform(bedTransform);
		vector mins, maxs;
		bed.GetBounds(mins, maxs);

		// The stand offset's side first, then the other; on each side the spot nearest the stand
		// offset's place along the bed where a body fits (a wall or furniture beside the bed pushed
		// a body spawned into it toward the foot end, and the hips landed past the mattress).
		vector preferred = SitLocal(mins, maxs, standOffset);
		vector local = preferred;
		vector feet;
		bool found = FindSitSpot(bed, standOffset, local, feet);
		if (!found)
		{
			bool fallbackProbed;
			feet = OnFloor(bed, bedTransform[3] + preferred.Multiply3(bedTransform), null, fallbackProbed);
			LF_Debug.Warn("Respawn", string.Format("no room for a body beside bed %1 (%2) at %3; it wakes at %4 anyway", bed.GetName(), PrefabOf(bed), bed.GetOrigin(), feet));
		}

		vector outward = OutwardLocal(mins, maxs, local).Multiply3(bedTransform);
		outward[1] = 0;
		outward.Normalize();

		Math3D.DirectionAndUpMatrix(-outward, "0 1 0", transform);
		transform[3] = feet;
	}

	//------------------------------------------------------------------------------------------------
	//! The first sit point (SitCandidates order) on the floor with room for a standing body.
	//! \param local The chosen point in bed space.
	//! \param feet The chosen point in the world, probed to the floor.
	//! \return False when none has room (local and feet are then untouched).
	static bool FindSitSpot(IEntity bed, vector standOffset, out vector local, out vector feet)
	{
		vector bedTransform[4];
		bed.GetWorldTransform(bedTransform);
		vector mins, maxs;
		bed.GetBounds(mins, maxs);

		array<vector> candidates = {};
		SitCandidates(mins, maxs, standOffset, candidates);
		foreach (vector candidate : candidates)
		{
			bool probed;
			vector probe = OnFloor(bed, bedTransform[3] + candidate.Multiply3(bedTransform), null, probed);
			if (probed && BodyFits(probe, bed))
			{
				local = candidate;
				feet = probe;
				return true;
			}
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Bed space: sit points to try, nearest first. Both long sides (the stand offset's first), each
	//! from the preferred place along the bed outward in SIT_STEP steps and kept off both ends; then
	//! the foot and the head end; then the same again RING_STEP further out, RING_COUNT times, for a
	//! bed with a wall, a desk or a second bed close beside it (#5153). Never a point whose body
	//! overlaps the bed (InsideBed).
	static void SitCandidates(vector mins, vector maxs, vector standOffset, notnull array<vector> outCandidates)
	{
		outCandidates.Clear();
		vector preferred = SitLocal(mins, maxs, standOffset);
		float low = mins[2] + END_MARGIN;
		float high = maxs[2] - END_MARGIN;
		float otherSide = maxs[0] + SIT_OUT;
		if (preferred[0] >= 0)
			otherSide = mins[0] - SIT_OUT;

		float middle = (mins[0] + maxs[0]) * 0.5;
		for (int ring = 0; ring <= RING_COUNT; ring++)
		{
			float extra = ring * RING_STEP;
			array<float> sides = {preferred[0], otherSide};
			foreach (float side : sides)
			{
				float x = side + extra;
				if (side < middle)
					x = side - extra;

				AddCandidate(outCandidates, mins, maxs, Vector(x, 0, preferred[2]));
				for (int step = 1; step <= SIT_STEPS; step++)
				{
					float ahead = preferred[2] + step * SIT_STEP;
					float behind = preferred[2] - step * SIT_STEP;
					if (ahead <= high)
						AddCandidate(outCandidates, mins, maxs, Vector(x, 0, ahead));
					if (behind >= low)
						AddCandidate(outCandidates, mins, maxs, Vector(x, 0, behind));
				}
			}

			AddCandidate(outCandidates, mins, maxs, Vector(middle, 0, mins[2] - SIT_OUT - extra));
			AddCandidate(outCandidates, mins, maxs, Vector(middle, 0, maxs[2] + SIT_OUT + extra));
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void AddCandidate(notnull array<vector> candidates, vector mins, vector maxs, vector local)
	{
		if (!InsideBed(mins, maxs, local))
			candidates.Insert(local);
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: a standing body at the bed-space point overlaps the bed's bounds (x/z footprint).
	static bool InsideBed(vector mins, vector maxs, vector local)
	{
		return local[0] > mins[0] - BODY_HALF_WIDTH && local[0] < maxs[0] + BODY_HALF_WIDTH && local[2] > mins[2] - BODY_HALF_WIDTH && local[2] < maxs[2] + BODY_HALF_WIDTH;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: bed-space direction from the bed toward a sit point, along the axis it lies off the bed
	//! on (the waking body faces the other way, onto the mattress).
	static vector OutwardLocal(vector mins, vector maxs, vector local)
	{
		if (local[0] > maxs[0])
			return "1 0 0";

		if (local[0] < mins[0])
			return "-1 0 0";

		if (local[2] > maxs[2])
			return "0 0 1";

		return "0 0 -1";
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the first point whose body footprint touches none of the blocking boxes (bed space, x/z).
	//! \return Index into candidates, -1 when every point is blocked.
	static int FirstClear(notnull array<vector> candidates, notnull array<vector> blockMins, notnull array<vector> blockMaxs)
	{
		for (int i = 0; i < candidates.Count(); i++)
		{
			bool blocked = false;
			for (int b = 0; b < blockMins.Count() && b < blockMaxs.Count(); b++)
			{
				vector c = candidates[i];
				if (c[0] + BODY_HALF_WIDTH > blockMins[b][0] && c[0] - BODY_HALF_WIDTH < blockMaxs[b][0] && c[2] + BODY_HALF_WIDTH > blockMins[b][2] && c[2] - BODY_HALF_WIDTH < blockMaxs[b][2])
				{
					blocked = true;
					break;
				}
			}

			if (!blocked)
				return i;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure, bed space: where a patient laid on the bed lies: the middle of the bed's bounds, just
	//! above the top (bed.GetBounds). #5153: the old lay point was the bed origin, on the floor under the
	//! frame.
	static vector LayLocal(vector mins, vector maxs)
	{
		return Vector((mins[0] + maxs[0]) * 0.5, maxs[1] + LAY_CLEARANCE, (mins[2] + maxs[2]) * 0.5);
	}

	//------------------------------------------------------------------------------------------------
	//! World point on the mattress top: LayLocal, with the height taken from a probe down onto the bed
	//! itself when it hits the bed (a lower mattress under a taller headboard), else from the bounds.
	static vector LayPoint(IEntity bed)
	{
		vector bedTransform[4];
		bed.GetWorldTransform(bedTransform);
		vector mins, maxs;
		bed.GetBounds(mins, maxs);
		vector local = LayLocal(mins, maxs);
		vector point = bedTransform[3] + local.Multiply3(bedTransform);

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return point;

		TraceParam trace = new TraceParam();
		trace.Start = point + Vector(0, LAY_PROBE_UP, 0);
		trace.End = point - Vector(0, LAY_PROBE_UP + maxs[1], 0);
		trace.Flags = TraceFlags.ENTS;
		trace.LayerMask = EPhysicsLayerPresets.Projectile;
		float fraction = world.TraceMove(trace, null);
		if (fraction < 1 && trace.TraceEnt == bed)
		{
			float top = trace.Start[1] - fraction * (trace.Start[1] - trace.End[1]);
			if (top < point[1] + LAY_PROBE_UP && top > bedTransform[3][1])
				point[1] = top + LAY_CLEARANCE;
		}

		return point;
	}

	//------------------------------------------------------------------------------------------------
	//! A prefab name for a log line.
	static string PrefabOf(IEntity entity)
	{
		if (!entity)
			return "";

		EntityPrefabData data = entity.GetPrefabData();
		if (!data)
			return "no prefab";

		return data.GetPrefabName();
	}

	//------------------------------------------------------------------------------------------------
	//! The floor spot for a woken patient to step down to: the stand offset's point when a body fits
	//! there, else the first sit spot with room (both sides, the foot, wider rings). A bed with no room
	//! at all logs a warning with its prefab and gives false: the patient stays on the mattress rather
	//! than being put somewhere under or inside the frame.
	static bool FindStandSpot(IEntity bed, vector standOffset, IEntity ignore, out vector spot)
	{
		vector bedTransform[4];
		bed.GetWorldTransform(bedTransform);
		bool probed;
		vector feet = OnFloor(bed, bedTransform[3] + standOffset.Multiply3(bedTransform), ignore, probed);
		if (probed && BodyFits(feet, bed))
		{
			spot = feet;
			return true;
		}

		vector local;
		if (FindSitSpot(bed, standOffset, local, spot))
			return true;

		LF_Debug.Warn("Respawn", string.Format("no room for a body beside bed %1 (%2) at %3: the patient stays on the mattress", bed.GetName(), PrefabOf(bed), bed.GetOrigin()));
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! True when a standing body with its feet at the point touches nothing but the floor and the bed (null: no bed, a plain spawn point).
	static bool BodyFits(vector feet, IEntity bed)
	{
		IEntity blocker;
		return BodyFitsBy(feet, bed, blocker);
	}

	//------------------------------------------------------------------------------------------------
	//! BodyFits, and what is in the way when it does not fit (the entity the box found, null when the
	//! engine reports none).
	static bool BodyFitsBy(vector feet, IEntity bed, out IEntity blocker)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return true;

		TraceBox box = new TraceBox();
		box.Start = feet;
		box.Mins = Vector(-BODY_HALF_WIDTH, FEET_CLEARANCE, -BODY_HALF_WIDTH);
		box.Maxs = Vector(BODY_HALF_WIDTH, BODY_HEIGHT, BODY_HALF_WIDTH);
		box.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		box.LayerMask = EPhysicsLayerPresets.Projectile;
		box.ExcludeArray = RoomExclusions(bed);
		float fraction = world.TracePosition(box, null);
		blocker = box.TraceEnt;
		return fraction >= 0;
	}

	//------------------------------------------------------------------------------------------------
	//! What the room test sees through: the bed and every player's body. A player standing at a bed
	//! (or the owner's own body, already at it) must not move the wake point; occupancy is the bed
	//! pick's job.
	protected static array<IEntity> RoomExclusions(IEntity bed)
	{
		array<IEntity> exclude = {};
		if (bed)
			exclude.Insert(bed);

		IEntity local = LF_Utils.GetLocalCharacter();
		if (local)
			exclude.Insert(local);

		PlayerManager players = GetGame().GetPlayerManager();
		if (!players)
			return exclude;

		array<int> ids = {};
		players.GetPlayers(ids);
		foreach (int id : ids)
		{
			IEntity body = players.GetPlayerControlledEntity(id);
			if (body && exclude.Find(body) < 0)
				exclude.Insert(body);
		}

		return exclude;
	}

	//------------------------------------------------------------------------------------------------
	//! A bed-space point beside the bed moved to the floor under it. A point with no room for a standing
	//! body (a stand offset only 0.27 m off the mattress overlapped the bed by 3 cm, and a nightstand beside
	//! it did worse: #3912) gives way to the wake point, which the sit search already picked with room.
	static vector StandOnFloor(IEntity bed, vector standOffset, IEntity ignore = null)
	{
		vector bedTransform[4];
		bed.GetWorldTransform(bedTransform);
		bool probed;
		vector feet = OnFloor(bed, bedTransform[3] + standOffset.Multiply3(bedTransform), ignore, probed);
		if (!probed || BodyFits(feet, bed))
			return feet;

		vector sit[4];
		SitTransform(bed, standOffset, sit);
		return sit[3];
	}

	//------------------------------------------------------------------------------------------------
	//! The point moved straight down (or up) to the first floor near the bed's height that is not
	//! the bed itself or a character. With no floor found the point is returned unchanged and the
	//! miss is logged: a bed placed off any floor is a placement bug, not a reason to fail the wake.
	static vector OnFloor(IEntity bed, vector point, IEntity ignore, out bool probed)
	{
		probed = false;
		BaseWorld world = GetGame().GetWorld();
		if (!world || !bed)
			return point;

		float top = bed.GetOrigin()[1] + FLOOR_SEARCH_UP;
		float bottom = bed.GetOrigin()[1] - FLOOR_SEARCH;
		array<IEntity> exclude = {bed};
		if (ignore)
			exclude.Insert(ignore);

		// A character in the way is seen through and the trace starts again from the top: starting
		// again just under its feet can start below the floor and find the slab underneath.
		for (int i = 0; i < 4; i++)
		{
			TraceParam trace = new TraceParam();
			trace.Start = Vector(point[0], top, point[2]);
			trace.End = Vector(point[0], bottom, point[2]);
			trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
			trace.LayerMask = EPhysicsLayerPresets.Projectile;
			trace.ExcludeArray = exclude;
			float fraction = world.TraceMove(trace, null);
			if (fraction >= 1)
				break;

			IEntity hit = trace.TraceEnt;
			if (!ChimeraCharacter.Cast(hit))
			{
				probed = true;
				return Vector(point[0], top - fraction * (top - bottom), point[2]);
			}

			exclude.Insert(hit);
		}

		LF_Debug.Warn("Respawn", string.Format("no floor under %1 near bed %2; the wake keeps the bed height", point, bed.GetName()));
		return point;
	}
}
