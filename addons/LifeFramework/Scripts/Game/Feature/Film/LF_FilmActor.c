// LF_FilmActor.c - scripted actors for the video pipeline (#393, epic #390).
//
// DebugWorld has no AIWorld and no navmesh, so an actor is a spawned character driven from script on a
// timed list of steps: walk, run, enter, drive, exit, use <action>, wait. Waypoints are authored.
// A character walks through CharacterControllerComponent; a driver sits in the seat and a tick sets the
// vehicle's throttle, steering and brake (VehicleWheeledSimulation). An actor that makes no progress for
// STUCK_SECONDS logs and stops. Harness-only: scenes start from the `scene` harness verb, which passes
// LF_DevGate; actors carry a stand-in player uid only in a build compiled with -scrDefine LF_HARNESS.

class LF_FilmStep : Managed
{
	string Kind;               // walk | run | enter | drive | exit | use | wait
	float Start;               // earliest scene second to begin
	ref array<vector> Path = {};
	string Arg;                // enter: vehicle key; use: action class name
	float Value;               // drive: km/h; wait: seconds
	float Radius = 1.5;        // last waypoint reached within this many metres
};

//------------------------------------------------------------------------------------------------
//! Pure maths, covered by LF_Test_FilmActorMath.
class LF_FilmMath
{
	//------------------------------------------------------------------------------------------------
	static float WrapRad(float angle)
	{
		while (angle > Math.PI)
			angle -= Math.PI2;
		while (angle < -Math.PI)
			angle += Math.PI2;
		return angle;
	}

	//------------------------------------------------------------------------------------------------
	//! Steering input in -1..1 (positive turns toward +yaw) from the heading error to a waypoint.
	static float Steer(float forwardYaw, float desiredYaw, float gain = 1.2)
	{
		return Math.Clamp(WrapRad(desiredYaw - forwardYaw) * gain, -1, 1);
	}

	//------------------------------------------------------------------------------------------------
	//! Throttle 0..1 and brake 0..1 holding targetKmh.
	static void Pedals(float speedKmh, float targetKmh, out float throttle, out float brake)
	{
		throttle = 0;
		brake = 0;
		float err = targetKmh - speedKmh;
		if (targetKmh <= 0.5)
		{
			brake = 1;
			return;
		}
		if (err >= 0)
			throttle = Math.Clamp(err * 0.15, 0, 1);
		else
			brake = Math.Clamp(-err * 0.1, 0, 1);
	}
};

//------------------------------------------------------------------------------------------------
//! No progress for N seconds. Feed it the distance left to the target each tick.
class LF_FilmStuck : Managed
{
	protected float m_fBest = 1000000;
	protected float m_fIdle;
	protected float m_fLimit;

	void LF_FilmStuck(float limitSeconds = 6)
	{
		m_fLimit = limitSeconds;
	}

	//------------------------------------------------------------------------------------------------
	void Reset()
	{
		m_fBest = 1000000;
		m_fIdle = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! True once the distance has not shrunk by 0.25 m for the limit.
	bool Update(float dt, float distance)
	{
		if (distance < m_fBest - 0.25)
		{
			m_fBest = distance;
			m_fIdle = 0;
			return false;
		}

		m_fIdle += dt;
		return m_fIdle >= m_fLimit;
	}
};

//------------------------------------------------------------------------------------------------
//! Stand-in player uids for actors. Read by LF_Utils.GetPlayerUID in a LF_HARNESS build only.
class LF_FilmIdentity
{
	protected static ref map<string, string> s_mUids = new map<string, string>();

	//------------------------------------------------------------------------------------------------
	static void Set(IEntity actor, string uid)
	{
		if (actor)
			s_mUids.Set(actor.GetID().ToString(), uid);
	}

	//------------------------------------------------------------------------------------------------
	static void Clear()
	{
		s_mUids.Clear();
	}

	//------------------------------------------------------------------------------------------------
	static string Get(IEntity actor)
	{
		if (!actor || s_mUids.IsEmpty())
			return "";
		string uid;
		if (s_mUids.Find(actor.GetID().ToString(), uid))
			return uid;
		return "";
	}
};

//------------------------------------------------------------------------------------------------
class LF_FilmActor : Managed
{
	protected static const string FEATURE = "FilmActor";
	protected static const float STUCK_SECONDS = 6;
	//! Pedal inputs never moved the car in the 2026-11-10 spike (docs/film-pipeline.md); velocity drive does.
	protected static const int START_PHASE = 3;

