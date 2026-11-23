//------------------------------------------------------------------------------------------------
//! Where a bus sets a passenger down (server). The stop's arrival spot first, then rings round it,
//! until one is a standing spot for a character (docs/architecture/placement-system.md "Characters"):
//! ground found by the shared snap trace (LF_PlaceSnap.Down: never a character, an item or a tree),
//! within a step of the wanted height (not a shelter roof), off the road, a 0.35 m capsule free of
//! props and other characters, with head room. The feet go on the ground, never a raised origin.
//------------------------------------------------------------------------------------------------
class LF_BusArrival
{
	//! Rings searched round the wanted spot, metres. The first ring is the spot itself.
	static const float RING_STEP = 0.75;	// lf-fixed: half a stride, so a sign or a bin beside the spot is stepped round
	static const int RINGS = 4;	// lf-fixed: out to 2.25 m, still "at the stop"
	static const int DIRECTIONS = 8;	// lf-fixed: compass points per ring
	//! The probe starts this far over the wanted height and reaches this far down.
	static const float LIFT = 1.2;	// lf-fixed: under a shelter roof (about 2.4 m), over a kerb or a slope
	static const float REACH = 3.5;	// lf-fixed: LIFT plus a slope of a couple of metres
	//! A support this far off the wanted height is a roof, a wall top or a ditch, not the pavement.
	static const float MAX_STEP = 1.0;	// lf-fixed: a kerb or a slope, never a roof
	//! A standing character (placement-system.md "Characters").
	static const float CAPSULE_RADIUS = 0.35;	// lf-fixed: mirrors the placement rule for a character spot
	static const float HEAD_ROOM = 1.8;	// lf-fixed: a standing character's height
	//! Feet rest this far above the support (within the 5 cm of the placement rule).
	static const float FEET_CLEARANCE = 0.02;	// lf-fixed: no z-fight, no visible float
	//! The line of sight from the sign to a spot runs at this height (over a kerb, under a shelter roof).
	static const float SIGHT_HEIGHT = 1.0;	// lf-fixed: chest height
	//! A spot handed out stays taken this long (the passenger may still be fading in, or the dedicated
	//! server may not see their client-made move yet), so two passengers on one bus never share it.
	static const float RESERVE_SECONDS = 15;	// lf-fixed: longer than a fade plus a move round trip
	//! The arrival spot in the sign frame when the stop has no component: beside the sign, face side.
	static const vector DEFAULT_ARRIVAL = "1.0 0 0.6";	// lf-fixed: mirrors LF_BusStopComponent m_vArrival

	protected static ref array<vector> s_aReserved;
	protected static ref array<float> s_aReservedAt;

	protected static IEntity s_QuerySelf;
	protected static vector s_vQueryCentre;
	protected static bool s_bQueryBlocked;

