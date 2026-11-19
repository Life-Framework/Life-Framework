// LF_PlaceSnap.c - rest an entity's real bottom on the surface under its footprint (placement spike #2952; docs/placement-tool.md "Contact sheets").
//
// Never trusts the pivot or a catalog extent. The contact is measured on the entity as it stands in the world:
//   bottom    the lowest face of its local mesh bounds, taken through its own transform (scale and rotation applied),
//             so a backpack whose pivot is 1.09 m under its mesh, or a vice whose pivot is 0.1 m above its base,
//             rests by the mesh;
//   footprint a GRID x GRID set of points on that bottom face, INSET in from each side, so the support is
//             what is under the item and not what is under its pivot;
//   support   per point, a trace straight down from LIFT above the bottom. Two traces per point: the
//             projectile layer (the precise geometry vanilla's own item placement uses, SCR_ItemPlacementComponent)
//             and every layer (some small props have no projectile geometry above their base plate,
//             LF_PlacementAudit.SurfaceTrace). A trace that stops at its own start began inside a collider: a
//             solid proxy box round a whole shelf unit (ShelfUniversal_01_wide carries one, 0 to 2.59 m) is not a
//             board, so such a hit is dropped. The support is the highest kept hit under the footprint.
//
// Settle moves the entity so its bottom is CLEARANCE above the support and measures again: the result carries
// the gap before and after, the spread of the support under the footprint (a lip, an edge) and what it stands on.

class LF_PlaceSnapResult
{
	float m_fBottomY;
	float m_fSupportY;
	float m_fGap;
	float m_fGapBefore;
	//! Highest minus lowest support under the footprint points that hit something.
	float m_fSpread;
	int m_iHits;
	int m_iRays;
	string m_sOn;

	//------------------------------------------------------------------------------------------------
	string Format()
	{
		return string.Format("bottom=%1 support=%2 gap=%3 before=%4 spread=%5 hits=%6/%7 on=%8", m_fBottomY, m_fSupportY, m_fGap, m_fGapBefore, m_fSpread, m_iHits, m_iRays, m_sOn);
	}
}

class LF_PlaceSnap
{
	static const string FEATURE = "PlaceSnap";
	//! How far above the bottom each support trace starts. Under the 0.39 m between two gondola boards.
	static const float LIFT = 0.15;
	static const float REACH = 0.6;
	static const int GRID = 3;
	static const float INSET = 0.2;
	//! What is left between the bottom and the support: under a millimetre, not visible, no z-fight.
	static const float CLEARANCE = 0.001;
	protected static const int SKIP_TRIES = 4;

	//------------------------------------------------------------------------------------------------
	//! World points on the bottom face of the entity's local bounds; returns the lowest point's y.
	static float ContactPoints(notnull IEntity entity, notnull array<vector> points)
	{
		vector mins;
		vector maxs;
		entity.GetBounds(mins, maxs);
		float lowest = 1e9;
		for (int i = 0; i < GRID; i++)
		{
			float u = INSET + (1 - 2 * INSET) * i / (GRID - 1);
			for (int j = 0; j < GRID; j++)
			{
				float v = INSET + (1 - 2 * INSET) * j / (GRID - 1);
				vector local = Vector(mins[0] + (maxs[0] - mins[0]) * u, mins[1], mins[2] + (maxs[2] - mins[2]) * v);
				vector world = entity.CoordToParent(local);
				points.Insert(world);
				if (world[1] < lowest)
					lowest = world[1];
			}
		}

		return lowest;
	}

	//------------------------------------------------------------------------------------------------
	//! Moves the entity flat so the centre of its mesh bounds (not its pivot) is over (x, z).
	static void CentreOn(notnull IEntity entity, float x, float z)
	{
		vector mins;
		vector maxs;
		entity.GetBounds(mins, maxs);
		vector centre = entity.CoordToParent((mins + maxs) * 0.5);
		vector origin = entity.GetOrigin();
		origin[0] = origin[0] + x - centre[0];
		origin[2] = origin[2] + z - centre[2];
		entity.SetOrigin(origin);
		entity.Update();
	}

