//------------------------------------------------------------------------------------------------
//! The evidence convoy (#3382, chapter #1023). When the evidence locker orders a convoy
//! (LF_OnEvidenceConvoyDue, #3378) the server starts one event scene: a truck with a driver and an
//! escort car spawn at the scene's spawn marker and drive the scene's placed route to its target (the
//! central store), where they despawn out of sight of the players. The on-duty police get a dispatch
//! toast when it leaves. The seized rows ride in the truck's crate: a snapshot of the locker's rows is
//! held by the convoy and the crate prop is a child of the truck. The rows stay in the locker until the
//! truck ARRIVES (LF_EvidenceLocker.RemoveDelivered); a convoy that is lost, timed out or ended by a
//! restart delivers nothing, so the case file is never lost on the road (a hit on the truck is #3384).
//! Core knows no place: the route, spawn and target are the scene's placed event markers
//! (SCENE_ID), which a pack places (#3541). No markers, no convoy: logged, the locker keeps its rows.
//! The truck is driven by LF_EventSceneDrive (#3380); the escort by a second drive over the same route
//! that starts behind the truck. Server only; one convoy at a time; a restart drops it.
class LF_EvidenceConvoy
{
	static const string FEATURE = "EvidenceConvoy";
	//! The scene id the pack's convoy markers carry (event_spawn, event_waypoint, event_target).
	static const string SCENE_ID = "evidence_convoy";
	//! Vanilla prefabs (a core slot names no vehicle): the civilian truck, the police car, a civilian driver, a crate.
	static const string TRUCK = "{A2E23CB8DDCFE82E}Prefabs/Vehicles/Wheeled/Ural4320/Ural4320_transport_CIV_base.et";
	static const ResourceName ESCORT = "{EFEB161F9BC2031D}Prefabs/Vehicles/Wheeled/UAZ469/LF_UAZ469_Police.et";
	static const ResourceName DRIVER = "{11EB9A0D2A5899EA}Prefabs/Characters/Factions/CIV/GenericCivilians/Character_CIV_DenimJacket_1.et";
	static const ResourceName CRATE = "{7030822F7A54F479}Prefabs/Props/Crates/CrateWooden_01_base.et";
	static const int TICK_MS = 1000; // lf-fixed: progress check resolution only
	//! The convoy is given this long to arrive before it is called off (rows stay in the locker).
	static const float MAX_SECONDS = 900; // lf-fixed: a lane across the island at AI speed, with room for a stuck recovery
	//! The escort starts this far behind the truck.
	static const float ESCORT_GAP_M = 9; // lf-fixed: a truck's length plus a car's, so they do not overlap
	//! The crate sits this far above the truck's origin (on the load bed).
	static const float CRATE_LIFT_M = 1.6; // lf-fixed: the Ural's bed height

	protected static ref LF_EvidenceConvoy s_Active;
	protected static ref LF_EvidenceConvoySubscriber s_Subscriber;
	protected static string s_sLastOver;
	protected static int s_iDelivered;

	string m_sSceneId;
	ref LF_EventSceneCrew m_Crew;
	ref LF_EventSceneDrive m_Drive;
	ref LF_EvidenceConvoyEscort m_Escort;
	//! The locker's rows at departure: what rides in the crate.
	ref array<ref LF_EvidenceLockerRow> m_aRows = {};
	EntityID m_CrateId = EntityID.INVALID;
	EntityID m_EscortId = EntityID.INVALID;
	float m_fElapsed;
	float m_fDespawnWait;
	//! The truck reached the target and the rows left the locker.
	bool m_bDelivered;
	//! The convoy is over (delivered or called off) and waits to despawn out of sight.
	bool m_bEnding;
	bool m_bDone;
	bool m_bNotified;
	//! Tests: the eyes the despawn checks instead of the connected players.
	ref array<vector> m_aTestEyes;

	//------------------------------------------------------------------------------------------------
	static LF_EvidenceConvoy Active()
	{
		return s_Active;
	}

	//------------------------------------------------------------------------------------------------
	static string LastOver()
	{
		return s_sLastOver;
	}

