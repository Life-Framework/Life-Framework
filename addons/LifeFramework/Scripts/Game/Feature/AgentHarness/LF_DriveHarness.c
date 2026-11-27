//------------------------------------------------------------------------------------------------
//! Harness verbs that drive a car the way a player does (agent-box/proofs/drive-proof.mjs):
//!   vehicle enter driver|passenger [at=x,z] [veh_<text>] [r=<m>]
//!                     seats this client's character in the nearest vehicle (to the character, or to
//!                     at=x,z), within r metres (default 8); veh_<text> narrows it to a prefab path
//!   vehicle exit [force]   gets out (refused above 3 km/h unless `force`)
//!   vehicle stop      ends a drive, brakes to a standstill and sets the handbrake
//!   vehicle state     seat, speed, position, heading, engine, gear, handbrake, health, owner, drive
//!   vehicle engine on|off, vehicle lights on|off
//!   drive to <x,z> [speed=<kmh>] [timeout=<s>] [tol=<m>] [dist=<m>] [mode=input|sim] [steer=flip] [wait]
//!                     dist=<m> stops after driving that far (status `distance`)
//!   drive path <x,z;x,z;...> [same options]
//!   drive road <x,z> [same options]   follows the AI road network from here to the point
//!   drive wait        answers when the running drive ends (arrived, timeout, stuck, stopped)
//! A drive answers at once (`ok driving ...`) unless `wait` is given; `vehicle state` shows its
//! progress and `drive wait` its end. Options are key=value tokens so they fit the five words a
//! feature verb gets.
//! It runs on the client that sits at the wheel. That client owns the car while it drives, so the
//! inputs it gives are the ones the server sees, as with a player's keys. Steering is a pure pursuit
//! of a point a little ahead on the path; throttle and brake hold a target speed that drops in bends
//! and before the last point. mode=input (default) gives the car's own input actions every frame
//! (CarThrust, CarBrake, CarSteering, CarHandBrake), as the keys would; mode=sim sets the wheeled
//! simulation directly, the way vanilla's cinematic tracks drive a car. The harness poll itself is
//! behind the dev gate.
//------------------------------------------------------------------------------------------------
class LF_DriveHarness
{
	static const string FEATURE = "AgentHarness";

	//! CarSteering turns the wheels left for a positive value (seen in the island session): a target to
	//! the right (positive angle) needs a negative input.
	static const float STEER_SIGN = -1; // lf-fixed: the input axis direction
	static const float DEFAULT_SPEED_KMH = 30; // lf-fixed: harness default, not a gameplay knob
	static const float MAX_SPEED_KMH = 90; // lf-fixed: harness guard
	static const float DEFAULT_TIMEOUT_S = 120; // lf-fixed: harness default
	static const float MAX_TIMEOUT_S = 600; // lf-fixed: harness guard
	static const float DEFAULT_TOLERANCE_M = 2.5; // lf-fixed: harness default
	static const float ENTER_RADIUS_M = 8; // lf-fixed: harness default
	//! Below this speed the car counts as standing still.
	static const float STILL_KMH = 1.5; // lf-fixed: harness geometry
	//! Comfortable deceleration for the speed ahead of the last point, m/s^2.
	static const float DECEL = 2.0; // lf-fixed: harness tuning
	//! Steering is full lock at this angle to the pursuit point, degrees.
	static const float FULL_LOCK_DEG = 35; // lf-fixed: harness tuning
	//! The speed cap while the pursuit point is far off the nose, km/h.
	static const float BEND_KMH = 15; // lf-fixed: harness tuning
	static const float STUCK_S = 4; // lf-fixed: harness tuning
	static const float BACKUP_S = 2.5; // lf-fixed: harness tuning
	static const int MAX_BACKUPS = 3; // lf-fixed: harness guard
	//! Road routes keep this far right of the road's middle line.
	static const float LANE_M = 1.5; // lf-fixed: harness geometry
	static const float ROAD_MARGIN_M = 250; // lf-fixed: harness geometry
	static const float ROAD_JOIN_M = 6; // lf-fixed: harness geometry
	static const int MAX_ROAD_NODES = 6000; // lf-fixed: harness guard

	// Drive state, all on the driving client.
	protected static ref array<vector> s_aRoute = {};
	protected static int s_iSeg;
	protected static string s_sStatus = "idle";
	protected static string s_sMode = "input";
	protected static float s_fSpeedCap = DEFAULT_SPEED_KMH;
	protected static float s_fTolerance = DEFAULT_TOLERANCE_M;
	protected static float s_fMaxDistance;
	protected static float s_fSteerSign = STEER_SIGN;
	protected static float s_fStartMs; // lf-float: world time in ms, harness only
	protected static float s_fEndMs; // lf-float: world time in ms, harness only
	protected static float s_fStillSinceMs; // lf-float: world time in ms, harness only
	protected static float s_fBackupUntilMs; // lf-float: world time in ms, harness only
	protected static float s_fDoneMs; // lf-float: world time in ms, harness only
	protected static int s_iBackups;
	protected static float s_fDriven;
	protected static vector s_vLast;
	protected static vector s_vStart;
	protected static bool s_bStopping;
	protected static bool s_bActive;
	protected static int s_iWaitId;
	protected static int s_iTicks;
	protected static int s_iPrepares;
	protected static float s_fMaxKmh;
	protected static string s_sResult;

	// The inputs this frame.
	static float s_fThrust;
	static float s_fBrake;
	static float s_fSteer;
	static float s_fHand;

	//------------------------------------------------------------------------------------------------
	static SCR_ChimeraCharacter Character()
	{
		return LF_Utils.GetLocalCharacter();
	}

