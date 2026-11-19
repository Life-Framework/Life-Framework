//------------------------------------------------------------------------------------------------
//! One timed round on a range lane (#3184): who started it, how many targets, which are down, the
//! clock. Pure: no entity, no world. Times are world milliseconds passed in by the caller.
class LF_RangeRound : Managed
{
	static const int LIMIT_MS = 60000;	// lf-fixed: the card's round length, 60 s

	string m_sLane;
	string m_sShooter;
	int m_iPlayerId;
	int m_iTargets;
	int m_iStartMs;
	int m_iEndMs;
	bool m_bOver;
	ref array<string> m_aDown = {};

	//------------------------------------------------------------------------------------------------
	void LF_RangeRound(string lane, string shooter, int playerId, int targets, int nowMs)
	{
		m_sLane = lane;
		m_sShooter = shooter;
		m_iPlayerId = playerId;
		m_iTargets = targets;
		m_iStartMs = nowMs;
		m_iEndMs = nowMs;
	}

	//------------------------------------------------------------------------------------------------
	bool IsOver()
	{
		return m_bOver;
	}

	//------------------------------------------------------------------------------------------------
	int Hits()
	{
		return m_aDown.Count();
	}

	//------------------------------------------------------------------------------------------------
	int Targets()
	{
		return m_iTargets;
	}

	//------------------------------------------------------------------------------------------------
	//! True when every target of the lane went down before the limit.
	bool Cleared()
	{
		return m_bOver && m_iTargets > 0 && m_aDown.Count() >= m_iTargets;
	}

	//------------------------------------------------------------------------------------------------
	//! The round's length: start to the last hit on a clear, the full limit on a timeout.
	int ElapsedMs()
	{
		return m_iEndMs - m_iStartMs;
	}

