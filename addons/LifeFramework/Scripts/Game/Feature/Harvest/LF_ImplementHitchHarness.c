// LF_ImplementHitchHarness.c: harness verbs for the hitch session proof (#3284, agent-box/proofs/hitch-sync.mjs).
// The harness cannot sit in a seat and an empty seat gives no drive torque, so the server pushes the empty UAZ469
// along its own forward axis (brake off, engine on, physics velocity). Verbs:
//   hitch spawn     the server spawns a UAZ469 on open DebugWorld grass south of the grid (60 5) and hitches a reaper
//   hitch drive     the server lets go of the brake and pushes the UAZ east at 6 m/s
//   hitch stop      the server takes the throttle off and brakes
//   hitch state     the server's view: vehicle origin, metres moved since the spawn, implement rpl id
//   hitch delete    the server deletes the vehicle the way the garage and impound do (DeleteEntityAndChildren)
//   hitch seen      THIS machine's view: every implement within 300 m, its parent, local offset, behind, proxy
// Dev gate: a client asks through its character, refused outside a harness or dev build.

//------------------------------------------------------------------------------------------------
class LF_ImplementHitchHarness
{
	static const ResourceName UAZ = "{C8B16ABCA57679A2}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469_covered_CIV_blue.et";
	//! Open DebugWorld grass south of the debug grid (cells start at z 22); the UAZ faces east along it.
	static const vector SPOT = "60 0 5";
	static const float HEADING = 90; // lf-fixed: harness geometry, not a gameplay knob
	//! Ground speed the server holds the UAZ at while the proof drives (an empty seat gives no drive torque).
	static const float SPEED_MS = 6; // lf-fixed: harness speed, not a gameplay knob
	//! Milliseconds between throttle refreshes while the proof drives.
	static const int PEDAL_MS = 250; // lf-fixed: harness timing, not a gameplay knob
	//! How far `seen` looks for implements.
	static const float SEEN_RADIUS_M = 300; // lf-fixed: harness reach, not a gameplay knob

	protected static EntityID s_Vehicle = EntityID.INVALID;
	protected static vector s_vStart;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		GetGame().GetCallqueue().Remove(Pedal);
		s_Vehicle = EntityID.INVALID;
		s_vStart = vector.Zero;
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
	static string Spawn(IEntity character)
	{
		IEntity old = HarnessVehicle();
		if (old)
			SCR_EntityHelper.DeleteEntityAndChildren(old);

		vector at = SPOT;
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
		vector velocity = forward * SPEED_MS;
		velocity[1] = physics.GetVelocity()[1];
		physics.SetVelocity(velocity);
	}

	//------------------------------------------------------------------------------------------------
	static string Stop()
	{
		GetGame().GetCallqueue().Remove(Pedal);
		IEntity vehicle = HarnessVehicle();
		if (!vehicle)
			return "error no_vehicle";

		VehicleWheeledSimulation simulation = VehicleWheeledSimulation.Cast(vehicle.FindComponent(VehicleWheeledSimulation));
		if (simulation)
		{
			simulation.SetThrottle(0);
			simulation.SetBreak(1, true);
		}

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
		return string.Format("ok vehicle=%1 at=%2 moved=%3 implement=%4", LF_ImplementHitch.RplIdOf(vehicle), FormatPos(now), Math.Round(moved * 10) / 10, LF_ImplementHitch.RplIdOf(LF_ImplementHitch.FindHitched(vehicle)));
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

		if (!character || (cmd.Arg1 != "spawn" && cmd.Arg1 != "drive" && cmd.Arg1 != "stop" && cmd.Arg1 != "state" && cmd.Arg1 != "delete"))
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "usage: hitch spawn|drive|stop|state|delete|seen (all but seen need a character)");
			return true;
		}

		character.LF_AskHarnessHitch(cmd.Id, cmd.Arg1);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_ChimeraCharacter
{
	//------------------------------------------------------------------------------------------------
	void LF_AskHarnessHitch(int commandId, string action)
	{
		if (Replication.IsServer())
		{
			LF_HarnessHitchDo(commandId, action);
			return;
		}

		Rpc(RpcAsk_LF_HarnessHitch, commandId, action);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_HarnessHitch(int commandId, string action)
	{
		if (!LF_DevGate.CheckHarnessRequest(this, "hitch"))
		{
			LF_HarnessReply(commandId, "error", LF_AgentHarness.REFUSED_DEV_GATE);
			return;
		}

		LF_HarnessHitchDo(commandId, action);
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_HarnessHitchDo(int commandId, string action)
	{
		string answer;
		if (action == "spawn")
			answer = LF_ImplementHitchHarness.Spawn(this);
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
