//------------------------------------------------------------------------------------------------
//! One walker of a scene walk (#1956): how far along the route it is and whether it is done.
class LF_EventSceneWalker
{
	EntityID m_Id = EntityID.INVALID;
	//! Index of the route point the walker heads for (the target is the last one).
	int m_iNext;
	//! Least route left to walk so far, and the seconds since that last got better.
	float m_fBest = 1000000;
	float m_fSinceProgress;
	//! Seconds since it joined the walk.
	float m_fWalked;
	bool m_bArrived;
	bool m_bTimedOut;
	//! Timed out, waiting for nobody to see it before it is removed.
	bool m_bDespawnPending;
	//! Seconds it has waited in that state; past the plan's m_fForceDespawnSeconds it goes in sight (#2406).
	float m_fPendingS;
}

//------------------------------------------------------------------------------------------------
//! The walk of a running scene (#1956, chapter #1015): the crew of LF_EventSceneSpawn follows the
//! scene's waypoints in plan order to the target. The walkers move through the vanilla AI: one
//! SCR_AIGroup per scene (its formation keeps them a loose group) with one AIWaypoint_Move per route
//! point, added in order; a walker joins the group as soon as its stagger spawns it. Progress is read
//! from positions only, so the same bookkeeping runs where no AI moves anyone (GridWorld, the tests).
//! A walker that stops getting further along the route for STUCK_SECONDS, or is still walking when the
//! plan's walk timeout runs out, times out (logged) and is removed once no player sees it.
//! Server only; one walk at a time, like the scene. A restart drops it.
class LF_EventSceneWalk
{
	static const string FEATURE = "Events";
	static const int TICK_MS = 1000; // lf-fixed: progress check resolution only; the plan sets the time budget
	//! A walker this close to the target has arrived; the AI waypoints complete at the same radius.
	static const float ARRIVE_M = 3; // lf-fixed: a little over a walking group's spread at a waypoint
	//! Once the AI group has finished its route, its members stop in formation around the leader, a
	//! line abreast about this far apart: the outer member of six stood 18.8 m out (#2174).
	static const float FORMATION_SPACING_M = 4; // lf-fixed: the vanilla line's spacing seen in the #2174 runs
	//! Getting this much further along the route counts as progress.
	static const float PROGRESS_M = 0.5; // lf-fixed: well under a second of walking
	//! A walker that makes no progress this long is stuck.
	static const float STUCK_SECONDS = 20; // lf-fixed: long enough for the AI to path around a door or a fence
	//! Vanilla group and move waypoint, as the base game's own spawners use them.
	static const ResourceName GROUP_PREFAB = "{000CD338713F2B5A}Prefabs/AI/Groups/Group_Base.et";
	static const ResourceName MOVE_WAYPOINT_PREFAB = "{750A8D1695BD6998}Prefabs/AI/Waypoints/AIWaypoint_Move.et";

	protected static ref LF_EventSceneWalk s_Active;
	//! The result of the last walk that ended ("6 reached the target, 0 timed out"); "" before one.
	protected static string s_sLastOver;

	ref LF_EventSceneCrew m_Crew;
	//! Route point positions in walking order; the last is the target.
	ref array<vector> m_aRoute = {};
	ref array<ref LF_EventSceneWalker> m_aWalkers = {};
	EntityID m_GroupId = EntityID.INVALID;
	ref array<EntityID> m_aAiWaypoints = {};
	//! The route went out: every planned walker has spawned and the group has its waypoints. Until
	//! then the walkers wait at the spawn marker and their stuck and walk clocks do not run.
	bool m_bRouteOut;
	float m_fElapsed;
	//! The players' eyes collected once for the current Step (#2406), not once per walker.
	protected ref array<vector> m_aStepEyes = {};
	protected ref array<IEntity> m_aStepBodies = {};
	protected bool m_bStepEyes;
	//! The log line of the last time out, for the caller and the tests.
	string m_sLastTimeout;
	protected bool m_bDoneLogged;

	//------------------------------------------------------------------------------------------------
	//! The running walk, or null.
	static LF_EventSceneWalk Active()
	{
		return s_Active;
	}

