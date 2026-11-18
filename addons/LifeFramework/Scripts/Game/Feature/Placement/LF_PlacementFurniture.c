//------------------------------------------------------------------------------------------------
//! House furniture (#1033, slice #2654, epic #747): the snap modes a chair or a shelf uses.
//! FLOOR lies where GROUND does (the ground probe reads the floor of a building as the highest
//! surface under the spot); where it may go is the policy zone (OWNED_PROPERTY, #2447). WALL puts the
//! thing flush against the wall face the player aims at: a ray strikes the wall, the face's horizontal
//! normal is the heading (LF_PlacementGeometry.WallForward) and the footprint's back edge lies
//! WALL_GAP_M off the face. The floor under it comes from the normal probe (LF_PlacementService.ProbeAt),
//! so a shelf stands on the floor at the foot of the wall. The server re-checks the gap on a
//! proposal (WallProblem), so a client cannot hang a shelf a metre off the wall.
//------------------------------------------------------------------------------------------------
class LF_PlacementFurniture
{
	static const string FEATURE = "Placement";
	//! Metres between the wall face and the footprint's back edge.
	static const float WALL_GAP_M = 0.01; // lf-fixed: geometry, under the 2 cm flush rule
	//! The most gap the server accepts on a proposal.
	static const float WALL_TOLERANCE_M = 0.02; // lf-fixed: the flush rule of #2654
	//! How much of the footprint's back the clearance box leaves out: the wall itself lies there.
	static const float WALL_SKIN_M = 0.05; // lf-fixed: geometry, a few cm over the flush rule
	//! Height above the player's feet the plain Place action's wall ray starts at.
	static const float CHEST_M = 1.0; // lf-fixed: geometry of the ray, not a gameplay knob
	//! Height above the floor the server's flush check looks at the wall.
	static const float CHECK_HEIGHT_M = 0.5; // lf-fixed: geometry of the check

