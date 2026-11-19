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
		bool found;
		array<vector> candidates = {};
		SitCandidates(mins, maxs, standOffset, candidates);
		foreach (vector candidate : candidates)
		{
			bool probed;
			feet = OnFloor(bed, bedTransform[3] + candidate.Multiply3(bedTransform), null, probed);
			if (probed && BodyFits(feet, bed))
			{
				local = candidate;
				found = true;
				break;
			}
		}

		if (!found)
		{
			bool fallbackProbed;
			feet = OnFloor(bed, bedTransform[3] + preferred.Multiply3(bedTransform), null, fallbackProbed);
			LF_Debug.Warn("Respawn", string.Format("no room for a body beside bed %1 at %2; it wakes at %3 anyway", bed.GetName(), bed.GetOrigin(), feet));
		}

		vector outward = bedTransform[0];
		if (local[0] < 0)
			outward = -outward;
		outward[1] = 0;
		outward.Normalize();

		Math3D.DirectionAndUpMatrix(-outward, "0 1 0", transform);
		transform[3] = feet;
	}

	//------------------------------------------------------------------------------------------------
	//! Bed space: sit points to try, the stand offset's side first, each side from the preferred
	//! place along the bed outward in SIT_STEP steps, kept off both ends.
	static void SitCandidates(vector mins, vector maxs, vector standOffset, notnull array<vector> outCandidates)
	{
		outCandidates.Clear();
		vector preferred = SitLocal(mins, maxs, standOffset);
		float low = mins[2] + END_MARGIN;
		float high = maxs[2] - END_MARGIN;
		array<float> sides = {preferred[0], 0};
		if (preferred[0] >= 0)
			sides[1] = mins[0] - SIT_OUT;
		else
			sides[1] = maxs[0] + SIT_OUT;

		foreach (float x : sides)
		{
			outCandidates.Insert(Vector(x, 0, preferred[2]));
			for (int step = 1; step <= SIT_STEPS; step++)
			{
				float ahead = preferred[2] + step * SIT_STEP;
				float behind = preferred[2] - step * SIT_STEP;
				if (ahead <= high)
					outCandidates.Insert(Vector(x, 0, ahead));
				if (behind >= low)
					outCandidates.Insert(Vector(x, 0, behind));
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! True when a standing body with its feet at `feet` touches nothing but the floor and the bed.
	static bool BodyFits(vector feet, IEntity bed)
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
		return world.TracePosition(box, null) >= 0;
	}

	//------------------------------------------------------------------------------------------------
	//! What the room test sees through: the bed and every player's body. A player standing at a bed
	//! (or the owner's own body, already at it) must not move the wake point; occupancy is the bed
	//! pick's job.
	protected static array<IEntity> RoomExclusions(IEntity bed)
	{
		array<IEntity> exclude = {bed};
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
	//! A bed-space point beside the bed moved to the floor under it.
	static vector StandOnFloor(IEntity bed, vector standOffset, IEntity ignore = null)
	{
		vector bedTransform[4];
		bed.GetWorldTransform(bedTransform);
		bool probed;
		return OnFloor(bed, bedTransform[3] + standOffset.Multiply3(bedTransform), ignore, probed);
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