	string Name;
	EntityID Id = EntityID.INVALID;
	bool Finished;
	ref array<ref LF_FilmStep> Steps = {};
	protected int m_iStep;
	protected int m_iPoint;
	protected float m_fStepStart = -1;
	protected ref LF_FilmStuck m_Stuck = new LF_FilmStuck(STUCK_SECONDS);
	protected EntityID m_VehicleId = EntityID.INVALID;
	protected int m_iLogTicks;
	protected bool m_bActed;
	protected int m_iPhase = START_PHASE;
	protected float m_fPhaseIdle;

	//------------------------------------------------------------------------------------------------
	LF_FilmStep AddStep(string kind, float start, string arg = "", float value = 0, float radius = 1.5)
	{
		LF_FilmStep step = new LF_FilmStep();
		step.Kind = kind;
		step.Start = start;
		step.Arg = arg;
		step.Value = value;
		step.Radius = radius;
		Steps.Insert(step);
		return step;
	}

	//------------------------------------------------------------------------------------------------
	protected void Finish(string why)
	{
		Finished = true;
		LF_Debug.Info(FEATURE, string.Format("%1 finished: %2", Name, why));
		StopMotion();
	}

	//------------------------------------------------------------------------------------------------
	protected void NextStep()
	{
		m_iStep++;
		m_iPoint = 0;
		m_fStepStart = -1;
		m_bActed = false;
		m_Stuck.Reset();
		if (m_iStep >= Steps.Count())
			Finish("all steps done");
	}

	//------------------------------------------------------------------------------------------------
	protected void StopMotion()
	{
		IEntity vehicle = GetGame().GetWorld().FindEntityByID(m_VehicleId);
		if (vehicle)
		{
			VehicleWheeledSimulation sim = VehicleWheeledSimulation.Cast(vehicle.FindComponent(VehicleWheeledSimulation));
			if (sim)
			{
				sim.SetThrottle(0);
				sim.SetSteering(0);
				sim.SetBreak(1, true);
			}
		}

		ChimeraCharacter character = ChimeraCharacter.Cast(GetGame().GetWorld().FindEntityByID(Id));
		if (character && character.GetCharacterController())
			character.GetCharacterController().SetMovement(0, vector.Zero);
	}

	//------------------------------------------------------------------------------------------------
	//! One server tick. sceneTime and dt in seconds. vehicles maps a vehicle key to its entity id.
	void Tick(float sceneTime, float dt, map<string, EntityID> vehicles)
	{
		if (Finished)
			return;

		BaseWorld world = GetGame().GetWorld();
		ChimeraCharacter character = ChimeraCharacter.Cast(world.FindEntityByID(Id));
		if (!character)
		{
			Finish("error: actor entity gone");
			return;
		}

		LF_FilmStep step = Steps[m_iStep];
		if (sceneTime < step.Start)
			return;
		if (m_fStepStart < 0)
		{
			m_fStepStart = sceneTime;
			LF_Debug.Info(FEATURE, string.Format("%1 step %2 %3 begins at t=%4", Name, m_iStep, step.Kind, sceneTime));
		}

		if (step.Kind == "walk" || step.Kind == "run")
			TickMove(character, step, dt, step.Kind == "run");
		else if (step.Kind == "drive")
			TickDrive(character, step, dt);
		else if (step.Kind == "enter")
			TickEnter(character, step, sceneTime, vehicles);
		else if (step.Kind == "exit")
			TickExit(character, step);
		else if (step.Kind == "use")
			TickUse(character, step);
		else if (step.Kind == "wait")
		{
			if (sceneTime - m_fStepStart >= step.Value)
				NextStep();
		}
		else
			Finish("error: unknown step kind " + step.Kind);
	}

	//------------------------------------------------------------------------------------------------
	protected void TickMove(ChimeraCharacter character, LF_FilmStep step, float dt, bool run)
	{
		CharacterControllerComponent controller = character.GetCharacterController();
		if (!controller || step.Path.IsEmpty())
		{
			Finish("error: no controller or empty path");
			return;
		}

		vector pos = character.GetOrigin();
		vector target = step.Path[m_iPoint];
		vector delta = target - pos;
		delta[1] = 0;
		float dist = delta.Length();
		bool last = m_iPoint >= step.Path.Count() - 1;
		float radius = 1.0;
		if (last)
			radius = step.Radius;
		if (dist <= radius)
		{
			if (last)
			{
				controller.SetMovement(0, vector.Zero);
				NextStep();
				return;
			}
			m_iPoint++;
			m_Stuck.Reset();
			return;
		}

		if (m_Stuck.Update(dt, dist))
		{
			LF_Debug.Warn(FEATURE, string.Format("%1 stuck at %2, %3 m from waypoint %4: stopping", Name, pos, dist, m_iPoint));
			Finish("stuck");
			return;
		}

		float speed = 1;
		if (run)
			speed = 2;
		controller.SetHeadingAngle(Math.Atan2(delta[0], delta[2]));
		controller.SetMovement(speed, Vector(0, 0, 1));
	}

