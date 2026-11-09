//------------------------------------------------------------------------------------------------
//! One actor>target pair's aim streak.
class LF_AimStreak : Managed
{
	float m_fStart;
	float m_fLastSeen;
}

//------------------------------------------------------------------------------------------------
//! Aiming a weapon at a player is intent (#245, epic #145 Moderation). SERVER only, on a timer
//! (config m_fAimCheckSeconds, default 1 s). Each tick looks only at characters with a raised
//! weapon, so unarmed players cost nothing; a pair that stays inside range and the aim cone for
//! the whole window publishes one LF_OnAimedAt, then needs another full window for the next row.
//! The incident log records it (kind "aim"). It changes no damage rule.
//------------------------------------------------------------------------------------------------
class LF_AimIntent
{
	static const string FEATURE = "AimIntent";
	static const int MAX_PAIRS = 256;

	protected static ref map<string, ref LF_AimStreak> s_mStreaks;
	protected static bool s_bRunning;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		if (s_bRunning)
			GetGame().GetCallqueue().Remove(Tick);

		s_mStreaks = null;
		s_bRunning = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: start the timer. Safe to call twice.
	static void Start()
	{
		if (!Replication.IsServer() || s_bRunning)
			return;

		s_bRunning = true;
		GetGame().GetCallqueue().CallLater(Tick, LF_IncidentConfig.AimCheckSeconds() * 1000, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: is the target inside range and the aim cone? direction is the aim direction, any length.
	static bool IsAimedAt(vector from, vector direction, vector to, float range, float coneDegrees)
	{
		vector offset = to - from;
		float distance = offset.Length();
		if (distance > range)
			return false;

		if (distance < 0.01)
			return true;

		if (direction.Length() < 0.001)
			return false;

		offset.Normalize();
		direction.Normalize();
		return vector.Dot(direction, offset) >= Math.Cos(coneDegrees * Math.DEG2RAD);
	}

	//------------------------------------------------------------------------------------------------
	//! Pure sampler step for one pair that is aimed at this tick. \return true when this tick
	//! completes the window and one intent row is due. A gap longer than maxGapSeconds ends the
	//! streak, so aim that stopped and resumed starts over.
	static bool Offer(string pairKey, float nowSeconds, float windowSeconds, float maxGapSeconds)
	{
		if (!s_mStreaks)
			s_mStreaks = new map<string, ref LF_AimStreak>();

		LF_AimStreak streak = s_mStreaks.Get(pairKey);
		if (!streak)
		{
			// The table cannot grow without bound: start over past the cap.
			if (s_mStreaks.Count() >= MAX_PAIRS)
				s_mStreaks.Clear();

			streak = new LF_AimStreak();
			streak.m_fStart = nowSeconds;
			s_mStreaks.Set(pairKey, streak);
		}
		else if (nowSeconds - streak.m_fLastSeen > maxGapSeconds)
		{
			streak.m_fStart = nowSeconds;
		}

		streak.m_fLastSeen = nowSeconds;
		if (nowSeconds - streak.m_fStart < windowSeconds)
			return false;

		streak.m_fStart = nowSeconds;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tick()
	{
		if (!Replication.IsServer() || !LF_IncidentConfig.IsEnabled(LF_IncidentConfig.KIND_AIM))
			return;

		PlayerManager players = GetGame().GetPlayerManager();
		if (!players)
			return;

		array<int> ids = {};
		players.GetPlayers(ids);
		if (ids.Count() < 2)
			return;

		float now = GetGame().GetWorld().GetWorldTime() / 1000;
		float range = LF_IncidentConfig.AimRange();
		float cone = LF_IncidentConfig.AimConeDegrees();
		float window = LF_IncidentConfig.AimWindowSeconds();
		bool aimAlone = LF_IncidentConfig.AimAloneCounts();
		float gap = LF_IncidentConfig.AimCheckSeconds() * 2.5;

		foreach (int actorId : ids)
		{
			IEntity actor = players.GetPlayerControlledEntity(actorId);
			vector direction;
			if (!RaisedAimDirection(actor, direction))
				continue;

			vector from = actor.GetOrigin() + "0 1.5 0";
			foreach (int targetId : ids)
			{
				if (targetId == actorId)
					continue;

				IEntity target = players.GetPlayerControlledEntity(targetId);
				if (!target || !IsAlive(target))
					continue;

				if (!IsAimedAt(from, direction, target.GetOrigin() + "0 1.2 0", range, cone))
					continue;

				if (!aimAlone && !HasLineOfSight(actor, target, from))
					continue;

				if (!Offer(string.Format("%1>%2", actorId, targetId), now, window, gap))
					continue;

				string actorUid = LF_Utils.GetPlayerUID(actorId);
				string targetUid = LF_Utils.GetPlayerUID(targetId);
				if (actorUid.IsEmpty() || targetUid.IsEmpty())
					continue;

				LF_Debug.Info(FEATURE, string.Format("aim: %1 -> %2 held for %3 s", actorUid, targetUid, window));
				LF_EventBus.Publish(LF_OnAimedAt.Create(actorUid, targetUid, actor.GetOrigin()));
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The aim direction in world space when the character has a weapon raised, else false.
	protected static bool RaisedAimDirection(IEntity character, out vector direction)
	{
		ChimeraCharacter chimera = ChimeraCharacter.Cast(character);
		if (!chimera)
			return false;

		CharacterControllerComponent controller = chimera.GetCharacterController();
		if (!controller || !controller.IsWeaponRaised())
			return false;

		CharacterAimingComponent aiming = controller.GetAimingComponent();
		if (!aiming)
			return false;

		direction = aiming.GetAimingDirectionWorld();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Only used when aim alone does not count: the first thing the aim line hits is the target.
	protected static bool HasLineOfSight(IEntity actor, IEntity target, vector from)
	{
		TraceParam trace = new TraceParam();
		trace.Start = from;
		trace.End = target.GetOrigin() + "0 1.2 0";
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		trace.Exclude = actor;
		trace.LayerMask = EPhysicsLayerPresets.Projectile;
		GetGame().GetWorld().TraceMove(trace, null);
		return !trace.TraceEnt || trace.TraceEnt == target;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsAlive(IEntity character)
	{
		ChimeraCharacter chimera = ChimeraCharacter.Cast(character);
		if (!chimera)
			return false;

		CharacterControllerComponent controller = chimera.GetCharacterController();
		return controller && controller.GetLifeState() == ECharacterLifeState.ALIVE;
	}
}
