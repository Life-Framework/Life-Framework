// LF_ImplementHitchHarness.c: harness verbs for the hitch session proof (#3284, agent-box/proofs/hitch-sync.mjs).
// The harness cannot sit in a seat and an empty seat gives no drive torque, so the server pushes the empty UAZ469
// along its own forward axis (brake off, engine on, physics velocity). Verbs:
//   hitch spawn     the server spawns a UAZ469 on open DebugWorld grass south of the grid (60 5) and hitches a reaper
//   hitch fieldspawn  as spawn, but on the wheat field of grid cell slice-3287, bar just west of the field (#3289)
//   hitch drive     the server lets go of the brake and pushes the UAZ east at 4 m/s (inside the reaper's cutting band)
//   hitch stop      the server takes the throttle off and brakes
//   hitch state     the server's view: vehicle origin, metres moved since the spawn, implement rpl id
//   hitch delete    the server deletes the vehicle the way the garage and impound do (DeleteEntityAndChildren)
//   hitch seen      THIS machine's view: every implement within 300 m, its parent, local offset, behind, proxy
// The island proof (agent-box/proofs/harvest-island.mjs, #3302) adds:
//   hitch rackspawn   the server spawns a plain UAZ469 (no reaper) on empty ground within 8 m of the nearest implement rack
//   hitch goto X Z S  the server steers the pushed vehicle to X Z at S m/s (whole numbers), then brakes; state says arrived=1
//   hitch seat        the server seats the asking character at the wheel of the harness vehicle
//   hitch warp X Z Y  the server sets the vehicle down at X Z facing yaw Y degrees (a fallback when a building is in the way)
// Dev gate: a client asks through its character, refused outside a harness or dev build.

//------------------------------------------------------------------------------------------------
class LF_ImplementHitchHarness
{
	static const ResourceName UAZ = "{C8B16ABCA57679A2}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469_covered_CIV_blue.et";
	//! Open DebugWorld grass south of the debug grid (cells start at z 22); the UAZ faces east along it.
	static const vector SPOT = "60 0 5";
	//! How far `fieldspawn` looks for the nearest wheat field (the proof runs on the grid cell that has one).
	static const float FIELD_REACH_M = 3000; // lf-fixed: harness reach, not a gameplay knob
	static const float HEADING = 90; // lf-fixed: harness geometry, not a gameplay knob
	//! Ground speed the server holds the UAZ at while the proof drives (an empty seat gives no drive torque).
	static const float SPEED_MS = 4; // lf-fixed: harness speed, not a gameplay knob
	//! Milliseconds between throttle refreshes while the proof drives.
	static const int PEDAL_MS = 250; // lf-fixed: harness timing, not a gameplay knob
	//! How far `seen` looks for implements.
	static const float SEEN_RADIUS_M = 300; // lf-fixed: harness reach, not a gameplay knob

	//! The rack prefab `rackspawn` looks for.
	static const ResourceName RACK_PREFAB = "{21B9456F3B79410E}Prefabs/Harvest/LF_ImplementRack.et";
	//! `goto` counts the waypoint reached within this many metres.
	static const float ARRIVE_M = 2.5; // lf-fixed: harness geometry, not a gameplay knob
	//! `goto` steering: radians per second of yaw rate per radian of heading error, and its cap.
	static const float STEER_GAIN = 1.5; // lf-fixed: harness steering, not a gameplay knob
	static const float STEER_MAX = 0.9; // lf-fixed: harness steering, not a gameplay knob
	//! Above this heading error (radians) `goto` slows to a crawl so the turn is tight.
	static const float TURN_SLOW_RAD = 0.6; // lf-fixed: harness steering, not a gameplay knob
	static const float TURN_SPEED_MS = 1.5; // lf-fixed: harness steering, not a gameplay knob

