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
	//! Once the AI group has finished its route, its members stop in formation around the leader:
	//! a walker this close to the target has arrived with it (#2174: six stopped 1 to 11 m out).
	static const float FORMATION_ARRIVE_M = 15; // lf-fixed: a six-person formation's spread around its leader
	//! Getting this much further along the route counts as progress.
	static const float PROGRESS_M = 0.5; // lf-fixed: well under a second of walking
	//! A walker that makes no progress this long is stuck.
	static const float STUCK_SECONDS = 20; // lf-fixed: long enough for the AI to path around a door or a fence
	//! Vanilla group and move waypoint, as the base game's own spawners use them.
	static const ResourceName GROUP_PREFAB = "{000CD338713F2B5A}Prefabs/AI/Groups/Group_Base.et";
	static const ResourceName MOVE_WAYPOINT_PREFAB = "{750A8D1695BD6998}Prefabs/AI/Waypoints/AIWaypoint_Move.et";

	protected static ref LF_EventSceneWalk s_Active;

	ref LF_EventSceneCrew m_Crew;
	//! Route point positions in walking order; the last is the target.
	ref array<vector> m_aRoute = {};
	ref array<ref LF_EventSceneWalker> m_aWalkers = {};
	EntityID m_GroupId = EntityID.INVALID;
	ref array<EntityID> m_aAiWaypoints = {};
	float m_fElapsed;
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
			group.AddWaypoint(waypoint);
			m_aAiWaypoints.Insert(waypoint.GetID());
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Takes in the walkers the stagger spawned since the last step and hands them to the AI group.
	protected void JoinNewWalkers()
	{
		BaseWorld world = GetGame().GetWorld();
		AIGroup group;
		if (world)
			group = AIGroup.Cast(world.FindEntityByID(m_GroupId));

		for (int i = m_aWalkers.Count(); i < m_Crew.m_aPeople.Count(); i++)
		{
			LF_EventSceneWalker walker = new LF_EventSceneWalker();
			walker.m_Id = m_Crew.m_aPeople[i];
			m_aWalkers.Insert(walker);
			if (group && world)
				JoinGroup(group, world.FindEntityByID(walker.m_Id), i);
		}
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
	//! stops in formation around its leader, so any member within FORMATION_ARRIVE_M has arrived.
	static bool HasArrived(bool passedEveryWaypoint, float toTargetM, bool groupDone)
	{
		if (groupDone)
			return toTargetM <= FORMATION_ARRIVE_M;

		return passedEveryWaypoint && toTargetM <= ARRIVE_M;
	}

	//------------------------------------------------------------------------------------------------
	//! The walk's AI group exists, was given the route and has completed every waypoint of it.
	protected bool IsGroupDone(notnull BaseWorld world)
	{
		if (m_aAiWaypoints.IsEmpty())
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
		JoinNewWalkers();
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

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
				TryRemove(walker, person, eyes, bodies);
				continue;
			}

			walker.m_fWalked += seconds;
			vector at = person.GetOrigin();
			while (walker.m_iNext < m_aRoute.Count() - 1 && Passed(at, walker.m_iNext))
				walker.m_iNext++;

			float toTarget = vector.DistanceXZ(at, target);
			if (HasArrived(walker.m_iNext == m_aRoute.Count() - 1, toTarget, groupDone))
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
				why = string.Format("stuck %1 s with %2 m of the route left", walker.m_fSinceProgress, remaining);
			else if (walker.m_fWalked >= m_Crew.m_Plan.m_fWalkTimeoutSeconds)
				why = string.Format("%1 m of the route left after the %2 s walk budget", remaining, m_Crew.m_Plan.m_fWalkTimeoutSeconds);

			if (why.IsEmpty())
				continue;

			walker.m_bTimedOut = true;
			walker.m_bDespawnPending = true;
			m_sLastTimeout = string.Format("scene %1 walker %2 timed out: %3; removed once out of sight", m_Crew.m_sSceneId, index, why);
			LF_Debug.Warn(FEATURE, m_sLastTimeout);
			TryRemove(walker, person, eyes, bodies);
		}

		if (IsDone() && !m_bDoneLogged)
		{
			m_bDoneLogged = true;
			LF_Debug.Info(FEATURE, string.Format("scene %1 walk over: %2 reached the target, %3 timed out", m_Crew.m_sSceneId, CountArrived(), CountTimedOut()));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Removes a timed-out walker when no player sees it; otherwise leaves it pending for the next step.
	protected void TryRemove(LF_EventSceneWalker walker, IEntity person, array<vector> eyes, array<IEntity> bodies)
	{
		array<vector> lookers = {};
		array<IEntity> lookerBodies = {};
		if (eyes)
		{
			lookers.Copy(eyes);
			if (bodies)
				lookerBodies.Copy(bodies);
		}
		else
		{
			LF_EventSceneSpawn.CollectPlayerEyes(lookers, lookerBodies);
		}

		array<IEntity> self = {person};
		foreach (int i, vector eye : lookers)
		{
			IEntity body;
			if (i < lookerBodies.Count())
				body = lookerBodies[i];

			if (LF_EventSceneSpawn.Sees(eye, body, person, self))
				return;
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

			IEntity group = world.FindEntityByID(m_GroupId);
			if (group)
				SCR_EntityHelper.DeleteEntityAndChildren(group);
		}

		m_aAiWaypoints.Clear();
		m_GroupId = EntityID.INVALID;
		if (s_Active == this)
			s_Active = null;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
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
