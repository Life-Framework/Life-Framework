// LF_NpcSpots.c - one spot holds one character (#3831). Residents, scene walkers, event crews, hosts and
// quest givers are put into the world by different systems, each reading its own place from data: a
// resident's timetable line on an anchor, a walker on a spawn marker. None of them knew about the others,
// so a third resident on a two-seat bench, a resident on the anchor a scene's walkers stood on, or the
// walkers of two markers a metre apart ended up standing inside each other ("two people spawned inside each
// other"). Every system that puts a character down asks here for a free spot first, and every character
// that arrives on its own (a scene's walkers at the target) is settled off whoever it landed on.
// The data side of the same rule is tools/lib/npc-spots.mjs and `validate-npc-spots` (MIN_M is shared).
// Server side; nothing here persists.

//------------------------------------------------------------------------------------------------
//! The characters a sphere query finds: where each stands. Players count; a player is a body too.
class LF_NpcSpotQuery
{
	EntityID m_Ignore = EntityID.INVALID;
	ref array<vector> m_aAt = {};
	ref array<EntityID> m_aIds = {};

	//------------------------------------------------------------------------------------------------
	bool Add(IEntity entity)
	{
		if (!ChimeraCharacter.Cast(entity) || entity.GetID() == m_Ignore)
			return true;

		m_aAt.Insert(entity.GetOrigin());
		m_aIds.Insert(entity.GetID());
		return true;
	}
}

//------------------------------------------------------------------------------------------------
class LF_NpcSpots
{
	static const string FEATURE = "Npc";
	//! Characters closer than this (metres, horizontal) share a body. A shoulder is about 0.5 m.
	static const float MIN_M = 0.8; // lf-fixed: a body's width plus a hand; the same number the data check uses
	//! The free-spot search steps outward this far per ring.
	static const float STEP_M = 1.0; // lf-fixed: one body and a gap
	static const int RINGS = 3; // lf-fixed: three metres out is still "at the door"
	static const int DIRECTIONS = 8; // lf-fixed: compass points around the wanted spot
	//! Characters this far from the wanted spot cannot touch a candidate within the search.
	static const float QUERY_M = 5; // lf-fixed: RINGS * STEP_M plus the clearance, with room

	//! A body claimed a moment ago is counted even before the world's spatial query sees it.
	static const int CLAIM_MS = 10000; // lf-fixed: longer than the frame a spawn needs to reach the query
	protected static ref array<EntityID> s_aClaimIds;
	protected static ref array<int> s_aClaimTicks;
	protected static int s_iMoved;
	protected static int s_iCrowded;

