//------------------------------------------------------------------------------------------------
//! Harness stand-ins for the park brake proof (#979, agent-box/proofs/park-brake-proof.mjs). The
//! harness cannot drive, and DebugWorld's terrain is flat, so:
//!   slopecar    the server sets a base-game concrete vehicle ramp on open ground and an empty S105
//!               on it (the locked child a garage hands out), and reports the measured tilt
//!   slopecheck  the server's view of that car: metres moved from where it was set, speed, brake
//!   slopedrive  lets go of the brake the way a driver does and gives it throttle with the engine
//!               on, then `slopecheck` shows it leave
//! Everything between is the engine's own physics.
//------------------------------------------------------------------------------------------------
class LF_ParkBrakeHarness
{
	static const ResourceName RAMP = "{BB64FBA6986C62F7}Prefabs/Structures/BuildingParts/Entry/ConcreteSlope_02_MediumVehicle.et";
	static const vector SPOT = "300 0 100";
	//! Metres above the ramp's own top surface the car is set down (it settles onto its wheels).
	static const float DROP = 0.4; // lf-fixed: harness geometry, not a gameplay knob
	//! Metres the ramp's lowest point stands above the terrain.
	static const float LIFT = 0.1; // lf-fixed: harness geometry, not a gameplay knob
	//! Milliseconds between wake-ups while the proof holds the car.
	static const int WAKE_MS = 1000; // lf-fixed: harness timing, not a gameplay knob

	static const string FEATURE = "ParkBrake";

	protected static EntityID s_Car;
	protected static EntityID s_Ramp;
	protected static vector s_vStart;
	protected static float s_fTilt;

