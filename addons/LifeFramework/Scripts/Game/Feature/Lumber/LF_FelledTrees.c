//------------------------------------------------------------------------------------------------
//! Collects the entities a sphere query finds, for the felling probes.
class LF_FellQuery : Managed
{
	ref array<IEntity> m_aHits = {};

	//------------------------------------------------------------------------------------------------
	bool Collect(IEntity entity)
	{
		if (entity)
			m_aHits.Insert(entity);

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The server's felled-tree ledger and the world facts a fell needs (#3260, design
//! docs/roadmap/lumber.md "Felling a real map tree"). The ledger is LF_TreeFellingLedger on the world
//! clock; here it only grows (replication is #3261, regrow and save are #3262). The tree is hidden
//! by flags, never killed, so it can stand again. Server only: every call here is a no-op elsewhere.
class LF_FelledTrees
{
	static const string FEATURE = "Lumber";
	//! Metres around a tree the building probe looks (the config's building clearance is smaller).
	static const float PROBE_BUILDING_M = 30;
	//! Height above a probe point the road trace starts.
	static const float PROBE_UP_M = 2;

	protected static ref LF_TreeFellingLedger s_Ledger;
	protected static ref map<string, int> s_mLayers;

	//------------------------------------------------------------------------------------------------
	static LF_TreeFellingLedger Ledger()
	{
		if (!s_Ledger)
			s_Ledger = new LF_TreeFellingLedger();

		return s_Ledger;
	}

	//------------------------------------------------------------------------------------------------
	//! Whole seconds of world time, the ledger's clock.
	static int NowS()
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return 0;

		return world.GetWorldTime() / 1000;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsFelled(string key)
	{
		return Ledger().IsFelled(key, NowS());
	}

	//------------------------------------------------------------------------------------------------
	static int Count()
	{
		return Ledger().Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Takes a tree out of the world as a player sees and bumps into it: not drawn, not hit by a trace,
	//! no collision. The collision layer is kept so a regrow can give it back.
	static void Hide(notnull IEntity tree, string key)
	{
		tree.ClearFlags(EntityFlags.VISIBLE | EntityFlags.TRACEABLE, false);
		Physics physics = tree.GetPhysics();
		if (!physics)
			return;

		if (!s_mLayers)
			s_mLayers = new map<string, int>();

		if (!s_mLayers.Contains(key))
			s_mLayers.Set(key, physics.GetInteractionLayer());

		physics.SetInteractionLayer(0);
	}

	//------------------------------------------------------------------------------------------------
	//! True when the tree is not drawn (a fell hid it).
	static bool IsHidden(notnull IEntity tree)
	{
		return (tree.GetFlags() & EntityFlags.VISIBLE) == 0;
	}

	//------------------------------------------------------------------------------------------------
	//! The nearest standing map tree to `from` within `radius`, or null. A hidden tree does not count.
	static IEntity NearestTree(vector from, float radius)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return null;

		LF_FellQuery query = new LF_FellQuery();
		world.QueryEntitiesBySphere(from, radius, query.Collect, null, EQueryEntitiesFlags.ALL);
		IEntity best;
		float bestGap = float.MAX;
		foreach (IEntity entity : query.m_aHits)
		{
			if (!Tree.Cast(entity) || IsHidden(entity))
				continue;

			float gap = vector.Distance(entity.GetOrigin(), from);
			if (gap < bestGap)
			{
				bestGap = gap;
				best = entity;
			}
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Metres from `at` to the nearest hard surface (road, pavement) within `clearM`, or -1 when none.
	//! Samples the ground on the spot and on a ring of eight points `clearM` out.
	static float RoadDistance(vector at, float clearM)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return -1;

		float best = -1;
		for (int i = 0; i < 9; i++)
		{
			vector spot = at;
			float reach = 0;
			if (i > 0)
			{
				float angle = (i - 1) * 45 * Math.DEG2RAD;
				reach = clearM;
				spot = at + Vector(Math.Sin(angle) * reach, 0, Math.Cos(angle) * reach);
			}

			TraceParam trace = new TraceParam();
			trace.Start = spot + Vector(0, PROBE_UP_M, 0);
			trace.End = spot - Vector(0, PROBE_UP_M, 0);
			trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
			trace.LayerMask = EPhysicsLayerPresets.Projectile;
			float fraction = world.TraceMove(trace, null);
			if (fraction >= 1)
				continue;

			if (!LF_PlantRules.IsHardSurface(trace.TraceMaterial))
				continue;

			if (best < 0 || reach < best)
				best = reach;
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Metres in the ground plane from `at` to the nearest building's bounds within PROBE_BUILDING_M, or -1.
	static float BuildingDistance(vector at)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return -1;

		LF_FellQuery query = new LF_FellQuery();
		world.QueryEntitiesBySphere(at, PROBE_BUILDING_M, query.Collect, null, EQueryEntitiesFlags.STATIC);
		float best = -1;
		foreach (IEntity entity : query.m_aHits)
		{
			if (!Building.Cast(entity))
				continue;

			vector mins;
			vector maxs;
			entity.GetWorldBounds(mins, maxs);
			float dx = Math.Max(Math.Max(mins[0] - at[0], at[0] - maxs[0]), 0);
			float dz = Math.Max(Math.Max(mins[2] - at[2], at[2] - maxs[2]), 0);
			float gap = Math.Sqrt(dx * dx + dz * dz);
			if (best < 0 || gap < best)
				best = gap;
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Ledger = null;
		s_mLayers = null;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_FelledTrees's statics (LF_StaticReset finds it by reflection).
class LF_FelledTrees_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_FelledTrees";
	}

	override void Apply()
	{
		LF_FelledTrees.ResetStatics();
	}
}
