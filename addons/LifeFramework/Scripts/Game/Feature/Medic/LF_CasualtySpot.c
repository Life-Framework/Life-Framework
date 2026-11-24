//------------------------------------------------------------------------------------------------
//! What the world holds around one candidate spot for a casualty (#4863).
class LF_CasualtySpotProbe
{
	//! Height of what the body would lie on: the terrain, or the built surface a downward trace met first.
	float m_fLieY;
	//! The surface under the body is built (a prop, a floor, a deck), not the terrain.
	bool m_bBuiltSurface;
	//! Something covers the spot within a standing person's reach and a bit more.
	bool m_bRoofed;
	bool m_bWater;
	//! Largest height difference between the spot and the four points a body's length away.
	float m_fSpread;
	//! A wall, fence or prop stands within a body's width of the spot.
	bool m_bCrowded;
}

//------------------------------------------------------------------------------------------------
//! Where a casualty may lie (#4863). The generator used to drop the body at a random point near the
//! place and read only the terrain height, so the first casualty of the medic hour lay on a hay bale in
//! the farm barn. A spot is clear when the body lies on open ground (not a prop, floor or roof), nothing
//! stands over or beside it and the ground is level. A row on a built deck (a quay, a floor) lets the
//! surface be built and covered; it still has to be level and uncrowded.
class LF_CasualtySpot
{
	static const string FEATURE = "CasualtyCall";
	//! Candidates tried per ring before the ring widens.
	static const int ATTEMPTS = 14; // lf-fixed: search effort, not a game rule
	//! Rings: the row's radius, then twice it.
	static const int RINGS = 2; // lf-fixed: search effort, not a game rule
	//! Half a lying body's length: where the level check samples.
	static const float HALF_BODY_M = 0.9; // lf-fixed: body size
	//! Beside the spot a wall or prop this near is too close.
	static const float CLEARANCE_M = 1.1; // lf-fixed: body size
	//! Highest step between the spot and the points a body's length around it.
	static const float MAX_SPREAD_M = 0.3; // lf-fixed: what still lies flat
	//! A downward trace starts this far above the ground.
	static const float TRACE_UP_M = 3.0; // lf-fixed: probe geometry
	//! Surface this far above the terrain counts as built.
	static const float BUILT_M = 0.15; // lf-fixed: probe geometry
	//! Cover above the spot within this height is a roof.
	static const float ROOF_M = 6.0; // lf-fixed: probe geometry

