// LF_ResidentWalk.c - a live resident whose timetable line moves on walks to the next anchor instead of
// popping (#2909, epic #1012). Server side. The spawner (#2858) used to delete a resident whose line
// moved to another place or anchor and spawn them again at the new spot; when a player is near the
// resident that popped in sight. Now the spawner asks Move first:
//   WALKING  a player is within NEAR_M of the resident, the new spot is within MAX_WALK_M and fewer than
//            the cap are walking: the resident is released from their seat, their AI is switched on for
//            the walk (one vanilla group with one move waypoint, walking pace) and they walk. On arrival
//            the AI is switched off again and the new line's activity starts (LF_ResidentActivity).
//   PLACED   nobody is near (or the walk is not possible: too far, cap reached, no AIWorld) but a player is
//            within the despawn radius of the new spot: the body is moved there directly.
//   NONE     nobody could see either end: the spawner despawns them as before.
// A walk that has not arrived after its timeout moves the body to the spot and logs a warning. Progress is
// read from positions only, so the books are the same where no AI moves anyone (GridWorld has no AIWorld:
// the mover is swapped by the test). Entries hold EntityID, never IEntity. Nothing persists: a restart
// respawns by proximity.

//------------------------------------------------------------------------------------------------
enum LF_EResidentMove
{
	NONE,
	WALKING,
	PLACED
}

//------------------------------------------------------------------------------------------------
//! One resident walking: who, where to, and the clock.
class LF_ResidentWalkState
{
	string m_sId;
	EntityID m_Entity = EntityID.INVALID;
	ref LF_ResidentSpot m_Spot;
	string m_sActivity;
	float m_fElapsed;
	float m_fTimeout;
	EntityID m_Group = EntityID.INVALID;
	EntityID m_Waypoint = EntityID.INVALID;
}

//------------------------------------------------------------------------------------------------
//! What moves the body. The default drives the vanilla AI; a test overrides it to stand in for one.
class LF_ResidentMover
{
	//! True when this mover can move characters in this world (the vanilla one needs an AIWorld).
	bool Available()
	{
		return GetGame() && GetGame().GetAIWorld();
	}

	//------------------------------------------------------------------------------------------------
	//! Starts moving `body` to `to`: one vanilla AI group holding the character, one move waypoint on the
	//! spot, walking pace. \return false (logged) when it could not start; nothing is left behind.
	bool Begin(notnull IEntity body, vector to, notnull LF_ResidentWalkState walk)
	{
		AIControlComponent control = AIControlComponent.Cast(body.FindComponent(AIControlComponent));
		if (!control)
		{
			LF_Debug.Error(LF_ResidentWalk.FEATURE, string.Format("resident '%1' has no AIControlComponent; it cannot walk", walk.m_sId));
			return false;
		}

		AIGroup group = AIGroup.Cast(LF_Utils.SpawnEntityPrefab(LF_EventSceneWalk.GROUP_PREFAB, body.GetOrigin()));
		AIWaypoint waypoint = AIWaypoint.Cast(LF_Utils.SpawnEntityPrefab(LF_EventSceneWalk.MOVE_WAYPOINT_PREFAB, to));
		if (!group || !waypoint)
		{
			LF_Debug.Error(LF_ResidentWalk.FEATURE, string.Format("resident '%1': the AI group or move waypoint did not spawn; it cannot walk", walk.m_sId));
			SCR_EntityHelper.DeleteEntityAndChildren(group);
			SCR_EntityHelper.DeleteEntityAndChildren(waypoint);
			return false;
		}

		walk.m_Group = group.GetID();
		walk.m_Waypoint = waypoint.GetID();
		waypoint.SetCompletionRadius(LF_ResidentWalk.WAYPOINT_RADIUS_M);
		control.ActivateAI();
		AIAgent agent = control.GetControlAIAgent();
		if (!agent)
		{
			LF_Debug.Error(LF_ResidentWalk.FEATURE, string.Format("resident '%1' has no AI agent; it cannot walk", walk.m_sId));
			End(body, walk);
			return false;
		}

		// Walking pace for the group's member; an agent that joins gets the group's character settings.
		SCR_AIGroupSettingsComponent settings = SCR_AIGroupSettingsComponent.Cast(group.FindComponent(SCR_AIGroupSettingsComponent));
		if (settings)
			settings.AddSetting(SCR_AICharacterMovementSpeedSetting.Create(SCR_EAISettingOrigin.SCENARIO, SCR_EAIBehaviorCause.ALWAYS, EMovementType.WALK), false);

		group.AddAgent(agent);
		group.AddWaypoint(waypoint);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Called every walk step with the seconds since the last one; the AI needs nothing here.
	void Tick(IEntity body, notnull LF_ResidentWalkState walk, float seconds)
	{
	}

	//------------------------------------------------------------------------------------------------
	//! Stops the AI on `body` (null when the character is gone) and deletes the group and waypoint.
	void End(IEntity body, notnull LF_ResidentWalkState walk)
	{
		BaseWorld world = GetGame().GetWorld();
		if (body)
		{
			AIControlComponent control = AIControlComponent.Cast(body.FindComponent(AIControlComponent));
			if (control)
			{
				AIAgent agent = control.GetControlAIAgent();
				AIGroup group;
				if (world)
					group = AIGroup.Cast(world.FindEntityByID(walk.m_Group));

				if (agent && group)
					group.RemoveAgent(agent);

				if (control.IsAIActivated())
					control.DeactivateAI();
			}
		}

		if (world)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(world.FindEntityByID(walk.m_Group));
			SCR_EntityHelper.DeleteEntityAndChildren(world.FindEntityByID(walk.m_Waypoint));
		}

		walk.m_Group = EntityID.INVALID;
		walk.m_Waypoint = EntityID.INVALID;
	}
}

