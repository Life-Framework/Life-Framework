//------------------------------------------------------------------------------------------------
//! The apprentice bonus (0.3.0 A10, #4407): a character of level 10 or more in a trade who earns XP
//! within 15 m of a character under level 10 in the same trade adds a small bonus to both of them, a
//! master teaching a learner. The award can be either one's. The bonus is 10% of the award (at least
//! 1) and a pair earns it at most once per 60 s. It is paid through `LF_JobLevels.AddXp` with the
//! reason `apprentice`, and an award with that reason never starts another, so the bonus cannot loop.
//! Only the nearest qualifying partner is paid, and two characters of one account earn nothing from
//! each other. There is no visible number: the level-up card and F2 show the XP. Server only.
//------------------------------------------------------------------------------------------------
class LF_Apprentice : LF_EventSubscriber
{
	static const string FEATURE = "Apprentice";
	//! The XP reason of the bonus; the handler skips awards that carry it.
	static const string REASON = "apprentice";

	static const int MASTER_LEVEL = 10;	// lf-fixed: plan 0.3.0 A10, a master is level 10 or more in the trade
	static const float RADIUS_M = 15;	// lf-fixed: plan 0.3.0 A10, "within 15 m"
	static const int BONUS_PERCENT = 10;	// lf-fixed: plan 0.3.0 A10, 10% of the award
	static const int BONUS_MIN = 1;	// lf-fixed: plan 0.3.0 A10, at least 1 XP
	static const float PAIR_COOLDOWN_SECONDS = 60;	// lf-fixed: plan 0.3.0 A10, once per 60 s per pair

	protected static ref LF_Apprentice s_Subscriber;
	//! a|b|job -> world time (s) the pair last earned the bonus. Session-only.
	protected static ref map<string, float> s_mPairAt;
	//! Tests only: uid -> a body no player controls.
	protected static ref map<string, EntityID> s_mTestBodies;

	//------------------------------------------------------------------------------------------------
	//! Server: subscribes once (game start). A bus reset drops the subscription; the next call restores it.
	static bool EnsureSubscribed()
	{
		if (!Replication.IsServer())
			return false;

		if (!s_Subscriber)
			s_Subscriber = new LF_Apprentice();

		if (LF_EventBus.IsSubscribed(LF_OnJobXpAwarded, s_Subscriber))
			return true;

		return LF_EventBus.Subscribe(LF_OnJobXpAwarded, s_Subscriber);
	}