	//------------------------------------------------------------------------------------------------
	//! Pure: why the probe's spot is not clear, or an empty string when it is.
	//! \param built The row lies on a built deck: a built surface and a roof are fine.
	static string Verdict(LF_CasualtySpotProbe probe, bool built)
	{
		if (!probe)
			return "no probe";

		if (probe.m_bWater)
			return "in water";

		if (!built && probe.m_bBuiltSurface)
			return "on a prop or a floor";

		if (!built && probe.m_bRoofed)
			return "under a roof";

		if (probe.m_fSpread > MAX_SPREAD_M)
			return "uneven ground";

		if (probe.m_bCrowded)
			return "against a wall or prop";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Server: read the world at (x, z) for a body about to lie there.
	static LF_CasualtySpotProbe ProbeWorld(float x, float z)
	{
		LF_CasualtySpotProbe probe = new LF_CasualtySpotProbe();
		BaseWorld world;
		if (GetGame())
			world = GetGame().GetWorld();

		if (!world)
		{
			probe.m_bCrowded = true;
			return probe;
		}

		bool built;
		probe.m_fLieY = LieY(world, x, z, built);
		probe.m_bBuiltSurface = built;
		probe.m_bWater = probe.m_fLieY < world.GetOceanBaseHeight() + 0.05;

		vector centre = Vector(x, probe.m_fLieY, z);
		probe.m_bRoofed = Blocked(world, centre + "0 0.3 0", centre + Vector(0, ROOF_M, 0));

		array<vector> around = { Vector(HALF_BODY_M, 0, 0), Vector(-HALF_BODY_M, 0, 0), Vector(0, 0, HALF_BODY_M), Vector(0, 0, -HALF_BODY_M) };
		foreach (vector offset : around)
		{
			bool sampleBuilt;
			float y = LieY(world, x + offset[0], z + offset[2], sampleBuilt);
			float spread = Math.AbsFloat(y - probe.m_fLieY);
			if (spread > probe.m_fSpread)
				probe.m_fSpread = spread;
		}

		array<vector> sides = { Vector(CLEARANCE_M, 0, 0), Vector(-CLEARANCE_M, 0, 0), Vector(0, 0, CLEARANCE_M), Vector(0, 0, -CLEARANCE_M) };
		vector chest = centre + "0 0.6 0";
		foreach (vector side : sides)
		{
			if (Blocked(world, chest, chest + side))
			{
				probe.m_bCrowded = true;
				break;
			}
		}

		return probe;
	}

	//------------------------------------------------------------------------------------------------
	//! The height of what a body at (x, z) would lie on: a trace down from above the terrain, skipping
	//! characters. `built` is set when that surface is not the terrain.
	protected static float LieY(BaseWorld world, float x, float z, out bool built)
	{
		float groundY = world.GetSurfaceY(x, z);
		built = false;
		array<IEntity> skip = {};
		float top = groundY + TRACE_UP_M;
		for (int attempt = 0; attempt < 4; attempt++)
		{
			TraceParam trace = new TraceParam();
			trace.Start = Vector(x, top, z);
			trace.End = Vector(x, groundY - 0.5, z);
			trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
			trace.LayerMask = EPhysicsLayerPresets.Projectile;
			trace.ExcludeArray = skip;
			float fraction = world.TraceMove(trace, null);
			if (fraction >= 1)
				return groundY;

			if (trace.TraceEnt && ChimeraCharacter.Cast(trace.TraceEnt))
			{
				skip.Insert(trace.TraceEnt);
				continue;
			}

			float y = top - fraction * (top - (groundY - 0.5));
			if (y > groundY + BUILT_M)
				built = true;

			if (trace.TraceEnt)
			{
				// The terrain is an entity too (LF_Plants.OpenGroundAt); anything else the trace meets first is built.
				typename hitType = trace.TraceEnt.Type();
				if (!hitType.IsInherited(GenericTerrainEntity) && !hitType.IsInherited(GenericWorldEntity))
					built = true;
			}

			return y;
		}

		return groundY;
	}

	//------------------------------------------------------------------------------------------------
	//! Is something that is not a character in the way between two points.
	protected static bool Blocked(BaseWorld world, vector from, vector to)
	{
		array<IEntity> skip = {};
		for (int attempt = 0; attempt < 4; attempt++)
		{
			TraceParam trace = new TraceParam();
			trace.Start = from;
			trace.End = to;
			trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
			trace.LayerMask = EPhysicsLayerPresets.Projectile;
			trace.ExcludeArray = skip;
			float fraction = world.TraceMove(trace, null);
			if (fraction >= 1)
				return false;

			if (trace.TraceEnt && ChimeraCharacter.Cast(trace.TraceEnt))
			{
				skip.Insert(trace.TraceEnt);
				continue;
			}

			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a clear spot within `radius` of `origin` (then within twice it). A radius of 0 asks for the
	//! origin itself. The spot's height is what the body lies on.
	//! \return False when no candidate was clear; `why` then holds the last reason.
	static bool Find(vector origin, float radius, bool built, out vector spot, out string why)
	{
		for (int ring = 1; ring <= RINGS; ring++)
		{
			float reach = radius * ring;
			for (int i = 0; i < ATTEMPTS; i++)
			{
				float x = origin[0];
				float z = origin[2];
				if (reach > 0)
				{
					float angle = Math.RandomFloat(0, Math.PI2);
					float distance = Math.RandomFloat(reach * 0.3, reach);
					x = x + Math.Cos(angle) * distance;
					z = z + Math.Sin(angle) * distance;
				}

				LF_CasualtySpotProbe probe = ProbeWorld(x, z);
				why = Verdict(probe, built);
				if (why.IsEmpty())
				{
					spot = Vector(x, probe.m_fLieY, z);
					return true;
				}
			}

			if (radius <= 0)
				break;
		}

		return false;
	}
}