	//------------------------------------------------------------------------------------------------
	//! Loose things that never count as a support: characters, inventory items (store stock beside it), vegetation
	//! (a parked car seated on a juniper bush stood 0.23 m over the street, placement-system.md), the entity itself and its children.
	protected static bool Skip(IEntity self, IEntity hit)
	{
		if (!hit)
			return false;
		if (ChimeraCharacter.Cast(hit) || BaseTree.Cast(hit) || hit.FindComponent(InventoryItemComponent))
			return true;

		IEntity walk = hit;
		while (walk)
		{
			if (walk == self)
				return true;

			walk = walk.GetParent();
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! One trace down from `from`; the hit height, or -1e9 for no kept hit. A hit at the start (inside a
	//! collider) is dropped.
	static float Down(IEntity self, vector from, float reach, int layerMask, out IEntity hitEntity)
	{
		array<IEntity> skip = {};
		if (self)
			skip.Insert(self);

		for (int attempt = 0; attempt < SKIP_TRIES; attempt++)
		{
			TraceParam trace = new TraceParam();
			trace.Start = from;
			trace.End = from - Vector(0, reach, 0);
			trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
			trace.LayerMask = layerMask;
			trace.ExcludeArray = skip;
			float fraction = GetGame().GetWorld().TraceMove(trace, null);
			if (fraction >= 1)
				return -1e9;

			IEntity hit = trace.TraceEnt;
			if (Skip(self, hit))
			{
				skip.Insert(hit);
				continue;
			}

			if (fraction <= 0.0001)
				return -1e9;

			hitEntity = hit;
			return from[1] - reach * fraction;
		}

		return -1e9;
	}

	//------------------------------------------------------------------------------------------------
	//! Where the entity stands now: bottom, support under the footprint, gap.
	//! fromY, when given, is where every trace starts (a store slot's written height plus a margin) instead of
	//! lift above the bottom: an item spawned with its pivot on the slot can have its mesh far above it.
	static LF_PlaceSnapResult Measure(notnull IEntity entity, float lift = LIFT, float reach = REACH, float fromY = -1e9)
	{
		LF_PlaceSnapResult r = new LF_PlaceSnapResult();
		array<vector> points = {};
		r.m_fBottomY = ContactPoints(entity, points);
		r.m_iRays = points.Count();
		float startY = r.m_fBottomY + lift;
		if (fromY > -1e8)
		{
			startY = fromY;
			lift = 0;
			reach = reach + Math.Max(0, fromY - r.m_fBottomY);
		}
		float best = -1e9;
		float low = 1e9;
		IEntity bestHit;
		foreach (vector p : points)
		{
			vector from = Vector(p[0], startY, p[2]);
			IEntity hitA;
			IEntity hitB;
			float a = Down(entity, from, lift + reach, EPhysicsLayerPresets.Projectile, hitA);
			float b = Down(entity, from, lift + reach, -1, hitB);
			float y = a;
			IEntity hit = hitA;
			if (b > y)
			{
				y = b;
				hit = hitB;
			}

			if (y < -1e8)
				continue;

			r.m_iHits++;
			if (y < low)
				low = y;
			if (y > best)
			{
				best = y;
				bestHit = hit;
			}
		}

		if (r.m_iHits == 0)
		{
			r.m_fSupportY = -1e9;
			r.m_fGap = 1e9;
			r.m_sOn = "nothing";
			return r;
		}

		r.m_fSupportY = best;
		r.m_fSpread = best - low;
		r.m_fGap = r.m_fBottomY - best;
		r.m_sOn = "terrain";
		if (bestHit)
			r.m_sOn = LF_PlacementAudit.Label(bestHit);

		return r;
	}

	//------------------------------------------------------------------------------------------------
	//! Moves the entity straight up or down so its bottom rests on the support, and measures again.
	//! Null when nothing is under the footprint (the entity stays where it was; logged).
	static LF_PlaceSnapResult Settle(notnull IEntity entity, float lift = LIFT, float reach = REACH, float fromY = -1e9)
	{
		LF_PlaceSnapResult before = Measure(entity, lift, reach, fromY);
		if (before.m_iHits == 0)
		{
			LF_Debug.Warn(FEATURE, string.Format("%1: nothing under its footprint within %2 m; left where it was", LF_PlacementAudit.Label(entity), reach));
			return null;
		}

		vector origin = entity.GetOrigin();
		origin[1] = origin[1] - before.m_fGap + CLEARANCE;
		entity.SetOrigin(origin);
		entity.Update();
		LF_PlaceSnapResult after = Measure(entity, lift, reach);
		after.m_fGapBefore = before.m_fGap;
		return after;
	}
}
