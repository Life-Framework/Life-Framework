//------------------------------------------------------------------------------------------------
//! The placement service (#1026, docs/roadmap/building-foundation.md section 4). One path for
//! everything a player puts into the world from a carried LF_PlaceableComponent item:
//!   Plan     (both sides) the spot in front of the player by the item's snap mode, and its verdict;
//!            the client's ghost shows it, the server uses it as the reference for a proposal
//!   ProbeAt  (both sides) the probes at one given spot: ground, step, clearance, surface
//!   Confirm  (server) the client proposes a spot; the server rejects a malformed one, accepts it
//!            within the item's reach plus TOLERANCE_METRES of where the player stands now (and,
//!            across a road, within TOLERANCE_DEGREES of straight across), re-probes it, refuses
//!            anything not VALID, rate-limits (SPAWN), spawns the placed prefab and uses the item up
//! The proposal arrives through the placing action's own data (OnSaveActionData/OnLoadActionData,
//! LF_SpikeStripDeployAction), so there is no extra RPC. The ghost is never replicated.
//------------------------------------------------------------------------------------------------

//! Pure placement geometry (LOGIC-tested by LF_Test_SpikeStripGeometry and LF_Test_Placement).
class LF_PlacementGeometry
{
	//------------------------------------------------------------------------------------------------
	//! Horizontal unit vector of v, or zero when v is (nearly) vertical.
	static vector Flat(vector v)
	{
		vector flat = Vector(v[0], 0, v[2]);
		float length = flat.Length();
		if (length < 0.0001)
			return vector.Zero;

		return flat / length;
	}

	//------------------------------------------------------------------------------------------------
	//! Which way the placed thing's forward runs. With no road it runs where the player faces. With a
	//! road it runs straight across the road (perpendicular to roadDir), on the side the player faces.
	static vector Axis(vector playerForward, bool hasRoad, vector roadDir)
	{
		vector forward = Flat(playerForward);
		if (forward == vector.Zero)
			forward = "0 0 1";

		if (!hasRoad)
			return forward;

		vector along = Flat(roadDir);
		if (along == vector.Zero)
			return forward;

		vector across = Vector(-along[2], 0, along[0]);
		if (vector.Dot(across, forward) < 0)
			across = -across;

		return across;
	}

