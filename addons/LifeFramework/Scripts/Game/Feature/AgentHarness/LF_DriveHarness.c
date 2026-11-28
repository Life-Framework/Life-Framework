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
	static const float REPLAN_GAP_S = 4; // lf-fixed: harness tuning
	//! Road route points this close behind the car at the start are dropped.
	static const float TRIM_BEHIND_M = 12; // lf-fixed: harness geometry
	//! What stands in front of the bumper is looked for this far out.
	static const float BLOCKER_LOOK_M = 3; // lf-fixed: harness geometry
	//! A shut door (a boom barrier's bar, a yard gate) is looked for this far ahead of the bumper.
	static const float DOOR_LOOK_M = 4; // lf-fixed: harness geometry
	//! A door ray steps past this many parts of the car itself before it gives up.
	static const int DOOR_HOPS = 6; // lf-fixed: harness geometry
	// A shut gate on the route is looked for ahead while driving: the car comes to rest GATE_STOP_M short of the bar
	// (4 to 6 m), so the gate step can open it with the car out of the way; the look reaches its braking distance plus
	// GATE_MARGIN_M, within GATE_REACH_MIN_M..GATE_REACH_MAX_M, every GATE_EVERY_MS.
	static const float GATE_STOP_M = 5; // lf-fixed: harness geometry
	static const float GATE_MARGIN_M = 10; // lf-fixed: harness geometry
	static const float GATE_REACH_MIN_M = 20; // lf-fixed: harness geometry
	static const float GATE_REACH_MAX_M = 70; // lf-fixed: harness geometry
	static const float GATE_EVERY_MS = 400; // lf-fixed: harness tuning
	//! The stop starts at this speed once the bar is within GATE_STOP_M + 1; the speed cap runs down to it.
	static const float GATE_STOP_KMH = 8; // lf-fixed: harness tuning
	//! A shut door stops the car only when it crosses the car's centre line (within this many metres of it): a bar or a
	//! gate across the way does, a leaf standing at the edge of a yard gate's opening does not (the truck yard's did).
	static const float GATE_CENTRE_M = 0.4; // lf-fixed: harness geometry
	//! A gate this near the route (metres) is on it; a yard gate across a field beside a bend is not.
	static const float GATE_ROUTE_M = 5; // lf-fixed: harness geometry
	//! The route 12 m on from a gate lies at least this (cosine) along the car's heading from it.
	static const float GATE_THROUGH = 0.3; // lf-fixed: harness geometry
	//! A car at rest nearer than this to a shut bar backs off (at most BACKUP_MAX_KMH) before the gate is opened.
	static const float GATE_BACK_MIN_M = 3.5; // lf-fixed: harness geometry
	static const int GATE_BACKS = 2; // lf-fixed: harness guard
	static const float GATE_BACK_MAX_M = 9; // lf-fixed: harness guard
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

	// Final approach: the last metres off the road (a yard, a stand of trees, a bank) at a creep, the
	// heading picked each quarter second from a fan of short rays round the way to the goal.
	static const float CREEP_KMH = 9; // lf-fixed: harness tuning
	static const float CREEP_SLOW_KMH = 4; // lf-fixed: harness tuning
	static const float BACKUP_MAX_KMH = 6; // lf-fixed: harness guard
	static const float ROLLBACK_KMH = 2.5; // lf-fixed: harness guard
	// A normal drive stuck twice with this much route left creeps the rest.
	static const float APPROACH_REACH_M = 320; // lf-fixed: harness geometry
	static const float APPROACH_LOOK_M = 7; // lf-fixed: harness geometry
	static const int FAN_STEPS = 8; // lf-fixed: harness guard
	static const float FAN_STEP_DEG = 15; // lf-fixed: harness geometry
	static const float FAN_REACH_M = 8; // lf-fixed: harness geometry
	static const float FAN_CLEAR_M = 4.5; // lf-fixed: harness geometry
	static const float FAN_EVERY_MS = 250; // lf-fixed: harness tuning
	static const float FAN_SURFACE_M = 6; // lf-fixed: harness geometry
	static const float FAN_SURFACE_BONUS = 1.5; // lf-fixed: harness tuning
	static const float SLOPE_ARM_M = 6; // lf-fixed: harness geometry
	// Nose up this far is a climb: the stuck test waits longer and the box is held in first.
	static const float CLIMB_DEG = 6; // lf-fixed: harness tuning
	static const float CLIMB_SOFT_DEG = 11; // lf-fixed: harness tuning
	static const float CLIMB_MAX_DEG = 24; // lf-fixed: harness tuning
	static const float DROP_MAX_DEG = 30; // lf-fixed: harness tuning
	static const float GROUND_STEP_M = 1; // lf-fixed: harness geometry
	static const float GROUND_WINDOW_M = 2; // lf-fixed: harness geometry
	static const float CLIMB_STILL_KMH = 0.6; // lf-fixed: harness tuning
	static const float CLIMB_STUCK_S = 5; // lf-fixed: harness tuning
	static const float CLIMB_NO_PROGRESS_S = 14; // lf-fixed: harness tuning
	static const float APPROACH_NO_PROGRESS_S = 14; // lf-fixed: harness tuning
	// Back-ups in a row that gain under STRIKE_GAIN_M on the goal end the creep (a long climb gets more).
	static const int MAX_APPROACH_STRIKES = 8; // lf-fixed: harness guard
	static const float STRIKE_GAIN_M = 10; // lf-fixed: harness geometry
	static const float LIP_M = 2.5; // lf-fixed: harness geometry
	static const float AVOID_S = 30; // lf-fixed: harness tuning
	static const float AVOID_DEG = 25; // lf-fixed: harness geometry
	static const float NEAR_MIN_M = 8; // lf-fixed: harness geometry
	static const float NEAR_TOL_SHARE = 3; // lf-fixed: harness geometry
	// A normal drive stuck twice far from its end (more route left than APPROACH_REACH_M) creeps a local stretch of
	// the route (an escape): this many vehicle lengths, at least ESCAPE_MIN_M, then goes back to the normal drive.
	static const float ESCAPE_LENGTHS = 5; // lf-fixed: harness geometry
	static const float ESCAPE_MIN_M = 25; // lf-fixed: harness geometry
	static const float ESCAPE_DONE_M = 3; // lf-fixed: harness geometry
	static const int MAX_ESCAPES = 3; // lf-fixed: harness guard
	// Swept-body look: the body is checked along the arc it drives to turn onto a heading, in steps of this length.
	static const float SWEEP_STEP_M = 1.5; // lf-fixed: harness geometry
	static const float SWEEP_MARGIN_M = 0.15; // lf-fixed: harness geometry
	static const int SWEEP_CANDIDATES = 6; // lf-fixed: harness guard
	static const float SWEEP_MIN_DEG = 12; // lf-fixed: harness geometry
	// Turn about: a route that starts (or resumes) more than TURN_ABOUT_DEG off the nose, which the vehicle cannot swing
	// onto in one full-lock arc without its body meeting something, is turned onto by a three-point turn: forward and
	// reverse legs, each as long as the body's arc has room for, until the way on is within TURN_DONE_DEG of the nose.
	static const float TURN_ABOUT_DEG = 75; // lf-fixed: harness geometry
	static const float TURN_DONE_DEG = 40; // lf-fixed: harness geometry
	static const float TURN_START_KMH = 6; // lf-fixed: harness tuning
	static const float TURN_KMH = 5; // lf-fixed: harness tuning
	static const float TURN_LEG_MAX_M = 14; // lf-fixed: harness geometry
	static const float TURN_ROOM_MIN_M = 1.5; // lf-fixed: harness geometry
	static const float TURN_ROOM_STEP_M = 1.5; // lf-fixed: harness geometry
	static const float TURN_STALL_S = 2; // lf-fixed: harness tuning
	static const float TURN_PROBE_M = 2; // lf-fixed: harness geometry
	static const int TURN_MAX_LEGS = 14; // lf-fixed: harness guard
	static const int TURN_MAX_ABOUTS = 3; // lf-fixed: harness guard
	static const int TURN_PHASE_FORWARD = 1; // lf-fixed: harness guard
	static const int TURN_PHASE_REVERSE = 2; // lf-fixed: harness guard
	static const int TURN_PHASE_STOP = 3; // lf-fixed: harness guard
	// Smallest turning radius as a share of the wheelbase at full lock (1 / tan(32 degrees) = 1.6) plus slack for the tyres.
	static const float TURN_RADIUS_PER_WHEELBASE = 1.7; // lf-fixed: harness geometry

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
	protected static float s_fReplanAtMs = -100000; // lf-float: world time in ms, harness only
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
	protected static float s_fRearM = 2; // bounds behind the origin, metres
	protected static float s_fHalfWidthM = 0.8;
	protected static float s_fBottomM;
	protected static float s_fTopM = 1.8;
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

	// Shut gate ahead (see GateGuard).
	protected static float s_fGateAtMs; // lf-float: world time in ms, harness only
	protected static float s_fGateM; // metres from the bumper at the last look, -1 none
	protected static float s_fGateCapKmh = 999; // the speed that still stops short of it, 999 none
	protected static vector s_vGateFrom;
	protected static string s_sGateFound;
	protected static bool s_bGateStop; // the stop under way is the gate's
	protected static bool s_bGateReverse; // the back-up under way is the gate's
	protected static int s_iGateBacks;
	protected static float s_fBackupLimitM = BACKUP_M;
	// Where the last shut-door look found one: metres from the bumper (negative: the bar is over the car) and the hit.
	protected static float s_fShutDoorM;
	protected static vector s_vShutDoorAt;
	protected static bool s_bShutDoorAt; // the last Blocker found a shut door and s_vShutDoorAt is where

	// Final approach state.
	protected static bool s_bApproach;
	protected static bool s_bApproachTail; // enter at the off-road tail (road drives, approach=1)
	protected static bool s_bApproachFallback = true; // enter when stuck near the end (approach=off clears)
	protected static float s_fTailFrom;
	protected static float s_fNear;
	protected static float s_fCreepKmh = CREEP_KMH;
	protected static float s_fApproachBest;
	protected static float s_fApproachProgress;
	protected static float s_fApproachEndMs; // lf-float: world time in ms, harness only
	protected static float s_fApproachDriven;
	protected static float s_fFanAtMs; // lf-float: world time in ms, harness only
	protected static float s_fFanYaw;
	protected static float s_fFanClear;
	protected static float s_fGoalClear;
	protected static bool s_bFanOpen;
	protected static bool s_bFanHeld;
	protected static string s_sTrackMat; // the ground material under the car where the creep began, for the log
	protected static int s_iApproachBackups;
	protected static int s_iStrikes;
	protected static float s_fStrikeBest;
	protected static int s_iApproaches;
	// Turn about state.
	protected static int s_iTurnPhase; // 0 off
	protected static int s_iTurnNext; // the leg after the stop (TURN_PHASE_FORWARD or _REVERSE), 0 the turn is done
	protected static int s_iTurnLegs;
	protected static int s_iTurnLast; // the kind of the leg just driven
	protected static int s_iTurnAbouts;
	protected static float s_fTurnLegM;
	protected static float s_fTurnLock; // +1 right, -1 left: the way the wheels are turned in this leg
	protected static float s_fTurnStillMs; // lf-float: world time in ms, harness only
	protected static float s_fTurnLegAtMs; // lf-float: world time in ms, harness only
	protected static vector s_vTurnFrom;
	// An escape: a creep of a local stretch of the route (the goal of the creep is a point on it), ended by progress.
	protected static bool s_bEscape;
	protected static vector s_vEscapeGoal;
	protected static float s_fEscapeUntil; // route metres at which the escape is over
	protected static int s_iEscapes;
	protected static ref array<float> s_aAvoid; // yaw, until (ms), yaw, until, ...
	protected static array<float> LzAvoid()
	{
		if (!s_aAvoid)
			s_aAvoid = new array<float>();
		return s_aAvoid;
	}

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
	//! Speed along the car's nose (km/h): negative while it rolls backwards.
	static float ForwardKmh(IEntity car, vector forward)
	{
		Physics physics = car.GetPhysics();
		if (!physics)
			return 0;

		vector v = physics.GetVelocity();
		v[1] = 0;
		return vector.Dot(v, forward) * 3.6;
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
		line += string.Format(" drive=%1 driven=%2m mode=%3 ticks=%4 prep=%5 creep=%6", s_sStatus, Math.Round(s_fDriven), s_sMode, s_iTicks, s_iPrepares, s_bApproach);
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

		if (sub == "probe")
		{
			Probe(id, tokens);
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
			LzRouteTokens();
			s_aRouteTokens.Clear();
			LzRouteTokens();
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
	protected static ref array<string> s_aRouteTokens;
	protected static array<string> LzRouteTokens()
	{
		if (!s_aRouteTokens)
			s_aRouteTokens = new array<string>();
		return s_aRouteTokens;
	}

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
		LzRouteTokens();
		StartDrive(id, s_aRouteTokens, car, route);
	}

	protected static string s_sProbe;
	protected static vector s_vProbeAt;
	protected static int s_iProbeCount;
	protected static IEntity s_pProbeCar;

	//------------------------------------------------------------------------------------------------
	//! drive probe <x,z> [r=<m>]: what stands within r metres (default 4) of a spot where a drive got
	//! stuck: each entity's prefab (or class), flat distance and bounds height, nearest first not
	//! guaranteed; the ground height and the ground's climb either way. Debugging a stall, not a drive.
	static void Probe(int id, array<string> tokens)
	{
		vector at;
		if (tokens.Count() < 2 || !ParsePoint(tokens[1], at))
		{
			LF_AgentHarness.Answer(id, "error", "need_x,z");
			return;
		}

		float radius = 4;
		string r = Option(tokens, "r");
		if (!r.IsEmpty())
			radius = Math.Clamp(r.ToFloat(), 0.5, 30);

		BaseWorld world = GetGame().GetWorld();
		at[1] = world.GetSurfaceY(at[0], at[2]);
		s_vProbeAt = at;
		s_sProbe = "";
		s_iProbeCount = 0;
		s_pProbeCar = null;
		world.QueryEntitiesBySphere(at, radius, CollectProbe, null, EQueryEntitiesFlags.ALL);
		string line = string.Format("probe ground=%1 n=%2 %3", Round1(at[1]), s_iProbeCount, s_sProbe);
		// A car in the sphere: what its blocker check says where it stands (the shut-door test, off a drive).
		if (s_pProbeCar)
			line += " blocker=" + Blocker(s_pProbeCar, ForwardOf(s_pProbeCar));
		LF_Debug.Info(FEATURE, "drive " + line);
		LF_AgentHarness.Answer(id, "ok", line);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool CollectProbe(IEntity entity)
	{
		if (!entity || GenericTerrainEntity.Cast(entity))
			return true;

		// The list is capped, but a door is always listed: the gate step needs it.
		if (s_iProbeCount >= 40 && !IsDoor(entity))
			return true;

		string name = LF_Utils.GetPrefabName(entity);
		if (name.IsEmpty())
			name = entity.ClassName();

		int slash = name.LastIndexOf("/");
		if (slash >= 0)
			name = name.Substring(slash + 1, name.Length() - slash - 1);

		vector mins, maxs;
		entity.GetBounds(mins, maxs);
		vector o = entity.GetOrigin();
		if (!s_pProbeCar && Vehicle.Cast(entity))
			s_pProbeCar = entity;

		string door;
		if (IsDoor(entity))
		{
			door = ",door:shut";
			BaseDoorComponent doorState = DoorOfEntity(entity);
			if (doorState && doorState.IsOpen())
				door = ",door:open";
		}

		s_sProbe += string.Format("%1@%2m,h%3,at=%4;%5;%6%7 ", name, Round1(vector.DistanceXZ(o, s_vProbeAt)), Round1(maxs[1] - mins[1]), Round1(o[0]), Round1(o[1]), Round1(o[2]), door);
		if (door != "")
			s_sProbe += LayerReport(entity, mins, maxs) + " ";

		s_iProbeCount++;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Which physics layers a trace across a door's bounds hits (from each side of its local axes, at its
	//! middle height): `Layer:hits` for each layer tried, the letters naming the axis sides that hit it.
	protected static string LayerReport(IEntity door, vector mins, vector maxs)
	{
		array<string> names = {"Main", "Projectile", "Interaction", "BuildingFireView", "FireGeometry", "VehicleCast", "Vehicle", "Static", "Default", "Dynamic", "Camera", "Debris", "all"};
		array<int> masks = {EPhysicsLayerPresets.Main, EPhysicsLayerPresets.Projectile, EPhysicsLayerPresets.Interaction, EPhysicsLayerPresets.BuildingFireView, EPhysicsLayerDefs.FireGeometry, EPhysicsLayerDefs.VehicleCast, EPhysicsLayerDefs.Vehicle, EPhysicsLayerDefs.Static, EPhysicsLayerDefs.Default, EPhysicsLayerDefs.Dynamic, EPhysicsLayerDefs.Camera, EPhysicsLayerDefs.Debris, -1};
		vector mid = (mins + maxs) * 0.5;
		vector half = (maxs - mins) * 0.5;
		array<string> sides = {"+x", "-x", "+z", "-z"};
		string report = "layers[";
		foreach (int m, int mask : masks)
		{
			string hits;
			for (int s = 0; s < 4; s++)
			{
				vector far = mid;
				if (s == 0)
					far[0] = maxs[0] + 2;
				else if (s == 1)
					far[0] = mins[0] - 2;
				else if (s == 2)
					far[2] = maxs[2] + 2;
				else
					far[2] = mins[2] - 2;

				TraceParam trace = new TraceParam();
				trace.Start = door.CoordToParent(far);
				trace.End = door.CoordToParent(mid);
				trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
				trace.LayerMask = mask;
				trace.Exclude = null;
				float fraction = GetGame().GetWorld().TraceMove(trace, null);
				if (fraction >= 1 || !trace.TraceEnt || GenericTerrainEntity.Cast(trace.TraceEnt))
					continue;

				IEntity root = trace.TraceEnt;
				IEntity top = door;
				while (top.GetParent())
					top = top.GetParent();

				while (root.GetParent())
					root = root.GetParent();

				// `+x` the door leaf itself; `+x~` another part of its barrier (the post).
				if (trace.TraceEnt == door)
					hits += sides[s];
				else if (root == top)
					hits += sides[s] + "~";
			}

			if (hits.IsEmpty())
				hits = "-";

			report += names[m] + ":" + hits + " ";
		}

		return report + "half=" + Round1(half[0]) + "x" + Round1(half[1]) + "x" + Round1(half[2]) + "]";
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

		// The final approach: a road drive creeps its off-road tail; `approach=1` makes a straight
		// drive creep the whole way, `approach=off` keeps the old drive even when it gets stuck.
		string approach = Option(tokens, "approach");
		s_bApproachTail = s_bRoad || approach == "1" || approach == "on";
		s_bApproachFallback = approach != "off" && approach != "0";
		if (!s_bApproachFallback)
			s_bApproachTail = false;

		s_fCreepKmh = CREEP_KMH;
		string creep = Option(tokens, "creep");
		if (!creep.IsEmpty())
			s_fCreepKmh = Math.Clamp(creep.ToFloat(), 3, 25);

		// Close enough: within `near` of the goal with the way on shut ends the drive `close`.
		s_fNear = Math.Max(s_fTolerance * NEAR_TOL_SHARE, NEAR_MIN_M);
		string near = Option(tokens, "near");
		if (!near.IsEmpty())
			s_fNear = Math.Max(near.ToFloat(), s_fTolerance);

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
		s_fRearM = Math.Max(-mins[2], 0.5);
		s_fHalfWidthM = Math.Max((maxs[0] - mins[0]) * 0.5 - 0.15, 0.3);
		s_fBottomM = mins[1];
		s_fTopM = maxs[1];
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

		// The off-road tail is the last stretch, from the last road point (or yard waypoint) to the goal.
		s_fTailFrom = 0;
		for (int j = 1; j < raw.Count() - 1; j++)
			s_fTailFrom += vector.DistanceXZ(raw[j - 1], raw[j]);

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
		s_fGateAtMs = 0;
		s_fGateM = -1;
		s_sGateFound = "";
		s_bGateStop = false;
		s_bGateReverse = false;
		s_iGateBacks = 0;
		s_fBackupLimitM = BACKUP_M;
		s_bApproach = false;
		s_bEscape = false;
		s_iEscapes = 0;
		s_fReplanAtMs = -100000;
		s_iTurnPhase = 0;
		s_iTurnAbouts = 0;
		s_iApproaches = 0;
		s_iApproachBackups = 0;
		s_iStrikes = 0;
		s_fApproachDriven = 0;
		s_fFanAtMs = 0;
		s_bFanHeld = false;
		LzAvoid().Clear();
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
		if (s_iApproaches > 0)
			line += string.Format(" approach=%1m creepbackups=%2 near=%3m", Math.Round(s_fDriven - s_fApproachDriven), s_iApproachBackups, Round1(s_fNear));

		if (s_iEscapes > 0)
			line += string.Format(" escapes=%1", s_iEscapes);

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
		s_bApproach = false;
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
				// At rest short of a shut gate: too close to its bar, back off first.
				if (s_bGateStop && GateStopDone(car, at, now))
				{
					Push(car);
					return;
				}

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
		else if (!s_bApproach && s_fBackupUntilMs <= 0 && now - s_fLegStartMs > s_fLegTimeoutS * 1000)
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
			if (now < s_fBackupUntilMs && vector.DistanceXZ(at, s_vBackupFrom) < s_fBackupLimitM)
			{
				// The reverse pedal only up to a walking pace: a truck backing at 20 km/h down a yard
				// hits something hard enough to knock its driver out.
				s_fThrust = 0;
				s_fBrake = 1;
				if (-ForwardKmh(car, forward) > BACKUP_MAX_KMH)
					s_fBrake = 0;

				s_fHand = 0;
				s_fSteer = s_fBackupSteer;
				Push(car);
				return;
			}

			EndBackup(car, at, now);
			if (s_bReplanning || s_bStopping)
			{
				StopInputs(car, speed);
				Push(car);
				return;
			}
		}

		// A shut gate on the route ahead: slow for it and stop GATE_STOP_M short.
		if (GateGuard(car, at, forward, speed, now, toGoal))
		{
			Push(car);
			return;
		}

		// A route that starts far off the nose of a long vehicle: a three-point turn when its body has no room to swing
		// round in one arc (the approach has its own heading rules).
		if (!s_bApproach && TurnAbout(car, at, forward, right, speed, now))
		{
			Push(car);
			return;
		}

		// Still rolling backwards after a back-up (or down a bank): the brake pedal is the reverse
		// pedal in an automatic box, so only the handbrake stops it.
		if (-ForwardKmh(car, forward) > ROLLBACK_KMH)
		{
			s_fThrust = 0;
			s_fBrake = 0;
			s_fHand = 1;
			s_fSteer = 0;
			s_fStillSinceMs = 0;
			s_fProgressAtMs = Math.Max(s_fProgressAtMs, now - NO_PROGRESS_S * 500); // lf-float: world time in ms, harness only
			Push(car);
			return;
		}

		// The off-road tail (or a creep the stuck test started): its own steering and stuck rules.
		if (!s_bApproach && s_bApproachTail && s_fProgress >= s_fTailFrom - 1 && s_fRouteLength - s_fTailFrom > 3)
			BeginApproach(at, now, "tail");

		if (s_bApproach)
		{
			ApproachTick(car, at, transform, speed, now, toGoal);
			return;
		}

		// A road drive that has left its route (pushed off it, or took a wrong branch) asks again.
		float offRoute = vector.DistanceXZ(at, PointAt(s_fProgress));
		// (not again for REPLAN_GAP_S after the last ask: a refused answer used to burn all of them in a second)
		if (s_bRoad && offRoute > OFF_ROUTE_M && s_iReplans < MAX_REPLANS && now - s_fReplanAtMs > REPLAN_GAP_S * 1000)
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
		want = Math.Min(want, s_fGateCapKmh);

		float climb = ClimbDeg(transform);
		if (speed < want)
		{
			s_fThrust = Math.Clamp((want - speed) / 10 + 0.2, 0, 1);
			s_fBrake = 0;
			// Up a bank: more pedal, first gear held, so a slow climb does not die on the slope.
			if (climb > CLIMB_DEG && speed < 15)
			{
				s_fThrust = Math.Max(s_fThrust, Math.Clamp(0.6 + climb / 30, 0, 1));
				HoldLowGear(car);
			}
		}
		else
		{
			s_fThrust = 0;
			s_fBrake = Math.Clamp((speed - want) / 8, 0, 1);
			// A brake held near a standstill reverses an automatic box: let it roll instead.
			if (speed < 4)
				s_fBrake = 0;
		}

		// Stuck: the pedal is down and the car stands, or the route has not moved on for a while. A car
		// still crawling up a steep bank is climbing, not stuck.
		if (s_fThrust > 0.2 && speed < StillKmh(climb))
		{
			if (s_fStillSinceMs <= 0)
				s_fStillSinceMs = now;
		}
		else
		{
			s_fStillSinceMs = 0;
		}

		float noProgressS = NO_PROGRESS_S;
		if (climb > CLIMB_DEG && speed >= CLIMB_STILL_KMH)
			noProgressS = CLIMB_NO_PROGRESS_S;

		string reason;
		bool starting = now - s_fStartMs < START_GRACE_S * 1000;
		if (s_fStillSinceMs > 0 && now - s_fStillSinceMs > StuckS(climb) * 1000 && !starting)
			reason = "stalled";
		else if (now - s_fProgressAtMs > noProgressS * 1000)
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

		// A shut gate across the way: a player gets out and opens it. A car against its bar (the bar stays
		// shut under a car) backs off first; one with room ends the drive.
		if (blocker.StartsWith("shut_door") && IsGateOnWay(forward, vector.DistanceXZ(at, s_vGoal)))
		{
			s_sGateFound = blocker;
			if (BeginGateReverse(car, at, now, s_fShutDoorM))
				return true;

			Finish("stuck");
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
		s_fBackupLimitM = BACKUP_M;
		s_sStatus = "driving";
		// Backed off a shut gate: stand still here, then the drive ends at it.
		if (s_bGateReverse)
		{
			s_bGateReverse = false;
			s_bGateStop = true;
			s_bStopping = true;
			s_sResult = "stuck";
			s_sStatus = "braking";
			return;
		}

		if (s_bApproach)
		{
			s_fProgressAtMs = now;
			s_fStillSinceMs = 0;
			s_fFanAtMs = 0;
			s_bFanHeld = false;
			return;
		}

		// Stuck twice near the end (a tree on a forest track, a bank, a yard): creep the rest.
		if (s_bApproachFallback && s_iBackups >= 2 && s_fRouteLength - Project(at, 1000) <= APPROACH_REACH_M)
		{
			BeginApproach(at, now, "stuck_" + s_sWhy);
			return;
		}

		// Stuck twice far from the end (a long vehicle among sheds at the start of a long route): creep a local
		// stretch of the route, sized by the vehicle's own length, and take the normal drive up again after it. A road
		// drive asks the server for a new route first (a wrong branch is the commoner snag); the escape is the next try.
		if (s_bApproachFallback && s_iBackups >= 2 && s_iEscapes < MAX_ESCAPES && (!s_bRoad || s_iReplans > 0))
		{
			BeginEscape(at, now, "stuck_" + s_sWhy);
			return;
		}

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
	// Turn about
	//------------------------------------------------------------------------------------------------
	//! The metres (up to TURN_LEG_MAX_M) the body can drive the smallest-radius circle with the wheels locked toward
	//! `lock`, forward or in reverse, before a corner of it meets something.
	protected static float TurnRoom(IEntity car, vector at, float fwdYaw, float lock, float dirSign)
	{
		float room = 0;
		for (float m = TURN_ROOM_STEP_M; m <= TURN_LEG_MAX_M + 0.01; m += TURN_ROOM_STEP_M)
		{
			if (!SweepArc(car, at, fwdYaw, lock, m, dirSign))
				break;

			room = m;
		}

		return room;
	}

	//------------------------------------------------------------------------------------------------
	//! A three-point turn: the vehicle stands (or crawls) with the way on more than TURN_ABOUT_DEG off its nose and a
	//! full-lock arc onto it would run its body into something. Forward and reverse legs follow, each as long as the
	//! body's real outline (bounds, wheelbase) has room for on the smallest turn radius, wheels locked toward the way
	//! on going forward and away from it in reverse, until the way on is within TURN_DONE_DEG of the nose. True while it
	//! drives (the inputs are set), false when the normal pursuit should steer.
	protected static bool TurnAbout(IEntity car, vector at, vector forward, vector right, float speed, float now)
	{
		float fwdYaw = Math.Atan2(forward[0], forward[2]);
		float v = speed / 3.6;
		float look = Math.Clamp(LOOK_BASE_M + LOOK_GAIN_S * v, LOOK_MIN_M, LOOK_MAX_M);
		vector toTarget = PointAt(s_fProgress + look) - at;
		toTarget[1] = 0;
		float alpha = Math.Atan2(vector.Dot(toTarget, right), vector.Dot(toTarget, forward));
		float side = 1;
		if (alpha < 0)
			side = -1;

		if (s_iTurnPhase == 0)
		{
			if (s_iTurnAbouts >= TURN_MAX_ABOUTS || speed > TURN_START_KMH || Math.AbsFloat(alpha) < TURN_ABOUT_DEG * Math.DEG2RAD)
				return false;

			// The room for a plain full-lock arc: with it the normal pursuit turns the car.
			if (SweptClear(car, at, fwdYaw, fwdYaw + alpha))
				return false;

			s_iTurnAbouts++;
			s_iTurnLegs = 0;
			s_iTurnLast = 0;
			s_iTurnPhase = TURN_PHASE_STOP;
			LF_Debug.Info(FEATURE, string.Format("drive turn about at %1: the way on is %2 degrees off the nose and the body's turn is not clear (wheelbase %3, radius %4, body %5 x %6 m)", at, Math.Round(alpha * Math.RAD2DEG), Round1(s_fWheelbase), Round1(TurnRadius()), Round1(s_fFrontM + s_fRearM), Round1(s_fHalfWidthM * 2)));
		}

		if (s_iTurnPhase == TURN_PHASE_STOP)
		{
			StopInputs(car, speed);
			if (speed >= 1)
				return true;

			if (Math.AbsFloat(alpha) < TURN_DONE_DEG * Math.DEG2RAD || s_iTurnLegs >= TURN_MAX_LEGS)
			{
				EndTurnAbout(at, now, "done");
				return false;
			}

			// The next leg: the other direction than the last when it has room, else whichever has.
			float fwdRoom = TurnRoom(car, at, fwdYaw, side, 1);
			float revRoom = TurnRoom(car, at, fwdYaw, -side, -1);
			int kind = TURN_PHASE_FORWARD;
			float room = fwdRoom;
			if (revRoom > fwdRoom)
			{
				kind = TURN_PHASE_REVERSE;
				room = revRoom;
			}

			if (s_iTurnLast == TURN_PHASE_FORWARD && revRoom >= TURN_ROOM_MIN_M)
			{
				kind = TURN_PHASE_REVERSE;
				room = revRoom;
			}
			else if (s_iTurnLast == TURN_PHASE_REVERSE && fwdRoom >= TURN_ROOM_MIN_M)
			{
				kind = TURN_PHASE_FORWARD;
				room = fwdRoom;
			}

			// No room by the body's outline either way (it is a worst case at the smallest radius): a short leg at a
			// walking pace still, the other way round than the last, and the stall test ends it against the obstacle.
			if (room < TURN_ROOM_MIN_M)
			{
				kind = TURN_PHASE_FORWARD;
				if (s_iTurnLast == TURN_PHASE_FORWARD)
					kind = TURN_PHASE_REVERSE;

				room = TURN_PROBE_M + 0.3;
			}

			s_fTurnLegM = Math.Max(Math.Min(room, TURN_LEG_MAX_M) - 0.3, TURN_PROBE_M);
			s_fTurnLock = side;
			if (kind == TURN_PHASE_REVERSE)
				s_fTurnLock = -side;

			s_vTurnFrom = at;
			s_fTurnStillMs = 0;
			s_fTurnLegAtMs = now;
			s_iTurnLegs++;
			s_iTurnPhase = kind;
			string legName = "forward";
			if (kind == TURN_PHASE_REVERSE)
				legName = "reverse";

			LF_Debug.Info(FEATURE, string.Format("drive turn about leg %1 at %2 heading %3: %4 for %5 m (room forward %6 m, reverse %7 m, way on %8 degrees off)", s_iTurnLegs, at, Math.Round(fwdYaw * Math.RAD2DEG), legName, Round1(s_fTurnLegM), Round1(fwdRoom), Round1(revRoom), Math.Round(alpha * Math.RAD2DEG)));
		}

		// A leg under way.
		bool legOver = vector.DistanceXZ(at, s_vTurnFrom) >= s_fTurnLegM || Math.AbsFloat(alpha) < TURN_DONE_DEG * Math.DEG2RAD;
		if (!legOver && now - s_fTurnLegAtMs > 2000)
		{
			// Standing against something with the pedal down: the leg is over.
			if (speed < STILL_KMH)
			{
				if (s_fTurnStillMs <= 0)
					s_fTurnStillMs = now;
				else if (now - s_fTurnStillMs > TURN_STALL_S * 1000)
					legOver = true;
			}
			else
			{
				s_fTurnStillMs = 0;
			}
		}

		if (legOver)
		{
			s_iTurnLast = s_iTurnPhase;
			s_iTurnPhase = TURN_PHASE_STOP;
			StopInputs(car, speed);
			return true;
		}

		s_fHand = 0;
		s_fSteer = s_fSteerSign * s_fTurnLock;
		if (s_iTurnPhase == TURN_PHASE_FORWARD)
		{
			s_fBrake = 0;
			s_fThrust = 0;
			if (speed < TURN_KMH)
				s_fThrust = Math.Clamp((TURN_KMH - speed) / 6 + 0.3, 0, 1);
		}
		else
		{
			// The brake pedal at a standstill is the reverse pedal; held to a walking pace only.
			s_fThrust = 0;
			s_fBrake = 1;
			if (-ForwardKmh(car, forward) > BACKUP_MAX_KMH)
				s_fBrake = 0;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void EndTurnAbout(vector at, float now, string result)
	{
		LF_Debug.Info(FEATURE, string.Format("drive turn about %1 at %2 after %3 leg(s)", result, at, s_iTurnLegs));
		s_iTurnPhase = 0;
		s_iTurnLast = 0;
		Resume(now, Project(at, 1000));
	}

	//------------------------------------------------------------------------------------------------
	// Final approach
	//------------------------------------------------------------------------------------------------
	//! Leaves the normal drive for the creep: the rest of the route at walking pace, the heading picked
	//! from a fan of short rays round the way on, its own stuck rules and back-ups.
	protected static void BeginApproach(vector at, float now, string why)
	{
		s_bApproach = true;
		s_iApproaches++;
		s_fApproachDriven = s_fDriven;
		s_fProgress = Project(at, 1000);
		s_fApproachProgress = s_fProgress;
		s_fApproachBest = vector.DistanceXZ(at, ApproachGoal());
		s_sTrackMat = SurfaceAt(at);
		s_fProgressAtMs = now;
		s_fStillSinceMs = 0;
		s_fFanAtMs = 0;
		s_bFanHeld = false;
		float left = Math.Max(s_fRouteLength - s_fProgress, s_fApproachBest);
		s_fApproachEndMs = now + (60 + left * 2.5) * 1000;
		// The creep has its own budget; the drive's must not cut it off first (a tail of a few hundred metres at 9 km/h).
		s_fEndMs = Math.Max(s_fEndMs, s_fApproachEndMs); // lf-float: world time in ms, harness only
		LF_Debug.Info(FEATURE, string.Format("drive final approach from %1 (%2): %3 m of route left, %4 m straight to the goal, creep %5 km/h, close within %6 m, ground %7", at, why, Math.Round(s_fRouteLength - s_fProgress), Math.Round(s_fApproachBest), s_fCreepKmh, Round1(s_fNear), s_sTrackMat));
	}

	//------------------------------------------------------------------------------------------------
	//! What the creep is heading for: the drive's goal, or in an escape the point of the route it must clear.
	protected static vector ApproachGoal()
	{
		if (s_bEscape)
			return s_vEscapeGoal;

		return s_vGoal;
	}

	//------------------------------------------------------------------------------------------------
	//! The creep's "close enough to the goal to stop here" distance: none in an escape (it never ends the drive).
	protected static float ApproachNear()
	{
		if (s_bEscape)
			return 0;

		return s_fNear;
	}

	//------------------------------------------------------------------------------------------------
	//! A creep of a local stretch of a long route, the stretch sized by the vehicle's own length (a truck needs more
	//! room to swing between obstacles than a car): the creep's goal is a point of the route that far on, the normal
	//! drive takes over once the vehicle is past it (or the creep fails).
	protected static void BeginEscape(vector at, float now, string why)
	{
		float length = s_fFrontM + s_fRearM;
		float ahead = Math.Max(ESCAPE_MIN_M, length * ESCAPE_LENGTHS);
		float from = Project(at, 1000);
		s_fEscapeUntil = Math.Min(from + ahead, s_fRouteLength);
		s_vEscapeGoal = PointAt(s_fEscapeUntil);
		s_vEscapeGoal[1] = 0;
		s_bEscape = true;
		s_iEscapes++;
		BeginApproach(at, now, "escape_" + why);
		// BeginApproach sized the budget on the whole route: an escape only needs its own stretch.
		s_fApproachEndMs = now + (60 + (s_fEscapeUntil - from) * 4) * 1000;
		s_fEndMs = Math.Max(s_fEndMs, s_fApproachEndMs); // lf-float: world time in ms, harness only
	}

	//------------------------------------------------------------------------------------------------
	//! The escape is over (the vehicle is past its stretch, or the creep could not make it): the normal drive resumes
	//! from where the vehicle stands, its back-up count fresh when the escape got through.
	protected static void EndEscape(vector at, float now, string result)
	{
		LF_Debug.Info(FEATURE, string.Format("drive escape %1 at %2 (%3 m of route cleared, %4 of %5)", result, at, Math.Round(Project(at, 1000) - s_fApproachProgress), s_iEscapes, MAX_ESCAPES));
		s_bApproach = false;
		s_bEscape = false;
		s_bFanHeld = false;
		s_fFanAtMs = 0;
		s_iStrikes = 0;
		s_fBackupLimitM = BACKUP_M;
		s_sStatus = "driving";
		if (result == "through")
			s_iBackups = 0;

		Resume(now, Project(at, 1000));
	}

	//------------------------------------------------------------------------------------------------
	//! One frame of the creep.
	protected static void ApproachTick(IEntity car, vector at, vector transform[4], float speed, float now, float toGoal)
	{
		vector forward = Vector(transform[2][0], 0, transform[2][2]).Normalized();
		vector right = Vector(transform[0][0], 0, transform[0][2]).Normalized();
		if (s_bEscape)
		{
			// The distance that counts is the one to the end of the escape's stretch.
			toGoal = vector.DistanceXZ(at, s_vEscapeGoal);
			if (s_fProgress >= s_fEscapeUntil - ESCAPE_DONE_M || toGoal <= ESCAPE_DONE_M)
			{
				EndEscape(at, now, "through");
				return;
			}
		}

		if (now > s_fApproachEndMs)
		{
			ApproachEnd(car, forward, toGoal, "creep_budget_over");
			return;
		}

		bool moved = false;
		if (toGoal < s_fApproachBest - 1)
		{
			s_fApproachBest = toGoal;
			moved = true;
		}

		if (s_fProgress > s_fApproachProgress + 1.5)
		{
			s_fApproachProgress = s_fProgress;
			moved = true;
		}

		if (moved)
			s_fProgressAtMs = now;

		// The way on: the route point a few metres ahead (on the tail, the goal itself).
		vector aim = PointAt(s_fProgress + APPROACH_LOOK_M);
		if (vector.DistanceXZ(aim, at) < 1)
			aim = ApproachGoal();

		float aimYaw = Math.Atan2(aim[0] - at[0], aim[2] - at[2]);
		if (s_fFanAtMs <= 0 || now - s_fFanAtMs > FAN_EVERY_MS)
		{
			Fan(car, at, forward, now, toGoal, aimYaw);
			s_fFanAtMs = now;
		}

		// Close enough: near the goal with something right in front of the nose between it and the car.
		if (!s_bEscape && toGoal <= s_fNear && s_fGoalClear < Math.Min(3, toGoal - s_fFrontM))
		{
			ApproachEnd(car, forward, toGoal, "near_goal_way_shut");
			return;
		}

		if (!s_bFanOpen)
		{
			ApproachStuck(car, at, forward, right, now, toGoal, "boxed_in");
			return;
		}

		vector dir = Vector(Math.Sin(s_fFanYaw), 0, Math.Cos(s_fFanYaw));
		vector toTarget = dir * 6;
		float alpha = Math.Atan2(vector.Dot(toTarget, right), vector.Dot(toTarget, forward));
		float delta = Math.Atan2(2 * s_fWheelbase * Math.Sin(alpha), 6);
		if (Math.AbsFloat(alpha) > Math.PI * 0.5)
		{
			delta = MAX_STEER_DEG * Math.DEG2RAD;
			if (alpha < 0)
				delta = -delta;
		}

		s_fSteer = s_fSteerSign * Math.Clamp(delta * Math.RAD2DEG / MAX_STEER_DEG, -1, 1);
		s_fHand = 0;

		float want = s_fCreepKmh;
		if (s_fFanClear < 3 || Math.AbsFloat(alpha) * Math.RAD2DEG > 45)
			want = CREEP_SLOW_KMH;

		float stopKmh = Math.Sqrt(2 * DECEL * Math.Max(toGoal - s_fTolerance * 0.5, 0)) * 3.6;
		want = Math.Min(want, Math.Max(stopKmh, 3));
		want = Math.Min(want, s_fGateCapKmh);

		float climb = ClimbDeg(transform);
		if (speed < want)
		{
			s_fThrust = Math.Clamp((want - speed) / 6 + 0.25, 0, 1);
			s_fBrake = 0;
			if (climb > CLIMB_DEG)
			{
				s_fThrust = Math.Max(s_fThrust, Math.Clamp(0.6 + climb / 30, 0, 1));
				HoldLowGear(car);
			}
		}
		else
		{
			s_fThrust = 0;
			s_fBrake = 0;
			if (speed >= 3)
				s_fBrake = Math.Clamp((speed - want) / 5, 0, 1);
		}

		if (s_fThrust > 0.2 && speed < StillKmh(climb))
		{
			if (s_fStillSinceMs <= 0)
				s_fStillSinceMs = now;
		}
		else
		{
			s_fStillSinceMs = 0;
		}

		if (s_fStillSinceMs > 0 && now - s_fStillSinceMs > StuckS(climb) * 1000)
		{
			ApproachStuck(car, at, forward, right, now, toGoal, "stalled");
			return;
		}

		if (now - s_fProgressAtMs > APPROACH_NO_PROGRESS_S * 1000)
		{
			ApproachStuck(car, at, forward, right, now, toGoal, "no_progress");
			return;
		}

		Push(car);
	}

	//------------------------------------------------------------------------------------------------
	//! The creep ends short of the goal: `close` within `near` (a reachable spot), else `stuck`.
	protected static void ApproachEnd(IEntity car, vector forward, float toGoal, string reason)
	{
		s_sWhy = reason + "_" + Blocker(car, forward);
		// An escape that could not clear its stretch gives the drive back to the normal logic (more back-ups, a new route).
		if (s_bEscape)
		{
			EndEscape(car.GetOrigin(), GetGame().GetWorld().GetWorldTime(), "failed_" + reason); // lf-float: world time in ms, harness only
			return;
		}

		if (toGoal <= s_fNear)
		{
			s_bStopping = true;
			s_sResult = "close";
			s_sStatus = "braking";
			StopInputs(car, SpeedKmh(car));
			Push(car);
			return;
		}

		Finish("stuck");
	}

	//------------------------------------------------------------------------------------------------
	//! A creep stuck spot: close enough ends it; otherwise the heading it was on is avoided for a while
	//! and the car backs up with the wheels turned so the nose swings toward the way on (every third
	//! try the other way round), downhill on a bank it could not climb.
	protected static void ApproachStuck(IEntity car, vector at, vector forward, vector right, float now, float toGoal, string reason)
	{
		if (!s_sStuckAt.IsEmpty())
			s_sStuckAt += "|";

		s_sStuckAt += string.Format("%1,%2", Math.Round(at[0]), Math.Round(at[2]));
		string blocker = Blocker(car, forward);
		LF_Debug.Info(FEATURE, string.Format("drive creep stuck at %1 (%2, %3) %4 m from the goal, fan %5", at, reason, blocker, Round1(toGoal), FanLine(car, at, toGoal)));
		if (blocker == "out_of_fuel" || (blocker.StartsWith("shut_door") && IsGateOnWay(forward, toGoal)))
		{
			s_sWhy = "creep_" + reason + "_" + blocker;
			if (blocker == "out_of_fuel")
			{
				Finish("out_of_fuel");
				return;
			}

			s_sGateFound = blocker;
			if (BeginGateReverse(car, at, now, s_fShutDoorM))
			{
				s_bFanHeld = false;
				Push(car);
				return;
			}

			Finish("stuck");
			return;
		}

		// A strike is a back-up with under STRIKE_GAIN_M won since the last one that counted.
		if (s_iStrikes == 0 || s_fApproachBest < s_fStrikeBest - STRIKE_GAIN_M)
		{
			s_iStrikes = 0;
			s_fStrikeBest = s_fApproachBest;
		}

		s_iStrikes++;
		if (toGoal <= ApproachNear() || s_iStrikes > MAX_APPROACH_STRIKES)
		{
			ApproachEnd(car, forward, toGoal, "creep_" + reason);
			return;
		}

		s_sWhy = "creep_" + reason + "_" + blocker;
		s_iApproachBackups++;
		float heading = Math.Atan2(forward[0], forward[2]);
		if (s_bFanHeld)
			heading = s_fFanYaw;

		array<float> avoid = LzAvoid();
		avoid.Insert(heading);
		avoid.Insert(now + AVOID_S * 1000);

		vector toGoalDir = ApproachGoal() - at;
		toGoalDir[1] = 0;
		float side = 1;
		if (vector.Dot(toGoalDir, right) < 0)
			side = -1;

		if (s_iApproachBackups % 3 == 0)
			side = -side;

		s_fBackupSteer = -s_fSteerSign * side * BACKUP_LOCK;
		s_vBackupFrom = at;
		s_fBackupUntilMs = now + s_fBackupS * 1000;
		s_fStillSinceMs = 0;
		s_bFanHeld = false;
		s_sStatus = "backing";
		s_fThrust = 0;
		s_fBrake = 1;
		s_fHand = 0;
		s_fSteer = s_fBackupSteer;
		Push(car);
	}

	//------------------------------------------------------------------------------------------------
	//! Picks the creep heading: headings FAN_STEP_DEG apart either side of the way on; one is open when
	//! the rays along it are clear FAN_CLEAR_M past the nose and the ground does not climb past
	//! CLIMB_MAX_DEG. The cheapest open one wins: off the way on, a sharp turn from the nose, a steep
	//! climb, a heading that got stuck lately, a short clear all cost; the held heading gets a bonus.
	protected static void Fan(IEntity car, vector at, vector forward, float now, float toGoal, float aimYaw)
	{
		float fwdYaw = Math.Atan2(forward[0], forward[2]);
		float step = FAN_STEP_DEG * Math.DEG2RAD;
		float need = Math.Clamp(toGoal - 0.5, 1.5, FAN_CLEAR_M);
		float reach = Math.Max(Math.Min(FAN_REACH_M, toGoal + 1), need);
		array<float> costs = {};
		array<float> yaws = {};
		array<float> clears = {};
		for (int k = -FAN_STEPS; k <= FAN_STEPS; k++)
		{
			float yaw = aimYaw + k * step;
			vector dir = Vector(Math.Sin(yaw), 0, Math.Cos(yaw));
			float clear = ClearAlong(car, at, dir, reach);
			// A bank too steep to climb or a drop into a ditch shuts the heading where it starts.
			float lead = Math.Max(s_fFrontM * 0.85, 1.2);
			float shutAt = GroundShutAt(at, dir, lead + reach);
			if (shutAt >= 0)
				clear = Math.Min(clear, Math.Max(shutAt - lead, 0));

			if (k == 0)
				s_fGoalClear = clear;

			if (clear < need)
				continue;

			float slope = SlopeDeg(at, dir);

			float cost = Math.AbsInt(k) + 0.35 * AngleDiff(yaw, fwdYaw) / step + Math.Max(slope - CLIMB_SOFT_DEG, 0) * 0.5 + AvoidPenalty(yaw, now) + (reach - clear) * 0.15;
			if (s_bFanHeld && AngleDiff(yaw, s_fFanYaw) < step * 0.6)
				cost -= 0.75;

			// A road or track under the heading beats a line through the trees beside it.
			if (IsTrackSurface(SurfaceAt(at + dir * FAN_SURFACE_M)) || IsTrackSurface(SurfaceAt(at + dir * FAN_SURFACE_M * 2)))
				cost -= FAN_SURFACE_BONUS;

			costs.Insert(cost);
			yaws.Insert(yaw);
			clears.Insert(clear);
		}

		// The cheapest heading whose turn the whole body can make: the body is swept along the arc it drives to swing
		// onto the heading (its real length, width and turning radius), so a long truck does not commit to a line that
		// only the straight rays of its nose like. SWEEP_CANDIDATES headings are tried, cheapest first.
		bool open = false;
		float bestYaw = aimYaw;
		float bestClear = 0;
		// When no tried heading has a clear sweep the cheapest one is still taken (the sweep is the body's outline
		// at the smallest radius, a worst case): the stuck handling then backs up and tries again.
		bool haveFirst = false;
		float firstYaw = aimYaw;
		float firstClear = 0;
		for (int tried = 0; tried < SWEEP_CANDIDATES && !open; tried++)
		{
			int pick = -1;
			float best = 1000000;
			for (int i = 0; i < costs.Count(); i++)
			{
				if (costs[i] < best)
				{
					best = costs[i];
					pick = i;
				}
			}

			if (pick < 0)
				break;

			costs[pick] = 1000001;
			if (!haveFirst)
			{
				haveFirst = true;
				firstYaw = yaws[pick];
				firstClear = clears[pick];
			}

			if (!SweptClear(car, at, fwdYaw, yaws[pick]))
				continue;

			open = true;
			bestYaw = yaws[pick];
			bestClear = clears[pick];
		}

		if (!open && haveFirst)
		{
			open = true;
			bestYaw = firstYaw;
			bestClear = firstClear;
		}

		s_bFanOpen = open;
		if (!open)
			return;

		s_fFanYaw = bestYaw;
		s_fFanClear = bestClear;
		s_bFanHeld = true;
	}

	//------------------------------------------------------------------------------------------------
	//! The smallest turn radius of the vehicle's reference point at full lock, from its wheelbase.
	protected static float TurnRadius()
	{
		return Math.Max(s_fWheelbase * TURN_RADIUS_PER_WHEELBASE, 3);
	}

	//------------------------------------------------------------------------------------------------
	//! A heading as a unit vector on the ground.
	protected static vector YawDir(float yaw)
	{
		return Vector(Math.Sin(yaw), 0, Math.Cos(yaw));
	}

	//------------------------------------------------------------------------------------------------
	//! The car's right-hand side on the ground for a heading.
	protected static vector YawRight(float yaw)
	{
		return Vector(Math.Cos(yaw), 0, -Math.Sin(yaw));
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the whole body clears every obstacle while it turns from the nose (`fwdYaw`) onto the heading `yaw`:
	//! the reference point drives the arc of the smallest turn radius (a gentler turn needs less room, so this is the
	//! worst case), the body's corners and side midpoints are traced from each pose to the next at a low and a bumper
	//! height. The straight run after the turn is the straight rays' business (ClearAlong). A heading within a few
	//! degrees of the nose needs no turn.
	protected static bool SweptClear(IEntity car, vector at, float fwdYaw, float yaw)
	{
		float diff = yaw - fwdYaw;
		while (diff > Math.PI)
			diff -= 2 * Math.PI;

		while (diff < -Math.PI)
			diff += 2 * Math.PI;

		if (Math.AbsFloat(diff) < SWEEP_MIN_DEG * Math.DEG2RAD)
			return true;

		float sgn = 1;
		if (diff < 0)
			sgn = -1;

		// A turn past a right angle is driven as a right angle: the rest is a straight run.
		float turn = Math.Min(Math.AbsFloat(diff), Math.PI * 0.5);
		return SweepArc(car, at, fwdYaw, sgn, TurnRadius() * turn, 1);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the body clears everything while the car drives `arcM` metres of the smallest-radius circle with its
	//! wheels locked toward `lock` (+1 right, -1 left), forward (`dirSign` 1) or in reverse (-1): the body's corners and
	//! side midpoints are traced from each pose to the next, at a low and a bumper height, from the car's own bounds.
	protected static bool SweepArc(IEntity car, vector at, float fwdYaw, float lock, float arcM, float dirSign)
	{
		float radius = TurnRadius();
		float turn = dirSign * arcM / radius;
		int steps = Math.Max(Math.Ceil(arcM / SWEEP_STEP_M), 2);
		vector center = at + YawRight(fwdYaw) * (radius * lock);
		center[1] = 0;
		float halfWidth = s_fHalfWidthM + SWEEP_MARGIN_M;
		// Body points in the car's frame: (sideways, forward). Both ends, and the middle of each side for a long body.
		array<float> localX = {-halfWidth, halfWidth, -halfWidth, halfWidth, -halfWidth, halfWidth};
		array<float> localZ = {s_fFrontM, s_fFrontM, -s_fRearM, -s_fRearM, 0, 0};
		BaseWorld world = GetGame().GetWorld();
		array<float> heights = {0.55, 1.2};
		array<vector> previous = {};
		for (int step = 0; step <= steps; step++)
		{
			float theta = turn * step / steps;
			float heading = fwdYaw + lock * theta;
			vector pose = center - YawRight(heading) * (radius * lock);
			vector forward = YawDir(heading);
			vector side = YawRight(heading);
			for (int p = 0; p < localX.Count(); p++)
			{
				vector corner = pose + side * localX[p] + forward * localZ[p];
				if (step > 0)
				{
					foreach (float h : heights)
					{
						TraceParam trace = new TraceParam();
						trace.Start = Vector(previous[p][0], world.GetSurfaceY(previous[p][0], previous[p][2]) + h, previous[p][2]);
						trace.End = Vector(corner[0], world.GetSurfaceY(corner[0], corner[2]) + h, corner[2]);
						trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
						trace.LayerMask = EPhysicsLayerPresets.Main;
						trace.Exclude = car;
						float fraction = world.TraceMove(trace, null);
						if (fraction >= 1)
							continue;

						// The ground itself is the slope test's business.
						if (!trace.TraceEnt || GenericTerrainEntity.Cast(trace.TraceEnt))
							continue;

						return false;
					}
				}

				if (step == 0)
					previous.Insert(corner);
				else
					previous[p] = corner;
			}
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The fan's clear metres per heading for the log, the way on in the middle.
	protected static string FanLine(IEntity car, vector at, float toGoal)
	{
		vector aim = PointAt(s_fProgress + APPROACH_LOOK_M);
		float aimYaw = Math.Atan2(aim[0] - at[0], aim[2] - at[2]);
		float reach = Math.Max(Math.Min(FAN_REACH_M, toGoal + 1), 1.5);
		string line;
		for (int k = -FAN_STEPS; k <= FAN_STEPS; k += 2)
		{
			float yaw = aimYaw + k * FAN_STEP_DEG * Math.DEG2RAD;
			vector dir = Vector(Math.Sin(yaw), 0, Math.Cos(yaw));
			line += string.Format("%1:%2/%3d ", k * FAN_STEP_DEG, Round1(ClearAlong(car, at, dir, reach)), Math.Round(SlopeDeg(at, dir)));
		}

		return line;
	}

	//------------------------------------------------------------------------------------------------
	//! Clear metres ahead of the car's body along `dir` (up to `reach`): rays at both sides of the car's
	//! track and its middle, at a low and a bumper height above the car's floor, rising and falling with
	//! the ground. Entities only: the ground itself is the slope test's business.
	protected static float ClearAlong(IEntity car, vector at, vector dir, float reach)
	{
		return ClearAlongFrom(car, at, dir, reach, Math.Max(s_fFrontM * 0.85, 1.2));
	}

	//------------------------------------------------------------------------------------------------
	//! ClearAlong with the rays starting `lead` metres from the car's origin along `dir` (the rear's own for a reverse).
	protected static float ClearAlongFrom(IEntity car, vector at, vector dir, float reach, float lead)
	{
		BaseWorld world = GetGame().GetWorld();
		vector perp = Vector(dir[2], 0, -dir[0]);
		float width = s_fHalfWidthM + 0.2;
		float groundHere = world.GetSurfaceY(at[0], at[2]);
		float floor = Math.Max(at[1] + s_fBottomM, groundHere);
		float clear = reach;
		// Low, bumper and roof height: a boom barrier's bar passes over the bumper and meets the windscreen.
		array<float> heights = {0.55, 1.2, Math.Max(s_fTopM - s_fBottomM - 0.3, 1.5)};
		for (int side = -1; side <= 1; side++)
		{
			vector from = at + dir * lead + perp * (side * width);
			vector to = from + dir * reach;
			float rise = world.GetSurfaceY(to[0], to[2]) - world.GetSurfaceY(from[0], from[2]);
			foreach (float h : heights)
			{
				// WORLD as well as ENTS: placed static props (a tin wall, a wreck) answer only to WORLD.
				// The ground itself is the profile test's business.
				TraceParam trace = new TraceParam();
				trace.Start = Vector(from[0], floor + h, from[2]);
				trace.End = Vector(to[0], floor + h + rise, to[2]);
				trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
				trace.LayerMask = EPhysicsLayerPresets.Main;
				trace.Exclude = car;
				float fraction = world.TraceMove(trace, null);
				if (fraction >= 1)
					continue;

				// The ground met by the low ray right at the bumper is a lip the wheels cannot take (a
				// road cut's bank); further on it is only a crest, the profile test's business.
				bool ground = !trace.TraceEnt || GenericTerrainEntity.Cast(trace.TraceEnt);
				if (ground && (h > 1 || fraction * reach > LIP_M))
					continue;

				if (fraction * reach < clear)
					clear = fraction * reach;
			}
		}

		return clear;
	}

	//------------------------------------------------------------------------------------------------
	//! How steeply the ground climbs (degrees, downhill negative) over SLOPE_ARM_M along `dir`.
	protected static float SlopeDeg(vector at, vector dir)
	{
		BaseWorld world = GetGame().GetWorld();
		vector ahead = at + dir * SLOPE_ARM_M;
		float rise = world.GetSurfaceY(ahead[0], ahead[2]) - world.GetSurfaceY(at[0], at[2]);
		return Math.Atan2(rise, SLOPE_ARM_M) * Math.RAD2DEG;
	}

	//------------------------------------------------------------------------------------------------
	//! Where along `dir` (metres from `at`, up to `length`) the ground first climbs past CLIMB_MAX_DEG
	//! or drops past DROP_MAX_DEG over a GROUND_WINDOW_M stretch (a bank, a ditch's wall), or -1.
	static float GroundShutAt(vector at, vector dir, float length)
	{
		BaseWorld world = GetGame().GetWorld();
		float up = Math.Tan(CLIMB_MAX_DEG * Math.DEG2RAD) * GROUND_WINDOW_M;
		float down = Math.Tan(DROP_MAX_DEG * Math.DEG2RAD) * GROUND_WINDOW_M;
		for (float s = GROUND_WINDOW_M; s <= length + 0.01; s += GROUND_STEP_M)
		{
			vector p = at + dir * s;
			vector q = at + dir * (s - GROUND_WINDOW_M);
			float rise = world.GetSurfaceY(p[0], p[2]) - world.GetSurfaceY(q[0], q[2]);
			if (rise > up || rise < -down)
				return s - GROUND_WINDOW_M;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	static float AngleDiff(float a, float b)
	{
		float d = a - b;
		while (d > Math.PI)
			d -= 2 * Math.PI;

		while (d < -Math.PI)
			d += 2 * Math.PI;

		return Math.AbsFloat(d);
	}

	//------------------------------------------------------------------------------------------------
	//! A road, track or path ground material (by its resource name) as against grass, forest floor, rock or sand.
	protected static bool IsTrackSurface(string material)
	{
		if (material.IsEmpty())
			return false;

		string lower = material;
		lower.ToLower();
		return lower.Contains("road") || lower.Contains("track") || lower.Contains("path") || lower.Contains("asphalt") || lower.Contains("gravel");
	}

	//------------------------------------------------------------------------------------------------
	//! The ground's material name at a point (the terrain's own: a road, a track, a forest floor), or empty.
	protected static string SurfaceAt(vector p)
	{
		BaseWorld world = GetGame().GetWorld();
		TraceParam trace = new TraceParam();
		trace.Start = Vector(p[0], world.GetSurfaceY(p[0], p[2]) + 2, p[2]);
		trace.End = Vector(p[0], trace.Start[1] - 5, p[2]);
		trace.Flags = TraceFlags.WORLD;
		trace.LayerMask = EPhysicsLayerPresets.Main;
		world.TraceMove(trace, null);
		return trace.TraceMaterial;
	}

	//------------------------------------------------------------------------------------------------
	protected static float AvoidPenalty(float yaw, float now)
	{
		array<float> avoid = LzAvoid();
		float penalty = 0;
		for (int i = 0; i + 1 < avoid.Count(); i += 2)
		{
			if (avoid[i + 1] > now && AngleDiff(yaw, avoid[i]) < AVOID_DEG * Math.DEG2RAD)
				penalty += 4;
		}

		return penalty;
	}

	//------------------------------------------------------------------------------------------------
	//! How far the nose points up (degrees, nose down negative).
	static float ClimbDeg(vector transform[4])
	{
		return Math.Asin(Math.Clamp(transform[2][1], -1, 1)) * Math.RAD2DEG;
	}

	//------------------------------------------------------------------------------------------------
	//! Below this speed with the pedal down the car stands; up a bank a crawl still counts as moving.
	static float StillKmh(float climb)
	{
		if (climb > CLIMB_DEG)
			return CLIMB_STILL_KMH;

		return STILL_KMH;
	}

	//------------------------------------------------------------------------------------------------
	static float StuckS(float climb)
	{
		if (climb > CLIMB_DEG)
			return CLIMB_STUCK_S;

		return STUCK_S;
	}

	//------------------------------------------------------------------------------------------------
	//! First gear held on a slow climb (an automatic box can shift up and lose the bank).
	protected static void HoldLowGear(IEntity car)
	{
		VehicleWheeledSimulation sim = VehicleWheeledSimulation.Cast(car.FindComponent(VehicleWheeledSimulation));
		if (sim && sim.GetGear() > EVehicleGearboxGear.FORWARD)
			sim.SetGear(EVehicleGearboxGear.FORWARD);
	}

	//------------------------------------------------------------------------------------------------
	protected static void RequestReplan(SCR_ChimeraCharacter character, vector at, float now)
	{
		s_bReplanning = true;
		s_sStatus = "routing";
		s_iReplanSeq++;
		s_iReplanId = -s_iReplanSeq;
		s_fReplanUntilMs = now + REPLAN_WAIT_S * 1000;
		s_fReplanAtMs = now;
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
	//! A shut gate on the route, looked for ahead of the bumper every GATE_EVERY_MS (Projectile rays, the layer a
	//! boom barrier's bar answers to): the speed cap that still stops GATE_STOP_M short of it goes to
	//! s_fGateCapKmh (999 when none). True once the stop has begun (the caller returns); the drive then ends
	//! `stuck` with `why=gate_ahead_shut_door_...` and the night tour opens the gate with the car out of its way.
	//! A gate off the route (a yard gate across a field beside a bend) or past the goal does not count.
	protected static bool GateGuard(IEntity car, vector at, vector forward, float speed, float now, float toGoal)
	{
		s_fGateCapKmh = 999;
		if (now >= s_fGateAtMs)
		{
			s_fGateAtMs = now + GATE_EVERY_MS;
			s_fGateM = -1;
			float v = speed / 3.6;
			float reach = Math.Clamp(v * v / (2 * DECEL) + GATE_STOP_M + GATE_MARGIN_M, GATE_REACH_MIN_M, GATE_REACH_MAX_M);
			string found = GateLook(car, reach);
			if (!found.IsEmpty() && GateOnRoute(s_vShutDoorAt, reach, forward) && toGoal - s_fTolerance > s_fShutDoorM + s_fFrontM)
			{
				s_fGateM = s_fShutDoorM;
				s_vGateFrom = at;
				s_sGateFound = found;
			}
		}

		if (s_fGateM < 0)
			return false;

		// The look is up to GATE_EVERY_MS old: take what the car has driven since off it.
		float metres = s_fGateM - vector.DistanceXZ(at, s_vGateFrom);
		float stopAt = GATE_STOP_M + 1;
		if ((metres <= stopAt && speed <= GATE_STOP_KMH + 2) || metres <= 2.5)
		{
			s_bGateStop = true;
			s_bStopping = true;
			s_sResult = "stuck";
			s_sStatus = "braking";
			s_sWhy = "gate_ahead_" + s_sGateFound;
			LF_Debug.Info(FEATURE, string.Format("drive stops for a shut gate %1 m ahead at %2 km/h (%3)", Round1(metres), Round1(speed), s_sGateFound));
			StopInputs(car, speed);
			return true;
		}

		s_fGateCapKmh = GATE_STOP_KMH + Math.Sqrt(2 * DECEL * Math.Max(metres - stopAt, 0)) * 3.6;
		return false;
	}

	protected static float s_fLookReach;
	protected static float s_fLookBest;
	protected static string s_sLookFound;
	protected static vector s_vLookAt;

	//------------------------------------------------------------------------------------------------
	//! A shut gate-sized door whose bounds (sampled every half metre along their longest side) cross the car's lane
	//! between its bumper and `reach` metres on: `shut_door_<prefab>_at_<m>m_ahead`, "" when none. By geometry, not
	//! rays: a level ray from the bumper passes over or into the ground on a rise, and a bar beyond one is unseen.
	//! The nearest wins; s_fShutDoorM (metres from the bumper) and s_vShutDoorAt are left for the caller.
	protected static string GateLook(IEntity car, float reach)
	{
		float half = Math.Max(s_fFrontM, 1.5);
		s_pDoorCar = car;
		s_fLookReach = reach;
		s_fLookBest = reach + 1;
		s_sLookFound = "";
		vector centre = car.CoordToParent(Vector(0, 0, half + reach * 0.5));
		GetGame().GetWorld().QueryEntitiesBySphere(centre, reach * 0.5 + 10, CollectGate, null, EQueryEntitiesFlags.ALL);
		s_pDoorCar = null;
		if (!s_sLookFound.IsEmpty())
		{
			s_fShutDoorM = s_fLookBest;
			s_vShutDoorAt = s_vLookAt;
		}

		return s_sLookFound;
	}

	//------------------------------------------------------------------------------------------------
	//! One door of the look's sphere query: shut, gate-sized, not the car's own, with a sample on the car's centre
	//! line (GATE_CENTRE_M, within 8 m of its height) ahead of the bumper.
	protected static bool CollectGate(IEntity door)
	{
		if (!door || !s_pDoorCar)
			return true;

		BaseDoorComponent component = BaseDoorComponent.Cast(door.FindComponent(DoorComponent));
		if (!component || component.IsOpen() || !IsGateSized(door))
			return true;

		IEntity top = door;
		while (top.GetParent())
			top = top.GetParent();

		if (top == s_pDoorCar)
			return true;

		float half = Math.Max(s_fFrontM, 1.5);
		vector mins, maxs;
		door.GetBounds(mins, maxs);
		vector size = maxs - mins;
		int axis = 0;
		if (size[1] > size[axis])
			axis = 1;

		if (size[2] > size[axis])
			axis = 2;

		float dt = Math.Max(0.5 / Math.Max(size[axis], 0.5), 0.02);
		float carY = s_pDoorCar.GetOrigin()[1];
		float nearest = s_fLookBest;
		vector nearestAt;
		for (float t = 0; t <= 1.0001; t += dt)
		{
			vector p = (mins + maxs) * 0.5;
			p[axis] = mins[axis] + size[axis] * t;
			vector world = door.CoordToParent(p);
			vector local = s_pDoorCar.CoordToLocal(world);
			if (Math.AbsFloat(local[0]) > GATE_CENTRE_M || local[2] < half || local[2] > half + s_fLookReach)
				continue;

			if (Math.AbsFloat(world[1] - carY) > 8)
				continue;

			float metres = local[2] - half;
			if (metres < nearest)
			{
				nearest = metres;
				nearestAt = world;
			}
		}

		if (nearest >= s_fLookBest)
			return true;

		s_fLookBest = nearest;
		s_vLookAt = nearestAt;
		string name = LF_Utils.GetPrefabName(door);
		if (name.IsEmpty())
			name = door.ClassName();

		int slash = name.LastIndexOf("/");
		if (slash >= 0)
			name = name.Substring(slash + 1, name.Length() - slash - 1);

		name.Replace(".et", "");
		s_sLookFound = string.Format("shut_door_%1_at_%2m_ahead", name, Round1(nearest));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! True when the route runs through `p`: a route point within GATE_ROUTE_M of it, from just behind the car to
	//! `reach` (plus the margin) on, and the route 12 m further on lying the way the car faces from `p`. A building
	//! door beside the pad the route leaves (or a door at the goal) is not a gate: the route turns away from it.
	protected static bool GateOnRoute(vector p, float reach, vector forward)
	{
		float first = Math.Max(s_fProgress - 4, 0);
		float last = s_fProgress + reach + GATE_MARGIN_M;
		for (float along = first; along <= last; along += 2)
		{
			if (vector.DistanceXZ(PointAt(along), p) > GATE_ROUTE_M)
				continue;

			vector beyond = PointAt(along + 12) - p;
			beyond[1] = 0;
			if (beyond.Length() < 3)
				return false;

			return vector.Dot(beyond.Normalized(), forward) > GATE_THROUGH;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! A drive stuck at a shut door (the last Blocker's): a gate only when the route runs through it and the goal is
	//! not this side of it. Any other shut door is a wall to the stuck rules (a back-up, a new try).
	protected static bool IsGateOnWay(vector forward, float toGoal)
	{
		if (!s_bShutDoorAt)
			return false;

		if (toGoal - s_fTolerance <= s_fShutDoorM + s_fFrontM)
			return false;

		return GateOnRoute(s_vShutDoorAt, DOOR_LOOK_M + 8, forward);
	}

	//------------------------------------------------------------------------------------------------
	//! The stop at a shut gate has come to rest: say how far the bar is now in `why`. A car nearer the bar than
	//! GATE_BACK_MIN_M backs off first (true: the back-up has begun, its inputs set).
	protected static bool GateStopDone(IEntity car, vector at, float now)
	{
		s_bGateStop = false;
		string found = ShutDoorAhead(car, ForwardOf(car), DOOR_LOOK_M + GATE_STOP_M + 4, true);
		if (found.IsEmpty())
			found = ShutDoorInReach(car);

		if (found.IsEmpty())
			return false;

		s_sGateFound = found;
		s_sWhy = "gate_ahead_" + found;
		return BeginGateReverse(car, at, now, s_fShutDoorM);
	}

	//------------------------------------------------------------------------------------------------
	//! A car `metres` from a shut gate's bar (negative: under it) nearer than GATE_BACK_MIN_M backs straight
	//! off, at most BACKUP_MAX_KMH (the back-up rules), to about GATE_STOP_M from it: a bar that a car is
	//! under or against does not open. False when it has room, or has backed off GATE_BACKS times.
	protected static bool BeginGateReverse(IEntity car, vector at, float now, float metres)
	{
		if (metres >= GATE_BACK_MIN_M || s_iGateBacks >= GATE_BACKS)
			return false;

		s_iGateBacks++;
		s_bGateReverse = true;
		s_bStopping = false;
		s_bGateStop = false;
		s_fBackupLimitM = Math.Clamp(GATE_STOP_M + 0.5 - metres, 1.5, GATE_BACK_MAX_M);
		s_vBackupFrom = at;
		s_fBackupSteer = 0;
		s_fBackupUntilMs = now + (4 + s_fBackupLimitM * 1.5) * 1000;
		s_fStillSinceMs = 0;
		s_sStatus = "backing";
		LF_Debug.Info(FEATURE, string.Format("drive backs off a shut gate %1 m ahead (%2 m, try %3 of %4)", Round1(metres), Round1(s_fBackupLimitM), s_iGateBacks, GATE_BACKS));
		s_fThrust = 0;
		s_fBrake = 1;
		s_fHand = 0;
		s_fSteer = 0;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! What stands in front of the bumper: a prefab name, `terrain`, or `clear` (then the pitch, since
	//! a lip or a steep bank stops a car with nothing in front of it).
	protected static string Blocker(IEntity car, vector forward)
	{
		// An empty tank stops a car with nothing in front of it.
		if (FuelShare(car) == 0)
			return "out_of_fuel";

		// A boom barrier's bar is on the Projectile layer only (a probe across it: Main, Vehicle and Static
		// hit just its post), so the rays below never see it: look for shut doors on that layer first.
		s_bShutDoorAt = false;
		string shut = ShutDoorAhead(car, forward, DOOR_LOOK_M, true);
		if (shut.IsEmpty())
			shut = ShutDoorInReach(car);

		if (!shut.IsEmpty())
			return shut;

		// Three rays (both front corners and the middle) at a kerb height and a bumper height.
		array<float> heights = {0.35, 0.9, Math.Max(s_fTopM - 0.3, 1.4)};
		array<float> sides = {0, -1, 1};
		for (int ray = 0; ray < 9; ray++)
		{
			float side = sides[ray / 3];
			float h = heights[ray % 3];
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

			// A shut door or gate (a boom barrier is a door): a car does not push through it.
			if (IsDoor(hit) && IsGateSized(hit))
			{
				s_fShutDoorM = fraction * BLOCKER_LOOK_M;
				s_vShutDoorAt = trace.Start + (trace.End - trace.Start) * fraction;
				s_bShutDoorAt = true;
				return string.Format("shut_door_%1_at_%2m_%3", name, Round1(fraction * BLOCKER_LOOK_M), corner);
			}

			return string.Format("blocked_by_%1_at_%2m_%3", name, Round1(fraction * BLOCKER_LOOK_M), corner);
		}

		vector angles = car.GetYawPitchRoll();
		return string.Format("clear_pitch_%1", Math.Round(angles[1]));
	}

	//------------------------------------------------------------------------------------------------
	//! `shut_door_<prefab>_at_<m>m_<corner>` when a door that is not open stands within `lookM` ahead of
	//! the bumper, else "". Rays on the Projectile layer (the only one a boom barrier's bar answers to) at
	//! several heights, since a bar passes over the bumper; only a hit on a door counts, so trees and people
	//! on that layer are ignored. The nearest door hit wins; its distance (from the bumper) and place are left in
	//! s_fShutDoorM and s_vShutDoorAt. `fromRear`: the rays start at the car's rear (a car stopped against a bar
	//! has it over its bonnet, past a ray from the bumper), else at the bumper (a look down the road).
	static string ShutDoorAhead(IEntity car, vector forward, float lookM, bool fromRear)
	{
		array<float> heights = {0.4, 0.7, 1.0, 1.3, 1.6};
		array<float> sides = {0, -1, 1};
		// `metres` below is measured from the bumper (negative: the bar is over the car).
		float half = Math.Max(s_fFrontM, 1.5);
		float rear = half;
		if (fromRear)
			rear = -half;

		float behind = half - rear;
		float length = behind + lookM;
		float best = lookM + 1;
		string found;
		foreach (float side : sides)
		{
			foreach (float h : heights)
			{
				// The car's own parts (body, lights, the seated driver) answer on that layer too: step past
				// each and cast again.
				vector origin = car.CoordToParent(Vector(side * s_fHalfWidthM, h, rear));
				vector start = origin;
				vector end = origin + forward * length;
				IEntity hit;
				float fraction = 1;
				for (int hop = 0; hop < DOOR_HOPS; hop++)
				{
					TraceParam trace = new TraceParam();
					trace.Start = start;
					trace.End = end;
					trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
					trace.LayerMask = EPhysicsLayerPresets.Projectile;
					trace.Exclude = car;
					fraction = GetGame().GetWorld().TraceMove(trace, null);
					hit = trace.TraceEnt;
					if (fraction >= 1 || !hit)
					{
						hit = null;
						break;
					}

					IEntity root = hit;
					while (root.GetParent())
						root = root.GetParent();

					if (root != car)
						break;

					start = start + (end - start) * fraction + forward * 0.05;
					hit = null;
				}

				if (!hit || !IsDoor(hit) || !IsGateSized(hit))
					continue;

				// An open leaf or a raised bar beside the road is not in the way.
				BaseDoorComponent door = DoorOfEntity(hit);
				if (door && door.IsOpen())
					continue;

				vector hitAt = start + (end - start) * fraction;
				float metres = vector.Distance(origin, hitAt) - behind;
				if (metres >= best)
					continue;

				best = metres;
				s_fShutDoorM = metres;
				s_vShutDoorAt = hitAt;
				s_bShutDoorAt = true;
				string name = LF_Utils.GetPrefabName(hit);
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

				found = string.Format("shut_door_%1_at_%2m_%3", name, Round1(metres), corner);
			}
		}

		return found;
	}

	//! A door leaf at least this wide (its longest bound, metres) is a gate or a boom barrier's bar; a house door
	//! (about 1 m) beside the road is a wall to the harness, not a gate to open.
	static const float GATE_MIN_M = 1.8; // lf-fixed: harness geometry

	//------------------------------------------------------------------------------------------------
	//! True when the door entity (or its nearest door parent) is wide enough to be a gate or a bar.
	protected static bool IsGateSized(IEntity hit)
	{
		IEntity e = hit;
		while (e && !e.FindComponent(DoorComponent))
			e = e.GetParent();

		if (!e)
			return false;

		vector mins, maxs;
		e.GetBounds(mins, maxs);
		vector size = maxs - mins;
		return Math.Max(Math.Max(size[0], size[1]), size[2]) >= GATE_MIN_M;
	}

	protected static IEntity s_pDoorCar;
	protected static float s_fDoorBest;
	protected static string s_sDoorFound;

	//------------------------------------------------------------------------------------------------
	//! One door of the sphere query: not open, not the car's own, its bounds crossing the car's footprint.
	protected static bool CollectDoor(IEntity door)
	{
		if (!door || !s_pDoorCar)
			return true;

		BaseDoorComponent component = BaseDoorComponent.Cast(door.FindComponent(DoorComponent));
		if (!component || component.IsOpen() || !IsGateSized(door))
			return true;

		IEntity top = door;
		while (top.GetParent())
			top = top.GetParent();

		if (top == s_pDoorCar)
			return true;

		float half = Math.Max(s_fFrontM, 1.5);
		vector mins, maxs;
		door.GetBounds(mins, maxs);
		vector size = maxs - mins;
		int axis = 0;
		if (size[1] > size[axis])
			axis = 1;

		if (size[2] > size[axis])
			axis = 2;

		float nearest = s_fDoorBest;
		vector nearestAt;
		for (int k = 0; k <= 12; k++)
		{
			vector p = (mins + maxs) * 0.5;
			p[axis] = mins[axis] + size[axis] * k / 12;
			vector world = door.CoordToParent(p);
			vector local = s_pDoorCar.CoordToLocal(world);
			if (Math.AbsFloat(local[0]) > s_fHalfWidthM + 0.3 || local[2] < -half || local[2] > half + DOOR_LOOK_M || local[1] < -0.5 || local[1] > s_fTopM + 0.5)
				continue;

			float metres = local[2] - half;
			if (metres < nearest)
			{
				nearest = metres;
				nearestAt = world;
			}
		}

		if (nearest >= s_fDoorBest)
			return true;

		s_fDoorBest = nearest;
		s_fShutDoorM = nearest;
		s_vShutDoorAt = nearestAt;
		s_bShutDoorAt = true;
		string name = LF_Utils.GetPrefabName(door);
		if (name.IsEmpty())
			name = door.ClassName();

		int slash = name.LastIndexOf("/");
		if (slash >= 0)
			name = name.Substring(slash + 1, name.Length() - slash - 1);

		name.Replace(".et", "");
		s_sDoorFound = string.Format("shut_door_%1_at_%2m_geometry", name, Round1(nearest));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The same answer as ShutDoorAhead, from geometry instead of rays: a door that is not open whose
	//! bounds (sampled along its longest side) cross the car's footprint, from its rear to DOOR_LOOK_M
	//! beyond its bumper. Catches a bar the rays cannot reach.
	protected static string ShutDoorInReach(IEntity car)
	{
		float half = Math.Max(s_fFrontM, 1.5);
		s_pDoorCar = car;
		s_fDoorBest = DOOR_LOOK_M + 1;
		s_sDoorFound = "";
		GetGame().GetWorld().QueryEntitiesBySphere(car.GetOrigin(), half + DOOR_LOOK_M + 6, CollectDoor, null, EQueryEntitiesFlags.ALL);
		s_pDoorCar = null;
		return s_sDoorFound;
	}

	//------------------------------------------------------------------------------------------------
	//! The door component of an entity or of the nearest parent that has one.
	protected static BaseDoorComponent DoorOfEntity(IEntity hit)
	{
		IEntity e = hit;
		while (e)
		{
			BaseDoorComponent door = BaseDoorComponent.Cast(e.FindComponent(BaseDoorComponent));
			if (door)
				return door;

			e = e.GetParent();
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! A door, a yard gate or a boom barrier's bar (each a DoorComponent), or a part of one.
	static bool IsDoor(IEntity hit)
	{
		IEntity e = hit;
		while (e)
		{
			if (e.FindComponent(DoorComponent))
				return true;

			e = e.GetParent();
		}

		return false;
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
		// At a crawl, or rolling backwards, the brake would drive it in reverse: the handbrake holds it.
		if (speed < 3 || ForwardKmh(car, ForwardOf(car)) < 0)
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
		// Ground no car takes (a bank, a ditch's wall) shuts the line as a wall does.
		float shut = LF_DriveHarness.GroundShutAt(Vector(a[0], 0, a[2]), dir, vector.DistanceXZ(a, b));
		if (shut >= 0)
		{
			vector at = a + dir * shut;
			s_sLastHit = string.Format("steep_ground_at_%1,%2", Math.Round(at[0]), Math.Round(at[2]));
			return false;
		}

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
