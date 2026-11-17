//------------------------------------------------------------------------------------------------
//! Live map states (#2208, D14). A place belongs to a state, a plain number; the mission header's
//! m_aLF_LiveStates lists the states that are live. A place whose state is not listed does not
//! register in LF_Places, so every system that finds its places there stays dark and players see
//! vanilla terrain. The core knows only the numbers; what each state means (which places, which
//! chapter) lives in the pack. A header with an empty list, or no LF_MissionHeader at all (a bare
//! world, DebugWorld, every test world), has every state live.
//------------------------------------------------------------------------------------------------
class LF_PlaceLiveState
{
	//! A place with this state is always live, whatever the header lists.
	static const int ALWAYS_LIVE = -1;

	//------------------------------------------------------------------------------------------------
	//! Pure: whether a place of state is live, given the header's live list.
	//! \param state The place's state; ALWAYS_LIVE (or any negative) is live under every header.
	//! \param liveStates The header's list; null or empty means every state is live.
	static bool IsLive(int state, array<int> liveStates)
	{
		if (state < 0)
			return true;

		if (!liveStates || liveStates.IsEmpty())
			return true;

		return liveStates.Contains(state);
	}

	//------------------------------------------------------------------------------------------------
	//! \return the running mission header's live list; null when the header has none (all live).
	static array<int> HeaderStates()
	{
		LF_MissionHeader header = LF_MissionHeader.GetCurrent();
		if (!header)
			return null;

		return header.m_aLF_LiveStates;
	}

	//------------------------------------------------------------------------------------------------
	//! Registers entity as kind/id in LF_Places when state is live under liveStates. A dark place logs
	//! one info line and registers nothing.
	//! \return true when the place is live and now holds the key.
	static bool Register(string kind, string id, int state, array<int> liveStates, IEntity entity)
	{
		if (!IsLive(state, liveStates))
		{
			LF_Debug.Info(LF_Places.FEATURE, string.Format("dark: %1 '%2' waits for map state %3 (not live)", kind, id, state));
			return false;
		}

		return LF_Places.Register(kind, id, entity);
	}
}
