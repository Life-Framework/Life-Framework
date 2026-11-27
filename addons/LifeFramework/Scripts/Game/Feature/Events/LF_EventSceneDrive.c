//------------------------------------------------------------------------------------------------
//! A scene's parked vehicle driven along the scene's waypoints (#3380, chapter #1023): the crew joins one
//! vanilla AI group that boards the vehicle (a boarding waypoint) and then drives it along a move
//! waypoint on every route point, in order. Progress is read from the
//! vehicle's position only, so the same bookkeeping runs where no AI moves anything (GridWorld, the
//! tests). A vehicle that makes no progress for STUCK_SECONDS is moved on to the next waypoint (logged
//! [LFDebug:Events] warn) instead of sitting there for the whole event; the last point (the scene's
//! target) is where it stops. Opt-in per scene (#3831): a scene with `m_bDriveVehicle` is driven by the
//! scene operation (LF_EventSceneOperation starts it once the crew is spawned and holds the scene
//! until it is over) instead of walked by LF_EventSceneWalk. The driver must be an AI-capable prefab (AIControlComponent): the vanilla
//! civilians are, the `LF_Npc_*` outfits are not. Short per-lane waypoints work best: the AI follows
//! the nearest road or open ground, so a long cut across a field leaves it on the grass. Server only;
//! one drive at a time. A restart drops it.
class LF_EventSceneDrive
{
	static const string FEATURE = "Events";
	//! Vanilla boarding waypoint on an entity (the crew's vehicle), ahead of the move waypoints.
	static const ResourceName GETIN_WAYPOINT_PREFAB = "{8AD8C82346156494}Prefabs/AI/Waypoints/AIWaypoint_GetInSelected.et";
	static const int TICK_MS = 1000; // lf-fixed: progress check resolution only
	//! The vehicle within this of a route point has passed it; the AI waypoints complete at the same radius.
	static const float ARRIVE_M = 6; // lf-fixed: a truck's length, so a lane waypoint completes under its cab
	//! Getting this much nearer the next point counts as progress.
	static const float PROGRESS_M = 0.5; // lf-fixed: under a second of driving
	//! A vehicle that makes no progress this long is stuck.
	static const float STUCK_SECONDS = 20; // lf-fixed: long enough for a reverse and a turn round a wreck
	//! The crew has this long to climb in; the stuck timer starts when they are in, or when it runs out.
	static const float BOARD_TIMEOUT_S = 45; // lf-fixed: a crew lined up on a quay walks a few metres and climbs in
	//! A moved-on vehicle stands this far above the ground point, and settles.
	static const float DROP_M = 0.4; // lf-fixed: clears the road surface without a visible fall

	protected static ref LF_EventSceneDrive s_Active;
	protected static string s_sLastOver;

	ref LF_EventSceneCrew m_Crew;
	//! Route point positions in driving order; the last is the target.
	ref array<vector> m_aRoute = {};
	//! Index of the route point the vehicle heads for.
	int m_iNext;
	float m_fBest = 1000000;
	float m_fSinceProgress;
	float m_fElapsed;
	//! How many times a stuck vehicle was moved on.
	int m_iRecoveries;
	bool m_bArrived;
	bool m_bOver;
	string m_sLastRecovery;
	EntityID m_GroupId = EntityID.INVALID;
	ref array<EntityID> m_aAiWaypoints = {};
	//! The AI crew is still climbing in: the stuck timer waits for it.
	bool m_bBoarding;
	//! The boarding waypoint: deleted last, after the group (deleting it first crashed the server).
	EntityID m_BoardingId = EntityID.INVALID;

	//------------------------------------------------------------------------------------------------
	static LF_EventSceneDrive Active()
	{
		return s_Active;
	}

	//------------------------------------------------------------------------------------------------
	static string LastOver()
	{
		return s_sLastOver;
	}