	//------------------------------------------------------------------------------------------------
	//! Rows delivered to the central store since the world started.
	static int Delivered()
	{
		return s_iDelivered;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: subscribes the convoy to the locker's order, once per world (the bus table is cleared at world end).
	static void Install()
	{
		if (!Replication.IsServer())
			return;

		if (s_Subscriber)
			LF_EventBus.Unsubscribe(LF_OnEvidenceConvoyDue, s_Subscriber);

		s_Subscriber = new LF_EvidenceConvoySubscriber();
		LF_EventBus.Subscribe(LF_OnEvidenceConvoyDue, s_Subscriber);
	}

	//------------------------------------------------------------------------------------------------
	//! Starts the convoy on the scene `sceneId`: the truck, its driver, the escort and the crate spawn,
	//! the drives begin and the police are told. `driveAi` seats the AI drivers (a world with no AIWorld
	//! only keeps the books); `tick` puts the convoy on the call queue, a test steps it by hand.
	//! \return the convoy, or null (logged) when there is nothing to carry, no route, no AI budget, or one runs already.
	static LF_EvidenceConvoy Start(string sceneId, bool driveAi = true, bool tick = true)
	{
		if (!Replication.IsServer())
			return null;

		if (!LF_Systems.IsEnabled("Investigation"))
			return null;

		if (s_Active)
		{
			LF_Debug.Info(FEATURE, "convoy not started: one is under way");
			return null;
		}

		array<ref LF_EvidenceLockerRow> rows = LF_EvidenceLocker.ExportAll();
		if (rows.IsEmpty())
		{
			LF_Debug.Info(FEATURE, "convoy not started: the locker holds nothing to carry");
			return null;
		}

		array<IEntity> markers = {};
		array<string> waypointIds = {};
		LF_EventWaypointComponent.FindWaypoints(sceneId, markers);
		foreach (IEntity marker : markers)
		{
			LF_EventWaypointComponent waypoint = LF_EventWaypointComponent.Cast(marker.FindComponent(LF_EventWaypointComponent));
			if (waypoint)
				waypointIds.Insert(LF_Places.OrderedId(sceneId, waypoint.GetOrder()));
		}

		markers.Clear();
		if (LF_EventWaypointComponent.FindSpawns(sceneId, markers) == 0 || waypointIds.IsEmpty() || !LF_EventWaypointComponent.FindTarget(sceneId))
		{
			LF_Debug.Warn(FEATURE, string.Format("convoy not started: scene '%1' has no spawn, route and target placed; the locker keeps its %2 row(s)", sceneId, rows.Count()));
			return null;
		}

		int slots = 10;
		if (driveAi)
			slots = LF_EventSceneSpawn.FreeAiSlots();

		if (slots < 2)
		{
			LF_Debug.Warn(FEATURE, string.Format("convoy not started: AI budget used (%1 free, 2 needed); the locker keeps its rows", slots));
			return null;
		}

		LF_EventScene scene = LF_EventScene.Make(sceneId, waypointIds, 1, DRIVER);
		scene.m_fSpawnStaggerSeconds = 0;
		scene.m_sVehicleSlot = TRUCK;
		scene.m_aPrefabs = {DRIVER};
		LF_EventSceneCrew crew = LF_EventSceneSpawn.Start(LF_EventScenePlanner.Plan(scene, slots));
		if (!crew)
			return null;

		LF_EvidenceConvoy convoy = new LF_EvidenceConvoy();
		convoy.m_sSceneId = sceneId;
		convoy.m_Crew = crew;
		convoy.m_aRows = rows;
		s_Active = convoy;
		if (!convoy.Launch(driveAi))
		{
			convoy.Abandon();
			return null;
		}

		LF_Debug.Info(FEATURE, string.Format("convoy leaves with %1 row(s) in the crate", rows.Count()));
		convoy.NotifyPolice();
		if (tick)
			convoy.StartTicking();

		return convoy;
	}

	//------------------------------------------------------------------------------------------------
	//! The truck's crate, the escort and both drives. False (logged) when the truck did not spawn.
	protected bool Launch(bool driveAi)
	{
		BaseWorld world = GetGame().GetWorld();
		IEntity truck = world.FindEntityByID(m_Crew.m_VehicleId);
		if (!truck)
		{
			LF_Debug.Error(FEATURE, "convoy truck did not spawn; the locker keeps its rows");
			return false;
		}

		m_Drive = LF_EventSceneDrive.Begin(m_Crew, driveAi, false);
		if (!m_Drive)
			return false;

		SpawnCrate(truck);
		vector transform[4];
		truck.GetWorldTransform(transform);
		vector behind = transform[3] - transform[2] * ESCORT_GAP_M;
		IEntity escort = LF_Utils.SpawnEntityPrefab(ESCORT, behind, truck.GetYawPitchRoll());
		IEntity driver = LF_Utils.SpawnEntityPrefab(DRIVER, behind + Vector(0, 1, 0), truck.GetYawPitchRoll());
		if (!escort || !driver)
		{
			LF_Debug.Error(FEATURE, "convoy escort car or its driver did not spawn; the locker keeps its rows");
			if (escort)
				SCR_EntityHelper.DeleteEntityAndChildren(escort);

			if (driver)
				SCR_EntityHelper.DeleteEntityAndChildren(driver);

			return false;
		}

		// The convoy is session state: neither the escort car nor its driver may come back from the world save.
		LF_VehicleWorldSave.Hold(escort);
		LF_WorldSaveHold.Hold(driver);
		m_EscortId = escort.GetID();
		// The escort's driver is part of the scene's crew from here on, so the despawn waits for the sight line to clear.
		m_Crew.m_aPeople.Insert(driver.GetID());
		m_Escort = LF_EvidenceConvoyEscort.BeginEscort(m_Crew, escort.GetID(), driver.GetID(), driveAi);
		return m_Escort != null;
	}

	//------------------------------------------------------------------------------------------------
	//! The crate rides on the truck's bed as a child prop (it moves with the truck, takes no physics of its own).
	protected void SpawnCrate(notnull IEntity truck)
	{
		IEntity crate = LF_Utils.SpawnEntityPrefab(CRATE, truck.GetOrigin() + Vector(0, CRATE_LIFT_M, 0), truck.GetYawPitchRoll());
		if (!crate)
		{
			LF_Debug.Error(FEATURE, "convoy crate did not spawn; the rows still ride in the convoy's books");
			return;
		}

		Physics physics = crate.GetPhysics();
		if (physics)
			physics.SetActive(ActiveState.INACTIVE);

		truck.AddChild(crate, -1, EAddChildFlags.AUTO_TRANSFORM);
		m_CrateId = crate.GetID();
	}

	//------------------------------------------------------------------------------------------------
	//! The on-duty police are told the convoy has left (a world with no notification manager logs it).
	protected void NotifyPolice()
	{
		if (!LF_NotificationManagerComponent.GetInstance())
		{
			LF_Debug.Warn(FEATURE, "convoy dispatch toast skipped: no notification manager");
			return;
		}

		LF_NotificationManagerComponent.NotifyPolice("#LF-EvidenceConvoy_Title", "#LF-EvidenceConvoy_Underway", 8.0);
		m_bNotified = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Advances the books by `seconds`: both drives, the arrival, the timeout and the despawn.
	void Step(float seconds)
	{
		if (m_bDone)
			return;

		m_fElapsed += seconds;
		if (m_bEnding)
		{
			TryClear(seconds);
			return;
		}

		m_Drive.Step(seconds);
		m_Escort.Step(seconds);
		if (m_Drive.m_bOver)
		{
			if (m_Drive.m_bArrived)
				Deliver();
			else
				CallOff("the truck was lost on the road");
		}
		else if (m_fElapsed >= MAX_SECONDS)
		{
			CallOff(string.Format("the truck did not arrive in %1 s", MAX_SECONDS));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The truck reached the central store: the carried rows leave the locker.
	protected void Deliver()
	{
		array<int> numbers = {};
		foreach (LF_EvidenceLockerRow row : m_aRows)
		{
			numbers.Insert(row.m_iNumber);
		}

		int removed = LF_EvidenceLocker.RemoveDelivered(numbers);
		s_iDelivered += removed;
		m_bDelivered = true;
		m_bEnding = true;
		s_sLastOver = string.Format("delivered %1 row(s) to the central store after %2 s", removed, m_fElapsed);
		LF_Debug.Info(FEATURE, "convoy " + s_sLastOver);
	}

	//------------------------------------------------------------------------------------------------
	//! The convoy is called off: nothing is delivered, the rows stay in the locker.
	protected void CallOff(string why)
	{
		m_bEnding = true;
		s_sLastOver = "called off: " + why;
		LF_Debug.Warn(FEATURE, string.Format("convoy called off (%1); the locker keeps its %2 row(s)", why, m_aRows.Count()));
	}

	//------------------------------------------------------------------------------------------------
	//! Removes the convoy once no player sees it; a watcher holds it only for the scene's force-despawn time.
	protected void TryClear(float seconds)
	{
		m_fDespawnWait += seconds;
		bool force = m_fDespawnWait >= m_Crew.m_Plan.m_fForceDespawnSeconds;
		if (!LF_EventSceneSpawn.TryDespawn(m_Crew, m_aTestEyes, null, force))
			return;

		Cleanup();
		m_bDone = true;
		// The locker may still stand at the threshold after a delivery: order the next convoy.
		if (m_bDelivered)
		{
			LF_EvidenceLockerFill.ResetStatics();
			LF_EvidenceLockerFill.Check();
		}
		else
		{
			// Called off: the rows are still in the locker and the order is still "due"; re-arm it after a cool-down.
			LF_EvidenceLockerFill.ScheduleRearm();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! A partial start: everything the convoy spawned goes at once, nobody sees it leave.
	protected void Abandon()
	{
		array<vector> nobody = {};
		LF_EventSceneSpawn.TryDespawn(m_Crew, nobody);
		Cleanup();
		m_bDone = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Ends both drives and deletes the escort car (the truck, its crate and the people went with the scene).
	protected void Cleanup()
	{
		GetGame().GetCallqueue().Remove(Tick);
		if (m_Drive)
			m_Drive.Stop();

		if (m_Escort)
			m_Escort.Stop();

		IEntity escort = GetGame().GetWorld().FindEntityByID(m_EscortId);
		if (escort)
			SCR_EntityHelper.DeleteEntityAndChildren(escort);

		m_EscortId = EntityID.INVALID;
		m_CrateId = EntityID.INVALID;
		if (s_Active == this)
			s_Active = null;

		LF_Debug.Info(FEATURE, "convoy despawned");
	}

	//------------------------------------------------------------------------------------------------
	//! Puts the convoy on the call queue; Cleanup takes it off.
	protected void StartTicking()
	{
		GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Call queue tick.
	void Tick()
	{
		Step(TICK_MS / 1000.0);
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		if (s_Active && GetGame() && GetGame().GetCallqueue())
		{
			GetGame().GetCallqueue().Remove(s_Active.Tick);
			if (s_Active.m_Drive)
				s_Active.m_Drive.Stop();

			if (s_Active.m_Escort)
				s_Active.m_Escort.Stop();
		}

		s_Active = null;
		if (s_Subscriber)
			LF_EventBus.Unsubscribe(LF_OnEvidenceConvoyDue, s_Subscriber);

		s_Subscriber = null;
		s_sLastOver = "";
		s_iDelivered = 0;
	}
}

//------------------------------------------------------------------------------------------------
//! The escort car's drive: LF_EventSceneDrive over the same route, for its own one-person crew. It
//! is never the scene's active drive (the truck's is), and it follows the truck's scene rather than
//! running its own, so it ends with the convoy. Server only.
class LF_EvidenceConvoyEscort : LF_EventSceneDrive
{
	//------------------------------------------------------------------------------------------------
	//! \return the escort's drive, or null (logged) when its route is not placed.
	static LF_EvidenceConvoyEscort BeginEscort(notnull LF_EventSceneCrew truckCrew, EntityID vehicleId, EntityID driverId, bool driveAi)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world || !world.FindEntityByID(vehicleId))
			return null;

		LF_EventSceneCrew crew = new LF_EventSceneCrew();
		crew.m_sSceneId = truckCrew.m_sSceneId;
		crew.m_Plan = truckCrew.m_Plan;
		crew.m_VehicleId = vehicleId;
		crew.m_aPeople.Insert(driverId);
		LF_EvidenceConvoyEscort escort = new LF_EvidenceConvoyEscort();
		escort.m_Crew = crew;
		if (!escort.ResolveRoute())
			return null;

		if (driveAi)
			escort.SetUpAi(world);

		return escort;
	}

	//------------------------------------------------------------------------------------------------
	//! The convoy steps this drive itself.
	override void Tick()
	{
	}
}

//------------------------------------------------------------------------------------------------
//! Bus subscriber: the evidence locker orders a convoy.
class LF_EvidenceConvoySubscriber : LF_EventSubscriber
{
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnEvidenceConvoyDue due = LF_OnEvidenceConvoyDue.Cast(busEvent);
		if (!due)
			return true;

		// A refused start (no route markers, no AI budget, one under way) left the order "due" for good; re-arm after a cool-down.
		if (!LF_EvidenceConvoy.Start(LF_EvidenceConvoy.SCENE_ID))
			LF_EvidenceLockerFill.ScheduleRearm();

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_EvidenceConvoy's statics (LF_StaticReset finds it by reflection). A convoy
//! under way is not resumed; its rows are still in the locker.
class LF_EvidenceConvoy_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_EvidenceConvoy";
	}

	override void Apply()
	{
		LF_EvidenceConvoy.ResetStatics();
	}
}