	//------------------------------------------------------------------------------------------------
	//! World end (LF_StaticReset).
	static void ResetStatics()
	{
		if (s_Subscriber)
			LF_EventBus.Unsubscribe(LF_OnJobXpAwarded, s_Subscriber);

		s_Subscriber = null;
		s_mPairAt = null;
		s_mTestBodies = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: a body no player controls answers for a uid (and is on the roster).
	static void SetBodyForTest(string uid, IEntity body)
	{
		if (!s_mTestBodies)
			s_mTestBodies = new map<string, EntityID>();

		if (body)
			s_mTestBodies.Set(uid, body.GetID());
		else
			s_mTestBodies.Remove(uid);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The bonus for an award: 10% of it, at least 1; 0 for no award.
	static int BonusFor(int amount)
	{
		if (amount <= 0)
			return 0;

		return LF_Utils.MaxInt(BONUS_MIN, amount * BONUS_PERCENT / 100);
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when one of the two levels is a master's and the other a learner's.
	static bool IsPair(int levelA, int levelB)
	{
		bool masterA = levelA >= MASTER_LEVEL;
		bool masterB = levelB >= MASTER_LEVEL;
		return masterA != masterB;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: an award was made. Pays the pair bonus when a partner qualifies.
	//! \return The bonus each of the two got; 0 when none was paid.
	static int OnAwarded(string uid, string job, int amount, string reason, float now)
	{
		if (reason == REASON || uid.IsEmpty() || amount <= 0)
			return 0;

		IEntity body = BodyOf(uid);
		if (!body || !LF_Utils.IsAliveState(body))
			return 0;

		string partner = FindPartner(uid, job, body, now);
		if (partner.IsEmpty())
			return 0;

		int bonus = BonusFor(amount);
		// Booked before paying: a re-entrant award sees the pair as spent.
		NotePair(uid, partner, job, now);
		int own = LF_JobLevels.AddXp(uid, job, bonus, REASON);
		int other = LF_JobLevels.AddXp(partner, job, bonus, REASON);
		LF_Debug.Info(FEATURE, string.Format("%1 and %2 earned %3 %4 xp as master and learner (granted %5 and %6)", uid, partner, bonus, job, own, other));
		return bonus;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The uid of the nearest living character within RADIUS_M that makes a master and learner pair
	//! with `uid` in the job and whose pair is not on cooldown; empty when none.
	protected static string FindPartner(string uid, string job, notnull IEntity body, float now)
	{
		int level = LF_JobLevels.LevelOf(uid, job);
		string best;
		float bestDistance = RADIUS_M + 1;
		array<string> roster = {};
		Roster(roster);
		foreach (string other : roster)
		{
			if (other == uid || LF_DutyBonus.SameAccount(uid, other))
				continue;

			if (!IsPair(level, LF_JobLevels.LevelOf(other, job)) || PairCoolingDown(uid, other, job, now))
				continue;

			IEntity otherBody = BodyOf(other);
			if (!otherBody || !LF_Utils.IsAliveState(otherBody))
				continue;

			float distance = vector.Distance(body.GetOrigin(), otherBody.GetOrigin());
			if (distance <= RADIUS_M && distance < bestDistance)
			{
				best = other;
				bestDistance = distance;
			}
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! The uids of every character a player plays, plus the test bodies.
	protected static void Roster(notnull array<string> uids)
	{
		if (s_mTestBodies)
		{
			for (int i = 0; i < s_mTestBodies.Count(); i++)
				uids.Insert(s_mTestBodies.GetKey(i));
		}

		if (!GetGame())
			return;

		PlayerManager players = GetGame().GetPlayerManager();
		if (!players)
			return;

		array<int> playerIds = {};
		players.GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			string uid = LF_Utils.GetPlayerUID(playerId);
			if (!uid.IsEmpty() && uids.Find(uid) == -1)
				uids.Insert(uid);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static IEntity BodyOf(string uid)
	{
		if (s_mTestBodies && s_mTestBodies.Contains(uid) && GetGame())
			return GetGame().GetWorld().FindEntityByID(s_mTestBodies.Get(uid));

		return LF_QuestProgressSignals.BodyOfUid(uid);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool PairCoolingDown(string uidA, string uidB, string job, float now)
	{
		if (!s_mPairAt)
			return false;

		float at;
		if (!s_mPairAt.Find(PairKey(uidA, uidB, job), at))
			return false;

		return now - at < PAIR_COOLDOWN_SECONDS;
	}

	//------------------------------------------------------------------------------------------------
	protected static void NotePair(string uidA, string uidB, string job, float now)
	{
		if (!s_mPairAt)
			s_mPairAt = new map<string, float>();

		s_mPairAt.Set(PairKey(uidA, uidB, job), now);
	}

	//------------------------------------------------------------------------------------------------
	//! One key per pair whichever of the two earned: the smaller uid first.
	protected static string PairKey(string uidA, string uidB, string job)
	{
		if (uidA.Compare(uidB) <= 0)
			return uidA + "|" + uidB + "|" + job;

		return uidB + "|" + uidA + "|" + job;
	}

	//------------------------------------------------------------------------------------------------
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnJobXpAwarded awarded = LF_OnJobXpAwarded.Cast(busEvent);
		if (!awarded)
			return false;

		OnAwarded(awarded.m_sPlayerUid, awarded.m_sJob, awarded.m_iAmount, awarded.m_sReason, LF_JobGuards.Now());
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_Apprentice's statics (LF_StaticReset finds it by reflection).
class LF_Apprentice_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_Apprentice";
	}

	override void Apply()
	{
		LF_Apprentice.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! Subscribes at game start, without editing LF_GameModeRoleplay.c.
modded class LF_GameModeRoleplay
{
	//------------------------------------------------------------------------------------------------
	protected override void OnGameStart()
	{
		super.OnGameStart();
		LF_Apprentice.EnsureSubscribed();
	}
}
