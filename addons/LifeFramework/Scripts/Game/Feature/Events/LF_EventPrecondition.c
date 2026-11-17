//------------------------------------------------------------------------------------------------
//! What the event shell (#757) measured this tick, as plain numbers, so condition and abort
//! modules are pure and the LOGIC tier tests them with no world. Server seconds and server hours.
class LF_EventContext
{
	float m_fHour;			//!< In-game hour, 0 to 24 (LF_Atmosphere.HourNow()).
	int m_iPlayersOnline;	//!< Players connected.
	int m_iPlayersNearby;	//!< Players inside the radius of the event place (found by component through LF_Places).
	float m_fNow;			//!< Server seconds.
	float m_fLastEndedAt;	//!< When this event last ended; a negative value means it never has.
	float m_fPhaseStartedAt;//!< When the running phase began.

	//------------------------------------------------------------------------------------------------
	static LF_EventContext Make(float hour, int online, int nearby, float now)
	{
		LF_EventContext ctx = new LF_EventContext();
		ctx.m_fHour = hour;
		ctx.m_iPlayersOnline = online;
		ctx.m_iPlayersNearby = nearby;
		ctx.m_fNow = now;
		ctx.m_fLastEndedAt = -1;
		ctx.m_fPhaseStartedAt = now;
		return ctx;
	}
}

//------------------------------------------------------------------------------------------------
enum LF_EEventConditionKind
{
	HOUR_RANGE,			//!< Holds while the in-game hour is in [m_fFromHour, m_fToHour); may wrap midnight.
	PLAYERS_ONLINE,		//!< Holds while at least m_iMinPlayers are connected.
	PLAYERS_NEARBY,		//!< Holds while at least m_iMinPlayers are near the event place.
	COOLDOWN			//!< Holds once m_fSeconds have passed since the event last ended.
}

//------------------------------------------------------------------------------------------------
//! One condition module (#1949). A phase holds until every one of its conditions holds (AND,
//! LF_EventPhaseRules.AllHold). Data, not code: a pack config says which conditions a phase has.
[BaseContainerProps()]
class LF_EventPrecondition
{
	[Attribute("0", UIWidgets.ComboBox, "What the condition checks", enumType: LF_EEventConditionKind)]
	LF_EEventConditionKind m_eKind;

	[Attribute("0", UIWidgets.Auto, "HOUR_RANGE: the hour the window opens (0 to 24). From equal to To means always", params: "0 24")]
	float m_fFromHour;

	[Attribute("24", UIWidgets.Auto, "HOUR_RANGE: the hour the window closes (0 to 24). A To before From wraps past midnight (22 to 5)", params: "0 24")]
	float m_fToHour;

	[Attribute("1", UIWidgets.Auto, "PLAYERS_ONLINE and PLAYERS_NEARBY: the fewest players that satisfy it", params: "0 128")]
	int m_iMinPlayers;

	[Attribute("0", UIWidgets.Auto, "COOLDOWN: seconds that must pass since the event last ended", params: "0 604800")]
	float m_fSeconds;

	//------------------------------------------------------------------------------------------------
	static LF_EventPrecondition MakeHourRange(float fromHour, float toHour)
	{
		LF_EventPrecondition c = new LF_EventPrecondition();
		c.m_eKind = LF_EEventConditionKind.HOUR_RANGE;
		c.m_fFromHour = fromHour;
		c.m_fToHour = toHour;
		return c;
	}

	//------------------------------------------------------------------------------------------------
	static LF_EventPrecondition MakePlayers(LF_EEventConditionKind kind, int minPlayers)
	{
		LF_EventPrecondition c = new LF_EventPrecondition();
		c.m_eKind = kind;
		c.m_iMinPlayers = minPlayers;
		return c;
	}

	//------------------------------------------------------------------------------------------------
	static LF_EventPrecondition MakeCooldown(float seconds)
	{
		LF_EventPrecondition c = new LF_EventPrecondition();
		c.m_eKind = LF_EEventConditionKind.COOLDOWN;
		c.m_fSeconds = seconds;
		return c;
	}

	//------------------------------------------------------------------------------------------------
	//! \return true when the in-game hour is inside [from, to). Equal bounds mean always; to < from
	//! wraps midnight. Out-of-range hours are wrapped into [0, 24).
	static bool HourInRange(float hour, float fromHour, float toHour)
	{
		return LF_AtmosphereMix.InHours(LF_DayCycle.WrapHour(hour), LF_DayCycle.WrapHour(fromHour), LF_DayCycle.WrapHour(toHour));
	}

	//------------------------------------------------------------------------------------------------
	//! \return true when this condition holds for the measurements. A missing context never holds.
	bool Holds(LF_EventContext ctx)
	{
		if (!ctx)
			return false;

		switch (m_eKind)
		{
			case LF_EEventConditionKind.HOUR_RANGE:
				return HourInRange(ctx.m_fHour, m_fFromHour, m_fToHour);
			case LF_EEventConditionKind.PLAYERS_ONLINE:
				return ctx.m_iPlayersOnline >= m_iMinPlayers;
			case LF_EEventConditionKind.PLAYERS_NEARBY:
				return ctx.m_iPlayersNearby >= m_iMinPlayers;
			case LF_EEventConditionKind.COOLDOWN:
				// Never ended (negative) is over: the first run is not held back.
				return ctx.m_fLastEndedAt < 0 || ctx.m_fNow - ctx.m_fLastEndedAt >= m_fSeconds;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return true when every condition holds (AND). No conditions holds.
	static bool AllHold(array<ref LF_EventPrecondition> conditions, LF_EventContext ctx)
	{
		array<bool> results = {};
		if (conditions)
		{
			foreach (LF_EventPrecondition c : conditions)
			{
				if (!c)
				{
					LF_Debug.Warn("Events", "a null condition module holds nothing");
					results.Insert(false);
					continue;
				}
				results.Insert(c.Holds(ctx));
			}
		}
		return LF_EventPhaseRules.AllHold(results);
	}
}
