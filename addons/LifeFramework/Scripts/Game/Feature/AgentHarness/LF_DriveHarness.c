//------------------------------------------------------------------------------------------------
//! Harness verbs that drive a car the way a player does (agent-box/proofs/drive-proof.mjs):
//!   vehicle enter driver|passenger [at=x,z] [veh_<text>] [r=<m>]
//!                     seats this client's character in the nearest vehicle (to the character, or to
//!                     at=x,z), within r metres (default 8); veh_<text> narrows it to a prefab path
//!   vehicle exit [force]   gets out (refused above 3 km/h unless `force`)
//!   vehicle stop      ends a drive, brakes to a standstill and sets the handbrake
//!   vehicle state     seat, speed, position, heading, engine, gear, handbrake, health, owner, drive
//!   vehicle engine on|off, vehicle lights on|off
//!   drive to <x,z> [speed=<kmh>] [timeout=<s>] [legtimeout=<s>] [tol=<m>] [dist=<m>] [mode=input|sim] [steer=flip] [wait]
//!                     dist=<m> stops after driving that far (status `distance`)
//!   drive path <x,z;x,z;...> [same options]
//!   drive road <x,z> [same options]   follows the AI road network from here to the point
//!   drive wait        answers when the running drive ends (arrived, timeout, stuck, stopped)
//!   drive plan <x,z> [from=x,z] [near=x,z]   the road route only (length; route points near a spot)
//! A drive answers at once (`ok driving ...`) unless `wait` is given; `vehicle state` shows its
//! progress and `drive wait` its end. Options are key=value tokens so they fit the five words a
//! feature verb gets.
//! It runs on the client that sits at the wheel. That client owns the car while it drives, so the
//! inputs it gives are the ones the server sees, as with a player's keys. The route is resampled
//! every STEP_M; steering is a pure pursuit of a point that walks the route a speed-dependent
//! distance ahead, with the geometric steer angle for the car's wheelbase. The target speed is what
//! every bend ahead allows (from the route's curvature, braked for in time), and the stop at the
//! last point. Stuck (pedal down and standing, or no progress along the route), it backs up with
//! the wheels turned against the bend, then, on a road drive, asks the server for a new route from
//! where it stands. Each LEG_M of route has its own time budget, and a drive that ends early says
//! why (`why=`, with what stands in front of the bumper) and where (`stuckat=`).
//! mode=input (default) gives the car's own input actions every frame (CarThrust, CarBrake,
//! CarSteering, CarHandBrake), as the keys would; mode=sim sets the wheeled simulation directly,
//! the way vanilla's cinematic tracks drive a car. The harness poll itself is behind the dev gate.
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
	static const float MAX_TIMEOUT_S = 1800; // lf-fixed: harness guard
	static const float DEFAULT_TOLERANCE_M = 2.5; // lf-fixed: harness default
	static const float ENTER_RADIUS_M = 8; // lf-fixed: harness default
	//! Below this speed the car counts as standing still.
	static const float STILL_KMH = 1.5; // lf-fixed: harness geometry
	//! Comfortable deceleration for the speed ahead of a bend or the last point, m/s^2.
	static const float DECEL = 2.0; // lf-fixed: harness tuning
	//! The full lock of the wheels, degrees: the geometric steer angle is scaled to the input by it.
	static const float MAX_STEER_DEG = 32; // lf-fixed: harness tuning
	//! The speed cap while the pursuit point is far off the nose, km/h.
	static const float BEND_KMH = 12; // lf-fixed: harness tuning
	//! Sideways grip the corner speeds assume, m/s^2 (a car; a truck over HEAVY_KG gets less).
	static const float LATERAL_MPS2 = 2.6; // lf-fixed: harness tuning
	static const float LATERAL_HEAVY_MPS2 = 1.7; // lf-fixed: harness tuning
	static const float HEAVY_KG = 4500; // lf-fixed: harness tuning
	static const float CORNER_MIN_MPS = 2.5; // lf-fixed: harness tuning
	static const float CORNER_MAX_MPS = 30; // lf-fixed: harness tuning
	//! The bend radius is measured over this far either side of a route point.
	static const float CURVE_ARM_M = 6; // lf-fixed: harness geometry
	//! The route is resampled to a point every STEP_M.
	static const float STEP_M = 2; // lf-fixed: harness geometry
	//! Pursuit lookahead: LOOK_BASE_M + LOOK_GAIN_S * speed (m/s), within LOOK_MIN_M..LOOK_MAX_M.
	static const float LOOK_BASE_M = 3; // lf-fixed: harness tuning
	static const float LOOK_GAIN_S = 0.75; // lf-fixed: harness tuning
	static const float LOOK_MIN_M = 4.5; // lf-fixed: harness tuning
	static const float LOOK_MAX_M = 14; // lf-fixed: harness tuning
	//! How far ahead along the route the car's place on it is searched for each frame.
	static const float PROJECT_AHEAD = 25; // lf-fixed: harness geometry
	//! Stuck: pedal down and standing this long, or the route not PROGRESS_M further in NO_PROGRESS_S.
	static const float STUCK_S = 3; // lf-fixed: harness tuning
	static const float NO_PROGRESS_S = 7; // lf-fixed: harness tuning
	//! The engine starts and a gear goes in at the start of a drive: no stall counts this long.
	static const float START_GRACE_S = 6; // lf-fixed: harness tuning
	static const float PROGRESS_M = 1.5; // lf-fixed: harness tuning
	//! A back-up lasts BACKUP_S or BACKUP_M, whichever comes first, with BACKUP_LOCK of full lock.
	static const float BACKUP_S = 4; // lf-fixed: harness tuning
	static const float BACKUP_M = 5; // lf-fixed: harness tuning
	static const float BACKUP_LOCK = 0.8; // lf-fixed: harness tuning
	//! A truck over HEAVY_KG takes longer to start rolling backwards.
	static const float BACKUP_HEAVY_S = 7; // lf-fixed: harness tuning
	static const int MAX_BACKUPS = 6; // lf-fixed: harness guard
	static const int MAX_REPLANS = 6; // lf-fixed: harness guard
	static const float REPLAN_WAIT_S = 8; // lf-fixed: harness guard
	//! A road drive this far off its route asks for a new one.
	static const float OFF_ROUTE_M = 15; // lf-fixed: harness geometry
	//! Road route points this close behind the car at the start are dropped.
	static const float TRIM_BEHIND_M = 12; // lf-fixed: harness geometry
	//! What stands in front of the bumper is looked for this far out.
	static const float BLOCKER_LOOK_M = 3; // lf-fixed: harness geometry
	//! A leg is LEG_M of route; its budget is LEG_BASE_S plus the leg at LEG_MIN_MPS.
	static const float LEG_M = 400; // lf-fixed: harness tuning
	static const float LEG_BASE_S = 45; // lf-fixed: harness tuning
	static const float LEG_MIN_MPS = 2.5; // lf-fixed: harness tuning
	//! Road routes keep this far right of the road's middle line.
	static const float LANE_M = 1.5; // lf-fixed: harness geometry
	//! The road box round start and goal: at least ROAD_MARGIN_M, else this share of the distance.
	static const float ROAD_MARGIN_M = 250; // lf-fixed: harness geometry
	static const float ROAD_MARGIN_SHARE = 0.3; // lf-fixed: harness geometry
	static const float ROAD_JOIN_M = 6; // lf-fixed: harness geometry
	static const int MAX_ROAD_NODES = 150000; // lf-fixed: harness guard
	//! The road is joined at the nearest of this many road points, within this reach, with a clear line.
	static const int CLEAR_CANDIDATES = 12; // lf-fixed: harness guard
	static const int CLEAR_KEEP = 3; // lf-fixed: harness guard
	static const float CLEAR_REACH_M = 200; // lf-fixed: harness geometry
	static const float LINE_STEP_M = 4; // lf-fixed: harness geometry
	//! The way onto the road is checked this far either side of its middle (about half a truck).
	static const float SWEEP_HALF_M = 1.4; // lf-fixed: harness geometry
	//! The way out of a shut-in yard: waypoints in this many directions at 15, 30 and 45 m, each with
	//! up to VIA_TRIES road points within VIA_REACH_M tried.
	static const int VIA_DIRECTIONS = 16; // lf-fixed: harness guard
	static const int VIA_TRIES = 4; // lf-fixed: harness guard
	static const float VIA_REACH_M = 120; // lf-fixed: harness geometry
	//! `drive plan ... near=x,z` lists the route points this close to that point.
	static const float PLAN_NEAR_M = 45; // lf-fixed: harness geometry

	// Drive state, all on the driving client.
	protected static ref array<vector> s_aRoute = {};
	protected static ref array<float> s_aCum = {};
	protected static ref array<float> s_aCorner = {};
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
	protected static float s_fProgressAtMs; // lf-float: world time in ms, harness only
	protected static float s_fLegStartMs; // lf-float: world time in ms, harness only
	protected static float s_fReplanUntilMs; // lf-float: world time in ms, harness only
	protected static int s_iBackups;
	protected static int s_iReplans;
	protected static int s_iReplanId;
	protected static int s_iReplanSeq;
	protected static int s_iLeg;
	protected static int s_iLegCount;
	protected static float s_fLegTimeoutS;
	protected static float s_fLegFrom;
	protected static float s_fRouteLength;
	protected static float s_fProgress;
	protected static float s_fBestProgress;
	protected static float s_fWheelbase = 2.7;
	protected static float s_fFrontM = 2;
	protected static float s_fHalfWidthM = 0.8;
	protected static float s_fBackupS = BACKUP_S;
	protected static float s_fLateral = LATERAL_MPS2;
	protected static float s_fBackupSteer;
	protected static float s_fDriven;
	protected static vector s_vLast;
	protected static vector s_vStart;
	protected static vector s_vGoal;
	protected static vector s_vBackupFrom;
	protected static bool s_bStopping;
	protected static bool s_bActive;
	protected static bool s_bRoad;
	protected static bool s_bReplanning;
	protected static int s_iWaitId;
	protected static int s_iTicks;
	protected static int s_iPrepares;
	protected static float s_fMaxKmh;
	protected static string s_sResult;
	protected static string s_sWhy;
	protected static string s_sStuckAt;

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
		line += string.Format(" handbrake=%1 health=%2 owner=%3 lights=%4 fuel=%5", handbrake, Math.Round(LF_RepairKit.HealthOf(car) * 100) / 100, owner, lightsOn, Math.Round(FuelShare(car) * 100) / 100);
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

		if (sub == "plan")
		{
			Plan(id, tokens);
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
	//! The server answer to a road route request: "x,z;x,z;..." or an error reason. A negative id is
	//! a re-plan asked by a running drive after a stuck spot.
	static void OnRoute(int id, bool ok, string detail)
	{
		if (id != 0 && id == s_iPlanId)
		{
			s_iPlanId = 0;
			OnPlan(id, ok, detail);
			return;
		}

		if (id != 0 && id == s_iReplanId)
		{
			s_iReplanId = 0;
			OnReplan(ok, detail);
			return;
		}

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
		DecodeRoute(detail, route);
		StartDrive(id, s_aRouteTokens, car, route);
	}

	protected static int s_iPlanId;
	protected static vector s_vPlanFrom;
	protected static string s_sPlanNear;

	//------------------------------------------------------------------------------------------------
	//! drive plan <x,z> [from=x,z] [near=x,z]: the road route only, no drive. Answers its length and,
	//! with near=, the route points within PLAN_NEAR_M of that point (where the road runs there).
	static void Plan(int id, array<string> tokens)
	{
		SCR_ChimeraCharacter character = Character();
		vector goal;
		if (!character || tokens.Count() < 2 || !ParsePoint(tokens[1], goal))
		{
			LF_AgentHarness.Answer(id, "error", "need_character_and_x,z");
			return;
		}

		s_vPlanFrom = character.GetOrigin();
		vector given;
		if (ParsePoint(Option(tokens, "from"), given))
			s_vPlanFrom = given;

		s_sPlanNear = Option(tokens, "near");
		s_iPlanId = id;
		character.LF_AskDriveRoute(id, s_vPlanFrom, goal);
		LF_AgentHarness.Answer(id, "pending", "planning");
	}

	//------------------------------------------------------------------------------------------------
	protected static void OnPlan(int id, bool ok, string detail)
	{
		if (!ok)
		{
			LF_AgentHarness.Answer(id, "error", detail);
			return;
		}

		array<vector> route = {};
		DecodeRoute(detail, route);
		if (route.IsEmpty())
		{
			LF_AgentHarness.Answer(id, "error", "empty_route");
			return;
		}

		float length = vector.DistanceXZ(s_vPlanFrom, route[0]);
		for (int i = 1; i < route.Count(); i++)
			length += vector.DistanceXZ(route[i - 1], route[i]);

		string line = string.Format("plan points=%1 route=%2m straight=%3m", route.Count(), Math.Round(length), Math.Round(vector.DistanceXZ(s_vPlanFrom, route[route.Count() - 1])));
		vector near;
		if (ParsePoint(s_sPlanNear, near))
		{
			line += " near=";
			int shown = 0;
			foreach (vector p : route)
			{
				if (vector.DistanceXZ(p, near) > PLAN_NEAR_M)
					continue;

				if (shown > 0)
					line += ";";

				line += string.Format("%1,%2", Round1(p[0]), Round1(p[2]));
				shown++;
			}
		}

		LF_Debug.Info(FEATURE, "drive " + line);
		LF_AgentHarness.Answer(id, "ok", line);
	}

	//------------------------------------------------------------------------------------------------
	static void DecodeRoute(string detail, notnull array<vector> route)
	{
		array<string> points = {};
		detail.Split(";", points, true);
		foreach (string token : points)
		{
			vector routePoint;
			if (ParsePoint(token, routePoint))
				route.Insert(routePoint);
		}
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

		s_bRoad = sub == "road";
		s_vGoal = route[route.Count() - 1];
		s_vGoal[1] = 0;
		ReadCar(car);

		BeginTicks(car, "driving");
		SetRoute(car, route, s_bRoad);

		// The whole drive's budget: as given, or what the route needs at a slow 11 km/h.
		float timeout = Math.Max(DEFAULT_TIMEOUT_S, 60 + s_fRouteLength / 3);
		string t = Option(tokens, "timeout");
		if (!t.IsEmpty())
			timeout = Math.Clamp(t.ToFloat(), 1, MAX_TIMEOUT_S);

		s_fLegTimeoutS = LEG_BASE_S + LEG_M / LEG_MIN_MPS;
		string legTimeout = Option(tokens, "legtimeout");
		if (!legTimeout.IsEmpty())
			s_fLegTimeoutS = Math.Max(legTimeout.ToFloat(), 5);

		s_fEndMs = s_fStartMs + timeout * 1000;
		s_sResult = "";

		// What a driver does on getting in: the engine on, the handbrake off.
		VehicleControllerComponent controller = LF_Component<VehicleControllerComponent>.Find(car);
		if (controller && !controller.IsEngineOn())
			controller.StartEngine();

		CarControllerComponent carController = CarControllerComponent.Cast(controller);
		if (carController)
			carController.SetPersistentHandBrake(false);

		LF_Debug.Info(FEATURE, string.Format("drive %1 from %2 to %3 points=%4 route=%5m cap=%6kmh mode=%7 wheelbase=%8 lat=%9", sub, here, s_vGoal, s_aRoute.Count(), Math.Round(s_fRouteLength), s_fSpeedCap, s_sMode, Round1(s_fWheelbase), Round1(s_fLateral)));
		string started = string.Format("driving points=%1 straight=%2m route=%3m legs=%4 timeout=%5s", s_aRoute.Count(), Math.Round(vector.DistanceXZ(here, s_vGoal)), Math.Round(s_fRouteLength), s_iLegCount, Math.Round(timeout));
		if (HasWord(tokens, "wait"))
		{
			s_iWaitId = id;
			LF_AgentHarness.Answer(id, "pending", started);
			return;
		}

		LF_AgentHarness.Answer(id, "ok", started);
	}

	//------------------------------------------------------------------------------------------------
	//! The car's wheelbase and the sideways grip the corner speeds assume (a heavy truck leans more).
	protected static void ReadCar(IEntity car)
	{
		vector mins, maxs;
		car.GetBounds(mins, maxs);
		s_fWheelbase = Math.Clamp((maxs[2] - mins[2]) * 0.6, 2.2, 5.5);
		s_fFrontM = maxs[2];
		s_fHalfWidthM = Math.Max((maxs[0] - mins[0]) * 0.5 - 0.15, 0.3);
		s_fBackupS = BACKUP_S;
		s_fLateral = LATERAL_MPS2;
		Physics physics = car.GetPhysics();
		if (physics && physics.GetMass() > HEAVY_KG)
		{
			s_fLateral = LATERAL_HEAVY_MPS2;
			s_fBackupS = BACKUP_HEAVY_S;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The route from where the car stands: resampled every STEP_M with the distance along it and the
	//! speed each point's bend allows. On a road route the first points behind the car are dropped
	//! (the nearest road point is often just behind it in a yard).
	protected static void SetRoute(IEntity car, array<vector> points, bool trimBehind)
	{
		vector here = car.GetOrigin();
		here[1] = 0;
		vector transform[4];
		car.GetWorldTransform(transform);
		vector forward = Vector(transform[2][0], 0, transform[2][2]).Normalized();

		array<vector> raw = {here};
		int first = 0;
		if (trimBehind)
		{
			while (first < points.Count() - 1)
			{
				vector p = points[first];
				p[1] = 0;
				if (vector.DistanceXZ(p, here) > TRIM_BEHIND_M || vector.Dot(p - here, forward) > 0)
					break;

				first++;
			}
		}

		for (int i = first; i < points.Count(); i++)
			raw.Insert(Vector(points[i][0], 0, points[i][2]));

		Resample(raw, STEP_M, s_aRoute);
		s_aCum.Clear();
		float length = 0;
		foreach (int k, vector q : s_aRoute)
		{
			if (k > 0)
				length += vector.DistanceXZ(s_aRoute[k - 1], q);

			s_aCum.Insert(length);
		}

		CornerSpeeds(s_aRoute, s_fLateral, s_aCorner);
		s_fRouteLength = length;
		s_iSeg = 0;
		s_fBestProgress = 0;
		s_fLegFrom = 0;
		s_iLegCount = Math.Ceil(length / LEG_M);
		if (s_iLegCount < 1)
			s_iLegCount = 1;
	}

	//------------------------------------------------------------------------------------------------
	//! `points` with a point every `step` metres along it (the last point kept as it is).
	static void Resample(array<vector> points, float step, notnull array<vector> outPoints)
	{
		outPoints.Clear();
		if (points.IsEmpty())
			return;

		outPoints.Insert(points[0]);
		for (int i = 1; i < points.Count(); i++)
		{
			vector a = points[i - 1];
			vector b = points[i];
			float d = vector.DistanceXZ(a, b);
			int pieces = Math.Ceil(d / step);
			for (int k = 1; k <= pieces; k++)
			{
				float share = k;
				outPoints.Insert(a + (b - a) * (share / pieces));
			}
		}

		if (outPoints.Count() == 1)
			outPoints.Insert(points[0]);
	}

	//------------------------------------------------------------------------------------------------
	//! The speed (m/s) each point's bend allows: the radius from the turn over CURVE_ARM_M either side,
	//! then v = sqrt(lateral * radius), at least CORNER_MIN_MPS.
	static void CornerSpeeds(array<vector> points, float lateral, notnull array<float> speeds)
	{
		speeds.Clear();
		int n = points.Count();
		for (int i = 0; i < n; i++)
		{
			float allowed = CORNER_MAX_MPS;
			int back = i;
			float armA = 0;
			while (back > 0 && armA < CURVE_ARM_M)
			{
				armA += vector.DistanceXZ(points[back - 1], points[back]);
				back--;
			}

			int ahead = i;
			float armB = 0;
			while (ahead < n - 1 && armB < CURVE_ARM_M)
			{
				armB += vector.DistanceXZ(points[ahead], points[ahead + 1]);
				ahead++;
			}

			if (armA > 0.5 && armB > 0.5)
			{
				vector inDir = (points[i] - points[back]).Normalized();
				vector outDir = (points[ahead] - points[i]).Normalized();
				float turn = Math.Acos(Math.Clamp(vector.Dot(inDir, outDir), -1, 1));
				if (turn > 0.001)
				{
					float radius = (armA + armB) * 0.5 / turn;
					allowed = Math.Clamp(Math.Sqrt(lateral * radius), CORNER_MIN_MPS, CORNER_MAX_MPS);
				}
			}

			speeds.Insert(allowed);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void BeginTicks(IEntity car, string status)
	{
		s_bActive = true;
		s_bStopping = false;
		s_bReplanning = false;
		s_sStatus = status;
		s_iSeg = 0;
		s_iBackups = 0;
		s_iReplans = 0;
		s_iLeg = 0;
		s_iLegCount = 1;
		s_fDriven = 0;
		s_fMaxKmh = 0;
		s_iTicks = 0;
		s_iPrepares = 0;
		s_fStillSinceMs = 0;
		s_fBackupUntilMs = 0;
		s_fDoneMs = 0;
		s_fProgress = 0;
		s_fBestProgress = 0;
		s_fLegFrom = 0;
		s_iWaitId = 0;
		s_iReplanId = 0;
		s_sWhy = "";
		s_sStuckAt = "";
		s_vStart = car.GetOrigin();
		s_vLast = s_vStart;
		s_fStartMs = GetGame().GetWorld().GetWorldTime();
		s_fProgressAtMs = s_fStartMs;
		s_fLegStartMs = s_fStartMs;
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

		string line = string.Format("%1 driven=%2m time=%3s err=%4m max=%5kmh at=%6,%7 backups=%8 mode=%9", s_sStatus, Round1(s_fDriven), Round1(seconds), Round1(err), Round1(s_fMaxKmh), Round1(at[0]), Round1(at[2]), s_iBackups, s_sMode);
		line += string.Format(" progress=%1m route=%2m leg=%3/%4 replans=%5", Math.Round(s_fProgress), Math.Round(s_fRouteLength), LF_Utils.MinInt(s_iLeg + 1, s_iLegCount), s_iLegCount, s_iReplans);
		if (!s_sWhy.IsEmpty())
			line += " why=" + s_sWhy;

		if (!s_sStuckAt.IsEmpty())
			line += " stuckat=" + s_sStuckAt;

		return line;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Finish(string status)
	{
		if (!s_bActive)
			return;

		s_bActive = false;
		s_bReplanning = false;
		s_iReplanId = 0;
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

		if (status == "arrived" || status == "distance" || status == "stopped")
			s_sWhy = "";

		string summary = Summary();
		LF_Debug.Info(FEATURE, "drive end " + summary);
		if (s_iWaitId > 0)
		{
			string answer = "ok";
			if (status == "timeout" || status == "stuck" || status == "lost_vehicle" || status == "out_of_fuel")
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
			s_sWhy = string.Format("drive_budget_over_at_%1m_%2", Math.Round(s_fProgress), Blocker(car, ForwardOf(car)));
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

		// Waiting for the server's new route after a stuck spot: stand on the brakes.
		if (s_bReplanning)
		{
			StopInputs(car, speed);
			if (now > s_fReplanUntilMs)
			{
				LF_Debug.Warn(FEATURE, "drive re-plan got no answer; keeps the old route");
				s_bReplanning = false;
				s_iReplanId = 0;
				s_sStatus = "driving";
				Resume(now, Project(at, 1000));
			}

			Push(car);
			return;
		}

		vector transform[4];
		car.GetWorldTransform(transform);
		vector forward = Vector(transform[2][0], 0, transform[2][2]).Normalized();
		vector right = Vector(transform[0][0], 0, transform[0][2]).Normalized();

		s_fProgress = Project(at, PROJECT_AHEAD);
		if (s_fProgress > s_fBestProgress + PROGRESS_M)
		{
			s_fBestProgress = s_fProgress;
			s_fProgressAtMs = now;
		}

		float toGoal = vector.DistanceXZ(at, s_vGoal);
		if (s_fMaxDistance > 0 && s_fDriven >= s_fMaxDistance)
		{
			BeginStop(car, speed, "distance");
			return;
		}

		if (toGoal <= s_fTolerance)
		{
			BeginStop(car, speed, "");
			return;
		}

		// A leg is LEG_M of the route; each has its own budget.
		if (s_fProgress - s_fLegFrom >= LEG_M)
		{
			s_iLeg++;
			s_fLegFrom = s_fProgress;
			s_fLegStartMs = now;
		}
		else if (s_fBackupUntilMs <= 0 && now - s_fLegStartMs > s_fLegTimeoutS * 1000)
		{
			s_sWhy = string.Format("leg_%1_over_%2s_%3", s_iLeg + 1, Math.Round(s_fLegTimeoutS), Blocker(car, forward));
			Finish("timeout");
			return;
		}

		vector toTargetNow = PointAt(s_fProgress + 4) - at;
		toTargetNow[1] = 0;
		float alphaNow = Math.Atan2(vector.Dot(toTargetNow, right), vector.Dot(toTargetNow, forward));

		// Backing up out of a stuck spot: brake held at a standstill reverses an automatic box.
		if (s_fBackupUntilMs > 0)
		{
			if (now < s_fBackupUntilMs && vector.DistanceXZ(at, s_vBackupFrom) < BACKUP_M)
			{
				s_fThrust = 0;
				s_fBrake = 1;
				s_fHand = 0;
				s_fSteer = s_fBackupSteer;
				Push(car);
				return;
			}

			EndBackup(car, at, now);
			if (s_bReplanning)
			{
				StopInputs(car, speed);
				Push(car);
				return;
			}
		}

		// A road drive that has left its route (pushed off it, or took a wrong branch) asks again.
		float offRoute = vector.DistanceXZ(at, PointAt(s_fProgress));
		if (s_bRoad && offRoute > OFF_ROUTE_M && s_iReplans < MAX_REPLANS)
		{
			s_sWhy = string.Format("off_route_%1m", Math.Round(offRoute));
			RequestReplan(character, at, now);
			StopInputs(car, speed);
			Push(car);
			return;
		}

		// Pure pursuit of the point `look` metres on along the route (never a straight chord across a
		// bend: the point walks the route), with the geometric steer angle for this wheelbase.
		float v = speed / 3.6;
		float look = Math.Clamp(LOOK_BASE_M + LOOK_GAIN_S * v, LOOK_MIN_M, LOOK_MAX_M);
		vector target = PointAt(s_fProgress + look);
		vector toTarget = target - at;
		toTarget[1] = 0;
		float alpha = Math.Atan2(vector.Dot(toTarget, right), vector.Dot(toTarget, forward));
		float chord = Math.Max(toTarget.Length(), 1);
		float delta = Math.Atan2(2 * s_fWheelbase * Math.Sin(alpha), chord);
		if (Math.AbsFloat(alpha) > Math.PI * 0.5)
		{
			delta = MAX_STEER_DEG * Math.DEG2RAD;
			if (alpha < 0)
				delta = -delta;
		}

		s_fSteer = s_fSteerSign * Math.Clamp(delta * Math.RAD2DEG / MAX_STEER_DEG, -1, 1);
		s_fHand = 0;

		// Target speed: the cap, what every bend ahead allows once braked for, the goal stop, and a
		// crawl while the route points well off the nose.
		float want = Math.Min(s_fSpeedCap, AheadLimitMps(v) * 3.6);
		if (Math.AbsFloat(alpha) * Math.RAD2DEG > 45)
			want = Math.Min(want, BEND_KMH);

		float stopKmh = Math.Sqrt(2 * DECEL * Math.Max(toGoal - s_fTolerance * 0.5, 0)) * 3.6;
		want = Math.Min(want, Math.Max(stopKmh, 4));

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

		// Stuck: the pedal is down and the car stands, or the route has not moved on for a while.
		if (s_fThrust > 0.2 && speed < STILL_KMH)
		{
			if (s_fStillSinceMs <= 0)
				s_fStillSinceMs = now;
		}
		else
		{
			s_fStillSinceMs = 0;
		}

		string reason;
		bool starting = now - s_fStartMs < START_GRACE_S * 1000;
		if (s_fStillSinceMs > 0 && now - s_fStillSinceMs > STUCK_S * 1000 && !starting)
			reason = "stalled";
		else if (now - s_fProgressAtMs > NO_PROGRESS_S * 1000)
			reason = "no_progress";

		if (!reason.IsEmpty())
		{
			if (!StartBackup(car, forward, at, alphaNow, now, reason))
				return;
		}

		Push(car);
	}

	//------------------------------------------------------------------------------------------------
	protected static void BeginStop(IEntity car, float speed, string result)
	{
		s_bStopping = true;
		if (!result.IsEmpty())
			s_sResult = result;

		s_sStatus = "braking";
		StopInputs(car, speed);
		Push(car);
	}

	//------------------------------------------------------------------------------------------------
	//! Starts a back-up; false (after Finish) once MAX_BACKUPS are spent. Odd tries reverse with the
	//! wheels turned against the way the route goes (the nose swings toward it), even tries straight,
	//! the fourth the other way round in case the snag is on that side.
	protected static bool StartBackup(IEntity car, vector forward, vector at, float alpha, float now, string reason)
	{
		string blocker = Blocker(car, forward);
		if (!s_sStuckAt.IsEmpty())
			s_sStuckAt += "|";

		s_sStuckAt += string.Format("%1,%2", Math.Round(at[0]), Math.Round(at[2]));
		s_sWhy = reason + "_" + blocker;
		if (blocker == "out_of_fuel")
		{
			Finish("out_of_fuel");
			return false;
		}

		if (s_iBackups >= MAX_BACKUPS)
		{
			Finish("stuck");
			return false;
		}

		s_iBackups++;
		float side = 1;
		if (alpha < 0)
			side = -1;

		if (Math.AbsFloat(alpha) < 5 * Math.DEG2RAD && s_iBackups % 2 == 0)
			side = -side;

		s_fBackupSteer = 0;
		if (s_iBackups % 2 == 1)
			s_fBackupSteer = -s_fSteerSign * side * BACKUP_LOCK;
		else if (s_iBackups == 4)
			s_fBackupSteer = s_fSteerSign * side * BACKUP_LOCK;

		s_vBackupFrom = at;
		s_fBackupUntilMs = now + s_fBackupS * 1000;
		s_fStillSinceMs = 0;
		s_sStatus = "backing";
		LF_Debug.Info(FEATURE, string.Format("drive stuck at %1 (%2), backing up %3 of %4 steer=%5", at, s_sWhy, s_iBackups, MAX_BACKUPS, Round1(s_fBackupSteer)));
		s_fThrust = 0;
		s_fBrake = 1;
		s_fHand = 0;
		s_fSteer = s_fBackupSteer;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! After a back-up: a road drive from its second try on asks the server for a new route from here;
	//! otherwise the car rejoins the route at the point nearest to it.
	protected static void EndBackup(IEntity car, vector at, float now)
	{
		s_fBackupUntilMs = 0;
		s_sStatus = "driving";
		SCR_ChimeraCharacter character = Character();
		if (s_bRoad && s_iBackups >= 2 && s_iReplans < MAX_REPLANS && character)
		{
			RequestReplan(character, at, now);
			return;
		}

		Resume(now, Project(at, 1000));
	}

	//------------------------------------------------------------------------------------------------
	protected static void Resume(float now, float progress)
	{
		s_fProgress = progress;
		s_fBestProgress = progress;
		s_fProgressAtMs = now;
		s_fStillSinceMs = 0;
		s_fLegStartMs = Math.Max(s_fLegStartMs, now - s_fLegTimeoutS * 500);
	}

	//------------------------------------------------------------------------------------------------
	protected static void RequestReplan(SCR_ChimeraCharacter character, vector at, float now)
	{
		s_bReplanning = true;
		s_sStatus = "routing";
		s_iReplanSeq++;
		s_iReplanId = -s_iReplanSeq;
		s_fReplanUntilMs = now + REPLAN_WAIT_S * 1000;
		LF_Debug.Info(FEATURE, string.Format("drive re-plans from %1 (%2)", at, s_sWhy));
		character.LF_AskDriveRoute(s_iReplanId, at, s_vGoal);
	}

	//------------------------------------------------------------------------------------------------
	protected static void OnReplan(bool ok, string detail)
	{
		if (!s_bActive || !s_bReplanning)
			return;

		s_bReplanning = false;
		s_sStatus = "driving";
		s_iReplans++;
		float now = GetGame().GetWorld().GetWorldTime(); // lf-float: world time in ms, harness only
		IEntity car = VehicleOf(Character());
		array<vector> route = {};
		if (ok)
			DecodeRoute(detail, route);

		if (!car || route.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, "drive re-plan refused: " + detail);
			if (car)
				Resume(now, Project(car.GetOrigin(), 1000));

			return;
		}

		SetRoute(car, route, true);
		LF_Debug.Info(FEATURE, string.Format("drive re-planned %1 points route=%2m", s_aRoute.Count(), Math.Round(s_fRouteLength)));
		Resume(now, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! What stands in front of the bumper: a prefab name, `terrain`, or `clear` (then the pitch, since
	//! a lip or a steep bank stops a car with nothing in front of it).
	protected static string Blocker(IEntity car, vector forward)
	{
		// An empty tank stops a car with nothing in front of it.
		if (FuelShare(car) == 0)
			return "out_of_fuel";

		// Three rays (both front corners and the middle) at a kerb height and a bumper height.
		array<float> heights = {0.35, 0.9};
		array<float> sides = {0, -1, 1};
		for (int ray = 0; ray < 6; ray++)
		{
			float side = sides[ray / 2];
			float h = heights[ray % 2];
			TraceParam trace = new TraceParam();
			trace.Start = car.CoordToParent(Vector(side * s_fHalfWidthM, h, s_fFrontM - 0.2));
			trace.End = trace.Start + forward * BLOCKER_LOOK_M;
			trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
			trace.LayerMask = EPhysicsLayerPresets.Main;
			trace.Exclude = car;
			float fraction = GetGame().GetWorld().TraceMove(trace, null);
			if (fraction >= 1)
				continue;

			IEntity hit = trace.TraceEnt;
			if (!hit || GenericTerrainEntity.Cast(hit))
				return string.Format("terrain_at_%1m_h%2", Round1(fraction * BLOCKER_LOOK_M), h);

			string name = LF_Utils.GetPrefabName(hit);
			if (name.IsEmpty())
				name = hit.GetName();

			if (name.IsEmpty())
				name = hit.ClassName();

			int slash = name.LastIndexOf("/");
			if (slash >= 0)
				name = name.Substring(slash + 1, name.Length() - slash - 1);

			name.Replace(".et", "");
			string corner = "middle";
			if (side < 0)
				corner = "left";
			else if (side > 0)
				corner = "right";

			return string.Format("blocked_by_%1_at_%2m_%3", name, Round1(fraction * BLOCKER_LOOK_M), corner);
		}

		vector angles = car.GetYawPitchRoll();
		return string.Format("clear_pitch_%1", Math.Round(angles[1]));
	}

	//------------------------------------------------------------------------------------------------
	//! The fuel left as a share of the tank (0..1), or -1 for a car without a fuel manager.
	static float FuelShare(IEntity car)
	{
		SCR_FuelManagerComponent manager = SCR_FuelManagerComponent.Cast(car.FindComponent(SCR_FuelManagerComponent));
		if (!manager || manager.GetTotalMaxFuel() <= 0)
			return -1;

		return manager.GetTotalFuel() / manager.GetTotalMaxFuel();
	}

	//------------------------------------------------------------------------------------------------
	protected static vector ForwardOf(IEntity car)
	{
		vector transform[4];
		car.GetWorldTransform(transform);
		return Vector(transform[2][0], 0, transform[2][2]).Normalized();
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
	//! The distance along the route of the point nearest to `at`, searching from the current segment
	//! up to `ahead` metres on (a little behind too); moves the current segment there.
	protected static float Project(vector at, float ahead)
	{
		int last = s_aRoute.Count() - 1;
		if (last <= 0)
			return 0;

		int lo = LF_Utils.MaxInt(s_iSeg - 3, 0);
		if (ahead >= 1000)
			lo = 0;

		float bestD = 1000000000.0;
		int best = s_iSeg;
		float bestT = 0;
		for (int i = lo; i < last; i++)
		{
			if (i > s_iSeg && s_aCum[i] - s_aCum[s_iSeg] > ahead)
				break;

			float t;
			float d = SegmentDistance(at, s_aRoute[i], s_aRoute[i + 1], t);
			if (d < bestD - 0.01)
			{
				bestD = d;
				best = i;
				bestT = t;
			}
		}

		s_iSeg = best;
		return s_aCum[best] + (s_aCum[best + 1] - s_aCum[best]) * bestT;
	}

	//------------------------------------------------------------------------------------------------
	//! The route point `s` metres along it (the last point past the end).
	protected static vector PointAt(float s)
	{
		int last = s_aRoute.Count() - 1;
		if (last <= 0 || s >= s_fRouteLength)
			return s_aRoute[last];

		int i = s_iSeg;
		while (i > 0 && s_aCum[i] > s)
			i--;

		while (i < last - 1 && s_aCum[i + 1] < s)
			i++;

		float span = s_aCum[i + 1] - s_aCum[i];
		if (span < 0.001)
			return s_aRoute[i + 1];

		return s_aRoute[i] + (s_aRoute[i + 1] - s_aRoute[i]) * ((s - s_aCum[i]) / span);
	}

	//------------------------------------------------------------------------------------------------
	//! The highest speed (m/s) from which every bend ahead within braking reach can still be taken.
	protected static float AheadLimitMps(float v)
	{
		float horizon = v * v / (2 * DECEL) + 25;
		float limit = CORNER_MAX_MPS;
		for (int i = s_iSeg; i < s_aRoute.Count(); i++)
		{
			float d = s_aCum[i] - s_fProgress;
			if (d > horizon)
				break;

			if (d < 0)
				d = 0;

			float allowed = Math.Sqrt(s_aCorner[i] * s_aCorner[i] + 2 * DECEL * d);
			if (allowed < limit)
				limit = allowed;
		}

		return limit;
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
//! A route along the AI road network: A* over the points of the roads in the box round the start
//! and the goal, joined
//! where roads meet, kept LANE_M right of the middle line. A binary heap keeps a whole-island
//! route (thousands of points) to one quick pass on the server.
class LF_DriveRoute
{
	//------------------------------------------------------------------------------------------------
	//! Fills `route` (start excluded, goal last) and returns "" or the reason there is none.
	static string Build(vector start, vector goal, notnull array<vector> route)
	{
		ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld || !aiWorld.GetRoadNetworkManager())
			return "no_road_network";

		// A long route bends far off the straight line (6.3 km by road for 4.6 km straight to the
		// east compound): the box grows with the distance.
		float straight = vector.DistanceXZ(start, goal);
		float margin = Math.Max(LF_DriveHarness.ROAD_MARGIN_M, straight * LF_DriveHarness.ROAD_MARGIN_SHARE);
		vector mins = Vector(Math.Min(start[0], goal[0]) - margin, -1000, Math.Min(start[2], goal[2]) - margin);
		vector maxs = Vector(Math.Max(start[0], goal[0]) + margin, 5000, Math.Max(start[2], goal[2]) + margin);
		array<BaseRoad> roads = {};
		aiWorld.GetRoadNetworkManager().GetRoadsInAABB(mins, maxs, roads);
		if (roads.IsEmpty())
			return "no_roads_near";

		array<vector> nodes = {};
		array<int> roadOf = {};
		int rawPoints = 0;
		foreach (int r, BaseRoad road : roads)
		{
			array<vector> points = {};
			road.GetPoints(points);
			int count = points.Count();
			rawPoints += count;
			for (int k = 0; k < count; k++)
			{
				vector p = Vector(points[k][0], 0, points[k][2]);
				if (nodes.Count() >= LF_DriveHarness.MAX_ROAD_NODES)
					return string.Format("road_graph_too_big_%1_nodes", nodes.Count());

				nodes.Insert(p);
				roadOf.Insert(r);
			}
		}

		int n = nodes.Count();
		if (n < 2)
			return "no_road_points";

		map<int, ref array<int>> joins = new map<int, ref array<int>>();
		Joins(nodes, roadOf, LF_DriveHarness.ROAD_JOIN_M, joins);

		// The way onto the road and off it is a straight line: take the nearest road point that line
		// reaches without running into a wall, a fence or a wreck (a truck yard's nearest road point can
		// lie across a scrapyard dump, #5053; a walled smelter's across its tin wall).
		// Pairs are tried nearest first; the first that connects wins.
		array<int> fromList = {};
		array<int> toList = {};
		vector fromVia;
		vector toVia;
		bool fromViaUsed;
		bool toViaUsed;
		JoinCandidates(nodes, roadOf, start, fromList, fromVia, fromViaUsed);
		JoinCandidates(nodes, roadOf, goal, toList, toVia, toViaUsed);
		array<int> chain = {};
		float length = -1;
		int from = fromList[0];
		int to = toList[0];
		for (int sum = 0; sum <= fromList.Count() + toList.Count() - 2 && length < 0; sum++)
		{
			for (int fi = 0; fi < fromList.Count() && length < 0; fi++)
			{
				int ti = sum - fi;
				if (ti < 0 || ti >= toList.Count())
					continue;

				from = fromList[fi];
				to = toList[ti];
				length = Search(nodes, roadOf, joins, from, to, chain);
			}
		}

		if (length < 0)
		{
			LF_Debug.Warn(LF_DriveHarness.FEATURE, string.Format("drive road: no way from %1 to %2 over %3 road points (join candidates %4 at the start, %5 at the goal)", start, goal, n, fromList.Count(), toList.Count()));
			return "no_road_path";
		}

		int plainFrom = fromList[fromList.Count() - 1];
		int plainTo = toList[toList.Count() - 1];
		if (from != plainFrom || to != plainTo)
			LF_Debug.Info(LF_DriveHarness.FEATURE, string.Format("drive road joins at %1 (%2 m from the start) and leaves at %3 (%4 m from the goal), not the nearest road points %5 and %6: a straight line to those is blocked or they lead nowhere", nodes[from], Math.Round(vector.DistanceXZ(nodes[from], start)), nodes[to], Math.Round(vector.DistanceXZ(nodes[to], goal)), nodes[plainFrom], nodes[plainTo]));

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

		// A waypoint out of a shut-in yard belongs to the first road point only.
		if (fromViaUsed && from == fromList[0])
			route.InsertAt(fromVia, 0);

		if (toViaUsed && to == toList[0])
			route.Insert(toVia);

		route.Insert(Vector(goal[0], 0, goal[2]));
		LF_Debug.Info(LF_DriveHarness.FEATURE, string.Format("drive road route roads=%1 raw=%2 nodes=%3 joins=%4 points=%5 length=%6m straight=%7m", roads.Count(), rawPoints, n, joins.Count(), route.Count(), Math.Round(length), Math.Round(straight)));
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Links between points of different roads that lie within `join` metres (a junction), through a
	//! grid of `join`-metre cells. Along a road the neighbours are the points before and after it.
	static void Joins(array<vector> nodes, array<int> roadOf, float join, notnull map<int, ref array<int>> joins)
	{
		int n = nodes.Count();
		map<int, ref array<int>> grid = new map<int, ref array<int>>();
		for (int i = 0; i < n; i++)
		{
			int key = CellKey(nodes[i], join, 0, 0);
			array<int> bucket;
			if (!grid.Find(key, bucket))
			{
				bucket = {};
				grid.Insert(key, bucket);
			}

			bucket.Insert(i);
		}

		float join2 = join * join;
		for (int i3 = 0; i3 < n; i3++)
		{
			for (int dx = -1; dx <= 1; dx++)
			{
				for (int dz = -1; dz <= 1; dz++)
				{
					array<int> near;
					if (!grid.Find(CellKey(nodes[i3], join, dx, dz), near))
						continue;

					foreach (int j : near)
					{
						if (j <= i3 || roadOf[j] == roadOf[i3])
							continue;

						if (vector.DistanceSqXZ(nodes[i3], nodes[j]) <= join2)
						{
							AddJoin(joins, i3, j);
							AddJoin(joins, j, i3);
						}
					}
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void AddJoin(map<int, ref array<int>> joins, int a, int b)
	{
		array<int> list;
		if (!joins.Find(a, list))
		{
			list = {};
			joins.Insert(a, list);
		}

		list.Insert(b);
	}

	//------------------------------------------------------------------------------------------------
	//! A* from `from` to `to`: the road length, or -1 with no way through. `chain` gets the node
	//! indices, `from` first. Neighbours: the point before and after on the same road, and joins.
	static float Search(array<vector> nodes, array<int> roadOf, map<int, ref array<int>> joins, int from, int to, notnull array<int> chain)
	{
		chain.Clear();
		int n = nodes.Count();
		if (from < 0 || to < 0 || from >= n || to >= n)
			return -1;

		array<float> g = {};
		array<int> prev = {};
		array<bool> closed = {};
		for (int i = 0; i < n; i++)
		{
			g.Insert(1000000000.0);
			prev.Insert(-1);
			closed.Insert(false);
		}

		array<float> heapKeys = {};
		array<int> heapNodes = {};
		array<int> next = {};
		vector goalPoint = nodes[to];
		g[from] = 0;
		HeapPush(heapKeys, heapNodes, vector.DistanceXZ(nodes[from], goalPoint), from);
		while (!heapNodes.IsEmpty())
		{
			int u = HeapPop(heapKeys, heapNodes);
			if (closed[u])
				continue;

			closed[u] = true;
			if (u == to)
				break;

			next.Clear();
			if (u > 0 && roadOf[u - 1] == roadOf[u])
				next.Insert(u - 1);

			if (u + 1 < n && roadOf[u + 1] == roadOf[u])
				next.Insert(u + 1);

			array<int> joined;
			if (joins.Find(u, joined))
				next.InsertAll(joined);

			foreach (int v : next)
			{
				if (closed[v])
					continue;

				float alt = g[u] + vector.DistanceXZ(nodes[u], nodes[v]);
				if (alt < g[v])
				{
					g[v] = alt;
					prev[v] = u;
					HeapPush(heapKeys, heapNodes, alt + vector.DistanceXZ(nodes[v], goalPoint), v);
				}
			}
		}

		if (!closed[to])
			return -1;

		for (int at = to; at >= 0; at = prev[at])
			chain.InsertAt(at, 0);

		return g[to];
	}

	//------------------------------------------------------------------------------------------------
	//! A min-heap of (key, node) in two arrays.
	static void HeapPush(notnull array<float> keys, notnull array<int> values, float key, int value)
	{
		keys.Insert(key);
		values.Insert(value);
		int i = keys.Count() - 1;
		while (i > 0)
		{
			int parent = (i - 1) / 2;
			if (keys[parent] <= keys[i])
				break;

			HeapSwap(keys, values, i, parent);
			i = parent;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Takes the node with the smallest key off the heap (the heap must not be empty).
	static int HeapPop(notnull array<float> keys, notnull array<int> values)
	{
		int top = values[0];
		int last = keys.Count() - 1;
		keys[0] = keys[last];
		values[0] = values[last];
		keys.RemoveOrdered(last);
		values.RemoveOrdered(last);
		int count = last;
		int i = 0;
		while (true)
		{
			int left = 2 * i + 1;
			if (left >= count)
				break;

			int child = left;
			if (left + 1 < count && keys[left + 1] < keys[left])
				child = left + 1;

			if (keys[i] <= keys[child])
				break;

			HeapSwap(keys, values, i, child);
			i = child;
		}

		return top;
	}

	//------------------------------------------------------------------------------------------------
	protected static void HeapSwap(array<float> keys, array<int> values, int a, int b)
	{
		float key = keys[a];
		keys[a] = keys[b];
		keys[b] = key;
		int value = values[a];
		values[a] = values[b];
		values[b] = value;
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
	//! The nearest road point within CLEAR_REACH_M whose straight line from `at` meets no entity at
	//! bumper height, trying up to CLEAR_CANDIDATES points nearest first and at most two per road (the
	//! points of one road lie side by side, so a wall in front of one is in front of its neighbours);
	//! the plain nearest when none is clear. Fills `joinsAt` with up to CLEAR_KEEP clear points,
	//! nearest first, and the plain nearest last, for the search to fall back on when the first lies
	//! on a road stub that leads nowhere.
	protected static void JoinCandidates(array<vector> nodes, array<int> roadOf, vector at, notnull array<int> joinsAt, out vector via, out bool viaUsed)
	{
		viaUsed = false;
		joinsAt.Clear();
		int nearest = Nearest(nodes, at);
		array<int> near = {};
		array<float> nearD = {};
		foreach (int i, vector p : nodes)
		{
			float d = vector.DistanceXZ(p, at);
			if (d <= LF_DriveHarness.CLEAR_REACH_M)
			{
				near.Insert(i);
				nearD.Insert(d);
			}
		}

		map<int, int> perRoad = new map<int, int>();
		int tried = 0;
		string blocked;
		while (tried < LF_DriveHarness.CLEAR_CANDIDATES && joinsAt.Count() < LF_DriveHarness.CLEAR_KEEP && !near.IsEmpty())
		{
			int best = 0;
			for (int k = 1; k < near.Count(); k++)
			{
				if (nearD[k] < nearD[best])
					best = k;
			}

			int candidate = near[best];
			near.Remove(best);
			nearD.Remove(best);
			int seen = 0;
			perRoad.Find(roadOf[candidate], seen);
			if (seen >= 2)
				continue;

			perRoad.Set(roadOf[candidate], seen + 1);
			tried++;
			if (LineClear(at, nodes[candidate]))
				joinsAt.Insert(candidate);
			else
				blocked += string.Format(" %1,%2:%3", Math.Round(nodes[candidate][0]), Math.Round(nodes[candidate][2]), s_sLastHit);
		}

		if (!blocked.IsEmpty())
			LF_Debug.Info(LF_DriveHarness.FEATURE, string.Format("drive road joins near %1 skip blocked road points:%2", at, blocked));

		// Shut in on every side (a yard with walls, a ramp and parked trucks between the car and every
		// road point): a waypoint with a clear line from here and a clear line on to a road point (the
		// yard gate) is the way out.
		if (joinsAt.IsEmpty())
		{
			int viaNode;
			if (ViaJoin(nodes, roadOf, at, via, viaNode))
			{
				viaUsed = true;
				joinsAt.Insert(viaNode);
				LF_Debug.Info(LF_DriveHarness.FEATURE, string.Format("drive road joins near %1 by way of %2 to the road point %3", at, via, nodes[viaNode]));
			}
		}

		if (joinsAt.Find(nearest) < 0)
			joinsAt.Insert(nearest);
	}

	//------------------------------------------------------------------------------------------------
	//! A waypoint VIA_RADII metres out in one of VIA_DIRECTIONS directions with a clear line from `at`
	//! and a clear line on to a road point (up to VIA_TRIES of the nearest to it, two per road).
	protected static bool ViaJoin(array<vector> nodes, array<int> roadOf, vector at, out vector via, out int node)
	{
		array<float> radii = {15, 30, 45};
		string tried;
		foreach (float radius : radii)
		{
			for (int k = 0; k < LF_DriveHarness.VIA_DIRECTIONS; k++)
			{
				float angle = k * 2 * Math.PI / LF_DriveHarness.VIA_DIRECTIONS;
				vector v = at + Vector(Math.Cos(angle) * radius, 0, Math.Sin(angle) * radius);
				if (!LineClear(at, v))
				{
					tried += string.Format(" %1,%2:%3", Math.Round(v[0]), Math.Round(v[2]), s_sLastHit);
					continue;
				}

				int found = FirstClear(nodes, roadOf, v);
				if (found >= 0)
				{
					via = Vector(v[0], 0, v[2]);
					node = found;
					return true;
				}

				tried += string.Format(" %1,%2:on_%3", Math.Round(v[0]), Math.Round(v[2]), s_sLastHit);
			}
		}

		LF_Debug.Info(LF_DriveHarness.FEATURE, string.Format("drive road finds no way out near %1:%2", at, tried));
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The nearest road point within VIA_REACH_M of `at` with a clear line to it (VIA_TRIES tried, two
	//! per road), or -1.
	protected static int FirstClear(array<vector> nodes, array<int> roadOf, vector at)
	{
		array<int> near = {};
		array<float> nearD = {};
		foreach (int i, vector p : nodes)
		{
			float d = vector.DistanceXZ(p, at);
			if (d <= LF_DriveHarness.VIA_REACH_M)
			{
				near.Insert(i);
				nearD.Insert(d);
			}
		}

		map<int, int> perRoad = new map<int, int>();
		int tried = 0;
		while (tried < LF_DriveHarness.VIA_TRIES && !near.IsEmpty())
		{
			int best = 0;
			for (int k = 1; k < near.Count(); k++)
			{
				if (nearD[k] < nearD[best])
					best = k;
			}

			int candidate = near[best];
			near.Remove(best);
			nearD.Remove(best);
			int seen = 0;
			perRoad.Find(roadOf[candidate], seen);
			if (seen >= 2)
				continue;

			perRoad.Set(roadOf[candidate], seen + 1);
			tried++;
			if (LineClear(at, nodes[candidate]))
				return candidate;
		}

		return -1;
	}

	protected static string s_sLastHit;

	//------------------------------------------------------------------------------------------------
	//! True when the way from a to b, 0.3 m and 0.8 m above the ground, meets nothing but the terrain, a vehicle
	//! or a character. It is traced in LINE_STEP_M pieces that follow the ground (one straight line
	//! would run into a hill), with WORLD as well as ENTS: placed static props (a wreck, a tin wall)
	//! answer only to WORLD.
	static const float OWN_CAR_M = 5; // lf-fixed: harness geometry

	static bool LineClear(vector a, vector b)
	{
		// A scrap pile lower than a bumper still stops a truck that climbs it, and one a metre beside
		// the middle line is under a truck's wheels: three lines, the middle and SWEEP_HALF_M either side.
		vector dir = Vector(b[0] - a[0], 0, b[2] - a[2]);
		if (dir.Length() < 0.01)
			return true;

		dir.Normalize();
		vector side = Vector(dir[2], 0, -dir[0]) * LF_DriveHarness.SWEEP_HALF_M;
		array<float> offsets = {0, -1, 1};
		foreach (float o : offsets)
		{
			vector from = a + side * o;
			vector to = b + side * o;
			if (!LineClearAt(from, to, a, 0.3) || !LineClearAt(from, to, a, 0.8))
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool LineClearAt(vector a, vector b, vector origin, float height)
	{
		BaseWorld world = GetGame().GetWorld();
		float length = vector.DistanceXZ(a, b);
		int pieces = Math.Ceil(length / LF_DriveHarness.LINE_STEP_M);
		if (pieces < 1)
			return true;

		vector last = Vector(a[0], world.GetSurfaceY(a[0], a[2]) + height, a[2]);
		for (int k = 1; k <= pieces; k++)
		{
			float share = k;
			share = share / pieces;
			vector p = a + (b - a) * share;
			vector next = Vector(p[0], world.GetSurfaceY(p[0], p[2]) + height, p[2]);
			TraceParam trace = new TraceParam();
			trace.Start = last;
			trace.End = next;
			trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
			trace.LayerMask = EPhysicsLayerPresets.Main;
			float fraction = world.TraceMove(trace, null);
			last = next;
			if (fraction >= 1)
				continue;

			IEntity hit = trace.TraceEnt;
			if (!hit || GenericTerrainEntity.Cast(hit))
				continue;

			// The driver's own car at the start and a person are not walls; a parked truck further on is.
			IEntity root = hit;
			while (root.GetParent())
				root = root.GetParent();

			vector contact = trace.Start + (trace.End - trace.Start) * fraction;
			if (ChimeraCharacter.Cast(root) || (Vehicle.Cast(root) && vector.DistanceXZ(contact, origin) < OWN_CAR_M))
				continue;

			string name = LF_Utils.GetPrefabName(root);
			int slash = name.LastIndexOf("/");
			if (slash >= 0)
				name = name.Substring(slash + 1, name.Length() - slash - 1);

			if (name.IsEmpty())
				name = root.ClassName();

			s_sLastHit = string.Format("%1@%2,%3", name, Math.Round(contact[0]), Math.Round(contact[2]));
			return false;
		}

		return true;
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