	//------------------------------------------------------------------------------------------------
	protected void TickEnter(ChimeraCharacter character, LF_FilmStep step, float sceneTime, map<string, EntityID> vehicles)
	{
		EntityID vid;
		IEntity vehicle;
		if (vehicles.Find(step.Arg, vid))
			vehicle = GetGame().GetWorld().FindEntityByID(vid);
		if (!vehicle)
		{
			Finish("error: no vehicle " + step.Arg);
			return;
		}

		SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(character.GetCompartmentAccessComponent());
		if (!access)
		{
			Finish("error: no compartment access");
			return;
		}

		if (!m_bActed)
		{
			m_bActed = true;
			bool moved = access.MoveInVehicle(vehicle, ECompartmentType.PILOT);
			LF_Debug.Info(FEATURE, string.Format("%1 MoveInVehicle(%2)=%3", Name, step.Arg, moved));
			if (!moved)
				Finish("error: seat refused");
			return;
		}

		if (!access.IsInCompartment())
		{
			if (sceneTime - m_fStepStart > 5)
				Finish("error: not seated after 5 s");
			return;
		}

		m_VehicleId = vehicle.GetID();
		VehicleControllerComponent engine = VehicleControllerComponent.Cast(vehicle.FindComponent(VehicleControllerComponent));
		if (engine && !engine.IsEngineOn())
		{
			engine.StartEngine();
			return;
		}

		LF_Debug.Info(FEATURE, string.Format("%1 seated in %2, engine on=%3", Name, step.Arg, engine && engine.IsEngineOn()));
		NextStep();
	}