	//------------------------------------------------------------------------------------------------
	//! Starts the crew's walk: resolves the plan's waypoints and the scene target, and when `driveAi`
	//! and the world has an AIWorld, sets up the AI group and its move waypoints. `tick` puts the walk
	//! on the call queue; a test steps it by hand. \return the walk, or null (logged) when the crew has
	//! no route or a walk runs already.
	static LF_EventSceneWalk Begin(LF_EventSceneCrew crew, bool driveAi = true, bool tick = true)
	{
		if (!Replication.IsServer() || !crew)
			return null;

		if (s_Active)
		{
			LF_Debug.Info(FEATURE, string.Format("scene %1 walk not started: scene %2 still walks", crew.m_sSceneId, s_Active.m_Crew.m_sSceneId));
			return null;
		}

		LF_EventSceneWalk walk = new LF_EventSceneWalk();
		walk.m_Crew = crew;
		if (!walk.ResolveRoute())
			return null;

		if (driveAi)
			walk.SetUpAi();

		s_Active = walk;
		s_sLastOver = "";
		walk.JoinNewWalkers();
		LF_Debug.Info(FEATURE, string.Format("scene %1 walk starts over %2 route points", crew.m_sSceneId, walk.m_aRoute.Count()));
		if (tick)
			walk.StartTicking();

		return walk;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts the walk on the call queue; Stop takes it off.
	protected void StartTicking()
	{
		GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	//! The plan's waypoints in order, then the target. False (logged) when a waypoint or the target is
	//! not placed: a scene with a hole in its route does not walk.
	protected bool ResolveRoute()
	{
		foreach (string waypointId : m_Crew.m_Plan.m_aWaypointIds)
		{
			IEntity point = LF_Places.Find(LF_EventWaypointComponent.KIND_WAYPOINT, waypointId);
			if (!point)
			{
				LF_Debug.Error(FEATURE, string.Format("scene %1: waypoint %2 is not placed; nobody walks", m_Crew.m_sSceneId, waypointId));
				return false;
			}

			m_aRoute.Insert(point.GetOrigin());
		}

		IEntity target = LF_EventWaypointComponent.FindTarget(m_Crew.m_sSceneId);
		if (!target)
		{
			LF_Debug.Error(FEATURE, string.Format("scene %1 has no target placed; nobody walks", m_Crew.m_sSceneId));
			return false;
		}

		m_aRoute.Insert(target.GetOrigin());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! One vanilla AI group with a move waypoint on every route point, in order. Without an AIWorld
	//! (GridWorld) nobody is driven and the walk only keeps the books (logged).
	protected void SetUpAi()
	{
		if (!GetGame().GetAIWorld())
		{
			LF_Debug.Warn(FEATURE, string.Format("scene %1: no AIWorld in this world; the walkers are not driven", m_Crew.m_sSceneId));
			return;
		}

		AIGroup group = AIGroup.Cast(LF_Utils.SpawnEntityPrefab(GROUP_PREFAB, m_aRoute[0]));
		if (!group)
		{
			LF_Debug.Error(FEATURE, string.Format("scene %1: AI group %2 did not spawn; the walkers stand", m_Crew.m_sSceneId, GROUP_PREFAB));
			return;
		}

		m_GroupId = group.GetID();
		foreach (vector point : m_aRoute)
		{
			AIWaypoint waypoint = AIWaypoint.Cast(LF_Utils.SpawnEntityPrefab(MOVE_WAYPOINT_PREFAB, point));
			if (!waypoint)
			{
				LF_Debug.Error(FEATURE, string.Format("scene %1: move waypoint %2 did not spawn", m_Crew.m_sSceneId, MOVE_WAYPOINT_PREFAB));
				continue;
			}

			waypoint.SetCompletionRadius(ARRIVE_M);
			m_aAiWaypoints.Insert(waypoint.GetID());
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Once every planned walker has spawned, hands the group its move waypoints in route order, as
	//! vanilla SCR_AIGroup.SpawnUnits does (members first, then AddWaypointsStatic). A group given its
	//! route at the first spawn finished it while the last of a staggered family was still 20 m behind.
	protected void SendRouteWhenAllSpawned(notnull BaseWorld world)
	{
		if (m_bRouteOut || !m_Crew.IsFullySpawned())
			return;

		m_bRouteOut = true;
		AIGroup group = AIGroup.Cast(world.FindEntityByID(m_GroupId));
		int given = 0;
		if (group)
		{
			foreach (EntityID id : m_aAiWaypoints)
			{
				AIWaypoint waypoint = AIWaypoint.Cast(world.FindEntityByID(id));
				if (!waypoint)
					continue;

				group.AddWaypoint(waypoint);
				given++;
			}
		}

		LF_Debug.Info(FEATURE, string.Format("scene %1: all %2 walkers spawned, the route goes out (%3 AI waypoint(s))", m_Crew.m_sSceneId, m_aWalkers.Count(), given));
	}

	//------------------------------------------------------------------------------------------------
	//! Takes in the walkers the stagger spawned since the last step and hands them to the AI group.
	protected void JoinNewWalkers()
	{
		BaseWorld world = GetGame().GetWorld();
		AIGroup group;
		if (world)
			group = AIGroup.Cast(world.FindEntityByID(m_GroupId));

		// By id, not by count: a walker removed from the crew must not hide the next one spawned.
		foreach (EntityID id : m_Crew.m_aPeople)
		{
			if (IsWalker(id))
				continue;

			LF_EventSceneWalker walker = new LF_EventSceneWalker();
			walker.m_Id = id;
			m_aWalkers.Insert(walker);
			if (group && world)
				JoinGroup(group, world.FindEntityByID(id), m_aWalkers.Count() - 1);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsWalker(EntityID id)
	{
		foreach (LF_EventSceneWalker walker : m_aWalkers)
		{
			if (walker.m_Id == id)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected void JoinGroup(notnull AIGroup group, IEntity person, int index)
	{
		if (!person)
			return;

		AIControlComponent control = AIControlComponent.Cast(person.FindComponent(AIControlComponent));
		if (!control)
		{
			LF_Debug.Error(FEATURE, string.Format("scene %1 walker %2 (%3) has no AIControlComponent; it cannot walk", m_Crew.m_sSceneId, index, LF_Utils.GetPrefabName(person)));
			return;
		}

		control.ActivateAI();
		AIAgent agent = control.GetControlAIAgent();
		if (!agent)
		{
			LF_Debug.Error(FEATURE, string.Format("scene %1 walker %2 has no AI agent; it cannot walk", m_Crew.m_sSceneId, index));
			return;
		}

		group.AddAgent(agent);
	}

	//------------------------------------------------------------------------------------------------
	//! Where walker `index` heads now: its next route point. The target once it has arrived.
	vector NextPoint(int index)
	{
		LF_EventSceneWalker walker = m_aWalkers[index];
		return m_aRoute[Math.Min(walker.m_iNext, m_aRoute.Count() - 1)];
	}

	//------------------------------------------------------------------------------------------------
	//! A walker at `at` has passed route point `index` when it is within ARRIVE_M of it, or nearer the
	//! following point than the waypoint itself is: a loose group walks past a waypoint spread out.
	bool Passed(vector at, int index)
	{
		if (vector.DistanceXZ(at, m_aRoute[index]) <= ARRIVE_M)
			return true;

		if (index + 1 >= m_aRoute.Count())
			return false;

		vector following = m_aRoute[index + 1];
		return vector.DistanceXZ(at, following) < vector.DistanceXZ(m_aRoute[index], following);
	}

	//------------------------------------------------------------------------------------------------
	//! Metres left to walk from `at`: to the walker's next point, then along the route to the target.
	float RemainingM(notnull LF_EventSceneWalker walker, vector at)
	{
		int next = Math.Min(walker.m_iNext, m_aRoute.Count() - 1);
		float metres = vector.DistanceXZ(at, m_aRoute[next]);
		for (int i = next + 1; i < m_aRoute.Count(); i++)
		{
			metres += vector.DistanceXZ(m_aRoute[i - 1], m_aRoute[i]);
		}

		return metres;
	}

	//------------------------------------------------------------------------------------------------
	//! Arrival rule. A walker that passed every waypoint arrives within ARRIVE_M of the target (standing
	//! near the target is not the walk). Once the AI group has completed its last waypoint, the group
	//! stops in formation around its leader, so a member within FormationArriveM(members) has arrived.
	static bool HasArrived(bool passedEveryWaypoint, float toTargetM, bool groupDone, int members = 1)
	{
		if (groupDone)
			return toTargetM <= FormationArriveM(members);

		return passedEveryWaypoint && toTargetM <= ARRIVE_M;
	}

	//------------------------------------------------------------------------------------------------
	//! How far from the target a member of a finished group of `members` may stand: the leader within
	//! ARRIVE_M, the line abreast FORMATION_SPACING_M per member beside it (23 m for six).
	static float FormationArriveM(int members)
	{
		return ARRIVE_M + FORMATION_SPACING_M * Math.Max(0, members - 1);
	}

	//------------------------------------------------------------------------------------------------
	//! The walk's AI group exists, was given the route and has completed every waypoint of it.
	protected bool IsGroupDone(notnull BaseWorld world)
	{
		if (!m_bRouteOut || m_aAiWaypoints.IsEmpty())
			return false;

		AIGroup group = AIGroup.Cast(world.FindEntityByID(m_GroupId));
		return group && !group.GetCurrentWaypoint();
	}

	//------------------------------------------------------------------------------------------------
	vector Target()
	{
		return m_aRoute[m_aRoute.Count() - 1];
	}

	//------------------------------------------------------------------------------------------------
	//! Every planned walker has spawned and each one arrived or timed out (and is gone).
	bool IsDone()
	{
		if (!m_Crew.IsFullySpawned())
			return false;

		foreach (LF_EventSceneWalker walker : m_aWalkers)
		{
			if (!walker.m_bArrived && !(walker.m_bTimedOut && !walker.m_bDespawnPending))
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	int CountArrived()
	{
		int arrived = 0;
		foreach (LF_EventSceneWalker walker : m_aWalkers)
		{
			if (walker.m_bArrived)
				arrived++;
		}

		return arrived;
	}

	//------------------------------------------------------------------------------------------------
	int CountTimedOut()
	{
		int timedOut = 0;
		foreach (LF_EventSceneWalker walker : m_aWalkers)
		{
			if (walker.m_bTimedOut)
				timedOut++;
		}

		return timedOut;
	}

	//------------------------------------------------------------------------------------------------
	//! Advances the books by `seconds`: new walkers join, each walker's next point moves on as it
	//! reaches it, arrivals are counted and stuck walkers time out and go once out of sight. `eyes`
	//! (with `bodies`) stands in for the players as in LF_EventSceneSpawn.TryDespawn; null reads them.
	void Step(float seconds, array<vector> eyes = null, array<IEntity> bodies = null)
	{
		m_fElapsed += seconds;
		m_aStepEyes.Clear();
		m_aStepBodies.Clear();
		m_bStepEyes = false;
		if (eyes)
		{
			m_aStepEyes.Copy(eyes);
			if (bodies)
				m_aStepBodies.Copy(bodies);

			m_bStepEyes = true;
		}

		JoinNewWalkers();
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		SendRouteWhenAllSpawned(world);
		vector target = Target();
		bool groupDone = IsGroupDone(world);
		foreach (int index, LF_EventSceneWalker walker : m_aWalkers)
		{
			if (walker.m_bArrived || (walker.m_bTimedOut && !walker.m_bDespawnPending))
				continue;

			IEntity person = world.FindEntityByID(walker.m_Id);
			if (!person)
			{
				// Removed by someone else (the scene despawn, an admin): nothing left to walk.
				walker.m_bTimedOut = true;
				walker.m_bDespawnPending = false;
				continue;
			}

			if (walker.m_bDespawnPending)
			{
				walker.m_fPendingS += seconds;
				TryRemove(walker, person);
				continue;
			}

			// Waiting at the spawn marker for the rest of the family is not walking, nor being stuck.
			if (!m_bRouteOut)
				continue;

			walker.m_fWalked += seconds;
			vector at = person.GetOrigin();
			while (walker.m_iNext < m_aRoute.Count() - 1 && Passed(at, walker.m_iNext))
				walker.m_iNext++;

			float toTarget = vector.DistanceXZ(at, target);
			if (HasArrived(walker.m_iNext == m_aRoute.Count() - 1, toTarget, groupDone, m_aWalkers.Count()))
			{
				walker.m_bArrived = true;
				LF_Debug.Info(FEATURE, string.Format("scene %1 walker %2 reached the target after %3 s", m_Crew.m_sSceneId, index, walker.m_fWalked));
				continue;
			}

			float remaining = RemainingM(walker, at);
			if (remaining < walker.m_fBest - PROGRESS_M)
			{
				walker.m_fBest = remaining;
				walker.m_fSinceProgress = 0;
			}
			else
			{
				walker.m_fSinceProgress += seconds;
			}

			string why;
			if (walker.m_fSinceProgress >= STUCK_SECONDS)
				why = string.Format("stuck %1 s with %2 m of the route left (%3 m from the target)", walker.m_fSinceProgress, remaining, toTarget);
			else if (walker.m_fWalked >= m_Crew.m_Plan.m_fWalkTimeoutSeconds)
				why = string.Format("%1 m of the route left after the %2 s walk budget", remaining, m_Crew.m_Plan.m_fWalkTimeoutSeconds);

			if (why.IsEmpty())
				continue;

			walker.m_bTimedOut = true;
			walker.m_bDespawnPending = true;
			m_sLastTimeout = string.Format("scene %1 walker %2 timed out: %3; removed once out of sight", m_Crew.m_sSceneId, index, why);
			LF_Debug.Warn(FEATURE, m_sLastTimeout);
			TryRemove(walker, person);
		}

		if (IsDone() && !m_bDoneLogged)
		{
			m_bDoneLogged = true;
			s_sLastOver = string.Format("%1 reached the target, %2 timed out", CountArrived(), CountTimedOut());
			LF_Debug.Info(FEATURE, string.Format("scene %1 walk over: %2", m_Crew.m_sSceneId, s_sLastOver));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Removes a timed-out walker when no player sees it; otherwise leaves it pending for the next step.
	//! A walker watched for the plan's m_fForceDespawnSeconds is removed anyway (#2406). The players'
	//! eyes are read once per Step, however many walkers wait.
	protected void TryRemove(LF_EventSceneWalker walker, IEntity person)
	{
		bool force = walker.m_fPendingS >= m_Crew.m_Plan.m_fForceDespawnSeconds;
		if (!force)
		{
			if (!m_bStepEyes)
			{
				LF_EventSceneSpawn.CollectPlayerEyes(m_aStepEyes, m_aStepBodies);
				m_bStepEyes = true;
			}

			array<IEntity> self = {person};
			foreach (int i, vector eye : m_aStepEyes)
			{
				IEntity body;
				if (i < m_aStepBodies.Count())
					body = m_aStepBodies[i];

				if (LF_EventSceneSpawn.Sees(eye, body, person, self))
					return;
			}
		}
		else
		{
			LF_Debug.Warn(FEATURE, string.Format("scene %1: a timed-out walker is removed in sight after %2 s", m_Crew.m_sSceneId, walker.m_fPendingS));
		}

		m_Crew.m_aPeople.RemoveItem(walker.m_Id);
		SCR_EntityHelper.DeleteEntityAndChildren(person);
		walker.m_bDespawnPending = false;
		LF_Debug.Info(FEATURE, string.Format("scene %1: a timed-out walker was removed out of sight", m_Crew.m_sSceneId));
	}

	//------------------------------------------------------------------------------------------------
	//! Call queue tick. Stops when the scene is no longer the running one or the walk is over.
	void Tick()
	{
		if (LF_EventSceneSpawn.Running() != m_Crew)
		{
			Stop();
			return;
		}

		Step(TICK_MS / 1000.0);
		if (IsDone())
			Stop();
	}

	//------------------------------------------------------------------------------------------------
	//! Ends the walk: off the call queue, the AI group and its waypoints deleted. The walkers stay; the
	//! scene's despawn removes them.
	void Stop()
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Tick);

		BaseWorld world;
		if (GetGame())
			world = GetGame().GetWorld();

		if (world)
		{
			foreach (EntityID id : m_aAiWaypoints)
			{
				IEntity waypoint = world.FindEntityByID(id);
				if (waypoint)
					SCR_EntityHelper.DeleteEntityAndChildren(waypoint);
			}

			AIGroup group = AIGroup.Cast(world.FindEntityByID(m_GroupId));
			if (group)
			{
				// Release the walkers first, as vanilla RemoveAIEntityFromGroup does: deleting the group
				// with its children took the walkers along the moment the walk ended (#2174 follow-up).
				array<AIAgent> agents = {};
				group.GetAgents(agents);
				foreach (AIAgent agent : agents)
				{
					group.RemoveAgent(agent);
				}

				SCR_EntityHelper.DeleteEntityAndChildren(group);
			}
		}

		m_aAiWaypoints.Clear();
		m_GroupId = EntityID.INVALID;
		if (s_Active == this)
			s_Active = null;
	}

	//------------------------------------------------------------------------------------------------
	//------------------------------------------------------------------------------------------------
	static string LastOver()
	{
		return s_sLastOver;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_sLastOver = "";
		if (s_Active)
			s_Active.Stop();

		s_Active = null;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_EventSceneWalk's statics (Core/LF_StaticReset.c). The walk is not resumed.
class LF_EventSceneWalk_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_EventSceneWalk_StaticResetHook());

	override string Owner()
	{
		return "LF_EventSceneWalk";
	}

	override void Apply()
	{
		LF_EventSceneWalk.ResetStatics();
	}
}