//------------------------------------------------------------------------------------------------
class LF_ResidentWalk
{
	static const string FEATURE = "Residents";
	//! A player this near the resident sees the line change, so the resident walks (horizontal metres).
	static const float NEAR_M = 50; // lf-fixed: close enough to watch the resident, well inside the spawn radius
	//! A new spot further than this is not walked to: the walk would outlast the player's attention.
	static const float MAX_WALK_M = 120; // lf-fixed: under a minute and a half at walking pace
	//! Walking pace the timeout assumes, in metres per second (the AI walks about 1.4).
	static const float PACE_MS = 0.9; // lf-fixed: under the AI's walking speed so a slow path still beats the clock
	//! Seconds added to the walk time before the walk is judged stuck.
	static const float GRACE_S = 20; // lf-fixed: a door or a fence the AI paths around
	//! A resident within this of the spot has arrived; the move waypoint completes inside WAYPOINT_RADIUS_M.
	static const float ARRIVE_M = 2.5; // lf-fixed: a little over the waypoint's completion radius
	static const float WAYPOINT_RADIUS_M = 1.5; // lf-fixed: a standing spot, not a crowd's rendezvous
	static const int DEFAULT_MAX_WALKING = 4; // lf-fixed: used only when the config leaves it unset
	static const int TICK_MS = 1000; // lf-fixed: progress check resolution only

	protected static ref map<string, ref LF_ResidentWalkState> s_mWalking;
	protected static ref LF_ResidentMover s_Mover;
	protected static bool s_bTicking;
	protected static bool s_bNoAiWarned;
	protected static int s_iStarted;
	protected static int s_iArrived;
	protected static int s_iTimedOut;
	protected static int s_iPlaced;