	//------------------------------------------------------------------------------------------------
	protected static CarControllerComponent Controller(IEntity car)
	{
		return CarControllerComponent.Cast(car.FindComponent(CarControllerComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! The vanilla persistent handbrake the prefab spawns with.
	protected static bool Braked(IEntity car)
	{
		CarControllerComponent controller = Controller(car);
		return controller && controller.GetPersistentHandBrake();
	}

	//------------------------------------------------------------------------------------------------
	//! The angle in degrees between the surface under (x, z) and level, found by a ray from above.
	protected static float Tilt(vector at, out float surfaceY)
	{
		TraceParam trace = new TraceParam();
		trace.Start = at + "0 20 0";
		trace.End = at + "0 -20 0";
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		trace.LayerMask = EPhysicsLayerPresets.Main;
		float fraction = GetGame().GetWorld().TraceMove(trace, null);
		if (fraction >= 1)
			return -1;

		surfaceY = trace.Start[1] + (trace.End[1] - trace.Start[1]) * fraction;
		return Math.Acos(Math.Clamp(trace.TraceNorm[1], -1, 1)) * Math.RAD2DEG;
	}

	//------------------------------------------------------------------------------------------------
	static string Spawn()
	{
		Cleanup();

		vector ground = SPOT;
		ground[1] = GetGame().GetWorld().GetSurfaceY(ground[0], ground[2]);
		IEntity ramp = LF_Utils.SpawnEntityPrefab(RAMP, ground);
		if (!ramp)
			return "error no_ramp";

		s_Ramp = ramp.GetID();
		vector mins, maxs;
		ramp.GetBounds(mins, maxs);
		// The mesh is a ramp down into a basement: it lies wholly below its origin. Lift it so its
		// low end rests on the terrain and the whole slope stands in the open.
		vector lifted = ground;
		lifted[1] = ground[1] - mins[1] + LIFT;
		ramp.SetOrigin(lifted);
		ramp.Update();
		// The middle of the ramp's footprint, where the car's wheelbase sits.
		vector middle = ramp.CoordToParent((mins + maxs) * 0.5);
		float surfaceY;
		s_fTilt = Tilt(middle, surfaceY);
		if (s_fTilt < 0)
			return string.Format("error ramp_has_no_surface bounds=%1..%2", mins, maxs);

		vector at = middle;
		at[1] = surfaceY + DROP;
		// Facing along the fall line: the ramp's own forward axis is its length.
		vector angles = ramp.GetYawPitchRoll();
		IEntity car = LF_Utils.SpawnEntityPrefab(LF_VehicleCatalog.S105_LOCKED, at, Vector(angles[0], 0, 0));
		if (!car)
			return "error no_car";

		s_Car = car.GetID();
		s_vStart = at;
		// An empty body goes to sleep and then sits still on any slope, which proves nothing. The
		// proof keeps it awake, so only the brake holds it.
		Wake();
		GetGame().GetCallqueue().Remove(Wake);
		GetGame().GetCallqueue().CallLater(Wake, WAKE_MS, true);
		LF_Debug.Info(FEATURE, string.Format("harness slope car at %1 tilt=%2 brake=%3", at, s_fTilt, Braked(car)));
		return string.Format("ok car at %1 tilt=%2deg ramp bounds=%3..%4 brake=%5", at, Math.Round(s_fTilt * 10) / 10, mins, maxs, Braked(car));
	}

	//------------------------------------------------------------------------------------------------
	protected static void Wake()
	{
		IEntity car = GetGame().GetWorld().FindEntityByID(s_Car);
		if (car && car.GetPhysics())
			car.GetPhysics().SetActive(ActiveState.ACTIVE);
	}

	//------------------------------------------------------------------------------------------------
	static string Check()
	{
		IEntity car = GetGame().GetWorld().FindEntityByID(s_Car);
		if (!car)
			return "error no_car";

		vector now = car.GetOrigin();
		// The first reading is taken a moment after the spawn, once the car has settled onto its
		// wheels: the proof compares two readings, not the spawn height.
		float moved = vector.Distance(Vector(now[0], 0, now[2]), Vector(s_vStart[0], 0, s_vStart[2]));
		float speed = 0;
		string sim = "";
		VehicleWheeledSimulation simulation = VehicleWheeledSimulation.Cast(car.FindComponent(VehicleWheeledSimulation));
		if (simulation)
		{
			speed = simulation.GetSpeedKmh();
			sim = string.Format(" gear=%1 simbrake=%2 clutch=%3 throttle=%4 engine=%5", simulation.GetGear(), simulation.GetBrake(), simulation.GetClutch(), simulation.GetThrottle(), simulation.EngineIsOn());
		}

		return string.Format("ok at=%1 moved=%2m flat speed=%3kmh brake=%4 tilt=%5deg%6", now, Math.Round(moved * 100) / 100, Math.Round(speed * 10) / 10, Braked(car), Math.Round(s_fTilt * 10) / 10, sim);
	}

	//------------------------------------------------------------------------------------------------
	static string Drive()
	{
		IEntity car = GetGame().GetWorld().FindEntityByID(s_Car);
		if (!car)
			return "error no_car";

		// What a driver does on getting in: the brake off, the engine on, the pedal down.
		CarControllerComponent controller = Controller(car);
		if (controller)
		{
			controller.SetPersistentHandBrake(false);
			controller.StartEngine();
		}

		VehicleWheeledSimulation simulation = VehicleWheeledSimulation.Cast(car.FindComponent(VehicleWheeledSimulation));
		if (simulation)
		{
			// The controller feeds the simulation its brake input only while a pilot sits in, so with
			// no one aboard the harness lets go of the simulation's own brake input as well.
			simulation.SetBreak(0, false);
			simulation.SetThrottle(1);
		}

		LF_Debug.Info(FEATURE, "harness slope car released and throttled");
		return string.Format("ok released brake=%1", Braked(car));
	}

	//------------------------------------------------------------------------------------------------
	static void Cleanup()
	{
		GetGame().GetCallqueue().Remove(Wake);
		IEntity car = GetGame().GetWorld().FindEntityByID(s_Car);
		if (car)
			SCR_EntityHelper.DeleteEntityAndChildren(car);

		IEntity ramp = GetGame().GetWorld().FindEntityByID(s_Ramp);
		if (ramp)
			SCR_EntityHelper.DeleteEntityAndChildren(ramp);
	}
}