	protected static EntityID s_Vehicle = EntityID.INVALID;
	protected static vector s_vStart;
	protected static EntityID s_Field = EntityID.INVALID;
	protected static bool s_bGoto;
	protected static bool s_bArrived;
	protected static vector s_vGoto;
	protected static float s_fGotoSpeed = SPEED_MS;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		GetGame().GetCallqueue().Remove(Pedal);
		s_Vehicle = EntityID.INVALID;
		s_vStart = vector.Zero;
		s_Field = EntityID.INVALID;
		s_bGoto = false;
		s_bArrived = false;
		s_vGoto = vector.Zero;
		s_fGotoSpeed = SPEED_MS;
	}

	//------------------------------------------------------------------------------------------------
	protected static IEntity HarnessVehicle()
	{
		if (s_Vehicle == EntityID.INVALID)
			return null;

		return GetGame().GetWorld().FindEntityByID(s_Vehicle);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a UAZ469 in front of the character, with a reaper hitched.
	static string Spawn(IEntity character, bool onField = false)
	{
		IEntity old = HarnessVehicle();
		if (old)
			SCR_EntityHelper.DeleteEntityAndChildren(old);

		vector at = SPOT;
		if (onField)
		{
			LF_WheatField field = NearestField(character);
			if (!field)
				return "error no_field";

			s_Field = field.GetID();

			// Middle row of the field, the bar (3.4 m behind the origin) 3 m west of the first cell.
			at = field.GetOrigin();
			at[0] = at[0] - field.m_fSizeX * 0.5 - 3 + 3.4;
		}

		at[1] = GetGame().GetWorld().GetSurfaceY(at[0], at[2]) + 0.3;
		IEntity vehicle = LF_Utils.SpawnEntityPrefab(UAZ, at, Vector(HEADING, 0, 0));
		if (!vehicle)
			return "error no_vehicle";

		s_Vehicle = vehicle.GetID();
		s_vStart = vehicle.GetOrigin();
		string reason;
		IEntity implement = LF_ImplementHitch.Hitch(vehicle, reason);
		if (!implement)
			return "error hitch_refused " + reason;

		return string.Format("ok vehicle=%1 implement=%2 at=%3", LF_ImplementHitch.RplIdOf(vehicle), LF_ImplementHitch.RplIdOf(implement), FormatPos(s_vStart));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a plain UAZ469 on empty ground within reach of the implement rack nearest the character, facing
	//! south, with no reaper: the rack's take-out hitches one (#3295). Also remembers the nearest wheat field.
	static string SpawnAtRack(IEntity character)
	{
		IEntity old = HarnessVehicle();
		if (old)
			SCR_EntityHelper.DeleteEntityAndChildren(old);

		vector around = vector.Zero;
		if (character)
			around = character.GetOrigin();

		LF_ImplementHitchHarnessRackQuery query = new LF_ImplementHitchHarnessRackQuery();
		GetGame().GetWorld().QueryEntitiesBySphere(around, FIELD_REACH_M, query.Collect, null, EQueryEntitiesFlags.ALL);
		IEntity rack = null;
		float best = FIELD_REACH_M * 2;
		foreach (IEntity candidate : query.m_aRacks)
		{
			float distance = vector.Distance(candidate.GetOrigin(), around);
			if (distance < best)
			{
				best = distance;
				rack = candidate;
			}
		}

		if (!rack)
			return "error no_rack";

		LF_WheatField field = NearestField(rack);
		if (field)
			s_Field = field.GetID();

		// Empty ground a UAZ fits on, at most 7.5 m from the rack (the take-out reaches 8 m).
		vector rackAt = rack.GetOrigin();
		array<vector> centres = {};
		centres.Insert(Vector(0, 0, 4));
		centres.Insert(Vector(0, 0, -4));
		centres.Insert(Vector(-4, 0, 0));
		centres.Insert(Vector(4, 0, 0));
		vector at = rackAt + centres[0];
		bool found = false;
		foreach (vector offset : centres)
		{
			if (SCR_WorldTools.FindEmptyTerrainPosition(at, rackAt + offset, 4, 2, 2.5))
			{
				found = true;
				break;
			}
		}

		if (!found)
		{
			at = rackAt + centres[0];
			LF_Debug.Warn(LF_ImplementHitch.FEATURE, "harness: no empty ground found beside the rack, the UAZ is set down 4 m north of it");
		}

		at[1] = GetGame().GetWorld().GetSurfaceY(at[0], at[2]) + 0.3;
		IEntity vehicle = LF_Utils.SpawnEntityPrefab(UAZ, at, Vector(180, 0, 0));
		if (!vehicle)
			return "error no_vehicle";

		s_Vehicle = vehicle.GetID();
		s_vStart = vehicle.GetOrigin();
		s_bGoto = false;
		s_bArrived = false;
		LF_Debug.Info(LF_ImplementHitch.FEATURE, string.Format("harness: a plain UAZ469 stands %1 m from the rack at %2", Math.Round(vector.Distance(at, rackAt) * 10) / 10, FormatPos(at)));
		return string.Format("ok vehicle=%1 at=%2 rack=%3 rackDist=%4 found=%5", LF_ImplementHitch.RplIdOf(vehicle), FormatPos(at), FormatPos(rackAt), Math.Round(vector.Distance(at, rackAt) * 10) / 10, found);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: seats the character at the wheel of the harness vehicle (the grain prompts need a driver).
	static string Seat(IEntity character)
	{
		IEntity vehicle = HarnessVehicle();
		if (!vehicle)
			return "error no_vehicle";

		ChimeraCharacter chimera = ChimeraCharacter.Cast(character);
		if (!chimera)
			return "error no_character";

		SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(chimera.GetCompartmentAccessComponent());
		if (!access)
			return "error no_access";

		if (!access.MoveInVehicle(vehicle, ECompartmentType.PILOT))
			return "error seat_refused";

		LF_Debug.Info(LF_ImplementHitch.FEATURE, "harness: the character sits at the wheel of the harness UAZ");
		return "ok seated";
	}

	//------------------------------------------------------------------------------------------------
	//! Server: steer the pushed vehicle to the point x z at `speed` m/s (the proof reads `arrived=1` in `state`).
	static string Goto(string x, string z, string speed)
	{
		IEntity vehicle = HarnessVehicle();
		if (!vehicle)
			return "error no_vehicle";

		s_vGoto = Vector(x.ToFloat(), 0, z.ToFloat());
		s_fGotoSpeed = speed.ToFloat();
		if (s_fGotoSpeed <= 0)
			s_fGotoSpeed = SPEED_MS;

		s_bGoto = true;
		s_bArrived = false;
		CarControllerComponent controller = CarControllerComponent.Cast(vehicle.FindComponent(CarControllerComponent));
		if (controller)
		{
			controller.SetPersistentHandBrake(false);
			controller.StartEngine();
		}

		Pedal();
		GetGame().GetCallqueue().Remove(Pedal);
		GetGame().GetCallqueue().CallLater(Pedal, PEDAL_MS, true);
		LF_Debug.Info(LF_ImplementHitch.FEATURE, string.Format("harness: the UAZ is steered to %1 at %2 m/s", FormatPos(s_vGoto), s_fGotoSpeed));
		return string.Format("ok goto=%1 speed=%2", FormatPos(s_vGoto), s_fGotoSpeed);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: set the vehicle down at x z facing `yaw` degrees, at rest. A fallback only: the proof says so when it uses it.
	static string Warp(string x, string z, string yaw)
	{
		IEntity vehicle = HarnessVehicle();
		if (!vehicle)
			return "error no_vehicle";

		GetGame().GetCallqueue().Remove(Pedal);
		s_bGoto = false;
		float px = x.ToFloat();
		float pz = z.ToFloat();
		vehicle.SetAngles(Vector(0, yaw.ToFloat(), 0));
		vehicle.SetOrigin(Vector(px, GetGame().GetWorld().GetSurfaceY(px, pz) + 0.4, pz));
		Physics physics = vehicle.GetPhysics();
		if (physics)
		{
			physics.SetVelocity(vector.Zero);
			physics.SetAngularVelocity(vector.Zero);
			physics.SetActive(ActiveState.ACTIVE);
		}

		LF_Debug.Warn(LF_ImplementHitch.FEATURE, string.Format("harness: WARP the UAZ to %1 facing %2 (not driven)", FormatPos(vehicle.GetOrigin()), yaw));
		return string.Format("ok warped at=%1", FormatPos(vehicle.GetOrigin()));
	}

	//------------------------------------------------------------------------------------------------
	//! The wheat field closest to the character, or null.
	protected static LF_WheatField NearestField(IEntity character)
	{
		vector around = vector.Zero;
		if (character)
			around = character.GetOrigin();

		LF_ReaperFieldQuery query = new LF_ReaperFieldQuery();
		GetGame().GetWorld().QueryEntitiesBySphere(around, FIELD_REACH_M, query.Collect, null, EQueryEntitiesFlags.ALL);
		LF_WheatField nearest = null;
		float best = FIELD_REACH_M * 2;
		foreach (LF_WheatField candidate : query.m_aFields)
		{
			float distance = vector.Distance(candidate.GetOrigin(), around);
			if (distance < best)
			{
				best = distance;
				nearest = candidate;
			}
		}

		return nearest;
	}

	//------------------------------------------------------------------------------------------------
	static string Drive()
	{
		IEntity vehicle = HarnessVehicle();
		if (!vehicle)
			return "error no_vehicle";

		CarControllerComponent controller = CarControllerComponent.Cast(vehicle.FindComponent(CarControllerComponent));
		if (controller)
		{
			controller.SetPersistentHandBrake(false);
			controller.StartEngine();
		}

		s_bGoto = false;
		Pedal();
		GetGame().GetCallqueue().Remove(Pedal);
		GetGame().GetCallqueue().CallLater(Pedal, PEDAL_MS, true);
		LF_Debug.Info(LF_ImplementHitch.FEATURE, "harness: the hitched UAZ drives off");
		return "ok driving";
	}

	//------------------------------------------------------------------------------------------------
	protected static void Pedal()
	{
		IEntity vehicle = HarnessVehicle();
		if (!vehicle)
		{
			GetGame().GetCallqueue().Remove(Pedal);
			return;
		}

		VehicleWheeledSimulation simulation = VehicleWheeledSimulation.Cast(vehicle.FindComponent(VehicleWheeledSimulation));
		if (simulation)
		{
			simulation.SetBreak(0, false);
			simulation.SetThrottle(1);
		}

		// An empty seat gives the wheels no drive torque (the throttle alone creeps 1 m a minute), so the
		// physics body is pushed along its own forward axis; wheels, contacts and replication stay the engine's.
		Physics physics = vehicle.GetPhysics();
		if (!physics)
			return;

		physics.SetActive(ActiveState.ACTIVE);
		vector transform[4];
		vehicle.GetWorldTransform(transform);
		vector forward = transform[2];
		forward[1] = 0;
		forward.Normalize();
		float speed = SPEED_MS;
		if (s_bGoto)
		{
			vector here = vehicle.GetOrigin();
			float dx = s_vGoto[0] - here[0];
			float dz = s_vGoto[2] - here[2];
			if (Math.Sqrt(dx * dx + dz * dz) <= ARRIVE_M)
			{
				s_bGoto = false;
				s_bArrived = true;
				Stop();
				LF_Debug.Info(LF_ImplementHitch.FEATURE, "harness: the UAZ reached " + FormatPos(here));
				return;
			}

			// Heading is a yaw with 0 facing +Z; a positive spin about Y turns +Z toward +X.
			float error = Math.Atan2(dx, dz) - Math.Atan2(forward[0], forward[2]);
			while (error > Math.PI)
			{
				error -= Math.PI * 2;
			}

			while (error < -Math.PI)
			{
				error += Math.PI * 2;
			}

			physics.SetAngularVelocity(Vector(0, Math.Clamp(error * STEER_GAIN, -STEER_MAX, STEER_MAX), 0));
			speed = s_fGotoSpeed;
			if (Math.AbsFloat(error) > TURN_SLOW_RAD && speed > TURN_SPEED_MS)
				speed = TURN_SPEED_MS;
		}

		vector velocity = forward * speed;
		velocity[1] = physics.GetVelocity()[1];
		physics.SetVelocity(velocity);
	}

	//------------------------------------------------------------------------------------------------
	static string Stop()
	{
		GetGame().GetCallqueue().Remove(Pedal);
		s_bGoto = false;
		IEntity vehicle = HarnessVehicle();
		if (!vehicle)
			return "error no_vehicle";

		VehicleWheeledSimulation simulation = VehicleWheeledSimulation.Cast(vehicle.FindComponent(VehicleWheeledSimulation));
		if (simulation)
		{
			simulation.SetThrottle(0);
			simulation.SetBreak(1, true);
		}

		Physics physics = vehicle.GetPhysics();
		if (physics)
			physics.SetAngularVelocity(vector.Zero);

		return "ok stopped";
	}

	//------------------------------------------------------------------------------------------------
	static string State()
	{
		IEntity vehicle = HarnessVehicle();
		if (!vehicle)
			return "error no_vehicle";

		vector now = vehicle.GetOrigin();
		float moved = vector.Distance(Vector(now[0], 0, now[2]), Vector(s_vStart[0], 0, s_vStart[2]));
		IEntity implement = LF_ImplementHitch.FindHitched(vehicle);
		int hopper = -1;
		LF_ImplementComponent component = LF_ImplementComponent.Of(implement);
		if (component)
			hopper = component.GetHopperKg();

		int cut = -1;
		IEntity fieldEntity = null;
		if (s_Field != EntityID.INVALID)
			fieldEntity = GetGame().GetWorld().FindEntityByID(s_Field);

		LF_WheatField field = LF_WheatField.Cast(fieldEntity);
		if (field)
			cut = field.CutCount();

		return string.Format("ok vehicle=%1 at=%2 moved=%3 implement=%4 hopper=%5 cut=%6 arrived=%7 going=%8", LF_ImplementHitch.RplIdOf(vehicle), FormatPos(now), Math.Round(moved * 10) / 10, LF_ImplementHitch.RplIdOf(implement), hopper, cut, s_bArrived, s_bGoto);
	}

	//------------------------------------------------------------------------------------------------
	static string Delete()
	{
		GetGame().GetCallqueue().Remove(Pedal);
		IEntity vehicle = HarnessVehicle();
		if (!vehicle)
			return "error no_vehicle";

		string id = LF_ImplementHitch.RplIdOf(vehicle);
		SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
		s_Vehicle = EntityID.INVALID;
		LF_Debug.Info(LF_ImplementHitch.FEATURE, "harness: deleted the hitched vehicle " + id + " (the garage path)");
		return "ok deleted vehicle=" + id;
	}

	//------------------------------------------------------------------------------------------------
	//! This machine: `implements=N` and, for the first, its vehicle, local offset, world positions, behind and
	//! the gap between where the implement is and where the vehicle's frame says it should be.
	static string Seen(IEntity character)
	{
		vector around = vector.Zero;
		if (character)
			around = character.GetOrigin();

		LF_ImplementHitchHarnessQuery query = new LF_ImplementHitchHarnessQuery();
		GetGame().GetWorld().QueryEntitiesBySphere(around, SEEN_RADIUS_M, query.Collect, null, EQueryEntitiesFlags.ALL);
		int count = query.m_aFound.Count();
		if (count == 0)
			return "ok implements=0";

		IEntity implement = query.m_aFound[0];
		IEntity parent = implement.GetParent();
		RplComponent rpl = RplComponent.Cast(implement.FindComponent(RplComponent));
		bool proxy = rpl && rpl.IsProxy();
		vector local[4];
		implement.GetLocalTransform(local);
		if (!parent)
			return string.Format("ok implements=%1 implement=%2 parent=none at=%3 proxy=%4", count, LF_ImplementHitch.RplIdOf(implement), FormatPos(implement.GetOrigin()), proxy);

		vector inVehicle = parent.CoordToLocal(implement.GetOrigin());
		LF_HitchRow row = LF_ImplementHitch.RowFor(parent);
		float gap = -1;
		if (row)
			gap = vector.Distance(implement.GetOrigin(), parent.CoordToParent(row.m_vOffset));

		return string.Format("ok implements=%1 implement=%2 parent=%3 vehicleAt=%4 at=%5 local=%6 behind=%7 gap=%8 proxy=%9",
			count, LF_ImplementHitch.RplIdOf(implement), LF_ImplementHitch.RplIdOf(parent), FormatPos(parent.GetOrigin()),
			FormatPos(implement.GetOrigin()), FormatPos(local[3]), inVehicle[2] < 0, Math.Round(gap * 100) / 100, proxy);
	}

	//------------------------------------------------------------------------------------------------
	protected static string FormatPos(vector v)
	{
		return string.Format("%1,%2,%3", Math.Round(v[0] * 10) / 10, Math.Round(v[1] * 10) / 10, Math.Round(v[2] * 10) / 10);
	}
}

//------------------------------------------------------------------------------------------------
//! One `hitch seen` sphere query: the implements it met, dropped with the query.
class LF_ImplementHitchHarnessQuery
{
	ref array<IEntity> m_aFound = {};

	//------------------------------------------------------------------------------------------------
	bool Collect(IEntity entity)
	{
		if (LF_ImplementComponent.Of(entity))
			m_aFound.Insert(entity);

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! One `hitch rackspawn` sphere query: the implement racks it met, dropped with the query.
class LF_ImplementHitchHarnessRackQuery
{
	ref array<IEntity> m_aRacks = {};

	//------------------------------------------------------------------------------------------------
	bool Collect(IEntity entity)
	{
		EntityPrefabData prefab = entity.GetPrefabData();
		if (prefab && LF_Utils.SamePrefab(prefab.GetPrefabName(), LF_ImplementHitchHarness.RACK_PREFAB))
			m_aRacks.Insert(entity);

		return true;
	}
}

//------------------------------------------------------------------------------------------------
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "hitch")
			return super.Handle(cmd);

		SCR_ChimeraCharacter character = LF_Utils.GetLocalCharacter();
		if (cmd.Arg1 == "seen")
		{
			string seen = LF_ImplementHitchHarness.Seen(character);
			LF_AgentHarness.Answer(cmd.Id, "ok", seen.Substring(3, seen.Length() - 3)); // lf-bounded: a one-line harness reply
			return true;
		}

		if (!character || (cmd.Arg1 != "spawn" && cmd.Arg1 != "fieldspawn" && cmd.Arg1 != "rackspawn" && cmd.Arg1 != "seat" && cmd.Arg1 != "goto" && cmd.Arg1 != "warp" && cmd.Arg1 != "drive" && cmd.Arg1 != "stop" && cmd.Arg1 != "state" && cmd.Arg1 != "delete"))
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "usage: hitch spawn|fieldspawn|rackspawn|seat|goto X Z SPEED|warp X Z YAW|drive|stop|state|delete|seen (all but seen need a character)");
			return true;
		}

		character.LF_AskHarnessHitch(cmd.Id, cmd.Arg1, cmd.Arg2, cmd.Arg3, cmd.Arg4);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_ChimeraCharacter
{
	//------------------------------------------------------------------------------------------------
	void LF_AskHarnessHitch(int commandId, string action, string arg1 = "", string arg2 = "", string arg3 = "")
	{
		if (Replication.IsServer())
		{
			LF_HarnessHitchDo(commandId, action, arg1, arg2, arg3);
			return;
		}

		Rpc(RpcAsk_LF_HarnessHitch, commandId, action, arg1, arg2, arg3);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_HarnessHitch(int commandId, string action, string arg1, string arg2, string arg3)
	{
		if (!LF_DevGate.CheckHarnessRequest(this, "hitch"))
		{
			LF_HarnessReply(commandId, "error", LF_AgentHarness.REFUSED_DEV_GATE);
			return;
		}

		LF_HarnessHitchDo(commandId, action, arg1, arg2, arg3);
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_HarnessHitchDo(int commandId, string action, string arg1, string arg2, string arg3)
	{
		string answer;
		if (action == "spawn")
			answer = LF_ImplementHitchHarness.Spawn(this);
		else if (action == "fieldspawn")
			answer = LF_ImplementHitchHarness.Spawn(this, true);
		else if (action == "rackspawn")
			answer = LF_ImplementHitchHarness.SpawnAtRack(this);
		else if (action == "seat")
			answer = LF_ImplementHitchHarness.Seat(this);
		else if (action == "goto")
			answer = LF_ImplementHitchHarness.Goto(arg1, arg2, arg3);
		else if (action == "warp")
			answer = LF_ImplementHitchHarness.Warp(arg1, arg2, arg3);
		else if (action == "drive")
			answer = LF_ImplementHitchHarness.Drive();
		else if (action == "stop")
			answer = LF_ImplementHitchHarness.Stop();
		else if (action == "delete")
			answer = LF_ImplementHitchHarness.Delete();
		else
			answer = LF_ImplementHitchHarness.State();

		if (answer.StartsWith("ok"))
			LF_HarnessReply(commandId, "ok", answer.Substring(3, answer.Length() - 3)); // lf-bounded: a one-line harness reply
		else
			LF_HarnessReply(commandId, "error", answer.Substring(6, answer.Length() - 6)); // lf-bounded: a one-line harness reply
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of the harness's vehicle and pedal (LF_StaticReset finds it by reflection).
class LF_ImplementHitchHarness_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_ImplementHitchHarness";
	}

	override void Apply()
	{
		LF_ImplementHitchHarness.ResetStatics();
	}
}