	//------------------------------------------------------------------------------------------------
	//! Pure: where a thing whose footprint starts at `minZ` (negative, behind its origin) lies flush on
	//! the wall face struck at `hit`, heading `forward` out of the wall.
	static vector FlushOrigin(vector hit, vector forward, float minZ)
	{
		return LF_PlacementGeometry.WallOrigin(hit, forward, -minZ + WALL_GAP_M);
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the gap between the wall and the footprint's back edge, given the distance from the
	//! origin back to the wall face.
	static float GapOf(float distanceToWall, float minZ)
	{
		return distanceToWall + minZ;
	}

	//------------------------------------------------------------------------------------------------
	//! The first thing a ray strikes (not `exclude`) within `length`: where, and its face normal.
	static bool Strike(vector from, vector dir, float length, array<IEntity> exclude, out vector hit, out vector normal)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;

		TraceParam trace = new TraceParam();
		trace.Start = from;
		trace.End = from + dir * length;
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		trace.LayerMask = EPhysicsLayerPresets.Projectile;
		trace.ExcludeArray = exclude;
		float fraction = world.TraceMove(trace, null);
		if (fraction >= 1)
			return false;

		hit = trace.Start + (trace.End - trace.Start) * fraction;
		normal = trace.TraceNorm;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server and client: the wall in front of `player` (the plain Place action).
	static LF_EPlacementVerdict PlanWall(IEntity player, notnull LF_PlaceableComponent placeable, IEntity ignore, out vector transform[4], notnull LF_PlacementProbe probe)
	{
		vector playerTransform[4];
		player.GetWorldTransform(playerTransform);
		vector from = playerTransform[3] + Vector(0, CHEST_M, 0);
		bool clamped;
		return PlanWallRay(player, placeable, ignore, from, LF_PlacementGeometry.Flat(playerTransform[2]), transform, probe, clamped);
	}

	//------------------------------------------------------------------------------------------------
	//! Client (the ghost): the wall the camera aims at.
	static LF_EPlacementVerdict PlanWallAimed(IEntity player, notnull LF_PlaceableComponent placeable, IEntity ignore, vector eye, vector aimDir, out vector transform[4], notnull LF_PlacementProbe probe, out bool clamped)
	{
		return PlanWallRay(player, placeable, ignore, eye, aimDir, transform, probe, clamped);
	}

	//------------------------------------------------------------------------------------------------
	//! The wall a ray from `from` along `dir` strikes within the placeable's aim reach, the spot flush on
	//! it, probed like any placement. No wall (nothing struck, out of reach, a floor or ceiling face) is
	//! NO_GROUND with the spot level at the player's feet, so the ghost has a transform.
	static LF_EPlacementVerdict PlanWallRay(IEntity player, notnull LF_PlaceableComponent placeable, IEntity ignore, vector from, vector dir, out vector transform[4], notnull LF_PlacementProbe probe, out bool clamped)
	{
		clamped = false;
		vector playerTransform[4];
		player.GetWorldTransform(playerTransform);
		array<IEntity> exclude = {player};
		if (ignore)
			exclude.Insert(ignore);

		float reach = placeable.GetAimReach();
		vector hit;
		vector normal;
		vector forward = vector.Zero;
		if (Strike(from, dir, reach + LF_PlacementService.AIM_RAY_EXTRA, exclude, hit, normal))
		{
			forward = LF_PlacementGeometry.WallForward(normal);
			float away = vector.Distance(Vector(hit[0], 0, hit[2]), Vector(playerTransform[3][0], 0, playerTransform[3][2]));
			clamped = away > reach;
		}

		if (forward == vector.Zero || clamped)
		{
			LF_PlacementGeometry.Transform(playerTransform[3] - "0 0 0.5", playerTransform[3] + "0 0 0.5", transform);
			probe.m_bGround = false;
			return LF_PlacementRules.Judge(probe, LF_PlacementService.MAX_STEP);
		}

		vector origin = FlushOrigin(hit, forward, placeable.GetFootprintMins()[2]);
		return ProbeWall(player, placeable, origin, forward, exclude, transform, probe);
	}

	//------------------------------------------------------------------------------------------------
	//! The probes for a thing flush on a wall: the floor under its middle and its front edge (never under
	//! the back edge, which lies 1 cm off a face whose cap or footing the vertical trace would read as
	//! floor), the clearance box over it. `origin` is where its z = 0 stands, `forward` out of the wall.
	protected static LF_EPlacementVerdict ProbeWall(IEntity player, notnull LF_PlaceableComponent placeable, vector origin, vector forward, array<IEntity> exclude, out vector transform[4], notnull LF_PlacementProbe probe)
	{
		vector mins = placeable.GetFootprintMins();
		vector maxs = placeable.GetFootprintMaxs();
		vector front = origin + forward * maxs[2];
		float headY = player.GetOrigin()[1] + 1.8; // lf-fixed: a standing player's head, the highest floor that counts
		float middleY = FloorY(origin, headY, exclude);
		float frontY = FloorY(front, headY, exclude);
		probe.m_bGround = true;
		probe.m_fLowest = Math.Min(middleY, frontY);
		probe.m_fHighest = Math.Max(middleY, frontY);
		vector spot = origin;
		spot[1] = probe.m_fHighest + placeable.GetHangHeight(); // a hung mirror or picture lifts off the floor; 0 stands on it
		LF_PlacementGeometry.Transform(spot - forward * 0.5, spot + forward * 0.5, transform);
		transform[3] = spot;
		// The back WALL_SKIN_M lies against the wall by design, so the clearance box starts in front of it.
		mins[2] = mins[2] + WALL_SKIN_M;
		probe.m_sBlocker = LF_PlacementRules.FindBlocker(transform, mins, maxs, exclude);
		LF_EPlacementVerdict verdict = LF_PlacementRules.Judge(probe, LF_PlacementService.MAX_STEP);
		LF_Debug.Log(FEATURE, string.Format("wall probe at %1 heading %2: floor %3..%4, %5", spot, forward, probe.m_fLowest, probe.m_fHighest, LF_PlacementRules.Name(verdict)));
		return verdict;
	}

	//------------------------------------------------------------------------------------------------
	//! The higher of the terrain and the first thing struck straight down from `fromY` over `pos` (the
	//! player's head height: a floor above the player is a ceiling, not the floor they stand on).
	protected static float FloorY(vector pos, float fromY, array<IEntity> exclude)
	{
		float y = GetGame().GetWorld().GetSurfaceY(pos[0], pos[2]);
		vector hit;
		vector normal;
		vector from = Vector(pos[0], fromY, pos[2]);
		if (Strike(from, "0 -1 0", 4, exclude, hit, normal) && hit[1] > y)
			y = hit[1];

		return y;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the probes for a WALL proposal (LF_PlacementService.Confirm).
	static LF_EPlacementVerdict ProbeProposal(IEntity player, notnull LF_PlaceableComponent placeable, IEntity item, vector proposal[4], out vector transform[4], notnull LF_PlacementProbe probe)
	{
		array<IEntity> exclude = {};
		if (player)
			exclude.Insert(player);
		if (item)
			exclude.Insert(item);

		return ProbeWall(player, placeable, proposal[3], LF_PlacementGeometry.Flat(proposal[2]), exclude, transform, probe);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: why a WALL proposal does not lie flush on a wall, or "" (also "" for any other snap).
	//! Looks from CHECK_HEIGHT_M above the proposal's floor back along its heading for the wall face.
	static string WallProblem(notnull LF_PlaceableComponent placeable, vector proposal[4], IEntity player, IEntity item)
	{
		if (placeable.GetSnap() != LF_EPlacementSnap.WALL)
			return "";

		float minZ = placeable.GetFootprintMins()[2];
		vector forward = LF_PlacementGeometry.Flat(proposal[2]);
		vector from = proposal[3] + Vector(0, CHECK_HEIGHT_M, 0);
		array<IEntity> exclude = {};
		if (player)
			exclude.Insert(player);
		if (item)
			exclude.Insert(item);

		vector hit;
		vector normal;
		if (!Strike(from, forward * -1, -minZ + 1, exclude, hit, normal))
			return "no wall behind it";

		float gap = GapOf(vector.Distance(from, hit), minZ);
		if (Math.AbsFloat(gap) > WALL_TOLERANCE_M)
			return string.Format("%1 m off the wall (most %2 m)", gap, WALL_TOLERANCE_M);

		return "";
	}
}