	//------------------------------------------------------------------------------------------------
	//! Ends the round when the limit has passed. \return True when this call ended it.
	bool Tick(int nowMs)
	{
		if (m_bOver)
			return false;

		if (nowMs - m_iStartMs < LIMIT_MS)
			return false;

		m_bOver = true;
		m_iEndMs = m_iStartMs + LIMIT_MS;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A target went down. Counted only from the shooter who started the round, once per target,
	//! before the limit. \return True when the hit counted.
	bool RecordHit(string shooter, string targetKey, int nowMs)
	{
		Tick(nowMs);
		if (m_bOver)
			return false;

		if (shooter != m_sShooter || targetKey.IsEmpty())
			return false;

		if (m_aDown.Find(targetKey) != -1)
			return false;

		m_aDown.Insert(targetKey);
		if (m_aDown.Count() >= m_iTargets)
		{
			m_bOver = true;
			m_iEndMs = nowMs;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Best clear time in ms per player record key, for the session (no save: a prize or a board is a
//! follow-up card).
class LF_RangeBests
{
	protected static ref map<string, int> s_mBest;

	//------------------------------------------------------------------------------------------------
	//! \return The best clear time in ms, 0 when the key has none.
	static int Get(string key)
	{
		if (!s_mBest || key.IsEmpty())
			return 0;

		int best;
		if (!s_mBest.Find(key, best))
			return 0;

		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when `ms` is a new best: the first one, or strictly faster than the stored one.
	static bool Offer(string key, int ms)
	{
		if (key.IsEmpty() || ms <= 0)
			return false;

		if (!s_mBest)
			s_mBest = new map<string, int>();

		int best;
		if (s_mBest.Find(key, best) && best <= ms)
			return false;

		s_mBest.Set(key, ms);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mBest = null;
	}
}

//------------------------------------------------------------------------------------------------
enum LF_ERangeStart
{
	OK,
	RUNNING,
	NO_TARGETS,
	NO_SHOOTER
}

//------------------------------------------------------------------------------------------------
//! The server side of the range (#3184): start a round on a lane, count hits, end on all-down or
//! the time limit, toast the result and keep the bests. One round per lane at a time. Lanes are
//! found by component (LF_RangeLaneComponent), never by coordinates or names.
class LF_RangeRun
{
	static const string SYSTEM = "Range";
	static const string FEATURE = "Range";
	static const int TICK_MS = 250;			// lf-fixed: how often a running round checks its clock
	static const float TOAST_SECONDS = 6.0;	// lf-fixed: long enough to read the result line

	protected static ref map<string, ref LF_RangeRound> s_mRounds;

	//------------------------------------------------------------------------------------------------
	static bool IsRunning(string laneId)
	{
		if (!s_mRounds)
			return false;

		return s_mRounds.Contains(laneId);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The distinct targets the running round on this lane has counted, 0 with no round.
	static int HitsOf(string laneId)
	{
		LF_RangeRound round = RoundOf(laneId);
		if (!round)
			return 0;

		return round.Hits();
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_RangeRound RoundOf(string laneId)
	{
		if (!s_mRounds)
			return null;

		return s_mRounds.Get(laneId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server. The action's entry: the user's record key and player id, the world clock.
	static LF_ERangeStart Start(IEntity user, string laneId)
	{
		if (!user || !GetGame() || !GetGame().GetWorld())
			return LF_ERangeStart.NO_SHOOTER;

		string key = LF_Utils.GetPlayerUID(user);
		if (key.IsEmpty())
			return LF_ERangeStart.NO_SHOOTER;

		int playerId = 0;
		if (GetGame().GetPlayerManager())
			playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(user);

		return StartAs(laneId, key, playerId, GetGame().GetWorld().GetWorldTime());
	}

	//------------------------------------------------------------------------------------------------
	//! Server. Starts a round for `shooterKey` on the lane: raises every target and holds them down
	//! when hit, until the round ends. Refused while a round runs on that lane.
	static LF_ERangeStart StartAs(string laneId, string shooterKey, int playerId, int nowMs)
	{
		if (!Replication.IsServer())
			return LF_ERangeStart.NO_SHOOTER;

		if (LF_Systems.Refuses(SYSTEM, "range round start"))
			return LF_ERangeStart.NO_SHOOTER;

		if (shooterKey.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, "round start refused: no record key for the shooter");
			return LF_ERangeStart.NO_SHOOTER;
		}

		if (IsRunning(laneId))
		{
			LF_Debug.Info(FEATURE, string.Format("round start on lane %1 refused for %2: a round is running", laneId, shooterKey));
			return LF_ERangeStart.RUNNING;
		}

		array<SCR_FiringRangeTarget> targets = {};
		LF_RangeLaneComponent.TargetsOf(laneId, targets);
		if (targets.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, string.Format("round start refused: lane %1 has no targets", laneId));
			return LF_ERangeStart.NO_TARGETS;
		}

		foreach (SCR_FiringRangeTarget target : targets)
		{
			target.SetAutoResetTarget(false);
			target.SetState(ETargetState.TARGET_UP);
		}

		if (!s_mRounds)
			s_mRounds = new map<string, ref LF_RangeRound>();

		s_mRounds.Set(laneId, new LF_RangeRound(laneId, shooterKey, playerId, targets.Count(), nowMs));
		if (s_mRounds.Count() == 1 && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().CallLater(TickNow, TICK_MS, true);

		LF_Debug.Info(FEATURE, string.Format("round started on lane %1 by %2: %3 targets", laneId, shooterKey, targets.Count()));
		Toast(playerId, LF_Terms.Text("#LF-Range_Started", targets.Count().ToString(), (LF_RangeRound.LIMIT_MS / 1000).ToString()));
		return LF_ERangeStart.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Server. A target of a lane went down because `shooterKey` hit it.
	static void OnHit(SCR_FiringRangeTarget target, string shooterKey, int nowMs)
	{
		if (!target || !s_mRounds || s_mRounds.IsEmpty())
			return;

		LF_RangeLaneComponent lane = LF_RangeLaneComponent.Cast(target.FindComponent(LF_RangeLaneComponent));
		if (!lane)
			return;

		LF_RangeRound round = RoundOf(lane.GetLaneId());
		if (!round)
			return;

		round.RecordHit(shooterKey, target.GetID().ToString(), nowMs);
		if (round.IsOver())
			Finish(round);
	}

	//------------------------------------------------------------------------------------------------
	//! Server. The callqueue's clock check while any round runs.
	static void TickNow()
	{
		if (!GetGame() || !GetGame().GetWorld())
			return;

		TickAt(GetGame().GetWorld().GetWorldTime());
	}

	//------------------------------------------------------------------------------------------------
	static void TickAt(int nowMs)
	{
		if (!s_mRounds || s_mRounds.IsEmpty())
			return;

		array<ref LF_RangeRound> rounds = {};
		foreach (string laneId, LF_RangeRound round : s_mRounds)
			rounds.Insert(round);

		foreach (LF_RangeRound each : rounds)
		{
			each.Tick(nowMs);
			if (each.IsOver())
				Finish(each);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Ends a round: the lane is free, the targets pop back up by themselves, the player is told.
	protected static void Finish(LF_RangeRound round)
	{
		if (!s_mRounds || s_mRounds.Get(round.m_sLane) != round)
			return;

		s_mRounds.Remove(round.m_sLane);
		if (s_mRounds.IsEmpty() && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(TickNow);

		array<SCR_FiringRangeTarget> targets = {};
		LF_RangeLaneComponent.TargetsOf(round.m_sLane, targets);
		foreach (SCR_FiringRangeTarget target : targets)
			target.SetAutoResetTarget(true);

		string line;
		if (round.Cleared())
		{
			bool isBest = LF_RangeBests.Offer(round.m_sShooter, round.ElapsedMs());
			string best = SecondsText(LF_RangeBests.Get(round.m_sShooter));
			line = LF_Terms.Text("#LF-Range_Result", round.Hits().ToString(), round.Targets().ToString(), SecondsText(round.ElapsedMs()), best);
			LF_Debug.Info(FEATURE, string.Format("round on lane %1 by %2 cleared %3/%3 in %4 ms (new best %5)", round.m_sLane, round.m_sShooter, round.Hits(), round.ElapsedMs(), isBest));
		}
		else
		{
			line = LF_Terms.Text("#LF-Range_TimeUp", round.Hits().ToString(), round.Targets().ToString(), SecondsText(round.ElapsedMs()));
			LF_Debug.Info(FEATURE, string.Format("round on lane %1 by %2 timed out at %3/%4", round.m_sLane, round.m_sShooter, round.Hits(), round.Targets()));
		}

		Toast(round.m_iPlayerId, line);
	}

	//------------------------------------------------------------------------------------------------
	//! 14200 -> "14.2".
	static string SecondsText(int ms)
	{
		int tenths = ms / 100;
		return string.Format("%1.%2", tenths / 10, tenths % 10);
	}

	//------------------------------------------------------------------------------------------------
	static void Toast(int playerId, string text)
	{
		if (playerId <= 0 || text.IsEmpty() || !GetGame() || !GetGame().GetPlayerManager())
			return;

		if (GetGame().GetPlayerManager().IsPlayerConnected(playerId))
			LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-Range_Title", text, TOAST_SECONDS);
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		if (s_mRounds && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(TickNow);

		s_mRounds = null;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of the range's statics.
class LF_RangeRun_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_RangeRun";
	}

	override void Apply()
	{
		LF_RangeRun.ResetStatics();
		LF_RangeBests.ResetStatics();
	}
}