	//------------------------------------------------------------------------------------------------
	static IEntity VehicleOf(SCR_ChimeraCharacter character)
	{
		if (!character)
			return null;

		CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
		if (!access)
			return null;

		return access.GetVehicleIn(character);
	}

	//------------------------------------------------------------------------------------------------
	static bool IsDriver(SCR_ChimeraCharacter character)
	{
		if (!character)
			return false;

		CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
		return access && PilotCompartmentSlot.Cast(access.GetCompartment()) != null;
	}

	//------------------------------------------------------------------------------------------------
	static float SpeedKmh(IEntity car)
	{
		VehicleWheeledSimulation sim = VehicleWheeledSimulation.Cast(car.FindComponent(VehicleWheeledSimulation));
		if (sim)
			return sim.GetSpeedKmh();

		Physics physics = car.GetPhysics();
		if (physics)
			return physics.GetVelocity().Length() * 3.6;

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! A "x,z" token, or false.
	static bool ParsePoint(string token, out vector point)
	{
		array<string> xz = {};
		token.Split(",", xz, true);
		if (xz.Count() != 2)
			return false;

		point = Vector(xz[0].ToFloat(), 0, xz[1].ToFloat());
		return point[0] != 0 || point[2] != 0;
	}

	//------------------------------------------------------------------------------------------------
	//! The value of key=<value> (or --key=<value>) among the tokens, or "".
	static string Option(array<string> tokens, string key)
	{
		foreach (string token : tokens)
		{
			string t = token;
			if (t.StartsWith("--"))
				t = t.Substring(2, t.Length() - 2);

			if (t.StartsWith(key + "="))
				return t.Substring(key.Length() + 1, t.Length() - key.Length() - 1);
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	static bool HasWord(array<string> tokens, string word)
	{
		foreach (string token : tokens)
		{
			if (token == word || token == "--" + word)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	static string Round1(float value)
	{
		return (Math.Round(value * 10) / 10).ToString();
	}

	//------------------------------------------------------------------------------------------------
	// vehicle enter / exit / stop / state / engine / lights
	//------------------------------------------------------------------------------------------------
	static void Enter(int id, array<string> tokens)
	{
		SCR_ChimeraCharacter character = Character();
		if (!character)
		{
			LF_AgentHarness.Answer(id, "error", "no_character");
			return;
		}

		if (VehicleOf(character))
		{
			LF_AgentHarness.Answer(id, "error", "already_in_vehicle");
			return;
		}

		ECompartmentType seat = ECompartmentType.PILOT;
		if (HasWord(tokens, "passenger"))
			seat = ECompartmentType.CARGO;

		vector around = character.GetOrigin();
		vector point;
		if (ParsePoint(Option(tokens, "at"), point))
		{
			around = point;
			around[1] = GetGame().GetWorld().GetSurfaceY(point[0], point[2]);
		}

		float radius = ENTER_RADIUS_M;
		string r = Option(tokens, "r");
		if (!r.IsEmpty())
			radius = r.ToFloat();

		string prefabText;
		foreach (string token : tokens)
		{
			if (token.StartsWith("veh_"))
				prefabText = token.Substring(4, token.Length() - 4);
		}

		IEntity best;
		float bestDistance = radius * radius + 1;
		foreach (IEntity candidate : LF_Utils.GetNearbyVehicles(around, radius))
		{
			// lf-path-ok: harness substring filter typed by the tester
			if (!prefabText.IsEmpty() && !LF_Utils.GetPrefabName(candidate).Contains(prefabText))
				continue;

			float d = vector.DistanceSqXZ(candidate.GetOrigin(), around);
			if (d < bestDistance)
			{
				bestDistance = d;
				best = candidate;
			}
		}

		if (!best)
		{
			LF_AgentHarness.Answer(id, "error", string.Format("no_vehicle_within_%1m", radius));
			return;
		}

		SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(character.GetCompartmentAccessComponent());
		if (!access)
		{
			LF_AgentHarness.Answer(id, "error", "no_access");
			return;
		}

		if (!access.MoveInVehicle(best, seat))
		{
			LF_AgentHarness.Answer(id, "error", "seat_refused");
			return;
		}

		LF_Debug.Info(FEATURE, string.Format("vehicle enter %1 %2 at %3", typename.EnumToString(ECompartmentType, seat), LF_Utils.GetPrefabName(best), best.GetOrigin()));
		LF_AgentHarness.Answer(id, "pending", "seating");
		s_iEnterId = id;
		s_iEnterTries = 0;
		GetGame().GetCallqueue().Remove(CheckSeated);
		GetGame().GetCallqueue().CallLater(CheckSeated, 250, true);
	}

	protected static int s_iEnterId;
	protected static int s_iEnterTries;

	//------------------------------------------------------------------------------------------------
	protected static void CheckSeated()
	{
		s_iEnterTries++;
		SCR_ChimeraCharacter character = Character();
		IEntity car = VehicleOf(character);
		if (car)
		{
			GetGame().GetCallqueue().Remove(CheckSeated);
			LF_AgentHarness.Answer(s_iEnterId, "ok", "in " + StateLine());
			return;
		}

		if (s_iEnterTries > 40)
		{
			GetGame().GetCallqueue().Remove(CheckSeated);
			LF_AgentHarness.Answer(s_iEnterId, "error", "seat_timeout");
		}
	}

	//------------------------------------------------------------------------------------------------
	static void Exit(int id, array<string> tokens)
	{
		SCR_ChimeraCharacter character = Character();
		IEntity car = VehicleOf(character);
		if (!car)
		{
			LF_AgentHarness.Answer(id, "error", "not_in_vehicle");
			return;
		}

		float speed = SpeedKmh(car);
		if (speed > 3 && !HasWord(tokens, "force"))
		{
			LF_AgentHarness.Answer(id, "error", string.Format("moving_%1kmh", Round1(speed)));
			return;
		}

		Finish("exited");
		vector at = car.GetOrigin();
		CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
		if (!access.GetOutVehicle(EGetOutType.TELEPORT, 0, ECloseDoorAfterActions.CLOSE_DOOR, false))
		{
			LF_AgentHarness.Answer(id, "error", "leave_refused");
			return;
		}

		LF_Debug.Info(FEATURE, string.Format("vehicle exit at %1", at));
		LF_AgentHarness.Answer(id, "ok", string.Format("left car=%1,%2,%3", Round1(at[0]), Round1(at[1]), Round1(at[2])));
	}

	//------------------------------------------------------------------------------------------------
	static void Stop(int id)
	{
		IEntity car = VehicleOf(Character());
		if (!car)
		{
			LF_AgentHarness.Answer(id, "error", "not_in_vehicle");
			return;
		}

		if (!s_bActive)
			BeginTicks(car, "stop");

		s_bStopping = true;
		s_sResult = "stopped";
		s_iWaitId = id;
		s_fEndMs = GetGame().GetWorld().GetWorldTime() + 15000;
		LF_AgentHarness.Answer(id, "pending", "stopping");
	}

	//------------------------------------------------------------------------------------------------
	static void Engine(int id, array<string> tokens)
	{
		IEntity car = VehicleOf(Character());
		VehicleControllerComponent controller;
		if (car)
			controller = LF_Component<VehicleControllerComponent>.Find(car);

		if (!controller)
		{
			LF_AgentHarness.Answer(id, "error", "not_in_vehicle");
			return;
		}

		if (HasWord(tokens, "off"))
			controller.StopEngine(false);
		else if (!controller.IsEngineOn())
			controller.StartEngine();

		LF_AgentHarness.Answer(id, "ok", string.Format("engine requested on=%1", controller.IsEngineOn()));
	}

	//------------------------------------------------------------------------------------------------
	static void Lights(int id, array<string> tokens)
	{
		IEntity car = VehicleOf(Character());
		BaseLightManagerComponent lights;
		if (car)
			lights = BaseLightManagerComponent.Cast(car.FindComponent(BaseLightManagerComponent));

		if (!lights)
		{
			LF_AgentHarness.Answer(id, "error", "no_lights");
			return;
		}

		bool on = !HasWord(tokens, "off");
		lights.SetLightsState(ELightType.Head, on);
		LF_AgentHarness.Answer(id, "ok", string.Format("lights head=%1", lights.GetLightsState(ELightType.Head)));
	}

	//------------------------------------------------------------------------------------------------
	//! One space-separated line of key=value pairs about the vehicle this client sits in.
	static string StateLine()
	{
		SCR_ChimeraCharacter character = Character();
		IEntity car = VehicleOf(character);
		if (!car)
			return string.Format("seat=none drive=%1", s_sStatus);

		string seat = "cargo";
		if (IsDriver(character))
			seat = "pilot";

		vector at = car.GetOrigin();
		vector angles = car.GetYawPitchRoll();
		bool engine;
		VehicleControllerComponent controller = LF_Component<VehicleControllerComponent>.Find(car);
		if (controller)
			engine = controller.IsEngineOn();

		int gear;
		bool handbrake;
		VehicleWheeledSimulation sim = VehicleWheeledSimulation.Cast(car.FindComponent(VehicleWheeledSimulation));
		if (sim)
			gear = sim.GetGear();

		CarControllerComponent carController = CarControllerComponent.Cast(controller);
		if (carController)
			handbrake = carController.GetPersistentHandBrake() || carController.GetHandBrake();

		bool owner;
		RplComponent rpl = RplComponent.Cast(car.FindComponent(RplComponent));
		if (rpl)
			owner = rpl.IsOwner();

		bool lightsOn;
		BaseLightManagerComponent lights = BaseLightManagerComponent.Cast(car.FindComponent(BaseLightManagerComponent));
		if (lights)
			lightsOn = lights.GetLightsState(ELightType.Head);

		string prefab = LF_Utils.GetPrefabName(car);
		int slash = prefab.LastIndexOf("/");
		if (slash >= 0)
			prefab = prefab.Substring(slash + 1, prefab.Length() - slash - 1);

		string line = string.Format("seat=%1 veh=%2 speed=%3kmh pos=%4,%5,%6 heading=%7 engine=%8 gear=%9", seat, prefab, Round1(SpeedKmh(car)), Round1(at[0]), Round1(at[1]), Round1(at[2]), Math.Round(angles[0]), engine, gear);
		line += string.Format(" handbrake=%1 health=%2 owner=%3 lights=%4", handbrake, Math.Round(LF_RepairKit.HealthOf(car) * 100) / 100, owner, lightsOn);
		line += string.Format(" drive=%1 driven=%2m mode=%3 ticks=%4 prep=%5", s_sStatus, Math.Round(s_fDriven), s_sMode, s_iTicks, s_iPrepares);
		return line;
	}

	//------------------------------------------------------------------------------------------------
	// drive
	//------------------------------------------------------------------------------------------------
	static void Drive(int id, array<string> tokens)
	{
		string sub;
		if (!tokens.IsEmpty())
			sub = tokens[0];

		if (sub == "wait")
		{
			if (!s_bActive)
			{
				LF_AgentHarness.Answer(id, "ok", Summary());
				return;
			}

			s_iWaitId = id;
			LF_AgentHarness.Answer(id, "pending", "driving");
			return;
		}

		SCR_ChimeraCharacter character = Character();
		IEntity car = VehicleOf(character);
		if (!car)
		{
			LF_AgentHarness.Answer(id, "error", "not_in_vehicle");
			return;
		}

		if (!IsDriver(character))
		{
			LF_AgentHarness.Answer(id, "error", "not_the_driver");
			return;
		}

		string where;
		if (tokens.Count() > 1)
			where = tokens[1];

		array<vector> route = {};
		vector here = car.GetOrigin();
		if (sub == "to")
		{
			vector point;
			if (!ParsePoint(where, point))
			{
				LF_AgentHarness.Answer(id, "error", "need_x,z");
				return;
			}

			route.Insert(point);
		}
		else if (sub == "path")
		{
			array<string> points = {};
			where.Split(";", points, true);
			foreach (string p : points)
			{
				vector pathPoint;
				if (!ParsePoint(p, pathPoint))
				{
					LF_AgentHarness.Answer(id, "error", "bad_point_" + p);
					return;
				}

				route.Insert(pathPoint);
			}
		}
		else if (sub == "road")
		{
			vector roadGoal;
			if (!ParsePoint(where, roadGoal))
			{
				LF_AgentHarness.Answer(id, "error", "need_x,z");
				return;
			}

			// The AI road network is loaded on the server only: the server builds the route (dev gate)
			// and sends its points back, then OnRoute starts the drive.
			s_iRouteId = id;
			s_aRouteTokens.Clear();
			s_aRouteTokens.InsertAll(tokens);
			character.LF_AskDriveRoute(id, here, roadGoal);
			LF_AgentHarness.Answer(id, "pending", "routing");
			return;
		}
		else
		{
			LF_AgentHarness.Answer(id, "error", "drive_to|path|road|wait");
			return;
		}

		StartDrive(id, tokens, car, route);
	}

	protected static int s_iRouteId;
	protected static ref array<string> s_aRouteTokens = {};

	//------------------------------------------------------------------------------------------------
	//! The server answer to a road route request: "x,z;x,z;..." or an error reason.
	static void OnRoute(int id, bool ok, string detail)
	{
		if (id != s_iRouteId)
			return;

		s_iRouteId = 0;
		if (!ok)
		{
			LF_AgentHarness.Answer(id, "error", detail);
			return;
		}

		SCR_ChimeraCharacter character = Character();
		IEntity car = VehicleOf(character);
		if (!car || !IsDriver(character))
		{
			LF_AgentHarness.Answer(id, "error", "not_the_driver");
			return;
		}

		array<vector> route = {};
		array<string> points = {};
		detail.Split(";", points, true);
		foreach (string token : points)
		{
			vector routePoint;
			if (ParsePoint(token, routePoint))
				route.Insert(routePoint);
		}

		StartDrive(id, s_aRouteTokens, car, route);
	}

	//------------------------------------------------------------------------------------------------
	protected static void StartDrive(int id, array<string> tokens, IEntity car, array<vector> route)
	{
		string sub = tokens[0];
		vector here = car.GetOrigin();
		if (route.IsEmpty())
		{
			LF_AgentHarness.Answer(id, "error", "empty_route");
			return;
		}

		s_fSpeedCap = DEFAULT_SPEED_KMH;
		string speed = Option(tokens, "speed");
		if (!speed.IsEmpty())
			s_fSpeedCap = Math.Clamp(speed.ToFloat(), 3, MAX_SPEED_KMH);

		float timeout = DEFAULT_TIMEOUT_S;
		string t = Option(tokens, "timeout");
		if (!t.IsEmpty())
			timeout = Math.Clamp(t.ToFloat(), 1, MAX_TIMEOUT_S);

		s_fTolerance = DEFAULT_TOLERANCE_M;
		string tol = Option(tokens, "tol");
		if (!tol.IsEmpty())
			s_fTolerance = Math.Max(tol.ToFloat(), 0.5);

		s_fMaxDistance = 0;
		string dist = Option(tokens, "dist");
		if (!dist.IsEmpty())
			s_fMaxDistance = dist.ToFloat();

		s_sMode = "input";
		if (Option(tokens, "mode") == "sim")
			s_sMode = "sim";

		s_fSteerSign = STEER_SIGN;
		if (Option(tokens, "steer") == "flip")
			s_fSteerSign = -STEER_SIGN;

		// The route starts where the car stands, so the first segment is the car's way onto it.
		s_aRoute.Clear();
		s_aRoute.Insert(Vector(here[0], 0, here[2]));
		foreach (vector routePoint : route)
			s_aRoute.Insert(Vector(routePoint[0], 0, routePoint[2]));

		BeginTicks(car, "driving");
		s_fEndMs = s_fStartMs + timeout * 1000;
		s_sResult = "";

		// What a driver does on getting in: the engine on, the handbrake off.
		VehicleControllerComponent controller = LF_Component<VehicleControllerComponent>.Find(car);
		if (controller && !controller.IsEngineOn())
			controller.StartEngine();

		CarControllerComponent carController = CarControllerComponent.Cast(controller);
		if (carController)
			carController.SetPersistentHandBrake(false);

		vector goal = s_aRoute[s_aRoute.Count() - 1];
		LF_Debug.Info(FEATURE, string.Format("drive %1 from %2 to %3 points=%4 cap=%5kmh mode=%6", sub, here, goal, s_aRoute.Count(), s_fSpeedCap, s_sMode));
		string started = string.Format("driving points=%1 straight=%2m route=%3m", s_aRoute.Count(), Math.Round(vector.DistanceXZ(here, goal)), Math.Round(RouteLength()));
		if (HasWord(tokens, "wait"))
		{
			s_iWaitId = id;
			LF_AgentHarness.Answer(id, "pending", started);
			return;
		}

		LF_AgentHarness.Answer(id, "ok", started);
	}

	//------------------------------------------------------------------------------------------------
	protected static float RouteLength()
	{
		float length = 0;
		for (int i = 1; i < s_aRoute.Count(); i++)
			length += vector.DistanceXZ(s_aRoute[i - 1], s_aRoute[i]);

		return length;
	}

	//------------------------------------------------------------------------------------------------
	protected static void BeginTicks(IEntity car, string status)
	{
		s_bActive = true;
		s_bStopping = false;
		s_sStatus = status;
		s_iSeg = 0;
		s_iBackups = 0;
		s_fDriven = 0;
		s_fMaxKmh = 0;
		s_iTicks = 0;
		s_iPrepares = 0;
		s_fStillSinceMs = 0;
		s_fBackupUntilMs = 0;
		s_fDoneMs = 0;
		s_iWaitId = 0;
		s_vStart = car.GetOrigin();
		s_vLast = s_vStart;
		s_fStartMs = GetGame().GetWorld().GetWorldTime();
		GetGame().GetCallqueue().Remove(Tick);
		GetGame().GetCallqueue().CallLater(Tick, 0, true);
	}

	//------------------------------------------------------------------------------------------------
	protected static string Summary()
	{
		IEntity car = VehicleOf(Character());
		float err = -1;
		vector at = s_vLast;
		if (car)
			at = car.GetOrigin();

		if (!s_aRoute.IsEmpty())
			err = vector.DistanceXZ(at, s_aRoute[s_aRoute.Count() - 1]);

		float seconds = (s_fDoneMs - s_fStartMs) / 1000;
		if (s_fDoneMs <= 0)
			seconds = (GetGame().GetWorld().GetWorldTime() - s_fStartMs) / 1000;

		return string.Format("%1 driven=%2m time=%3s err=%4m max=%5kmh at=%6,%7 backups=%8 mode=%9", s_sStatus, Round1(s_fDriven), Round1(seconds), Round1(err), Round1(s_fMaxKmh), Round1(at[0]), Round1(at[2]), s_iBackups, s_sMode);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Finish(string status)
	{
		if (!s_bActive)
			return;

		s_bActive = false;
		s_sStatus = status;
		s_fDoneMs = GetGame().GetWorld().GetWorldTime();
		GetGame().GetCallqueue().Remove(Tick);
		s_fThrust = 0;
		s_fBrake = 0;
		s_fSteer = 0;
		s_fHand = 0;
		ApplyInputs(GetGame().GetInputManager());
		IEntity car = VehicleOf(Character());
		if (car)
		{
			CarControllerComponent carController = CarControllerComponent.Cast(car.FindComponent(CarControllerComponent));
			if (carController && status != "exited")
				carController.SetPersistentHandBrake(true);

			VehicleWheeledSimulation sim = VehicleWheeledSimulation.Cast(car.FindComponent(VehicleWheeledSimulation));
			if (sim && s_sMode == "sim")
			{
				sim.SetThrottle(0);
				sim.SetSteering(0);
			}
		}

		string summary = Summary();
		LF_Debug.Info(FEATURE, "drive end " + summary);
		if (s_iWaitId > 0)
		{
			string answer = "ok";
			if (status == "timeout" || status == "stuck" || status == "lost_vehicle")
				answer = "error";

			LF_AgentHarness.Answer(s_iWaitId, answer, summary);
			s_iWaitId = 0;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! One frame of the drive: works out throttle, brake and steering from where the car is.
	protected static void Tick()
	{
		s_iTicks++;
		SCR_ChimeraCharacter character = Character();
		IEntity car = VehicleOf(character);
		if (!car || !IsDriver(character))
		{
			Finish("lost_vehicle");
			return;
		}

		float now = GetGame().GetWorld().GetWorldTime(); // lf-float: world time in ms, harness only
		vector at = car.GetOrigin();
		s_fDriven += vector.DistanceXZ(at, s_vLast);
		s_vLast = at;
		float speed = SpeedKmh(car);
		s_fMaxKmh = Math.Max(s_fMaxKmh, speed);

		if (now > s_fEndMs)
		{
			Finish("timeout");
			return;
		}

		if (s_bStopping)
		{
			StopInputs(car, speed);
			if (speed < STILL_KMH)
			{
				string done = "arrived";
				if (!s_sResult.IsEmpty())
					done = s_sResult;

				Finish(done);
				return;
			}

			Push(car);
			return;
		}

		vector goal = s_aRoute[s_aRoute.Count() - 1];
		float toGoal = vector.DistanceXZ(at, goal);
		if (s_fMaxDistance > 0 && s_fDriven >= s_fMaxDistance)
		{
			s_bStopping = true;
			s_sResult = "distance";
			s_sStatus = "braking";
			StopInputs(car, speed);
			Push(car);
			return;
		}

		if (toGoal <= s_fTolerance)
		{
			s_bStopping = true;
			s_sStatus = "braking";
			StopInputs(car, speed);
			Push(car);
			return;
		}

		vector transform[4];
		car.GetWorldTransform(transform);
		vector forward = Vector(transform[2][0], 0, transform[2][2]).Normalized();
		vector right = Vector(transform[0][0], 0, transform[0][2]).Normalized();

		// Backing up out of a stuck spot: brake held at a standstill reverses an automatic box.
		if (s_fBackupUntilMs > now)
		{
			vector backTarget = PursuitPoint(at, Math.Max(6, speed / 3.6 * 1.5));
			float backAlpha = Math.Atan2(vector.Dot(backTarget - at, right), vector.Dot(backTarget - at, forward));
			s_fThrust = 0;
			s_fBrake = 1;
			s_fHand = 0;
			s_fSteer = -s_fSteerSign * Math.Clamp(backAlpha * Math.RAD2DEG / FULL_LOCK_DEG, -1, 1);
			Push(car);
			return;
		}

		float lookahead = Math.Max(6, speed / 3.6 * 1.5);
		vector target = PursuitPoint(at, lookahead);
		vector toTarget = target - at;
		toTarget[1] = 0;
		float alpha = Math.Atan2(vector.Dot(toTarget, right), vector.Dot(toTarget, forward));
		float alphaDeg = alpha * Math.RAD2DEG;

		// Target speed: the cap, less in a bend, and what stops the car in time before the last point.
		float want = s_fSpeedCap;
		if (Math.AbsFloat(alphaDeg) > 30)
			want = Math.Min(want, BEND_KMH);

		float stopKmh = Math.Sqrt(2 * DECEL * Math.Max(toGoal - s_fTolerance * 0.5, 0)) * 3.6;
		want = Math.Min(want, Math.Max(stopKmh, 4));

		s_fSteer = s_fSteerSign * Math.Clamp(alphaDeg / FULL_LOCK_DEG, -1, 1);
		s_fHand = 0;
		if (speed < want)
		{
			s_fThrust = Math.Clamp((want - speed) / 10 + 0.2, 0, 1);
			s_fBrake = 0;
		}
		else
		{
			s_fThrust = 0;
			s_fBrake = Math.Clamp((speed - want) / 8, 0, 1);
			// A brake held near a standstill reverses an automatic box: let it roll instead.
			if (speed < 4)
				s_fBrake = 0;
		}

		// Stuck: throttle given and the car stands for STUCK_S.
		if (s_fThrust > 0.2 && speed < STILL_KMH)
		{
			if (s_fStillSinceMs <= 0)
				s_fStillSinceMs = now;
			else if (now - s_fStillSinceMs > STUCK_S * 1000)
			{
				if (s_iBackups >= MAX_BACKUPS)
				{
					Finish("stuck");
					return;
				}

				s_iBackups++;
				s_fBackupUntilMs = now + BACKUP_S * 1000;
				s_fStillSinceMs = 0;
				LF_Debug.Info(FEATURE, string.Format("drive stuck at %1, backing up (%2)", at, s_iBackups));
			}
		}
		else
		{
			s_fStillSinceMs = 0;
		}

		Push(car);
	}

	//------------------------------------------------------------------------------------------------
	protected static void StopInputs(IEntity car, float speed)
	{
		s_fThrust = 0;
		s_fSteer = 0;
		s_fBrake = 1;
		s_fHand = 1;
		// At a crawl the brake would start reversing: the handbrake holds it.
		if (speed < 3)
			s_fBrake = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! The point `lookahead` metres along the route past the car's place on it.
	protected static vector PursuitPoint(vector at, float lookahead)
	{
		int last = s_aRoute.Count() - 1;
		if (last <= 0)
			return s_aRoute[0];

		// Move on to a later segment while the car is nearer to it than to this one.
		while (s_iSeg < last - 1)
		{
			float tHere;
			float dHere = SegmentDistance(at, s_aRoute[s_iSeg], s_aRoute[s_iSeg + 1], tHere);
			float tNext;
			float dNext = SegmentDistance(at, s_aRoute[s_iSeg + 1], s_aRoute[s_iSeg + 2], tNext);
			if (tHere >= 1 || dNext < dHere)
				s_iSeg++;
			else
				break;
		}

		float t;
		SegmentDistance(at, s_aRoute[s_iSeg], s_aRoute[s_iSeg + 1], t);
		vector a = s_aRoute[s_iSeg];
		vector b = s_aRoute[s_iSeg + 1];
		vector p = a + (b - a) * t;
		float left = lookahead;
		int seg = s_iSeg;
		while (true)
		{
			vector end = s_aRoute[seg + 1];
			float d = vector.DistanceXZ(p, end);
			if (d >= left)
				return p + (end - p).Normalized() * left;

			left -= d;
			p = end;
			seg++;
			if (seg >= last)
				return s_aRoute[last];
		}

		return s_aRoute[last];
	}

	//------------------------------------------------------------------------------------------------
	//! Flat distance from `at` to the segment a-b; `t` is where along it (0..1) the nearest point is.
	static float SegmentDistance(vector at, vector a, vector b, out float t)
	{
		vector ab = Vector(b[0] - a[0], 0, b[2] - a[2]);
		vector ap = Vector(at[0] - a[0], 0, at[2] - a[2]);
		float len2 = vector.Dot(ab, ab);
		t = 0;
		if (len2 > 0.0001)
			t = Math.Clamp(vector.Dot(ap, ab) / len2, 0, 1);

		vector nearest = a + ab * t;
		return vector.DistanceXZ(at, nearest);
	}

	//------------------------------------------------------------------------------------------------
	//! Gives this frame's inputs to the car.
	protected static void Push(IEntity car)
	{
		if (s_sMode == "sim")
		{
			VehicleWheeledSimulation sim = VehicleWheeledSimulation.Cast(car.FindComponent(VehicleWheeledSimulation));
			if (sim)
			{
				sim.SetThrottle(s_fThrust);
				sim.SetSteering(s_fSteer);
				sim.SetBreak(s_fBrake, s_fHand > 0.5);
			}

			return;
		}

		ApplyInputs(GetGame().GetInputManager());
	}

	//------------------------------------------------------------------------------------------------
	static void ApplyInputs(ActionManager am)
	{
		if (!am)
			return;

		am.SetActionValue("CarThrust", s_fThrust);
		am.SetActionValue("CarBrake", s_fBrake);
		am.SetActionValue("CarSteering", s_fSteer);
		am.SetActionValue("CarHandBrake", s_fHand);
	}

	//------------------------------------------------------------------------------------------------
	//! True while a drive gives inputs (the character controller re-asserts them each frame).
	static bool GivesInput()
	{
		return s_bActive && s_sMode == "input";
	}

	//------------------------------------------------------------------------------------------------
	static void CountPrepare()
	{
		s_iPrepares++;
	}
}

//------------------------------------------------------------------------------------------------
//! A route along the AI road network: Dijkstra over the points of the roads near the start and
//! the goal, joined where roads meet, kept LANE_M right of the middle line.
class LF_DriveRoute
{
	//------------------------------------------------------------------------------------------------
	//! Fills `route` (start excluded, goal last) and returns "" or the reason there is none.
	static string Build(vector start, vector goal, notnull array<vector> route)
	{
		ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld || !aiWorld.GetRoadNetworkManager())
			return "no_road_network";

		float margin = LF_DriveHarness.ROAD_MARGIN_M;
		vector mins = Vector(Math.Min(start[0], goal[0]) - margin, -1000, Math.Min(start[2], goal[2]) - margin);
		vector maxs = Vector(Math.Max(start[0], goal[0]) + margin, 5000, Math.Max(start[2], goal[2]) + margin);
		array<BaseRoad> roads = {};
		aiWorld.GetRoadNetworkManager().GetRoadsInAABB(mins, maxs, roads);
		if (roads.IsEmpty())
			return "no_roads_near";

		// Nodes: every road point; edges: along each road, and between points of different roads
		// that lie within ROAD_JOIN_M (a junction).
		array<vector> nodes = {};
		array<int> roadOf = {};
		foreach (int r, BaseRoad road : roads)
		{
			array<vector> points = {};
			road.GetPoints(points);
			foreach (vector p : points)
			{
				if (nodes.Count() >= LF_DriveHarness.MAX_ROAD_NODES)
					return "road_graph_too_big";

				nodes.Insert(Vector(p[0], 0, p[2]));
				roadOf.Insert(r);
			}
		}

		int n = nodes.Count();
		if (n < 2)
			return "no_road_points";

		array<ref array<int>> links = {};
		for (int i = 0; i < n; i++)
			links.Insert(new array<int>());

		for (int i1 = 1; i1 < n; i1++)
		{
			if (roadOf[i1] == roadOf[i1 - 1])
			{
				links[i1].Insert(i1 - 1);
				links[i1 - 1].Insert(i1);
			}
		}

		// Junctions through a grid of ROAD_JOIN_M cells.
		float cell = LF_DriveHarness.ROAD_JOIN_M;
		map<int, ref array<int>> grid = new map<int, ref array<int>>();
		for (int i2 = 0; i2 < n; i2++)
		{
			int key = CellKey(nodes[i2], cell, 0, 0);
			array<int> bucket;
			if (!grid.Find(key, bucket))
			{
				bucket = {};
				grid.Insert(key, bucket);
			}

			bucket.Insert(i2);
		}

		float join2 = cell * cell;
		for (int i3 = 0; i3 < n; i3++)
		{
			for (int dx = -1; dx <= 1; dx++)
			{
				for (int dz = -1; dz <= 1; dz++)
				{
					array<int> near;
					if (!grid.Find(CellKey(nodes[i3], cell, dx, dz), near))
						continue;

					foreach (int j : near)
					{
						if (j <= i3 || roadOf[j] == roadOf[i3])
							continue;

						if (vector.DistanceSqXZ(nodes[i3], nodes[j]) <= join2)
						{
							links[i3].Insert(j);
							links[j].Insert(i3);
						}
					}
				}
			}
		}

		int from = Nearest(nodes, start);
		int to = Nearest(nodes, goal);
		array<float> dist = {};
		array<int> prev = {};
		array<bool> done = {};
		for (int i4 = 0; i4 < n; i4++)
		{
			dist.Insert(1000000000.0);
			prev.Insert(-1);
			done.Insert(false);
		}

		dist[from] = 0;
		for (int step = 0; step < n; step++)
		{
			int u = -1;
			float best = 1000000000.0;
			for (int i5 = 0; i5 < n; i5++)
			{
				if (!done[i5] && dist[i5] < best)
				{
					best = dist[i5];
					u = i5;
				}
			}

			if (u < 0 || u == to)
				break;

			done[u] = true;
			foreach (int v : links[u])
			{
				float alt = dist[u] + vector.DistanceXZ(nodes[u], nodes[v]);
				if (alt < dist[v])
				{
					dist[v] = alt;
					prev[v] = u;
				}
			}
		}

		if (to != from && prev[to] < 0)
			return "no_road_path";

		array<int> chain = {};
		for (int at = to; at >= 0; at = prev[at])
			chain.InsertAt(at, 0);

		// The middle line, dropping points under 3 m apart.
		array<vector> line = {};
		foreach (int idx : chain)
		{
			if (line.IsEmpty() || vector.DistanceXZ(line[line.Count() - 1], nodes[idx]) >= 3)
				line.Insert(nodes[idx]);
		}

		// Keep right of the middle line, except the last point (the goal is where it is).
		for (int i6 = 0; i6 < line.Count(); i6++)
		{
			vector dir;
			if (i6 + 1 < line.Count())
				dir = line[i6 + 1] - line[i6];
			else if (i6 > 0)
				dir = line[i6] - line[i6 - 1];

			dir[1] = 0;
			if (dir.Length() < 0.01)
			{
				route.Insert(line[i6]);
				continue;
			}

			dir.Normalize();
			vector rightOf = Vector(dir[2], 0, -dir[0]);
			route.Insert(line[i6] + rightOf * LF_DriveHarness.LANE_M);
		}

		route.Insert(Vector(goal[0], 0, goal[2]));
		LF_Debug.Info(LF_DriveHarness.FEATURE, string.Format("drive road route roads=%1 nodes=%2 points=%3 length=%4m", roads.Count(), n, route.Count(), Math.Round(dist[to])));
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! The route as "x,z;x,z;..." (one decimal), the form the server sends back to the driver.
	static string Encode(array<vector> route)
	{
		string text;
		foreach (int i, vector p : route)
		{
			if (i > 0)
				text += ";";

			text += string.Format("%1,%2", Math.Round(p[0] * 10) / 10, Math.Round(p[2] * 10) / 10);
		}

		return text;
	}

	//------------------------------------------------------------------------------------------------
	protected static int CellKey(vector p, float cell, int dx, int dz)
	{
		int cx = Math.Floor(p[0] / cell) + dx;
		int cz = Math.Floor(p[2] / cell) + dz;
		return cx * 100000 + cz;
	}

	//------------------------------------------------------------------------------------------------
	protected static int Nearest(array<vector> nodes, vector at)
	{
		int best = 0;
		float bestD = 1000000000.0;
		foreach (int i, vector p : nodes)
		{
			float d = vector.DistanceSqXZ(p, at);
			if (d < bestD)
			{
				bestD = d;
				best = i;
			}
		}

		return best;
	}
}

//------------------------------------------------------------------------------------------------
//! The road route request: the client asks, the server builds the route behind the dev gate and
//! sends the points back to the owner.
modded class SCR_ChimeraCharacter
{
	//------------------------------------------------------------------------------------------------
	void LF_AskDriveRoute(int commandId, vector start, vector goal)
	{
		if (Replication.IsServer())
		{
			LF_AnswerDriveRoute(commandId, start, goal);
			return;
		}

		Rpc(RpcAsk_LF_DriveRoute, commandId, start, goal);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_DriveRoute(int commandId, vector start, vector goal)
	{
		if (!LF_DevGate.CheckHarnessRequest(this, "driveroute"))
		{
			Rpc(RpcDo_LF_DriveRoute, commandId, false, LF_AgentHarness.REFUSED_DEV_GATE);
			return;
		}

		LF_AnswerDriveRoute(commandId, start, goal);
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_AnswerDriveRoute(int commandId, vector start, vector goal)
	{
		array<vector> route = {};
		string problem = LF_DriveRoute.Build(start, goal, route);
		bool ok = problem.IsEmpty();
		string detail = problem;
		if (ok)
			detail = LF_DriveRoute.Encode(route);

		if (LF_Utils.GetLocalCharacter() == this)
		{
			LF_DriveHarness.OnRoute(commandId, ok, detail);
			return;
		}

		Rpc(RpcDo_LF_DriveRoute, commandId, ok, detail);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_DriveRoute(int commandId, bool ok, string detail)
	{
		LF_DriveHarness.OnRoute(commandId, ok, detail);
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_CharacterControllerComponent
{
	override void OnPrepareControls(IEntity owner, ActionManager am, float dt, bool player)
	{
		super.OnPrepareControls(owner, am, dt, player);

		// The drive's inputs, given after the frame's own (absent) keys, like a held pedal.
		if (player && LF_DriveHarness.GivesInput())
		{
			LF_DriveHarness.CountPrepare();
			LF_DriveHarness.ApplyInputs(am);
		}
	}
}

//------------------------------------------------------------------------------------------------
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "vehicle" && cmd.Verb != "drive")
			return super.Handle(cmd);

		array<string> tokens = {};
		array<string> args = {cmd.Arg1, cmd.Arg2, cmd.Arg3, cmd.Arg4, cmd.Arg5};
		foreach (string arg : args)
		{
			if (!arg.IsEmpty())
				tokens.Insert(arg);
		}

		if (cmd.Verb == "drive")
		{
			LF_DriveHarness.Drive(cmd.Id, tokens);
			return true;
		}

		string sub;
		if (!tokens.IsEmpty())
			sub = tokens[0];

		if (sub == "enter")
			LF_DriveHarness.Enter(cmd.Id, tokens);
		else if (sub == "exit")
			LF_DriveHarness.Exit(cmd.Id, tokens);
		else if (sub == "stop")
			LF_DriveHarness.Stop(cmd.Id);
		else if (sub == "state")
			LF_AgentHarness.Answer(cmd.Id, "ok", LF_DriveHarness.StateLine());
		else if (sub == "engine")
			LF_DriveHarness.Engine(cmd.Id, tokens);
		else if (sub == "lights")
			LF_DriveHarness.Lights(cmd.Id, tokens);
		else
			LF_AgentHarness.Answer(cmd.Id, "error", "vehicle_enter|exit|stop|state|engine|lights");

		return true;
	}
}
