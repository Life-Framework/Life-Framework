//------------------------------------------------------------------------------------------------
enum LF_EEventAbortKind
{
	HOUR_IN_RANGE,		//!< Fires while the in-game hour is in [m_fFromHour, m_fToHour): the night is over.
	PLAYERS_BELOW,		//!< Fires while fewer than m_iMinPlayers are connected.
	NEARBY_BELOW,		//!< Fires while fewer than m_iMinPlayers are near the event place.
	PHASE_TOO_LONG		//!< Fires once the running phase has lasted m_fSeconds.
}

//------------------------------------------------------------------------------------------------
//! One abort module (#1949). Any one that fires ends the event into cooldown (OR,
//! LF_EventPhaseRules.AnyHolds). Same measurements as the conditions (LF_EventContext).
[BaseContainerProps()]
class LF_EventAbort
{
	[Attribute("0", UIWidgets.ComboBox, "What ends the event", enumType: LF_EEventAbortKind)]
	LF_EEventAbortKind m_eKind;

	[Attribute("0", UIWidgets.Auto, "HOUR_IN_RANGE: the hour the window opens (0 to 24). From equal to To means always", params: "0 24")]
	float m_fFromHour;

	[Attribute("24", UIWidgets.Auto, "HOUR_IN_RANGE: the hour the window closes (0 to 24). A To before From wraps past midnight", params: "0 24")]
	float m_fToHour;

	[Attribute("1", UIWidgets.Auto, "PLAYERS_BELOW and NEARBY_BELOW: the event ends while fewer players than this", params: "0 128")]
	int m_iMinPlayers;

	[Attribute("0", UIWidgets.Auto, "PHASE_TOO_LONG: seconds the phase may last", params: "0 86400")]
	float m_fSeconds;

	//------------------------------------------------------------------------------------------------
	static LF_EventAbort MakeHourRange(float fromHour, float toHour)
	{
		LF_EventAbort a = new LF_EventAbort();
		a.m_eKind = LF_EEventAbortKind.HOUR_IN_RANGE;
		a.m_fFromHour = fromHour;
		a.m_fToHour = toHour;
		return a;
	}

	//------------------------------------------------------------------------------------------------
	static LF_EventAbort MakePlayers(LF_EEventAbortKind kind, int minPlayers)
	{
		LF_EventAbort a = new LF_EventAbort();
		a.m_eKind = kind;
		a.m_iMinPlayers = minPlayers;
		return a;
	}

	//------------------------------------------------------------------------------------------------
	static LF_EventAbort MakePhaseLimit(float seconds)
	{
		LF_EventAbort a = new LF_EventAbort();
		a.m_eKind = LF_EEventAbortKind.PHASE_TOO_LONG;
		a.m_fSeconds = seconds;
		return a;
	}

	//------------------------------------------------------------------------------------------------
	//! \return true when this abort fires for the measurements. A missing context never fires: an
	//! unmeasured tick must not end an event.
	bool Fires(LF_EventContext ctx)
	{
		if (!ctx)
			return false;

		switch (m_eKind)
		{
			case LF_EEventAbortKind.HOUR_IN_RANGE:
				return LF_EventPrecondition.HourInRange(ctx.m_fHour, m_fFromHour, m_fToHour);
			case LF_EEventAbortKind.PLAYERS_BELOW:
				return ctx.m_iPlayersOnline < m_iMinPlayers;
			case LF_EEventAbortKind.NEARBY_BELOW:
				return ctx.m_iPlayersNearby < m_iMinPlayers;
			case LF_EEventAbortKind.PHASE_TOO_LONG:
				return ctx.m_fNow - ctx.m_fPhaseStartedAt >= m_fSeconds;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return true when any abort fires (OR). No aborts is false.
	static bool AnyFires(array<ref LF_EventAbort> aborts, LF_EventContext ctx)
	{
		array<bool> results = {};
		if (aborts)
		{
			foreach (LF_EventAbort a : aborts)
			{
				if (a)
					results.Insert(a.Fires(ctx));
			}
		}
		return LF_EventPhaseRules.AnyHolds(results);
	}
}
