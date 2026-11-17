//------------------------------------------------------------------------------------------------
//! Per-faction standing (#2696, docs/architecture/reputation.md). The code is Reputation; players
//! see "standing" and never a number. Server-only state: points per character and faction (town,
//! police, underworld), keyed by the character record key (LF_Utils.GetPlayerUID).
//!
//! TIERS. The store holds points; the tier is derived from the faction's thresholds on every read
//! (LF_ReputationFactionDef.TierFor), so a retuned table reaches existing characters. Points clamp
//! to the first and last threshold.
//!
//! EARNING. Only server code changes points: Add (a signed amount from a quest or dialogue) or Award
//! (an action id whose faction, points and cooldown are config). A client never sends standing.
//! - Cooldown: an action's m_fCooldownSeconds per character; an award inside it does nothing.
//! - Daily cap: a faction's m_iDailyCap gain per character per in-game day (2 real hours). The day
//!   key is compared on the next gain, there is no tick. A loss is never capped. Both clocks are
//!   session state and are not saved.
//!
//! EVENTS. LF_OnReputationChanged (LF_ReputationEvents.c) only when a tier changes.
//! Not here yet: persistence, event subscriptions, gates, the owner RPC (later cards of #1018).
//------------------------------------------------------------------------------------------------
class LF_Reputation
{
	static const string FEATURE = "Reputation";

	static const string FACTION_TOWN = "TOWN";
	static const string FACTION_POLICE = "POLICE";
	static const string FACTION_UNDERWORLD = "UNDERWORLD";

	//! Seconds in one in-game day (2 real hours, the M4 decision) for the daily cap.
	static const float DAY_SECONDS = 7200;	// lf-fixed: the length of an in-game day, the cap itself is config

	//! uid -> faction -> points.
	protected static ref map<string, ref map<string, int>> s_mPoints;
	//! uid|action -> world time (s) of the last award. Session-only.
	protected static ref map<string, float> s_mLastAward;
	//! uid|faction -> points gained on day s_mCapDay[key]. Session-only.
	protected static ref map<string, int> s_mGainedToday;
	protected static ref map<string, int> s_mCapDay;