	//------------------------------------------------------------------------------------------------
	//! Direction of the polyline segment nearest to pos (horizontal distance), or zero with fewer
	//! than two points.
	static vector NearestSegmentDirection(notnull array<vector> points, vector pos, out float distance)
	{
		distance = 1000000;
		vector best = vector.Zero;
		for (int i = 0; i + 1 < points.Count(); i++)
		{
			vector a = Vector(points[i][0], 0, points[i][2]);
			vector b = Vector(points[i + 1][0], 0, points[i + 1][2]);
			vector p = Vector(pos[0], 0, pos[2]);
			vector ab = b - a;
			float lengthSq = vector.Dot(ab, ab);
			if (lengthSq < 0.0001)
				continue;

			float t = Math.Clamp(vector.Dot(p - a, ab) / lengthSq, 0, 1);
			float d = vector.Distance(p, a + ab * t);
			if (d < distance)
			{
				distance = d;
				best = ab.Normalized();
			}
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! A world transform whose forward (z) runs from `start` to `end` and whose up is as close to
	//! world up as that allows: a thing lies on a slope instead of floating over one end.
	static void Transform(vector start, vector end, out vector transform[4])
	{
		vector forward = (end - start).Normalized();
		vector right = Cross(vector.Up, forward).Normalized();
		vector up = Cross(forward, right);
		transform[0] = right;
		transform[1] = up;
		transform[2] = forward;
		transform[3] = (start + end) * 0.5;
	}

	//------------------------------------------------------------------------------------------------
	static vector Cross(vector a, vector b)
	{
		return Vector(a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]);
	}

	//! Largest coordinate a proposal may carry (a 100 km world), so a garbage float never reaches a trace.
	static const float MAX_COORD = 100000; // lf-fixed: sanity bound on a wire value, not a gameplay knob

	//------------------------------------------------------------------------------------------------
	//! What is wrong with a proposal off the wire, or "" when it is usable: an origin that is not a
	//! finite number within the world's bounds, or a forward that is not a horizontal unit vector.
	//! Written as !(x < bound) so a NaN fails too.
	static string ProposalProblem(vector proposal[4])
	{
		vector origin = proposal[3];
		for (int i = 0; i < 3; i++)
		{
			if (!(Math.AbsFloat(origin[i]) < MAX_COORD))
				return string.Format("origin %1 is out of the world", origin);
		}

		vector forward = proposal[2];
		float length = forward.Length();
		if (!(length > 0.9 && length < 1.1))
			return string.Format("forward %1 is not a unit vector", forward);

		if (!(Math.AbsFloat(forward[1]) < 0.5))
			return string.Format("forward %1 is not horizontal", forward);

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the proposal's origin lies within `maxDistance` (horizontal) and `maxRise` (vertical)
	//! of `player`, where the player stands now. `reason` says how far it is either way, for the log.
	static bool WithinReach(vector player, vector proposal[4], float maxDistance, float maxRise, out string reason)
	{
		vector spot = proposal[3];
		float distance = vector.Distance(Vector(player[0], 0, player[2]), Vector(spot[0], 0, spot[2]));
		float rise = Math.AbsFloat(spot[1] - player[1]);
		reason = string.Format("%1 m from the player and %2 m above or below (most %3 m, %4 m)", distance, rise, maxDistance, maxRise);
		return distance <= maxDistance && rise <= maxRise;
	}

	//------------------------------------------------------------------------------------------------
	//! Degrees between the horizontal `heading` and straight across the road running along `roadDir`
	//! (either way across counts), or 90 when either is vertical.
	static float AcrossAngle(vector heading, vector roadDir)
	{
		vector a = Flat(heading);
		vector along = Flat(roadDir);
		if (a == vector.Zero || along == vector.Zero)
			return 90;

		vector across = Vector(-along[2], 0, along[0]);
		float dot = Math.AbsFloat(vector.Dot(a, across));
		return Math.Acos(Math.Clamp(dot, 0, 1)) * Math.RAD2DEG;
	}
}

//------------------------------------------------------------------------------------------------
class LF_PlacementService
{
	static const string FEATURE = "Placement";
	//! How much further than the item's reach a proposal may lie from where the player stands when
	//! the server sees it: a player walking or running between the ghost and the request (about
	//! 5 m/s over 300 ms). Turning moves the ghost along a circle of the reach, so it costs nothing.
	static const float TOLERANCE_METRES = 1.5; // lf-fixed: the #1026 confirm tolerance, latency slack not a gameplay knob
	//! Across a road, how far a proposal's heading may be from straight across.
	static const float TOLERANCE_DEGREES = 15; // lf-fixed: the #1026 confirm tolerance, a curved road's slack not a gameplay knob
	//! How far above or below the player's feet a proposal may lie.
	static const float MAX_RISE = 2; // lf-fixed: the #1026 confirm tolerance, a bank or a step, not a gameplay knob
	//! Largest rise along a footprint: the placement tool's 0.25 m ground rule.
	static const float MAX_STEP = 0.25; // lf-fixed: the placement tool's ground rule (docs/placement-tool.md)
	//! ACROSS_ROAD: a road whose centre line is this close to the spot turns the thing across it.
	static const float ROAD_REACH = 12; // lf-fixed: geometry, how far a road is looked for around the player

	protected static string s_sLastRefusal;

	//------------------------------------------------------------------------------------------------
	//! The last refusal on this machine (the tests read it).
	static string LastRefusal()
	{
		return s_sLastRefusal;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_sLastRefusal = "";
	}

	//------------------------------------------------------------------------------------------------
	//! Server and client: whether `user` may place `placeable`. The server reads the account, a
	//! client the replicated state (to hide an action the server would refuse). Cheap: an action's
	//! CanBeShown calls it every frame.
	static bool MayPlace(IEntity user, LF_PlaceableComponent placeable)
	{
		if (!user || !placeable)
			return false;

		if (placeable.GetWho() == LF_EPlacementWho.ON_DUTY_POLICE)
			return LF_PoliceUtils.IsOnDutyCharacter(user);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server and client: the spot in front of `player` by the placeable's snap mode, put on the
	//! ground in `transform`, and its verdict. `ignore` (the carried item itself) is kept out of the
	//! traces. Until the ground is measured the spot lies level at the player's feet, so a caller that
	//! shows it (the ghost) has a transform even for NO_GROUND.
	static LF_EPlacementVerdict Plan(IEntity player, notnull LF_PlaceableComponent placeable, IEntity ignore, out vector transform[4], notnull LF_PlacementProbe probe)
	{
		vector playerTransform[4];
		player.GetWorldTransform(playerTransform);
		vector origin = playerTransform[3];
		float near = -placeable.GetFootprintMins()[2];
		vector ahead = origin + LF_PlacementGeometry.Flat(playerTransform[2]) * (placeable.GetReach() + near);

		bool hasRoad = false;
		vector roadDir = vector.Zero;
		LF_EPlacementSnap snap = placeable.GetSnap();
		if (snap == LF_EPlacementSnap.ACROSS_ROAD)
			hasRoad = NearestRoad(ahead, roadDir);
		else if (snap != LF_EPlacementSnap.GROUND)
		{
			LF_Debug.Error(FEATURE, string.Format("snap mode %1 is not built yet (#1028); refusing", typename.EnumToString(LF_EPlacementSnap, snap)));
			LevelAt(ahead, LF_PlacementGeometry.Flat(playerTransform[2]), transform);
			probe.m_bGround = false;
			return LF_PlacementRules.Judge(probe, MAX_STEP);
		}

		vector axis = LF_PlacementGeometry.Axis(playerTransform[2], hasRoad, roadDir);
		vector centre = origin + axis * (placeable.GetReach() + near);
		vector level[4];
		LevelAt(centre, axis, level);
		return ProbeAt(player, placeable, ignore, level, transform, probe);
	}

	//------------------------------------------------------------------------------------------------
	//! Server and client: the probes at `spot` (its position and horizontal heading): the ground at
	//! the footprint's two ends and its middle, the step between them, the clearance box over it and
	//! what it would lie on. `transform` is the spot put on the ground, lying along the slope.
	static LF_EPlacementVerdict ProbeAt(IEntity player, notnull LF_PlaceableComponent placeable, IEntity ignore, vector spot[4], out vector transform[4], notnull LF_PlacementProbe probe)
	{
		vector mins = placeable.GetFootprintMins();
		vector maxs = placeable.GetFootprintMaxs();
		vector axis = LF_PlacementGeometry.Flat(spot[2]);
		if (axis == vector.Zero)
			axis = "0 0 1";

		vector centre = spot[3];
		vector start = centre + axis * mins[2];
		vector end = centre + axis * maxs[2];
		LevelAt(centre, axis, transform);

		array<IEntity> exclude = {};
		if (player)
			exclude.Insert(player);
		if (ignore)
			exclude.Insert(ignore);

		float startY;
		float endY;
		float middleY;
		IEntity startSurface;
		IEntity endSurface;
		IEntity middleSurface;
		probe.m_bGround = GroundY(start, exclude, startY, startSurface) && GroundY(end, exclude, endY, endSurface) && GroundY(centre, exclude, middleY, middleSurface);
		if (!probe.m_bGround)
			return LF_PlacementRules.Judge(probe, MAX_STEP);

		probe.m_fLowest = Math.Min(startY, Math.Min(endY, middleY));
		probe.m_fHighest = Math.Max(startY, Math.Max(endY, middleY));
		start[1] = startY;
		end[1] = endY;
		LF_PlacementGeometry.Transform(start, end, transform);
		// The origin is where the footprint's z = 0 lies on that slope, and never under the middle's ground.
		vector placed = start - transform[2] * mins[2];
		placed[1] = Math.Max(placed[1], middleY);
		transform[3] = placed;

		// Clearance: the footprint box, lifted off the ground, so a parked car, a post or a thing of
		// the same kind already there stops it (the vanilla item placement's space check).
		probe.m_sBlocker = LF_PlacementRules.FindBlocker(transform, mins, maxs, exclude);

		// What it would lie on: nothing that moves, and not a placement of the same prefab (the
		// clearance box starts above the surface, so it cannot see a 3 cm strip underneath).
		array<IEntity> surfaces = {startSurface, middleSurface, endSurface};
		foreach (IEntity surface : surfaces)
		{
			if (probe.m_sBlocker.IsEmpty())
				probe.m_sBlocker = SameKindBlocker(surface, placeable.GetPlacedPrefab());
		}

		LF_EPlacementVerdict verdict = LF_PlacementRules.Judge(probe, MAX_STEP);
		LF_Debug.Log(FEATURE, string.Format("probe at %1 heading %2: ground %3..%4, %5", transform[3], axis, probe.m_fLowest, probe.m_fHighest, LF_PlacementRules.Name(verdict)));
		return verdict;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: `player` places the `item` they carry (or that lies at their feet) at `proposal`, the
	//! spot their ghost showed. Refused with the reason logged (and toasted) unless they may place
	//! it, the proposal is well formed and within reach of where they stand, and the probes there say
	//! VALID. On success the placed prefab stands at the probed spot and the item is used up.
	//! \return the placed entity, or null.
	static IEntity Confirm(IEntity player, IEntity item, vector proposal[4])
	{
		if (!Replication.IsServer())
			return Refuse(player, "refused: not the server", "");

		if (!player || !item)
			return Refuse(player, "refused: no player or no item", "");

		LF_PlaceableComponent placeable = LF_Component<LF_PlaceableComponent>.Find(item);
		if (!placeable || placeable.GetPlacedPrefab().IsEmpty())
			return Refuse(player, "refused: the item names no placed prefab", "");

		if (!MayPlace(player, placeable))
			return Refuse(player, string.Format("refused: %1 is not on-duty police", LF_PoliceRanks.ResolveUid(player)), "#LF-Placement_NotAllowed");

		// Only an item on the ground or in the player's own inventory: not one in someone's car.
		IEntity holder = LF_InventoryUtils.GetStorageHierachyRoot(item);
		if (holder != item && holder != player)
			return Refuse(player, "refused: the item is in someone else's storage", "");

		// Before the probes, so a flood of proposals costs no traces.
		if (!LF_RateLimit.AllowCharacter(player, LF_ERequestKind.SPAWN, "placement confirm"))
		{
			s_sLastRefusal = "refused: rate limit";
			return null;
		}

		string problem = LF_PlacementGeometry.ProposalProblem(proposal);
		if (!problem.IsEmpty())
			return Refuse(player, "refused: malformed proposal: " + problem, "");

		// Where the player stands now, not a fresh plan: the ghost was planned a moment ago, and the
		// player may have walked or turned since. The reach bounds the spot; the probes judge it.
		string offBy;
		float maxDistance = placeable.GetReach() - placeable.GetFootprintMins()[2] + TOLERANCE_METRES;
		vector playerTransform[4];
		player.GetWorldTransform(playerTransform);
		if (!LF_PlacementGeometry.WithinReach(playerTransform[3], proposal, maxDistance, MAX_RISE, offBy))
			return Refuse(player, "refused: the proposal is " + offBy, "#LF-Placement_Moved");

		if (placeable.GetSnap() == LF_EPlacementSnap.ACROSS_ROAD)
		{
			string roadNote;
			string roadProblem = RoadHeadingProblem(proposal, roadNote);
			offBy += roadNote;
			if (!roadProblem.IsEmpty())
				return Refuse(player, "refused: " + roadProblem, "#LF-Placement_Moved");
		}

		vector transform[4];
		LF_PlacementProbe probe = new LF_PlacementProbe();
		LF_EPlacementVerdict verdict = ProbeAt(player, placeable, item, proposal, transform, probe);
		if (verdict == LF_EPlacementVerdict.BLOCKED)
			return Refuse(player, "refused: " + LF_PlacementRules.Reason(verdict, probe, MAX_STEP), "#LF-Placement_Blocked");

		if (verdict != LF_EPlacementVerdict.VALID)
			return Refuse(player, "refused: " + LF_PlacementRules.Reason(verdict, probe, MAX_STEP), "#LF-Placement_NoGround");

		IEntity placed = SpawnAt(placeable.GetPlacedPrefab(), transform);
		if (!placed)
			return Refuse(player, string.Format("refused: spawn failed for %1", placeable.GetPlacedPrefab()), "");

		SCR_EntityHelper.DeleteEntityAndChildren(item);
		LF_Debug.Info(FEATURE, string.Format("placed %1 for %2 at %3 heading %4 (%5), %6", LF_PlacementRules.Describe(placed), LF_PoliceRanks.ResolveUid(player), transform[3], transform[2], typename.EnumToString(LF_EPlacementLifetime, placeable.GetLifetime()), offBy));
		return placed;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: Confirm at the server's own plan, for a request that carried no proposal (a server-side
	//! perform, the harness, a listen host).
	static IEntity ConfirmOwnSpot(IEntity player, IEntity item)
	{
		vector own[4];
		LF_PlaceableComponent placeable = LF_Component<LF_PlaceableComponent>.Find(item);
		if (!player || !placeable)
			return Confirm(player, item, own);

		LF_PlacementProbe probe = new LF_PlacementProbe();
		Plan(player, placeable, item, own, probe);
		return Confirm(player, item, own);
	}

	//------------------------------------------------------------------------------------------------
	//! Level transform at `centre` whose forward is the horizontal `axis`.
	protected static void LevelAt(vector centre, vector axis, out vector transform[4])
	{
		LF_PlacementGeometry.Transform(centre - axis * 0.5, centre + axis * 0.5, transform);
	}

	//------------------------------------------------------------------------------------------------
	//! The direction of the AI road network's road nearest to `at`, when one lies within ROAD_REACH.
	//! A world with no AI road network (DebugWorld: its roads are spline meshes only) has none, and an
	//! ACROSS_ROAD item lies where the player faces there. LF_Test_PlacementRoad runs on Arland.
	static bool NearestRoad(vector at, out vector roadDir)
	{
		roadDir = vector.Zero;
		ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld || !aiWorld.GetRoadNetworkManager())
			return false;

		BaseRoad road;
		float roadDistance;
		aiWorld.GetRoadNetworkManager().GetClosestRoad(at, road, roadDistance);
		if (!road || roadDistance > ROAD_REACH)
			return false;

		array<vector> points = {};
		road.GetPoints(points);
		float segmentDistance;
		roadDir = LF_PlacementGeometry.NearestSegmentDirection(points, at, segmentDistance);
		return roadDir != vector.Zero;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: why an ACROSS_ROAD proposal's heading is refused, or "" when no road is near or it runs
	//! within TOLERANCE_DEGREES of straight across. `note` gets the angle for the placed log line.
	static string RoadHeadingProblem(vector proposal[4], out string note)
	{
		note = "";
		vector roadDir;
		if (!NearestRoad(proposal[3], roadDir))
			return "";

		float across = LF_PlacementGeometry.AcrossAngle(proposal[2], roadDir);
		note = string.Format(", %1 deg off across the road", across);
		if (across <= TOLERANCE_DEGREES)
			return "";

		return string.Format("the proposal is %1 deg off across the road (most %2 deg)", across, TOLERANCE_DEGREES);
	}

	//------------------------------------------------------------------------------------------------
	//! The higher of the terrain and whatever stands on it under pos (the placement tool's rule).
	//! `surface` is the entity that was hit when it stands above the terrain, else null.
	protected static bool GroundY(vector pos, array<IEntity> exclude, out float y, out IEntity surface)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;

		y = world.GetSurfaceY(pos[0], pos[2]);
		TraceParam trace = new TraceParam();
		trace.Start = Vector(pos[0], pos[1] + 2, pos[2]);
		trace.End = Vector(pos[0], pos[1] - 3, pos[2]);
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		trace.LayerMask = EPhysicsLayerPresets.Projectile;
		trace.ExcludeArray = exclude;
		float fraction = world.TraceMove(trace, null);
		surface = null;
		if (fraction < 1)
		{
			float hitY = trace.Start[1] + (trace.End[1] - trace.Start[1]) * fraction;
			if (hitY > y)
			{
				y = hitY;
				surface = trace.TraceEnt;
			}
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! What `surface` is when nothing may lie on it: anything dynamic, or a placement of `prefab`.
	protected static string SameKindBlocker(IEntity surface, ResourceName prefab)
	{
		if (!surface)
			return "";

		string moving = LF_PlacementRules.SurfaceBlocker(surface, null);
		if (!moving.IsEmpty())
			return moving;

		array<IEntity> entities = {surface, surface.GetRootParent()};
		foreach (IEntity entity : entities)
		{
			if (!entity || !entity.GetPrefabData())
				continue;

			if (LF_Utils.SamePrefab(entity.GetPrefabData().GetPrefabName(), prefab))
				return LF_PlacementRules.Describe(entity);
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	protected static IEntity SpawnAt(ResourceName prefab, vector transform[4])
	{
		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid())
		{
			LF_Debug.Error(FEATURE, string.Format("prefab does not resolve: %1", prefab));
			return null;
		}

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		for (int i = 0; i < 4; i++)
			params.Transform[i] = transform[i];

		return GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);
	}

	//------------------------------------------------------------------------------------------------
	protected static IEntity Refuse(IEntity player, string message, string toastKey)
	{
		s_sLastRefusal = message;
		LF_Debug.Warn(FEATURE, message);
		if (toastKey.IsEmpty() || !player || !GetGame() || !GetGame().GetPlayerManager())
			return null;

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(player);
		if (playerId > 0)
			LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-Placement_Title", toastKey, 3.0, LF_ENotificationType.WARNING);

		return null;
	}
}