	//------------------------------------------------------------------------------------------------
	//! Pure: the spots to try, nearest first: the wanted spot, then each ring's compass points.
	static void Candidates(vector wanted, notnull array<vector> into)
	{
		into.Insert(wanted);
		for (int ring = 1; ring < RINGS; ring++)
		{
			float radius = ring * RING_STEP;
			for (int a = 0; a < DIRECTIONS; a++)
			{
				float rad = a * Math.PI2 / DIRECTIONS;
				into.Insert(wanted + Vector(Math.Cos(rad) * radius, 0, Math.Sin(rad) * radius));
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the yaw in degrees (0 looks along +Z) a passenger faces on arrival: the way the sign faces,
	//! out to the open street (the stop stands with its back to a wall; re-review N6 had the passenger
	//! set down staring at it).
	static float FacingYaw(vector signForward)
	{
		signForward[1] = 0;
		if (signForward.LengthSq() < 0.0001)
			return 0;

		return Math.Atan2(signForward[0], signForward[2]) * Math.RAD2DEG;
	}

	//------------------------------------------------------------------------------------------------
	//! The yaw a passenger set down at this stop faces.
	static float FacingAt(notnull IEntity stop)
	{
		vector mat[4];
		stop.GetWorldTransform(mat);
		return FacingYaw(mat[2]);
	}

	//------------------------------------------------------------------------------------------------
	//! The wanted arrival spot of a stop entity: its component's spot, else 1.5 m behind the sign.
	static vector WantedAt(notnull IEntity stop)
	{
		LF_BusStopComponent component = LF_BusStopComponent.Of(stop);
		if (component)
			return component.GetArrivalWanted();

		return stop.CoordToParent(DEFAULT_ARRIVAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a clear standing spot at the stop for `passenger`, feet on the ground. False (logged)
	//! when the stop is gone or nothing round it is clear.
	static bool Find(IEntity stop, IEntity passenger, out vector spot)
	{
		if (!stop || !GetGame() || !GetGame().GetWorld())
		{
			LF_Debug.Error(LF_BusStop.FEATURE, "bus arrival: the destination stop is not placed");
			return false;
		}

		vector wanted = WantedAt(stop);
		// What the passenger must be able to walk to: seen from the sign at chest height, so a spot behind
		// the wall the sign stands on, inside a house or past a fence is never taken.
		vector eye = stop.GetOrigin() + Vector(0, SIGHT_HEIGHT, 0);
		float now = LF_CharacterSwitch.Now();
		array<vector> candidates = {};
		Candidates(wanted, candidates);
		foreach (int i, vector candidate : candidates)
		{
			float ground;
			if (!GroundAt(passenger, candidate, ground))
				continue;

			vector feet = Vector(candidate[0], ground + FEET_CLEARANCE, candidate[2]);
			if (LF_PlacementZones.IsOnRoad(feet))
				continue;

			if (IsReserved(feet, now) || !InSight(stop, passenger, eye, feet))
				continue;

			if (!IsClear(passenger, feet))
				continue;

			Reserve(feet, now);
			spot = feet;
			if (i > 0)
				LF_Debug.Info(LF_BusStop.FEATURE, string.Format("bus arrival: wanted spot %1 blocked, set down %2 m away", wanted, Math.Round(vector.DistanceXZ(wanted, feet) * 100) / 100));

			return true;
		}

		LF_Debug.Error(LF_BusStop.FEATURE, string.Format("bus arrival: no clear ground within %1 m of %2", (RINGS - 1) * RING_STEP, wanted));
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The support under (x, z) near the wanted height: the higher of the projectile trace and the
	//! every-layer trace (LF_PlaceSnap.Measure's rule). False with no support or one a step away.
	static bool GroundAt(IEntity self, vector wanted, out float ground)
	{
		vector from = wanted + Vector(0, LIFT, 0);
		IEntity hitA;
		IEntity hitB;
		float a = LF_PlaceSnap.Down(self, from, REACH, EPhysicsLayerPresets.Projectile, hitA);
		float b = LF_PlaceSnap.Down(self, from, REACH, -1, hitB);
		float y = a;
		if (b > y)
			y = b;

		if (y < -1e8)
			return false;

		if (Math.AbsFloat(y - wanted[1]) > MAX_STEP)
			return false;

		ground = y;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Nothing solid between the sign at chest height and the spot at chest height.
	static bool InSight(IEntity stop, IEntity passenger, vector eye, vector feet)
	{
		TraceParam trace = new TraceParam();
		trace.Start = eye;
		trace.End = feet + Vector(0, SIGHT_HEIGHT, 0);
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		array<IEntity> skip = {};
		if (stop)
			skip.Insert(stop);
		if (passenger)
			skip.Insert(passenger);
		trace.ExcludeArray = skip;
		trace.LayerMask = EPhysicsLayerPresets.Projectile;
		return GetGame().GetWorld().TraceMove(trace, null) >= 1;
	}

	//------------------------------------------------------------------------------------------------
	//! A spot within two capsule radii of one handed out lately (expired ones are dropped).
	static bool IsReserved(vector feet, float now)
	{
		if (!s_aReserved)
			return false;

		for (int i = s_aReserved.Count() - 1; i >= 0; i--)
		{
			if (now - s_aReservedAt[i] > RESERVE_SECONDS || now < s_aReservedAt[i])
			{
				s_aReserved.Remove(i);
				s_aReservedAt.Remove(i);
				continue;
			}

			if (vector.DistanceXZ(s_aReserved[i], feet) < CAPSULE_RADIUS * 2)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	static void Reserve(vector feet, float now)
	{
		if (!s_aReserved)
		{
			s_aReserved = {};
			s_aReservedAt = {};
		}

		s_aReserved.Insert(feet);
		s_aReservedAt.Insert(now);
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aReserved = null;
		s_aReservedAt = null;
		s_QuerySelf = null;
	}

	//------------------------------------------------------------------------------------------------
	//! A standing capsule at feet holds no other character and no prop, and has head room.
	static bool IsClear(IEntity self, vector feet)
	{
		s_QuerySelf = self;
		s_vQueryCentre = feet;
		s_bQueryBlocked = false;
		GetGame().GetWorld().QueryEntitiesBySphere(feet + "0 0.9 0", CAPSULE_RADIUS * 2 + 0.3, QueryHit, null, EQueryEntitiesFlags.ALL);
		if (s_bQueryBlocked)
			return false;

		array<float> heights = {0.4, 1.0, 1.6};
		foreach (float height : heights)
		{
			for (int a = 0; a < DIRECTIONS; a++)
			{
				float rad = a * Math.PI2 / DIRECTIONS;
				vector from = feet + Vector(0, height, 0);
				vector to = from + Vector(Math.Cos(rad), 0, Math.Sin(rad)) * CAPSULE_RADIUS;
				if (Trace(from, to, self) < 1)
					return false;
			}
		}

		return Trace(feet + "0 0.2 0", feet + Vector(0, HEAD_ROOM, 0), self) >= 1;
	}

	//------------------------------------------------------------------------------------------------
	//! Fraction of the segment that is free (1 = nothing hit).
	protected static float Trace(vector from, vector to, IEntity exclude)
	{
		TraceParam trace = new TraceParam();
		trace.Start = from;
		trace.End = to;
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		trace.Exclude = exclude;
		trace.LayerMask = EPhysicsLayerPresets.Projectile;
		return GetGame().GetWorld().TraceMove(trace, null);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool QueryHit(IEntity entity)
	{
		if (entity == s_QuerySelf || !ChimeraCharacter.Cast(entity))
			return true;

		vector delta = entity.GetOrigin() - s_vQueryCentre;
		delta[1] = 0;
		if (delta.Length() < CAPSULE_RADIUS * 2)
			s_bQueryBlocked = true;

		return !s_bQueryBlocked;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_BusArrival's statics (LF_StaticReset finds it by reflection).
class LF_BusArrival_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_BusArrival";
	}

	override void Apply()
	{
		LF_BusArrival.ResetStatics();
	}
}