	//------------------------------------------------------------------------------------------------
	//! Starts the drive: resolves the plan's waypoints and the scene target, and when `driveAi` and the
	//! world has an AIWorld, seats the crew and sets up the AI group. `tick` puts it on the call queue;
	//! a test steps it by hand. \return the drive, or null (logged) when the crew has no vehicle or no
	//! route, or a drive runs already.
	static LF_EventSceneDrive Begin(LF_EventSceneCrew crew, bool driveAi = true, bool tick = true)
	{
		if (!Replication.IsServer() || !crew)
			return null;

		if (s_Active)
		{
			LF_Debug.Info(FEATURE, string.Format("scene %1 drive not started: scene %2 still drives", crew.m_sSceneId, s_Active.m_Crew.m_sSceneId));
			return null;
		}

		BaseWorld world = GetGame().GetWorld();
		if (!world || !world.FindEntityByID(crew.m_VehicleId))
		{
			LF_Debug.Warn(FEATURE, string.Format("scene %1 has no parked vehicle; nothing to drive", crew.m_sSceneId));
			return null;
		}

		LF_EventSceneDrive drive = new LF_EventSceneDrive();
		drive.m_Crew = crew;
		if (!drive.ResolveRoute())
			return null;

		if (driveAi)
			drive.SetUpAi(world);

		s_Active = drive;
		s_sLastOver = "";
		LF_Debug.Info(FEATURE, string.Format("scene %1 drive starts over %2 route points", crew.m_sSceneId, drive.m_aRoute.Count()));
		if (tick)
			drive.StartTicking();

		return drive;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts the drive on the call queue; Stop takes it off.
	protected void StartTicking()
	{
		GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	//! The plan's waypoints in order, then the target. False (logged) when one is not placed.
	protected bool ResolveRoute()
	{
		foreach (string waypointId : m_Crew.m_Plan.m_aWaypointIds)
		{
			IEntity point = LF_Places.Find(LF_EventWaypointComponent.KIND_WAYPOINT, waypointId);
			if (!point)
			{
				LF_Debug.Error(FEATURE, string.Format("scene %1: waypoint %2 is not placed; nothing drives", m_Crew.m_sSceneId, waypointId));
				return false;
			}

			m_aRoute.Insert(point.GetOrigin());
		}

		IEntity target = LF_EventWaypointComponent.FindTarget(m_Crew.m_sSceneId);
		if (!target)
		{
			LF_Debug.Error(FEATURE, string.Format("scene %1 has no target placed; nothing drives", m_Crew.m_sSceneId));
			return false;
		}

		m_aRoute.Insert(target.GetOrigin());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts the crew in one vanilla AI group: a boarding waypoint on the vehicle first (the crew climbs in
	//! and the group counts the vehicle as usable), then a move waypoint on every route point. Without an
	//! AIWorld nobody is driven and the drive only keeps the books (logged).
	protected void SetUpAi(notnull BaseWorld world)
	{
		if (!GetGame().GetAIWorld())
		{
			LF_Debug.Warn(FEATURE, string.Format("scene %1: no AIWorld in this world; the vehicle is not driven", m_Crew.m_sSceneId));
			return;
		}

		IEntity vehicle = world.FindEntityByID(m_Crew.m_VehicleId);
		AIGroup group = AIGroup.Cast(LF_Utils.SpawnEntityPrefab(LF_EventSceneWalk.GROUP_PREFAB, vehicle.GetOrigin()));
		if (!group)
		{
			LF_Debug.Error(FEATURE, string.Format("scene %1: AI group %2 did not spawn; the vehicle stands", m_Crew.m_sSceneId, LF_EventSceneWalk.GROUP_PREFAB));
			return;
		}

		// A drive is session state: a saved group VMEs on load (SCR_AIGroupSerializer, no faction manager).
		LF_WorldSaveHold.Hold(group);
		m_GroupId = group.GetID();
		int enlisted = 0;
		foreach (int index, EntityID id : m_Crew.m_aPeople)
		{
			IEntity person = world.FindEntityByID(id);
			if (!person)
				continue;

			if (Enlist(group, person, index))
				enlisted++;
		}

		if (enlisted == 0)
		{
			LF_Debug.Error(FEATURE, string.Format("scene %1: nobody could join the AI group; the vehicle stands", m_Crew.m_sSceneId));
			return;
		}

		m_bBoarding = true;
		// The group only drives a vehicle it counts as usable, and the base game registers that through a
		// boarding waypoint on the vehicle (SCR_AIGetInActivity.OnActionSelected); its crew climbs in, and
		// the waypoint completes by itself once they have. Completing it from script crashed the server.
		SCR_BoardingEntityWaypoint boarding = SCR_BoardingEntityWaypoint.Cast(LF_Utils.SpawnEntityPrefab(GETIN_WAYPOINT_PREFAB, vehicle.GetOrigin()));
		if (boarding)
		{
			LF_WorldSaveHold.Hold(boarding);
			boarding.SetEntity(vehicle);
			boarding.SetCompletionRadius(ARRIVE_M);
			m_BoardingId = boarding.GetID();
			group.AddWaypoint(boarding);
		}
		else
		{
			LF_Debug.Error(FEATURE, string.Format("scene %1: boarding waypoint %2 did not spawn; the group may not drive the vehicle", m_Crew.m_sSceneId, GETIN_WAYPOINT_PREFAB));
		}

		foreach (vector point : m_aRoute)
		{
			AIWaypoint waypoint = AIWaypoint.Cast(LF_Utils.SpawnEntityPrefab(LF_EventSceneWalk.MOVE_WAYPOINT_PREFAB, point));
			if (!waypoint)
			{
				LF_Debug.Error(FEATURE, string.Format("scene %1: move waypoint %2 did not spawn", m_Crew.m_sSceneId, LF_EventSceneWalk.MOVE_WAYPOINT_PREFAB));
				continue;
			}

			LF_WorldSaveHold.Hold(waypoint);
			waypoint.SetCompletionRadius(ARRIVE_M);
			m_aAiWaypoints.Insert(waypoint.GetID());
			group.AddWaypoint(waypoint);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Person `index` of the crew into the AI group. False (logged) for a person with no AI.
	protected bool Enlist(notnull AIGroup group, notnull IEntity person, int index)
	{
		AIControlComponent control = AIControlComponent.Cast(person.FindComponent(AIControlComponent));
		if (!control)
		{
			LF_Debug.Error(FEATURE, string.Format("scene %1 crew %2 (%3) has no AIControlComponent; it cannot ride", m_Crew.m_sSceneId, index, LF_Utils.GetPrefabName(person)));
			return false;
		}

		control.ActivateAI();
		AIAgent agent = control.GetControlAIAgent();
		if (!agent)
		{
			LF_Debug.Error(FEATURE, string.Format("scene %1 crew %2 has no AI agent; it cannot ride", m_Crew.m_sSceneId, index));
			return false;
		}

		group.AddAgent(agent);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! How many of the crew sit in a vehicle seat.
	int CountSeated()
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return 0;

		int seated;
		foreach (EntityID id : m_Crew.m_aPeople)
		{
			IEntity person = world.FindEntityByID(id);
			if (!person)
				continue;

			SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(person.FindComponent(SCR_CompartmentAccessComponent));
			if (access && access.IsInCompartment())
				seated++;
		}

		return seated;
	}

	//------------------------------------------------------------------------------------------------
	//! Metres left to drive from `at`: to the next point, then along the route to the target.
	float RemainingM(vector at)
	{
		int next = Math.Min(m_iNext, m_aRoute.Count() - 1);
		float metres = vector.DistanceXZ(at, m_aRoute[next]);
		for (int i = next + 1; i < m_aRoute.Count(); i++)
		{
			metres += vector.DistanceXZ(m_aRoute[i - 1], m_aRoute[i]);
		}

		return metres;
	}

	//------------------------------------------------------------------------------------------------
	//! Advances the books by `seconds`: the next point moves on as the vehicle reaches it, arrival at
	//! the target ends the drive, and a vehicle with no progress for STUCK_SECONDS is moved on.
	void Step(float seconds)
	{
		if (m_bOver)
			return;

		m_fElapsed += seconds;
		BaseWorld world = GetGame().GetWorld();
		IEntity vehicle;
		if (world)
			vehicle = world.FindEntityByID(m_Crew.m_VehicleId);

		if (!vehicle)
		{
			// Removed by someone else (the scene despawn, an admin): nothing left to drive.
			Finish("the vehicle is gone");
			return;
		}

		vector at = vehicle.GetOrigin();
		int last = m_aRoute.Count() - 1;
		while (m_iNext < last && vector.DistanceXZ(at, m_aRoute[m_iNext]) <= ARRIVE_M)
		{
			m_iNext++;
		}

		if (m_iNext == last && vector.DistanceXZ(at, m_aRoute[last]) <= ARRIVE_M)
		{
			m_bArrived = true;
			Finish(string.Format("the vehicle reached the target after %1 s, %2 moved on", m_fElapsed, m_iRecoveries));
			return;
		}

		if (m_bBoarding)
		{
			if (CountSeated() < m_Crew.m_aPeople.Count() && m_fElapsed < BOARD_TIMEOUT_S)
				return;

			m_bBoarding = false;
			LF_Debug.Info(FEATURE, string.Format("scene %1: %2 of %3 crew in the vehicle after %4 s", m_Crew.m_sSceneId, CountSeated(), m_Crew.m_aPeople.Count(), m_fElapsed));
		}

		float remaining = RemainingM(at);
		if (remaining < m_fBest - PROGRESS_M)
		{
			m_fBest = remaining;
			m_fSinceProgress = 0;
			return;
		}

		m_fSinceProgress += seconds;
		if (m_fSinceProgress >= STUCK_SECONDS)
			MoveOn(vehicle, remaining);
	}

	//------------------------------------------------------------------------------------------------
	//! A stuck vehicle goes to the next route point (the target when it was heading for the last
	//! waypoint) and the AI group is told that point is done, so it drives on from there.
	protected void MoveOn(notnull IEntity vehicle, float remaining)
	{
		int last = m_aRoute.Count() - 1;
		int to = Math.Min(m_iNext + 1, last);
		if (m_iNext >= last)
			to = last;

		vector spot = m_aRoute[to];
		vector from = m_aRoute[Math.Min(m_iNext, last)];
		m_sLastRecovery = string.Format("scene %1: vehicle stuck %2 s with %3 m of the route left; moved on to route point %4 of %5", m_Crew.m_sSceneId, m_fSinceProgress, remaining, to, last);
		LF_Debug.Warn(FEATURE, m_sLastRecovery);

		vector dir = spot - from;
		dir[1] = 0;
		if (dir.Length() > 0.1)
			vehicle.SetYawPitchRoll(Vector(dir.VectorToAngles()[0], 0, 0));

		vehicle.SetOrigin(spot + Vector(0, DROP_M, 0));
		Physics physics = vehicle.GetPhysics();
		if (physics)
		{
			physics.SetVelocity(vector.Zero);
			physics.SetAngularVelocity(vector.Zero);
		}

		m_iRecoveries++;
		m_iNext = to;
		m_fBest = 1000000;
		m_fSinceProgress = 0;
		CompleteCurrentAiWaypoint();
	}

	//------------------------------------------------------------------------------------------------
	//! The group's current waypoint is behind the vehicle now: complete it so the next one is current.
	protected void CompleteCurrentAiWaypoint()
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		AIGroup group = AIGroup.Cast(world.FindEntityByID(m_GroupId));
		if (!group)
			return;

		AIWaypoint current = group.GetCurrentWaypoint();
		if (current)
			group.CompleteWaypoint(current);
	}

	//------------------------------------------------------------------------------------------------
	protected void Finish(string why)
	{
		m_bOver = true;
		s_sLastOver = why;
		LF_Debug.Info(FEATURE, string.Format("scene %1 drive over: %2", m_Crew.m_sSceneId, why));
	}

	//------------------------------------------------------------------------------------------------
	//! Call queue tick. Stops when the scene is no longer the running one or the drive is over.
	void Tick()
	{
		if (LF_EventSceneSpawn.Running() != m_Crew)
		{
			Stop();
			return;
		}

		Step(TICK_MS / 1000.0);
		if (m_bOver)
			Stop();
	}

	//------------------------------------------------------------------------------------------------
	//! Ends the drive: off the call queue, the AI group and its waypoints deleted. The crew stays; the
	//! scene's despawn removes it.
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
				// Release the crew first, as LF_EventSceneWalk.Stop does: deleting the group with its
				// children would take the driver along.
				array<AIAgent> agents = {};
				group.GetAgents(agents);
				foreach (AIAgent agent : agents)
				{
					group.RemoveAgent(agent);
				}

				SCR_EntityHelper.DeleteEntityAndChildren(group);
			}

			// Last: the group's boarding activity holds it, and deleting it first crashed the server.
			IEntity boarding = world.FindEntityByID(m_BoardingId);
			if (boarding)
				SCR_EntityHelper.DeleteEntityAndChildren(boarding);
		}

		m_aAiWaypoints.Clear();
		m_BoardingId = EntityID.INVALID;
		m_GroupId = EntityID.INVALID;
		if (s_Active == this)
			s_Active = null;
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
//! World-end reset of LF_EventSceneDrive's statics (Core/LF_StaticReset.c). The drive is not resumed.
class LF_EventSceneDrive_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_EventSceneDrive";
	}

	override void Apply()
	{
		LF_EventSceneDrive.ResetStatics();
	}
}