	//------------------------------------------------------------------------------------------------
	//! Tests: the mover that stands in for the AI (null restores the vanilla AI).
	static void UseMoverForTest(LF_ResidentMover mover)
	{
		s_Mover = mover;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_ResidentMover Mover()
	{
		if (!s_Mover)
			s_Mover = new LF_ResidentMover();

		return s_Mover;
	}

	//------------------------------------------------------------------------------------------------
	//! The rule, with no world: what happens to a resident whose line moved to a spot `toSpotM` away.
	//! \param[in] nearestPlayerM  Horizontal metres from the resident to the nearest player (negative: none).
	//! \param[in] playerToSpotM   Horizontal metres from the nearest player to the spot (negative: none).
	//! \param[in] walking         Residents walking now. \param[in] canWalk the mover can drive the body.
	static LF_EResidentMove Decide(float nearestPlayerM, float toSpotM, float playerToSpotM, int walking, int maxWalking, float despawnM, bool canWalk)
	{
		if (canWalk && nearestPlayerM >= 0 && nearestPlayerM <= NEAR_M && toSpotM <= MAX_WALK_M && walking < maxWalking)
			return LF_EResidentMove.WALKING;

		if (playerToSpotM >= 0 && playerToSpotM <= despawnM)
			return LF_EResidentMove.PLACED;

		return LF_EResidentMove.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the resident `id` (live as `body`) must go to the line `slot`. Walks them or places them
	//! (see the header) and says which; NONE leaves the body untouched for the spawner to despawn.
	//! \param[in] where Where the players are.
	static LF_EResidentMove Move(string id, IEntity body, LF_ResidentSlot slot, notnull array<vector> where, int maxWalking = 0, float despawnM = 0, LF_ResidentSpot spotOverride = null)
	{
		if (!body || !slot || !Replication.IsServer() || where.IsEmpty())
			return LF_EResidentMove.NONE;

		float nearest = -1;
		vector nearestAt;
		foreach (vector viewer : where)
		{
			float fromBody = vector.DistanceXZ(viewer, body.GetOrigin());
			if (nearest < 0 || fromBody < nearest)
			{
				nearest = fromBody;
				nearestAt = viewer;
			}
		}

		LF_ResidentSpot spot = spotOverride;
		if (!spot)
		{
			spot = new LF_ResidentSpot();
			if (!LF_ResidentAnchors.ResolveIn(slot.m_sKind, slot.m_sPlaceId, slot.m_sAnchor, nearestAt, spot))
				return LF_EResidentMove.NONE;
		}

		// The walk ends on a free spot, not on the one somebody else stands on (#3831).
		// and on the floor under it, as a spawned resident is (#3885): an anchor written at another height
		// walked the body to a spot in the air or under the slab.
		if (!spotOverride)
		{
			spot = LF_ResidentActivity.FreeSpot(spot, slot.m_sActivity, id, body.GetID());
			spot = LF_ResidentSpawner.OnFloor(spot, id);
		}

		// 0 reads the config (the spawner passes its own despawn radius).
		LF_ResidentsConfig config = LF_ResidentsConfig.Get();
		if (maxWalking <= 0)
		{
			maxWalking = DEFAULT_MAX_WALKING;
			if (config)
				maxWalking = config.MaxWalking();
		}

		if (despawnM <= 0)
		{
			despawnM = 200; // lf-fixed: the despawn radius default when no config resolves
			if (config)
				despawnM = config.DespawnRadius();
		}

		// A walk already under way is dropped: a newer line wins and the resident walks on from here.
		Cancel(id);

		bool canWalk = Mover().Available();
		if (!canWalk)
			WarnNoAi();

		float toSpot = vector.DistanceXZ(body.GetOrigin(), spot.m_vOrigin);
		float playerToSpot = -1;
		foreach (vector watcher : where)
		{
			float fromSpot = vector.DistanceXZ(watcher, spot.m_vOrigin);
			if (playerToSpot < 0 || fromSpot < playerToSpot)
				playerToSpot = fromSpot;
		}

		LF_EResidentMove verdict = Decide(nearest, toSpot, playerToSpot, WalkingCount(), maxWalking, despawnM, canWalk);
		if (verdict == LF_EResidentMove.WALKING && !StartWalk(id, body, slot, spot, toSpot))
		{
			if (playerToSpot >= 0 && playerToSpot <= despawnM)
				verdict = LF_EResidentMove.PLACED;
			else
				verdict = LF_EResidentMove.NONE;
		}

		if (verdict == LF_EResidentMove.PLACED)
			PlaceNow(id, body, slot.m_sActivity, spot);

		return verdict;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool StartWalk(string id, notnull IEntity body, notnull LF_ResidentSlot slot, notnull LF_ResidentSpot spot, float toSpotM)
	{
		LF_ResidentWalkState walk = new LF_ResidentWalkState();
		walk.m_sId = id;
		walk.m_Entity = body.GetID();
		walk.m_Spot = spot;
		walk.m_sActivity = slot.m_sActivity;
		walk.m_fTimeout = toSpotM / PACE_MS + GRACE_S;

		// Up from a bench or off a wall first: a seated or leaning body cannot walk.
		LF_ResidentActivity.Release(body);
		if (!Mover().Begin(body, spot.m_vOrigin, walk))
			return false;

		if (!s_mWalking)
			s_mWalking = new map<string, ref LF_ResidentWalkState>();

		s_mWalking.Set(id, walk);
		s_iStarted++;
		LF_Debug.Info(FEATURE, string.Format("resident '%1' walks %2 m to anchor '%3' (%4 s allowed)", id, Math.Round(toSpotM), slot.m_sAnchor, Math.Round(walk.m_fTimeout)));
		StartTicking();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Moves the body onto the spot and starts the activity there.
	protected static void PlaceNow(string id, notnull IEntity body, string activity, notnull LF_ResidentSpot spot)
	{
		LF_ResidentActivity.Release(body);
		Place(body, spot);
		LF_ResidentActivity.Apply(body, activity, spot);
		s_iPlaced++;
		LF_Debug.Info(FEATURE, string.Format("resident '%1' placed on the new anchor (nobody walked)", id));
	}

	//------------------------------------------------------------------------------------------------
	//! Puts the body on the spot, turned to its facing.
	protected static void Place(notnull IEntity body, notnull LF_ResidentSpot spot)
	{
		vector mat[4];
		Math3D.AnglesToMatrix(Vector(spot.m_vAngles[0], 0, 0), mat);
		mat[3] = spot.m_vOrigin;
		// A character is moved by Teleport: SetWorldTransform leaves a resident where they stand (the walk's
		// timeout and a placed resident never reached the cafe bench, #3707). A static body has no such call.
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(body);
		if (character)
		{
			character.Teleport(mat);
			return;
		}

		body.SetWorldTransform(mat);
		body.Update();
	}

	//------------------------------------------------------------------------------------------------
	protected static void StartTicking()
	{
		if (s_bTicking || !GetGame() || !GetGame().GetCallqueue())
			return;

		s_bTicking = true;
		GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tick()
	{
		Step(TICK_MS / 1000);
		if (WalkingCount() == 0 && s_bTicking && GetGame() && GetGame().GetCallqueue())
		{
			GetGame().GetCallqueue().Remove(Tick);
			s_bTicking = false;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: advances every walk by `seconds`: arrives the ones on their spot, times out the ones that
	//! ran out of time (moved to the spot, warned), forgets the ones whose character is gone.
	//! \return How many walks ended.
	static int Step(float seconds)
	{
		if (!s_mWalking || s_mWalking.IsEmpty())
			return 0;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return 0;

		int ended;
		array<string> done = {};
		foreach (string id, LF_ResidentWalkState walk : s_mWalking)
		{
			IEntity body = world.FindEntityByID(walk.m_Entity);
			walk.m_fElapsed += seconds;
			if (!body)
			{
				LF_Debug.Info(FEATURE, string.Format("resident '%1' is gone mid-walk; the walk is dropped", id));
				Mover().End(null, walk);
				done.Insert(id);
				ended++;
				continue;
			}

			Mover().Tick(body, walk, seconds);
			if (vector.DistanceXZ(body.GetOrigin(), walk.m_Spot.m_vOrigin) <= ARRIVE_M)
			{
				Mover().End(body, walk);
				LF_ResidentActivity.Apply(body, walk.m_sActivity, walk.m_Spot);
				s_iArrived++;
				LF_Debug.Info(FEATURE, string.Format("resident '%1' arrived after %2 s", id, Math.Round(walk.m_fElapsed)));
				done.Insert(id);
				ended++;
				continue;
			}

			if (walk.m_fElapsed >= walk.m_fTimeout)
			{
				Mover().End(body, walk);
				Place(body, walk.m_Spot);
				LF_ResidentActivity.Apply(body, walk.m_sActivity, walk.m_Spot);
				s_iTimedOut++;
				LF_Debug.Warn(FEATURE, string.Format("resident '%1' had not arrived after %2 s; moved to the anchor", id, Math.Round(walk.m_fElapsed)));
				done.Insert(id);
				ended++;
			}
		}

		foreach (string finished : done)
			s_mWalking.Remove(finished);

		return ended;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the walk of `id` (the AI group and waypoint go; the body stays where it is).
	static void Cancel(string id)
	{
		LF_ResidentWalkState walk;
		if (!s_mWalking || !s_mWalking.Find(id, walk))
			return;

		IEntity body;
		if (GetGame() && GetGame().GetWorld())
			body = GetGame().GetWorld().FindEntityByID(walk.m_Entity);

		Mover().End(body, walk);
		s_mWalking.Remove(id);
	}

	//------------------------------------------------------------------------------------------------
	static bool IsWalking(string id)
	{
		return s_mWalking && s_mWalking.Contains(id);
	}

	//------------------------------------------------------------------------------------------------
	//! True when `body` is a resident on a walk now. A walking resident opens the doors on its path, so a
	//! building lock (LF_BuildingLocks) lets it through; it keeps players out, never the town's own residents.
	static bool IsWalkingBody(IEntity body)
	{
		if (!body || !s_mWalking || s_mWalking.IsEmpty())
			return false;

		EntityID bodyId = body.GetID();
		foreach (string id, LF_ResidentWalkState walk : s_mWalking)
		{
			if (walk.m_Entity == bodyId)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: records `body` as walking as `id` without an AI order.
	static void MarkWalkingForTest(string id, notnull IEntity body)
	{
		LF_ResidentWalkState walk = new LF_ResidentWalkState();
		walk.m_sId = id;
		walk.m_Entity = body.GetID();
		if (!s_mWalking)
			s_mWalking = new map<string, ref LF_ResidentWalkState>();

		s_mWalking.Set(id, walk);
	}

	//------------------------------------------------------------------------------------------------
	static int WalkingCount()
	{
		if (!s_mWalking)
			return 0;

		return s_mWalking.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Seconds the walk of `id` is allowed, or -1 when `id` is not walking (tests read it).
	static float TimeoutOf(string id)
	{
		LF_ResidentWalkState walk;
		if (!s_mWalking || !s_mWalking.Find(id, walk))
			return -1;

		return walk.m_fTimeout;
	}

	static int ArrivedTotal()
	{
		return s_iArrived;
	}

	static int TimedOutTotal()
	{
		return s_iTimedOut;
	}

	static int PlacedTotal()
	{
		return s_iPlaced;
	}

	//------------------------------------------------------------------------------------------------
	//! One warning per world: no AIWorld, so a resident whose line moves is placed, never walked.
	protected static void WarnNoAi()
	{
		if (s_bNoAiWarned)
			return;

		s_bNoAiWarned = true;
		LF_Debug.Warn(FEATURE, "this world has no AIWorld: residents cannot walk and are placed on their new anchor directly");
	}

	//------------------------------------------------------------------------------------------------
	//! World end (and tests): stops the tick and forgets the walks. The AI groups go with the world.
	static void ResetStatics()
	{
		if (s_bTicking && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Tick);

		s_bTicking = false;
		s_mWalking = null;
		s_Mover = null;
		s_bNoAiWarned = false;
		s_iStarted = 0;
		s_iArrived = 0;
		s_iTimedOut = 0;
		s_iPlaced = 0;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of the walk statics (LF_StaticReset finds it by reflection).
class LF_ResidentWalk_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_ResidentWalk";
	}

	override void Apply()
	{
		LF_ResidentWalk.ResetStatics();
	}
}
