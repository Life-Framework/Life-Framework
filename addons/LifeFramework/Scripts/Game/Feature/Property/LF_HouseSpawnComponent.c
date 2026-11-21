// Derived from Overthrow (https://github.com/ArmaOverthrow/Overthrow.Arma4), used with the author's permission, relicensed MIT.
// From Scripts/Game/Components/OVT_SpawnPointComponent.c: authored points on a house, tried in random
// order, the first one clear of geometry wins. Ours adds an outside-the-door fallback and a person-sized
// clearance box.
//------------------------------------------------------------------------------------------------
class LF_HouseSpawnComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Where a player who owns this house wakes at login (#2652, spec #747; the choice is #2651). Sits on
//! the house entity beside LF_HouseManagerComponent. The pack authors the points (#812); the core
//! holds none. The points are in the house's space, so a moved house keeps them.
class LF_HouseSpawnComponent : ScriptComponent
{
	[Attribute(desc: "Spots inside the house a player may wake at. Tried in random order; the first with room for a person wins.", category: "House spawn")]
	protected ref array<ref PointInfo> m_aSpawnPoints;

	[Attribute(desc: "A spot just outside the front door. Used when no inside point is clear.", category: "House spawn")]
	protected ref PointInfo m_pOutsidePoint;

	//! Half the width of the box a person needs, and its height. The box starts above the floor so the
	//! floor under an authored point does not count as a blocker.
	static const float PERSON_HALF_WIDTH_M = 0.4;
	static const float PERSON_FEET_M = 0.1;
	static const float PERSON_HEIGHT_M = 1.8;

	//------------------------------------------------------------------------------------------------
	//! Server: is there room for a standing person at this spot? Solid geometry or a prop inside the
	//! person box blocks it.
	static bool IsClear(vector spot)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;

		TraceOBB trace = new TraceOBB();
		Math3D.MatrixIdentity3(trace.Mat);
		trace.Start = spot;
		trace.Mins = Vector(-PERSON_HALF_WIDTH_M, PERSON_FEET_M, -PERSON_HALF_WIDTH_M);
		trace.Maxs = Vector(PERSON_HALF_WIDTH_M, PERSON_HEIGHT_M, PERSON_HALF_WIDTH_M);
		trace.Flags = TraceFlags.ENTS;
		trace.LayerMask = EPhysicsLayerPresets.Projectile;
		world.TracePosition(trace, null);
		return !trace.TraceEnt;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the first clear spot of a set tried in random order, else the outside point.
	//! \param inside Candidate world positions inside the house.
	//! \param outside World position outside the door; ignored unless hasOutside.
	//! \param hasOutside True when the house has an outside point.
	//! \param spot Out: the chosen spot.
	//! \param startIndex Where in the set the walk starts (random in play; a test fixes it). Negative: random.
	//! \return False when nothing is clear and there is no outside point.
	static bool Choose(notnull array<vector> inside, vector outside, bool hasOutside, out vector spot, int startIndex = -1)
	{
		int count = inside.Count();
		if (count > 0)
		{
			int start = startIndex;
			if (start < 0)
				start = Math.RandomInt(0, count);

			for (int i = 0; i < count; i++)
			{
				vector candidate = inside[(start + i) % count];
				if (IsClear(candidate))
				{
					spot = candidate;
					return true;
				}
			}
		}

		if (!hasOutside)
			return false;

		spot = outside;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Every authored point in world space, inside points then the outside one, with a name each. The
	//! spawn-point check (LF_SpawnPointAudit) measures all of them, not just the one a login picks.
	void GetAllPoints(notnull array<vector> outPoints, notnull array<string> outNames)
	{
		IEntity owner = GetOwner();
		if (!owner)
			return;

		if (m_aSpawnPoints)
		{
			foreach (int i, PointInfo point : m_aSpawnPoints)
			{
				if (!point)
					continue;

				vector mat[4];
				point.Init(owner);
				point.GetWorldTransform(mat);
				outPoints.Insert(mat[3]);
				outNames.Insert(string.Format("inside %1", i));
			}
		}

		if (m_pOutsidePoint)
		{
			vector outsideMat[4];
			m_pOutsidePoint.Init(owner);
			m_pOutsidePoint.GetWorldTransform(outsideMat);
			outPoints.Insert(outsideMat[3]);
			outNames.Insert("outside");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a spot and heading for a player waking in this house.
	//! \return False when the house has no authored point or nothing is usable.
	bool GetSpawnPoint(out vector position, out vector yawPitchRoll)
	{
		IEntity owner = GetOwner();
		if (!owner)
			return false;

		array<vector> inside = {};
		array<vector> headings = {};
		if (m_aSpawnPoints)
		{
			foreach (PointInfo point : m_aSpawnPoints)
			{
				if (!point)
					continue;

				vector mat[4];
				point.Init(owner);
				point.GetWorldTransform(mat);
				inside.Insert(mat[3]);
				headings.Insert(Math3D.MatrixToAngles(mat));
			}
		}

		vector outside;
		vector outsideHeading;
		bool hasOutside = m_pOutsidePoint != null;
		if (hasOutside)
		{
			vector outsideMat[4];
			m_pOutsidePoint.Init(owner);
			m_pOutsidePoint.GetWorldTransform(outsideMat);
			outside = outsideMat[3];
			outsideHeading = Math3D.MatrixToAngles(outsideMat);
		}

		if (inside.IsEmpty() && !hasOutside)
			return false;

		vector spot;
		if (!Choose(inside, outside, hasOutside, spot))
			return false;

		position = spot;
		yawPitchRoll = outsideHeading;
		int index = inside.Find(spot);
		if (index >= 0)
			yawPitchRoll = headings[index];

		return true;
	}
}
