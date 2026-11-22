// red-proof: `test --no-build --red LF_Test_EventSceneVehicle` -> RED PROOF OK (2026-11-18): with the hook on the plan carries no vehicle, so `the parked vehicle spawned` fails (expected 3 entities, got 2); without it `PASS events/scene-vehicle (16 assertions)`.
//! Event scene parked vehicle (#1754): a plan of the grid cell `event-scene` (Grid_event_scene.layer)
//! with a vehicle parks it empty beside the first spawn marker, the despawn waits while a test eye
//! sees it, and removes it with the walkers once nobody does. A slot that resolves to nothing spawns
//! the walkers without a vehicle.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_EventSceneVehicle : LF_Test
{
	static const string SCENE = "grid_event_scene";
	//! Vanilla civilian car, given as a prefab (a core slot names no vehicle).
	static const string CAR = "{54C3CC22DEBD57BE}Prefabs/Vehicles/Wheeled/S105/S105_beige.et";

	override string GetName()
	{
		return "events/scene-vehicle";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_EventScenePlan MakePlan(string vehicle)
	{
		array<string> waypoints = {"grid_event_scene#0", "grid_event_scene#1"};
		LF_EventScene scene = LF_EventScene.Make(SCENE, waypoints, 2, "townsfolk");
		scene.m_fSpawnStaggerSeconds = 0;
		scene.m_sVehicleSlot = vehicle;
		return LF_EventScenePlanner.Plan(scene, 10, 3);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// Only this feature's hooks; LF_StaticReset.ResetAll() would wipe every feature's statics.
		LF_EventScenePlanner.ResetStatics();
		LF_EventSceneSpawn.ResetStatics();

		array<IEntity> spawns = {};
		LF_EventWaypointComponent.FindSpawns(SCENE, spawns);
		ctx.True(spawns.Count() > 0, "the cell holds a spawn marker");
		if (spawns.Count() == 0)
			return;

		ctx.EqualStr(CAR, LF_EventSceneSpawn.VehiclePrefabFor(CAR), "a {GUID}path is the vehicle prefab itself");

		// Red-proof hook: with --red the plan carries no vehicle, so every vehicle assertion fails.
		string vehicleSlot = CAR;
		if (RedProof())
			vehicleSlot = "";
		LF_EventSceneCrew crew = LF_EventSceneSpawn.Start(MakePlan(vehicleSlot));
		ctx.NotNull(crew, "the scene starts");
		if (!crew)
			return;

		BaseWorld world = GetGame().GetWorld();
		IEntity car = world.FindEntityByID(crew.m_VehicleId);
		ctx.NotNull(car, "the parked vehicle spawned");
		array<IEntity> entities = {};
		ctx.Equal(3, crew.GetEntities(entities), "two walkers and the vehicle belong to the scene");
		array<EntityID> ids = {};
		foreach (IEntity entity : entities)
			ids.Insert(entity.GetID());

		if (car)
		{
			ctx.True(Vehicle.Cast(car) != null, "the vehicle is a vehicle");
			LF_VehicleWorldSave.HoldAgain();
			ctx.False(LF_VehicleWorldSave.IsSaved(car), "the scene vehicle is out of the world save, so a restart puts no free copy back (#4122)");
			string prefabName = car.GetPrefabData().GetPrefabName();
			ctx.True(prefabName.EndsWith("S105_beige.et"), "the vehicle is the asked prefab (" + prefabName + ")");
			float off = vector.DistanceXZ(car.GetOrigin(), spawns[0].GetOrigin());
			ctx.True(Math.AbsFloat(off - LF_EventSceneSpawn.VEHICLE_SIDE_M) < 0.5, "the vehicle parks beside the first spawn marker (" + off + " m)");
			SCR_BaseCompartmentManagerComponent seats = SCR_BaseCompartmentManagerComponent.Cast(car.FindComponent(SCR_BaseCompartmentManagerComponent));
			if (seats)
			{
				array<IEntity> occupants = {};
				seats.GetOccupants(occupants);
				ctx.Equal(0, occupants.Count(), "nobody sits in the parked vehicle");
			}
		}

		// An eye 15 m over the car sees it: the despawn waits.
		vector over = spawns[0].GetOrigin() + "0 15 0";
		if (car)
			over = car.GetOrigin() + "0 15 0";
		array<vector> near = {over};
		ctx.False(LF_EventSceneSpawn.TryDespawn(crew, near), "despawn waits while the observer sees the scene");

		// Far out of range: the walkers and the car go together.
		vector far = spawns[0].GetOrigin() + Vector(LF_EventSceneSpawn.SIGHT_RANGE_M * 3, 15, 0);
		array<vector> away = {far};
		ctx.True(LF_EventSceneSpawn.TryDespawn(crew, away), "despawn goes once nobody sees them");
		int left = 0;
		foreach (EntityID id : ids)
		{
			if (world.FindEntityByID(id))
				left++;
		}
		ctx.Equal(0, left, "the walkers and the vehicle are removed");
		ctx.True(crew.m_VehicleId == EntityID.INVALID, "the crew forgets the vehicle");

		// A slot that resolves to nothing: the walkers still come, without a vehicle.
		LF_EventSceneCrew noCar = LF_EventSceneSpawn.Start(MakePlan("lf_test_no_such_vehicle_slot"));
		ctx.NotNull(noCar, "a scene with a broken vehicle slot still starts");
		if (noCar)
		{
			array<IEntity> people = {};
			ctx.Equal(2, noCar.GetEntities(people), "its two walkers spawn, no vehicle");
			ctx.True(noCar.m_VehicleId == EntityID.INVALID, "and no vehicle is held");
			LF_EventSceneSpawn.TryDespawn(noCar, away);
		}

		LF_EventScenePlanner.ResetStatics();
		LF_EventSceneSpawn.ResetStatics();
	}
}