	//------------------------------------------------------------------------------------------------
	//! World end (LF_StaticReset).
	static void ResetStatics()
	{
		s_mPoints = null;
		s_mLastAward = null;
		s_mGainedToday = null;
		s_mCapDay = null;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The faction's ladder from the loaded config, or null.
	static LF_ReputationFactionDef FactionDef(string faction)
	{
		LF_ReputationConfig config = LF_ReputationConfig.Get();
		if (!config)
			return null;

		return config.FindFaction(faction);
	}

	//------------------------------------------------------------------------------------------------
	//! \return A character's points with a faction; 0 when none.
	static int Points(string playerUid, string faction)
	{
		if (!s_mPoints || playerUid.IsEmpty())
			return 0;

		map<string, int> held = s_mPoints.Get(playerUid);
		if (!held)
			return 0;

		return held.Get(faction);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The character's tier with a faction (-2..3 with the shipped table); 0 for an unknown
	//! faction, an unknown character or an empty uid.
	static int Tier(string playerUid, string faction)
	{
		LF_ReputationFactionDef def = FactionDef(faction);
		if (!def)
			return 0;

		return def.TierFor(Points(playerUid, faction));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: changes one faction's points by a signed amount. A gain is cut to what the faction's daily
	//! cap still allows; a loss is not. The result clamps to the first and last threshold.
	//! \param[in] reason What moved it, for the log ("quest", "job_completed:PARCEL", ...).
	//! \return The points actually changed (signed); 0 when nothing moved or the call was refused.
	static int Add(string playerUid, string faction, int points, string reason)
	{
		if (Replication.IsRunning() && !Replication.IsServer())
			return 0;

		if (playerUid.IsEmpty())
		{
			LF_Debug.Error(FEATURE, string.Format("standing refused: empty player id (faction %1, %2)", faction, reason));
			return 0;
		}

		LF_ReputationFactionDef def = FactionDef(faction);
		if (!def)
		{
			LF_Debug.Warn(FEATURE, string.Format("standing skipped: faction '%1' is not in the reputation config (%2 for %3, %4)", faction, points, playerUid, reason));
			return 0;
		}

		if (points == 0)
		{
			LF_Debug.Info(FEATURE, string.Format("standing skipped: a change of 0 for %1 with %2 (%3)", playerUid, faction, reason));
			return 0;
		}

		int before = Points(playerUid, faction);
		int wanted = points;
		if (wanted > 0)
			wanted = CapToday(playerUid, def, wanted);

		int after = def.Clamp(before + wanted);
		int moved = after - before;
		if (wanted > 0 && moved < wanted)
			ReturnCap(playerUid, def, wanted - moved);

		if (moved == 0)
		{
			LF_Debug.Info(FEATURE, string.Format("standing unchanged: %1 %2 at %3, %4 moved nothing (cap %5 a day, %6)", playerUid, faction, before, points, def.m_iDailyCap, reason));
			return 0;
		}

		int tierBefore = def.TierFor(before);
		int tierAfter = def.TierFor(after);
		SetPoints(playerUid, faction, after);
		LF_Debug.Info(FEATURE, string.Format("%1 %2 %3 -> %4 (%5)", playerUid, faction, before, after, reason));
		if (tierAfter != tierBefore)
		{
			LF_Debug.Info(FEATURE, string.Format("%1 %2 tier %3 -> %4", playerUid, faction, tierBefore, tierAfter));
			LF_EventBus.Publish(LF_OnReputationChanged.Create(playerUid, faction, tierBefore, tierAfter));
		}

		return moved;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: awards an in-world action's points (config: faction, points per unit, cooldown).
	//! \param[in] units How many of the action; the points are per unit.
	//! \return Points actually changed (signed); 0 for an unknown action, on cooldown or capped.
	static int Award(string playerUid, string action, int units = 1)
	{
		if (Replication.IsRunning() && !Replication.IsServer())
			return 0;

		LF_ReputationConfig config = LF_ReputationConfig.Get();
		LF_ReputationAwardDef award;
		if (config)
			award = config.FindAward(action);

		if (!award)
		{
			LF_Debug.Warn(FEATURE, string.Format("standing skipped: action '%1' has no row in the reputation config", action));
			return 0;
		}

		if (playerUid.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, string.Format("standing skipped: action '%1' awarded to a body with no player id", action));
			return 0;
		}

		if (units <= 0 || award.m_iPoints == 0)
		{
			LF_Debug.Info(FEATURE, string.Format("standing skipped: action '%1' for %2 is worth nothing (%3 unit(s) of %4)", action, playerUid, units, award.m_iPoints));
			return 0;
		}

		float now = LF_JobGuards.Now();
		string key = playerUid + "|" + action;
		if (!s_mLastAward)
			s_mLastAward = new map<string, float>();

		if (award.m_fCooldownSeconds > 0 && s_mLastAward.Contains(key))
		{
			float since = now - s_mLastAward.Get(key);
			if (since < award.m_fCooldownSeconds)
			{
				LF_Debug.Info(FEATURE, string.Format("standing on cooldown: %1 %2 again after %3 s (%4 s since the last)", playerUid, action, award.m_fCooldownSeconds, since));
				return 0;
			}
		}

		// The cooldown starts only when points moved: a capped or refused award leaves it open.
		int moved = Add(playerUid, award.m_sFaction, award.m_iPoints * units, action);
		if (moved != 0)
			s_mLastAward.Set(key, now);

		return moved;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The part of `amount` the faction's daily cap still allows, and books it. Today is the
	//! in-game day since boot; a stored day that is not today starts from 0 gained.
	protected static int CapToday(string playerUid, notnull LF_ReputationFactionDef def, int amount)
	{
		if (def.m_iDailyCap <= 0)
			return amount;

		if (!s_mGainedToday)
			s_mGainedToday = new map<string, int>();
		if (!s_mCapDay)
			s_mCapDay = new map<string, int>();

		string key = playerUid + "|" + def.m_sId;
		int today = Math.Floor(LF_JobGuards.Now() / DAY_SECONDS);
		if (!s_mCapDay.Contains(key) || s_mCapDay.Get(key) != today)
		{
			s_mCapDay.Set(key, today);
			s_mGainedToday.Set(key, 0);
		}

		int gained = s_mGainedToday.Get(key);
		int granted = def.m_iDailyCap - gained;
		if (amount < granted)
			granted = amount;
		if (granted <= 0)
			return 0;

		s_mGainedToday.Set(key, gained + granted);
		return granted;
	}

	//------------------------------------------------------------------------------------------------
	//! Gives back cap booked for points the clamp then did not add, so a character at the top of the
	//! ladder does not burn the day's allowance.
	protected static void ReturnCap(string playerUid, notnull LF_ReputationFactionDef def, int unused)
	{
		if (def.m_iDailyCap <= 0 || !s_mGainedToday)
			return;

		string key = playerUid + "|" + def.m_sId;
		if (s_mGainedToday.Contains(key))
			s_mGainedToday.Set(key, Math.Max(0, s_mGainedToday.Get(key) - unused));
	}

	//------------------------------------------------------------------------------------------------
	protected static void SetPoints(string playerUid, string faction, int points)
	{
		if (!s_mPoints)
			s_mPoints = new map<string, ref map<string, int>>();

		map<string, int> held = s_mPoints.Get(playerUid);
		if (!held)
		{
			held = new map<string, int>();
			s_mPoints.Set(playerUid, held);
		}

		held.Set(faction, points);
	}

	//------------------------------------------------------------------------------------------------
	//! Drops a character's points and session clocks (test cleanup, admin wipe).
	static void Forget(string playerUid)
	{
		if (s_mPoints)
			s_mPoints.Remove(playerUid);

		ForgetTimes(s_mLastAward, playerUid);
		ForgetCounts(s_mGainedToday, playerUid);
		ForgetCounts(s_mCapDay, playerUid);
	}

	//------------------------------------------------------------------------------------------------
	protected static void ForgetTimes(map<string, float> clocks, string playerUid)
	{
		if (!clocks)
			return;

		array<string> keys = {};
		foreach (string key, float value : clocks)
		{
			if (key.StartsWith(playerUid + "|"))
				keys.Insert(key);
		}

		foreach (string key : keys)
		{
			clocks.Remove(key);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void ForgetCounts(map<string, int> clocks, string playerUid)
	{
		if (!clocks)
			return;

		array<string> keys = {};
		foreach (string key, int value : clocks)
		{
			if (key.StartsWith(playerUid + "|"))
				keys.Insert(key);
		}

		foreach (string key : keys)
		{
			clocks.Remove(key);
		}
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_Reputation's statics (registered at script load).
class LF_Reputation_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_Reputation_StaticResetHook());

	override string Owner()
	{
		return "LF_Reputation";
	}

	override void Apply()
	{
		LF_Reputation.ResetStatics();
	}
}
