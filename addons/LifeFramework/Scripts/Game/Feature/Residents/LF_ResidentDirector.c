// LF_ResidentDirector.c - which due residents go live when the cap is smaller than the list (#2910,
// epic #1012). Pure rules: the due residents (each with the timetable line and the anchor spot they
// stand on), the player positions and the free slots go in; the ids to spawn, best first, come out.
// The place a player stands in is filled before one they cannot see: a counter worker beside a player
// comes first (they are who the player walks up to), then everyone else nearest a player, ties by id.
// No entity, no world, no state: the spawner owns what is live.

//------------------------------------------------------------------------------------------------
//! One resident due to appear: who, the timetable line they stand for and the spot it resolved to.
class LF_ResidentCandidate
{
	string m_sId;
	ref LF_ResidentSlot m_Slot;
	ref LF_ResidentSpot m_Spot;

	//------------------------------------------------------------------------------------------------
	static LF_ResidentCandidate Make(string id, LF_ResidentSlot slot, LF_ResidentSpot spot)
	{
		LF_ResidentCandidate candidate = new LF_ResidentCandidate();
		candidate.m_sId = id;
		candidate.m_Slot = slot;
		candidate.m_Spot = spot;
		return candidate;
	}
}

//------------------------------------------------------------------------------------------------
class LF_ResidentDirector
{
	//! "Beside a player", horizontal metres: close enough that the player is looking at the counter.
	static const float BESIDE_M = 15;	// lf-fixed: a rule of thumb, not worth a conf yet
	static const string SERVE_ROLE = "work";
	static const string SERVE_ANCHOR = "counter";

	//------------------------------------------------------------------------------------------------
	//! A resident who stands behind a counter: a `work` line on the `counter` anchor.
	static bool IsCounterWorker(LF_ResidentCandidate candidate)
	{
		if (!candidate || !candidate.m_Slot)
			return false;

		return candidate.m_Slot.m_sRole == SERVE_ROLE && candidate.m_Slot.m_sAnchor == SERVE_ANCHOR;
	}

	//------------------------------------------------------------------------------------------------
	//! Horizontal distance from the candidate's spot to the nearest player; a huge number with no player.
	static float NearestPlayer(LF_ResidentCandidate candidate, notnull array<vector> players)
	{
		float best = 1000000;
		if (!candidate || !candidate.m_Spot)
			return best;

		vector at = candidate.m_Spot.m_vOrigin;
		foreach (vector player : players)
		{
			float dx = player[0] - at[0];
			float dz = player[2] - at[2];
			float dist = Math.Sqrt(dx * dx + dz * dz);
			if (dist < best)
				best = dist;
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! The rank of a candidate: a lower number goes live first. A counter worker beside a player is band 0,
	//! everyone else band 1; inside a band the nearest to a player wins (centimetres, so a float wobble
	//! never decides), then the lower id.
	static int Band(LF_ResidentCandidate candidate, float nearest)
	{
		if (IsCounterWorker(candidate) && nearest <= BESIDE_M)
			return 0;

		return 1;
	}

	//------------------------------------------------------------------------------------------------
	//! True when `a` ranks before `b`.
	protected static bool Before(int bandA, int cmA, string idA, int bandB, int cmB, string idB)
	{
		if (bandA != bandB)
			return bandA < bandB;

		if (cmA != cmB)
			return cmA < cmB;

		return idA < idB;
	}

	//------------------------------------------------------------------------------------------------
	//! Chooses up to `slots` of the due residents to go live.
	//! \param[in] due Every resident due with a player in spawn range.
	//! \param[in] players Player positions (world).
	//! \param[in] slots How many may still go live (the cap less the live count); zero or less picks nobody.
	//! \param[out] into The chosen ids, best first. Cleared first.
	//! \return How many were chosen.
	static int Pick(notnull array<ref LF_ResidentCandidate> due, notnull array<vector> players, int slots, notnull array<string> into)
	{
		into.Clear();
		if (slots <= 0 || due.IsEmpty())
			return 0;

		int count = due.Count();
		array<int> bands = {};
		array<int> cms = {};
		array<bool> taken = {};
		for (int i = 0; i < count; i++)
		{
			float nearest = NearestPlayer(due[i], players);
			bands.Insert(Band(due[i], nearest));
			int cm = nearest * 100;	// at most 1e8, inside an int
			cms.Insert(cm);
			taken.Insert(false);
		}

		int want = LF_Utils.MinInt(slots, count);
		for (int n = 0; n < want; n++)
		{
			int best = -1;
			for (int j = 0; j < count; j++)
			{
				if (taken[j] || !due[j])
					continue;

				if (best < 0 || Before(bands[j], cms[j], due[j].m_sId, bands[best], cms[best], due[best].m_sId))
					best = j;
			}

			if (best < 0)
				break;

			taken[best] = true;
			into.Insert(due[best].m_sId);
		}

		return into.Count();
	}
}