	//------------------------------------------------------------------------------------------------
	//! Pure: candidate number `index` around `wanted`. 0 is the wanted spot; then ring 1 at STEP_M
	//! (right, left, back, forward, then the four diagonals), ring 2 at twice that, and so on.
	//! `right` and `forward` are the unit axes of the wanted spot's facing.
	static vector Candidate(vector wanted, vector right, vector forward, int index)
	{
		if (index <= 0)
			return wanted;

		int ring = (index - 1) / DIRECTIONS + 1;
		int direction = (index - 1) % DIRECTIONS;
		float reach = ring * STEP_M;
		vector offset;
		switch (direction)
		{
			case 0: offset = right; break;
			case 1: offset = right * -1; break;
			case 2: offset = forward * -1; break;
			case 3: offset = forward; break;
			case 4: offset = right + forward; break;
			case 5: offset = (right + forward) * -1; break;
			case 6: offset = right - forward; break;
			default: offset = forward - right; break;
		}

		if (direction >= 4)
			offset = offset * 0.7071;

		return wanted + offset * reach;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: true when no spot of `taken` lies within `min` metres of `at`, horizontally.
	static bool IsFree(vector at, notnull array<vector> taken, float min = MIN_M)
	{
		foreach (vector other : taken)
		{
			if (vector.DistanceXZ(at, other) < min)
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the first candidate around `wanted` clear of every spot in `taken`, the wanted spot itself
	//! when it is clear. \param[out] found false when every candidate in reach is taken (the wanted spot
	//! comes back).
	static vector PickFree(vector wanted, vector right, vector forward, notnull array<vector> taken, out bool found)
	{
		int count = RINGS * DIRECTIONS + 1;
		for (int i = 0; i < count; i++)
		{
			vector at = Candidate(wanted, right, forward, i);
			if (IsFree(at, taken))
			{
				found = true;
				return at;
			}
		}

		found = false;
		return wanted;
	}

	//------------------------------------------------------------------------------------------------
	//! Where the characters near `at` stand, appended to `into`. `ignore` is left out (the body asking).
	//! \return how many.
	static int Occupants(vector at, float radius, notnull array<vector> into, EntityID ignore = EntityID.INVALID)
	{
		if (!GetGame() || !GetGame().GetWorld())
			return 0;

		LF_NpcSpotQuery query = new LF_NpcSpotQuery();
		query.m_Ignore = ignore;
		GetGame().GetWorld().QueryEntitiesBySphere(at, radius, query.Add, null, EQueryEntitiesFlags.ALL);
		int count = query.m_aAt.Count();
		foreach (vector found : query.m_aAt)
			into.Insert(found);

		// Two characters spawned in one frame: the second asks before the world's query holds the first.
		if (s_aClaimIds)
		{
			int now = System.GetTickCount();
			for (int i = s_aClaimIds.Count() - 1; i >= 0; i--)
			{
				IEntity claimed = GetGame().GetWorld().FindEntityByID(s_aClaimIds[i]);
				if (!claimed || now - s_aClaimTicks[i] > CLAIM_MS)
				{
					s_aClaimIds.Remove(i);
					s_aClaimTicks.Remove(i);
					continue;
				}

				if (s_aClaimIds[i] == ignore || query.m_aIds.Contains(s_aClaimIds[i]))
					continue;

				if (vector.DistanceXZ(claimed.GetOrigin(), at) <= radius)
				{
					into.Insert(claimed.GetOrigin());
					count++;
				}
			}
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! A character that was just put into the world: counted as standing there from now on, even in the
	//! frame before the world's spatial query can find it.
	static void Claim(IEntity body)
	{
		if (!body)
			return;

		if (!s_aClaimIds)
		{
			s_aClaimIds = {};
			s_aClaimTicks = {};
		}

		s_aClaimIds.Insert(body.GetID());
		s_aClaimTicks.Insert(System.GetTickCount());
	}

	//------------------------------------------------------------------------------------------------
	//! True when a character stands within MIN_M of `at` (not counting `ignore`).
	static bool IsOccupied(vector at, EntityID ignore = EntityID.INVALID)
	{
		array<vector> near = {};
		Occupants(at, MIN_M, near, ignore);
		return !near.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! The spot a character about to appear at `wanted` should take: `wanted` when nobody stands on it,
	//! else the nearest free spot around it facing the same way (`yawDegrees`). Everyone standing within
	//! reach counts: residents, scene walkers, placed hosts, players. When every spot in reach is taken
	//! it logs and returns `wanted`, so a crowded square still gets its person.
	//! \param[in] who What is arriving, for the log line.
	static vector Free(vector wanted, float yawDegrees, string who = "", EntityID ignore = EntityID.INVALID)
	{
		array<vector> taken = {};
		if (Occupants(wanted, QUERY_M, taken, ignore) == 0)
			return wanted;

		vector mat[4];
		Math3D.AnglesToMatrix(Vector(yawDegrees, 0, 0), mat);
		bool found;
		vector at = PickFree(wanted, mat[0], mat[2], taken, found);
		if (!found)
		{
			s_iCrowded++;
			LF_Debug.Warn(FEATURE, string.Format("%1: no free spot within %2 m of %3; it stands there anyway", who, RINGS * STEP_M, wanted));
			return wanted;
		}

		if (vector.DistanceXZ(at, wanted) > 0.01)
		{
			s_iMoved++;
			LF_Debug.Info(FEATURE, string.Format("%1: somebody already stands at %2; it takes %3 instead (%4 m)", who, wanted, at, vector.DistanceXZ(at, wanted)));
		}

		return at;
	}

	//------------------------------------------------------------------------------------------------
	//! A character that arrived on its own (a walker at its target) and stands inside another: moves it
	//! to the nearest free spot. \return true when it was moved.
	static bool Settle(IEntity body, string who = "")
	{
		if (!body || !Replication.IsServer())
			return false;

		vector at = body.GetOrigin();
		if (!IsOccupied(at, body.GetID()))
			return false;

		vector mat[4];
		body.GetWorldTransform(mat);
		vector angles = Math3D.MatrixToAngles(mat);
		vector free = Free(at, angles[0], who, body.GetID());
		if (vector.DistanceXZ(free, at) < 0.01)
			return false;

		mat[3] = free;
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(body);
		if (character)
			character.Teleport(mat);
		else
			body.SetWorldTransform(mat);

		Claim(body);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Every pair of characters in the world closer than MIN_M: "idA idB" per pair. The harness reads it
	//! in a session; a test reads it after the systems ran.
	static int Overlaps(notnull array<string> into)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return 0;

		LF_NpcSpotQuery query = new LF_NpcSpotQuery();
		world.QueryEntitiesBySphere("0 0 0", 100000, query.Add, null, EQueryEntitiesFlags.ALL);
		foreach (int i, EntityID idA : query.m_aIds)
		{
			for (int j = i + 1; j < query.m_aIds.Count(); j++)
			{
				if (vector.DistanceXZ(query.m_aAt[i], query.m_aAt[j]) < MIN_M && Math.AbsFloat(query.m_aAt[i][1] - query.m_aAt[j][1]) < 3)
					into.Insert(idA.ToString() + " " + query.m_aIds[j].ToString());
			}
		}

		return into.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! How many arrivals were moved off a taken spot, and how many found none free, since world start.
	static int MovedCount()
	{
		return s_iMoved;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_iMoved = 0;
		s_iCrowded = 0;
		s_aClaimIds = null;
		s_aClaimTicks = null;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_NpcSpots' statics (LF_StaticReset finds it by reflection).
class LF_NpcSpots_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_NpcSpots";
	}

	override void Apply()
	{
		LF_NpcSpots.ResetStatics();
	}
}
