//------------------------------------------------------------------------------------------------
//! Harness stand-ins for the spike strip proof (#646, agent-box/proofs/spike-strip-proof.mjs). The
//! harness cannot walk a player to a road or drive a car (movement keys are refused), so:
//!   spikeverge  the server puts the asking character on the main street's south verge, facing
//!               north across the road (the spot LF_Test_SpikeStrip uses)
//!   spikecar    the server sets an empty S105 on the road 12 m before the nearest deployed strip,
//!               in line with its far half, and sends it rolling across at about 30 km/h
//!   spikecheck  the server's view of that car: each wheel's damage state and radius, its speed
//!   spikeseen   (client) the same wheels as this machine sees them: the replicated damage
//! Everything after the push is the real path: the strip's own tick, the vanilla wheel damage.
//------------------------------------------------------------------------------------------------
class LF_SpikeStripHarness
{
	static const ResourceName CAR = "{54C3CC22DEBD57BE}Prefabs/Vehicles/Wheeled/S105/S105_beige.et";
	static const vector VERGE = "300 0 245.5";
	//! Metres before the strip the car starts, and its speed across (m/s).
	static const float RUN_UP = 12;
	static const float SPEED = 8.5;

	protected static EntityID s_LastCar;
	protected static vector s_vPush;

	//------------------------------------------------------------------------------------------------
	static string Verge(IEntity character)
	{
		if (!character)
			return "no_character";

		vector at = VERGE;
		at[1] = GetGame().GetWorld().GetSurfaceY(at[0], at[2]);
		vector transform[4];
		Math3D.AnglesToMatrix("0 0 0", transform);
		transform[3] = at;
		character.SetWorldTransform(transform);
		character.Update();
		return string.Format("verge at %1 facing north", at);
	}

	//------------------------------------------------------------------------------------------------
	static string LaunchCar(IEntity character)
	{
		if (!character)
			return "error no_character";

		LF_SpikeStripComponent strip = LF_SpikeStrips.Nearest(character.GetOrigin(), 40);
		if (!strip || !strip.GetOwner())
			return "error no_strip";

		vector transform[4];
		strip.GetOwner().GetWorldTransform(transform);
		vector axis = LF_SpikeStripGeometry.Flat(transform[2]);
		vector across = Vector(axis[2], 0, -axis[0]);
		// In line with the strip's far half: the wheels on one side cross it, the others pass its end.
		vector start = transform[3] + axis * 1.75 - across * RUN_UP;
		start[1] = GetGame().GetWorld().GetSurfaceY(start[0], start[2]) + 0.5;
		float yaw = Math.Atan2(across[0], across[2]) * Math.RAD2DEG;
		IEntity car = LF_Utils.SpawnEntityPrefab(CAR, start, Vector(yaw, 0, 0));
		if (!car)
			return "error no_car";

		s_LastCar = car.GetID();
		CarControllerComponent controller = CarControllerComponent.Cast(car.FindComponent(CarControllerComponent));
		if (controller)
			controller.SetPersistentHandBrake(false);

		s_vPush = across * SPEED;
		GetGame().GetCallqueue().CallLater(Push, 300, false);
		LF_Debug.Info(LF_SpikeStrips.FEATURE, string.Format("harness car at %1 heading %2, %3 m before the strip", start, across, RUN_UP));
		return string.Format("ok car at %1 heading %2", start, across);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Push()
	{
		IEntity car = GetGame().GetWorld().FindEntityByID(s_LastCar);
		if (!car || !car.GetPhysics())
		{
			LF_Debug.Error(LF_SpikeStrips.FEATURE, "harness car push: the car or its physics is gone");
			return;
		}

		car.GetPhysics().SetActive(ActiveState.ACTIVE);
		car.GetPhysics().SetVelocity(s_vPush);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the launched car's wheels. Client: the nearest vehicle's wheels as replicated here.
	static string Describe(IEntity vehicle, string who)
	{
		if (!vehicle)
			return "error no_car";

		array<SCR_WheelHitZone> wheels = {};
		LF_SpikeStrips.Wheels(vehicle, wheels);
		VehicleWheeledSimulation simulation = VehicleWheeledSimulation.Cast(vehicle.FindComponent(VehicleWheeledSimulation));
		string text = string.Format("%1 car at %2", who, vehicle.GetOrigin());
		if (simulation)
			text = text + string.Format(" speed=%1kmh", Math.Round(simulation.GetSpeedKmh()));

		int flat = 0;
		foreach (SCR_WheelHitZone wheel : wheels)
		{
			int index = wheel.GetWheelIndex();
			string radius = "";
			if (simulation && simulation.IsValid() && index < simulation.WheelCount())
				radius = string.Format(" r=%1/%2", simulation.WheelGetRadiusState(index), simulation.WheelGetRadius(index));

			bool destroyed = wheel.GetDamageState() == EDamageState.DESTROYED;
			if (destroyed)
				flat++;

			text = text + string.Format(" w%1=%2%3", index, wheel.GetDamageState(), radius);
		}

		LF_SpikeStripComponent strip = LF_SpikeStrips.Nearest(vehicle.GetOrigin(), 40);
		string punctures = "-";
		if (strip)
			punctures = strip.GetPunctures().ToString();

		return string.Format("ok flat=%1 of %2 punctures=%3 %4", flat, wheels.Count(), punctures, text);
	}

	//------------------------------------------------------------------------------------------------
	static string Check()
	{
		return Describe(GetGame().GetWorld().FindEntityByID(s_LastCar), "server");
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the vehicle nearest the local character, as this machine has it.
	static string Seen()
	{
		SCR_ChimeraCharacter character = LF_Utils.GetLocalCharacter();
		if (!character)
			return "error no_character";

		// The car that crossed: of the vehicles near the strip, the nearest with a flat wheel (the
		// fuel station's cars stand nearby), else the nearest.
		vector from = character.GetOrigin();
		LF_SpikeStripComponent strip = LF_SpikeStrips.Nearest(from, 40);
		if (strip && strip.GetOwner())
			from = strip.GetOwner().GetOrigin();

		array<IEntity> vehicles = LF_Utils.GetNearbyVehicles(from, 60);
		IEntity nearest;
		IEntity nearestFlat;
		float best = 1000;
		float bestFlat = 1000;
		array<SCR_WheelHitZone> wheels = {};
		foreach (IEntity vehicle : vehicles)
		{
			float d = vector.Distance(vehicle.GetOrigin(), from);
			if (d < best)
			{
				best = d;
				nearest = vehicle;
			}

			LF_SpikeStrips.Wheels(vehicle, wheels);
			foreach (SCR_WheelHitZone wheel : wheels)
			{
				if (wheel.GetDamageState() == EDamageState.DESTROYED && d < bestFlat)
				{
					bestFlat = d;
					nearestFlat = vehicle;
				}
			}
		}

		if (nearestFlat)
			return Describe(nearestFlat, "client");

		return Describe(nearest, "client");
	}
}