	//------------------------------------------------------------------------------------------------
	protected void TickDrive(ChimeraCharacter character, LF_FilmStep step, float dt)
	{
		IEntity vehicle = GetGame().GetWorld().FindEntityByID(m_VehicleId);
		if (!vehicle || step.Path.IsEmpty())
		{
			Finish("error: not in a vehicle or empty path");
			return;
		}

		VehicleWheeledSimulation sim = VehicleWheeledSimulation.Cast(vehicle.FindComponent(VehicleWheeledSimulation));
		if (!sim)
		{
			Finish("error: no wheeled simulation");
			return;
		}

		CarControllerComponent car = CarControllerComponent.Cast(vehicle.FindComponent(CarControllerComponent));
		if (car)
			car.SetPersistentHandBrake(false);

		vector mat[4];
		vehicle.GetWorldTransform(mat);
		vector pos = mat[3];
		vector target = step.Path[m_iPoint];
		vector delta = target - pos;
		delta[1] = 0;
		float dist = delta.Length();
		bool last = m_iPoint >= step.Path.Count() - 1;
		float speed = sim.GetSpeedKmh();

		float reach = 6;
		if (last)
			reach = step.Radius;
		if (dist <= reach)
		{
			if (last)
			{
				if (m_iPhase >= 3 && vehicle.GetPhysics())
				{
					vehicle.GetPhysics().SetVelocity(vector.Zero);
					vehicle.GetPhysics().SetAngularVelocity(vector.Zero);
				}
				sim.SetThrottle(0);
				sim.SetSteering(0);
				sim.SetBreak(1, true);
				if (speed < 2)
					NextStep();
				return;
			}
			m_iPoint++;
			m_Stuck.Reset();
			return;
		}

		float steer = LF_FilmMath.Steer(Math.Atan2(mat[2][0], mat[2][2]), Math.Atan2(delta[0], delta[2]));
		float targetSpeed = step.Value;
		if (Math.AbsFloat(steer) > 0.5)
			targetSpeed = Math.Min(targetSpeed, 15);
		if (last && m_iPhase < 3 && dist < speed * 0.7)
			targetSpeed = 0;

		float throttle;
		float brake;
		LF_FilmMath.Pedals(speed, targetSpeed, throttle, brake);

		// Escalation, logged: phase 0 pedals only; 1 adds clutch/gear/handbrake; 2 adds ForceEnableSimulation;
		// 3 drives the physics body (velocity) under the same steering; 4 moves the entity (kinematic).
		if (m_iPhase < 4 && speed < 0.5 && throttle > 0.5)
		{
			m_fPhaseIdle += dt;
			if (m_fPhaseIdle > 3)
			{
				m_iPhase++;
				m_fPhaseIdle = 0;
				m_Stuck.Reset();
				LF_Debug.Info(FEATURE, string.Format("%1 no movement with throttle on: escalating to phase %2", Name, m_iPhase));
			}
		}
		if (speed >= 0.5)
			m_fPhaseIdle = 0;

		if (m_iPhase >= 1)
		{
			sim.SetClutch(0);
			if (sim.GetGear() <= 1)
				sim.SetGear(2);
		}
		if (m_iPhase >= 2)
			sim.ForceEnableSimulation();

		sim.SetSteering(steer);
		sim.SetThrottle(throttle);
		sim.SetBreak(brake, false);

		if (m_iPhase >= 3)
		{
			vector fwd = mat[2];
			float ms = targetSpeed / 3.6;
			if (m_iPhase == 3)
			{
				Physics phys = vehicle.GetPhysics();
				if (phys)
				{
					float taper = 1;
					if (last)
						taper = Math.Clamp(dist / 20, 0.15, 1);
					phys.SetVelocity(fwd * (ms * taper));
					phys.SetAngularVelocity(Vector(0, steer * ms * 0.12, 0));
				}
			}
			else
			{
				vector kin[4];
				vehicle.GetWorldTransform(kin);
				float yaw = Math.Atan2(fwd[0], fwd[2]) + steer * ms * 0.12 * dt;
				kin[2] = Vector(Math.Sin(yaw), 0, Math.Cos(yaw));
				kin[0] = Vector(kin[2][2], 0, -kin[2][0]);
				kin[1] = vector.Up;
				kin[3] = kin[3] + kin[2] * ms * dt;
				kin[3][1] = GetGame().GetWorld().GetSurfaceY(kin[3][0], kin[3][2]);
				vehicle.SetWorldTransform(kin);
			}
		}

		m_iLogTicks++;
		if (m_iLogTicks >= 10)
		{
			m_iLogTicks = 0;
			string gear = "";
			if (car)
				gear = string.Format("gear=%1 hb=%2 auto=%3", car.GetCurrentGear(), car.GetHandBrake(), car.HasAutomaticGearbox());
			LF_Debug.Info(FEATURE, string.Format("%1 drive phase=%2 pos=%3 wp=%4 dist=%5 speed=%6 steer=%7 thr=%8", Name, m_iPhase, pos, m_iPoint, dist, speed, steer, throttle));
			LF_Debug.Info(FEATURE, string.Format("%1 brk=%2 clutch=%3 %4", Name, brake, sim.GetClutch(), gear));
		}

		if (m_Stuck.Update(dt, dist))
		{
			LF_Debug.Warn(FEATURE, string.Format("%1 stuck at %2, %3 m from waypoint %4: stopping", Name, pos, dist, m_iPoint));
			Finish("stuck");
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void TickExit(ChimeraCharacter character, LF_FilmStep step)
	{
		StopMotion();
		SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(character.GetCompartmentAccessComponent());
		if (access && access.IsInCompartment())
			access.GetOutVehicle(EGetOutType.TELEPORT, 0, ECloseDoorAfterActions.CLOSE_DOOR, false);
		m_VehicleId = EntityID.INVALID;
		NextStep();
	}

	//------------------------------------------------------------------------------------------------
	//! Perform every scripted action of class step.Arg on the nearest site within 6 m of the actor.
	protected void TickUse(ChimeraCharacter character, LF_FilmStep step)
	{
		IEntity site = LF_ActionSite.FindNear(character.GetOrigin(), 6, step.Arg);
		if (!site)
		{
			LF_Debug.Warn(FEATURE, string.Format("%1 use %2: no site within 6 m of %3", Name, step.Arg, character.GetOrigin()));
			Finish("error: no site");
			return;
		}

		BaseActionsManagerComponent manager = BaseActionsManagerComponent.Cast(site.FindComponent(BaseActionsManagerComponent));
		array<BaseUserAction> actions = {};
		if (manager)
			manager.GetActionsList(actions);
		bool performed;
		foreach (BaseUserAction action : actions)
		{
			ScriptedUserAction scripted = ScriptedUserAction.Cast(action);
			if (!scripted || action.Type().ToString() != step.Arg)
				continue;

			bool can = scripted.CanBePerformedScript(character);
			LF_Debug.Info(FEATURE, string.Format("%1 use %2 canPerform=%3 uid='%4'", Name, step.Arg, can, LF_Utils.GetPlayerUid(character)));
			scripted.PerformAction(site, character);
			performed = true;
		}

		if (!performed)
			LF_Debug.Warn(FEATURE, string.Format("%1 use %2: site has no such action", Name, step.Arg));
		NextStep();
	}
};

//------------------------------------------------------------------------------------------------
//! Spawns a named scene's actors and vehicles on the server and ticks them every 100 ms.
class LF_FilmDirector
{
	protected static const string FEATURE = "FilmActor";
	protected static const ResourceName CHARACTER = "{9B5BB216CC7FF18E}Prefabs/Characters/Core/Character_Roleplay.et";
	protected static const ResourceName CAR = "{54C3CC22DEBD57BE}Prefabs/Vehicles/Wheeled/S105/S105_beige.et";
	protected static const float TICK_MS = 100;
	protected static const float MAX_SECONDS = 90;

	protected static ref array<ref LF_FilmActor> s_aActors = {};
	protected static ref map<string, EntityID> s_mVehicles = new map<string, EntityID>();
	protected static ref array<EntityID> s_aSpawned = {};
	protected static ref LF_FilmSceneEnv s_Saved;     // the world before the scene, restored by Clear
	protected static string s_sScene;
	protected static float s_fStartMs;
	protected static bool s_bRunning;

	//------------------------------------------------------------------------------------------------
	static string Start(string scene, string hour = "", string weather = "")
	{
		if (!Replication.IsServer())
			return "error:not_server";
		if (s_bRunning)
			return "error:scene_running";
		bool envOnly = LF_FilmSceneEnv.IsEnvOnly(scene);
		if (scene != "street" && !envOnly)
			return "error:unknown_scene";

		LF_FilmSceneEnv env = LF_FilmSceneEnv.ForScene(scene);
		if (!env.Override(hour, weather))
			return "error:bad_hour";

		Clear();
		s_Saved = LF_FilmEnvironment.Capture();
		if (s_Saved)
			LF_FilmEnvironment.Apply(env, "scene " + scene + " set");
		if (envOnly)
		{
			// Light only: no actors, nothing to tick. Stop restores the clock like any scene.
			s_sScene = scene;
			s_bRunning = true;
			return string.Format("scene %1 started, light only", scene);
		}
		if (!BuildStreet())
		{
			Clear();
			return "error:build_failed";
		}

		s_sScene = scene;
		s_bRunning = true;
		s_fStartMs = GetGame().GetWorld().GetWorldTime();
		GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);
		return string.Format("scene %1 started, %2 actors", scene, s_aActors.Count());
	}

	//------------------------------------------------------------------------------------------------
	static string Status()
	{
		string text = string.Format("running=%1 scene=%2 restore=%3", s_bRunning, s_sScene, s_Saved != null);
		foreach (LF_FilmActor actor : s_aActors)
			text += string.Format(" %1:%2", actor.Name, actor.Finished);
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Reset between takes: despawn every actor and vehicle the scene made and give the clock and weather back.
	static string Stop()
	{
		bool restore = s_Saved != null;
		int removed = s_aActors.Count() + s_aSpawned.Count();
		Clear();
		return string.Format("scene cleared, %1 removed, clock and weather restored=%2", removed, restore);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Clear()
	{
		GetGame().GetCallqueue().Remove(Tick);
		s_bRunning = false;
		BaseWorld world = GetGame().GetWorld();
		foreach (LF_FilmActor actor : s_aActors)
		{
			IEntity entity = world.FindEntityByID(actor.Id);
			if (entity)
				SCR_EntityHelper.DeleteEntityAndChildren(entity);
		}
		foreach (EntityID id : s_aSpawned)
		{
			IEntity spawned = world.FindEntityByID(id);
			if (spawned)
				SCR_EntityHelper.DeleteEntityAndChildren(spawned);
		}
		if (s_Saved)
		{
			LF_FilmEnvironment.Apply(s_Saved, "scene reset");
			s_Saved = null;
		}
		s_sScene = "";
		s_aActors.Clear();
		s_aSpawned.Clear();
		s_mVehicles.Clear();
		LF_FilmIdentity.Clear();
	}

	//------------------------------------------------------------------------------------------------
	protected static vector OnGround(float x, float z)
	{
		return Vector(x, GetGame().GetWorld().GetSurfaceY(x, z), z);
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_FilmActor SpawnActor(string name, vector at)
	{
		IEntity entity = LF_Utils.SpawnEntityPrefab(CHARACTER, at);
		if (!entity)
		{
			LF_Debug.Error(FEATURE, "actor spawn failed: " + name);
			return null;
		}

		LF_FilmActor actor = new LF_FilmActor();
		actor.Name = name;
		actor.Id = entity.GetID();
		LF_FilmIdentity.Set(entity, "film-" + name);
		s_aActors.Insert(actor);
		return actor;
	}

	//------------------------------------------------------------------------------------------------
	//! "street": the walker crosses to the general store till and uses it; the driver gets into a car
	//! parked at the west end of the main street (z 250) and drives east past the store.
	protected static bool BuildStreet()
	{
		IEntity till = LF_ActionSite.FindNear(Vector(180, 0, 244), 80, "LF_StoreTillAction");
		if (!till)
		{
			LF_Debug.Error(FEATURE, "scene street: no store till within 80 m of 180 244");
			return false;
		}

		vector tillPos = till.GetOrigin();
		LF_FilmActor walker = SpawnActor("walker", OnGround(tillPos[0] + 14, tillPos[2]));
		if (!walker)
			return false;
		LF_FilmStep walk = walker.AddStep("walk", 1.0, "", 0, 2.5);
		walk.Path.Insert(OnGround(tillPos[0] + 7, tillPos[2] + 1));
		walk.Path.Insert(tillPos);
		walker.AddStep("use", 0, "LF_StoreTillAction");
		walker.AddStep("wait", 0, "", 2);

		vector carPos = OnGround(128, 250);
		IEntity car = LF_Utils.SpawnEntityPrefab(CAR, carPos, Vector(90, 0, 0));
		if (!car)
		{
			LF_Debug.Error(FEATURE, "scene street: car did not spawn");
			return false;
		}
		s_aSpawned.Insert(car.GetID());
		s_mVehicles.Set("car", car.GetID());
		LF_Debug.Info(FEATURE, string.Format("car spawned at %1", carPos));

		LF_FilmActor driver = SpawnActor("driver", OnGround(131, 247));
		if (!driver)
			return false;
		driver.AddStep("enter", 0.5, "car");
		LF_FilmStep drive = driver.AddStep("drive", 0, "", 30, 5);
		float x;
		for (x = 150; x <= 250; x += 25)
			drive.Path.Insert(OnGround(x, 250));
		driver.AddStep("wait", 0, "", 1);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tick()
	{
		float t = (GetGame().GetWorld().GetWorldTime() - s_fStartMs) / 1000.0;
		bool anyLeft;
		foreach (LF_FilmActor actor : s_aActors)
		{
			actor.Tick(t, TICK_MS / 1000.0, s_mVehicles);
			if (!actor.Finished)
				anyLeft = true;
		}

		if (!anyLeft || t > MAX_SECONDS)
		{
			LF_Debug.Info(FEATURE, string.Format("scene ended at t=%1 (%2)", t, Status()));
			GetGame().GetCallqueue().Remove(Tick);
			s_bRunning = false;
		}
	}
};

//------------------------------------------------------------------------------------------------
modded class SCR_ChimeraCharacter
{
	//------------------------------------------------------------------------------------------------
	void LF_AskFilmScene(int commandId, string mode, string scene, string hour, string weather)
	{
		if (Replication.IsServer())
		{
			LF_AgentHarness.Reply(commandId, "ok", LF_FilmSceneRequest(mode, scene, hour, weather));
			return;
		}

		Rpc(RpcAsk_LF_FilmScene, commandId, mode, scene, hour, weather);
	}

	//------------------------------------------------------------------------------------------------
	protected static string LF_FilmSceneRequest(string mode, string scene, string hour, string weather)
	{
		if (mode == "start")
			return LF_FilmDirector.Start(scene, hour, weather);
		if (mode == "stop" || mode == "reset")
			return LF_FilmDirector.Stop();
		return LF_FilmDirector.Status();
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_FilmScene(int commandId, string mode, string scene, string hour, string weather)
	{
		string result = "error:dev_gate";
		if (LF_DevGate.CheckHarnessRequest(this, "scene"))
			result = LF_FilmSceneRequest(mode, scene, hour, weather);

		Rpc(RpcDo_LF_FilmScene, commandId, result);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_FilmScene(int commandId, string result)
	{
		LF_AgentHarness.Reply(commandId, "ok", result);
	}
}
