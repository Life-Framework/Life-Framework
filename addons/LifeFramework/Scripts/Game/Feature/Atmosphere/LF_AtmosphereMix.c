//------------------------------------------------------------------------------------------------
//! One emitter as the mix sees it on one tick. Plain data, so the choice below is testable without
//! a world or a sound system.
class LF_AmbientCandidate
{
	int m_iId;
	//! Metres from the listener to the emitter.
	float m_fDistance;
	//! The emitter starts inside this distance.
	float m_fRadius;
	//! A playing emitter keeps playing this much further out, so a listener on the edge does not
	//! make it start and stop.
	float m_fMargin;
	//! Higher wins a voice when more emitters are in range than the cap allows.
	int m_iPriority;
	//! Emitters that share a group share one voice: the best of them plays. Empty is no group.
	string m_sGroup;
	//! Its condition and its hours both say it may sound now.
	bool m_bAllowed;
	bool m_bPlaying;
}

//------------------------------------------------------------------------------------------------
//! Which ambient emitters sound on this machine. LF_Atmosphere feeds it distances and states twice
//! a second and starts or stops voices to match the answer.
class LF_AtmosphereMix
{
	//------------------------------------------------------------------------------------------------
	//! True when `hour` (0 to 24) is inside the window. from == to is "always"; a window may wrap
	//! past midnight (22 to 5).
	static bool InHours(float hour, float from, float to)
	{
		if (from == to)
			return true;

		if (from < to)
			return hour >= from && hour < to;

		return hour >= from || hour < to;
	}

	//------------------------------------------------------------------------------------------------
	//! In range: inside the radius, or inside the radius plus the margin when it already plays.
	static bool InRange(notnull LF_AmbientCandidate c)
	{
		if (!c.m_bAllowed)
			return false;

		float limit = c.m_fRadius;
		if (c.m_bPlaying)
			limit += c.m_fMargin;

		return c.m_fDistance <= limit;
	}

	//------------------------------------------------------------------------------------------------
	//! True when a deserves a voice before b: priority first, then the nearer one. A playing emitter
	//! counts as its margin nearer, so two emitters at about the same distance do not swap every tick.
	static bool Better(notnull LF_AmbientCandidate a, notnull LF_AmbientCandidate b)
	{
		if (a.m_iPriority != b.m_iPriority)
			return a.m_iPriority > b.m_iPriority;

		float da = a.m_fDistance;
		if (a.m_bPlaying)
			da -= a.m_fMargin;

		float db = b.m_fDistance;
		if (b.m_bPlaying)
			db -= b.m_fMargin;

		if (da != db)
			return da < db;

		return a.m_iId < b.m_iId;
	}

	//------------------------------------------------------------------------------------------------
	//! Fills outIds with the ids that should sound: in range and allowed, one per group, at most
	//! `cap` in all, best first.
	static void Select(notnull array<ref LF_AmbientCandidate> all, int cap, notnull array<int> outIds)
	{
		outIds.Clear();

		array<LF_AmbientCandidate> winners = {};
		foreach (LF_AmbientCandidate c : all)
		{
			if (!c || !InRange(c))
				continue;

			int rival = -1;
			if (!c.m_sGroup.IsEmpty())
			{
				for (int i = 0; i < winners.Count(); i++)
				{
					if (winners[i].m_sGroup == c.m_sGroup)
					{
						rival = i;
						break;
					}
				}
			}

			if (rival < 0)
				winners.Insert(c);
			else if (Better(c, winners[rival]))
				winners[rival] = c;
		}

		while (outIds.Count() < cap && !winners.IsEmpty())
		{
			int best = 0;
			for (int i = 1; i < winners.Count(); i++)
			{
				if (Better(winners[i], winners[best]))
					best = i;
			}

			outIds.Insert(winners[best].m_iId);
			winners.Remove(best);
		}
	}
}
