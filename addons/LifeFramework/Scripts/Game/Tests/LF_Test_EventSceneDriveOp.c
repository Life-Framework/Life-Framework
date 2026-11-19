// red-proof: `test --no-build --red LF_Test_EventSceneDriveOp` -> with the hook on the scene's drive flag is off, so the operation walks and never drives: `a flagged scene starts its drive` and the wait assertions fail; without it the test passes.
//! Scene operation drives a flagged scene's vehicle (#3831): a scene with `m_bDriveVehicle` starts
//! `LF_EventSceneDrive` (not the walk) once its crew is spawned and holds its end until the drive is
//! over. The grid cell `scene-drive` has the lane. GridWorld has no AIWorld, so the test passes
//! driveAi off and moves the truck the way the AI crew would. A drive that never arrives is ended by
//! the scene's walk timeout. A flagged scene that names no vehicle is refused.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_EventSceneDriveOp : LF_Test
{
	static const string SCENE = "grid_scene_drive";
	//! Vanilla civilian truck, given as a prefab (a core slot names no vehicle).
	static const string TRUCK = "{A2E23CB8DDCFE82E}Prefabs/Vehicles/Wheeled/Ural4320/Ural4320_transport_CIV_base.et";

	override string GetName()
	{
		return "events/scene-drive-op";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_EventSceneOperation MakeOperation(bool drive, float walkTimeout)
	{
		array<string> waypoints = {"grid_scene_drive#0", "grid_scene_drive#1", "grid_scene_drive#2"};
		LF_EventScene scene = LF_EventScene.Make(SCENE, waypoints, 2, "townsfolk");
		scene.m_fSpawnStaggerSeconds = 0;
		scene.m_fDespawnDelaySeconds = 0;
		scene.m_fWalkTimeoutSeconds = walkTimeout;
		scene.m_sVehicleSlot = TRUCK;
		scene.m_bDriveVehicle = drive;
		LF_EventSceneOperation operation = new LF_EventSceneOperation("test_drive_op", scene);
		operation.m_iFreeAiOverride = 10;
		operation.m_bDriveAi = false;
		operation.m_bTick = false;
		return operation;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// Only this feature's hooks; LF_StaticReset.ResetAll() would wipe every feature's statics.
		LF_EventScenePlanner.ResetStatics();
		LF_EventSceneSpawn.ResetStatics();
		LF_EventSceneWalk.ResetStatics();
		LF_EventSceneDrive.ResetStatics();

		// The flag reaches the plan, and a flagged scene with no vehicle is refused.
		LF_EventSceneOperation noTruck = MakeOperation(true, 120);
		noTruck.m_Scene.m_sVehicleSlot = "";
		ctx.True(LF_EventScenePlanner.Validate(noTruck.m_Scene).Contains("vehicle"), "a scene that drives but has no vehicle is refused");
		LF_EventScenePlan plan = LF_EventScenePlanner.Plan(MakeOperation(true, 120).m_Scene, 10);
		ctx.True(plan.m_bGo && plan.m_bDriveVehicle, "the plan carries the drive flag");
		LF_EventScenePlanner.ResetStatics();

		// A flagged scene drives, and the scene waits for the drive.
		bool drive = true;
		// Red-proof hook: with --red the flag is off, so the scene walks and never drives.
		if (RedProof())
			drive = false;

		LF_EventSceneOperation operation = MakeOperation(drive, 120);
		ctx.True(operation.Run("test_event", "test_phase", null), "the scene goes on stage");
		LF_EventSceneCrew crew = operation.Crew();
		ctx.NotNull(crew, "the operation holds the crew");
		if (!crew)
			return;

		ctx.True(LF_EventSceneWalk.Active() == null, "a scene that drives does not walk");
		ctx.True(crew.IsFullySpawned(), "the crew is spawned");
		ctx.False(operation.Advance(1), "the scene is on stage on its first tick");
		LF_EventSceneDrive driving = LF_EventSceneDrive.Active();
		ctx.NotNull(driving, "a flagged scene starts its drive");
		BaseWorld world = GetGame().GetWorld();
		IEntity truck = world.FindEntityByID(crew.m_VehicleId);
		ctx.NotNull(truck, "the scene parked its truck");
		if (!driving || !truck)
		{
			array<vector> far = {Vector(100000, 0, 100000)};
			LF_EventSceneSpawn.TryDespawn(crew, far);
			LF_EventScenePlanner.ResetStatics();
			LF_EventSceneSpawn.ResetStatics();
			LF_EventSceneWalk.ResetStatics();
			LF_EventSceneDrive.ResetStatics();
			return;
		}

		ctx.True(driving.m_Crew == crew, "the drive is the scene's");
		vector start = truck.GetOrigin();
		ctx.False(operation.Advance(5), "the scene waits while the truck has not arrived");
		ctx.False(operation.Advance(5), "and keeps waiting past its despawn delay");
		ctx.True(operation.Crew() == crew, "the crew is still on stage");

		for (int i = 0; i < driving.m_aRoute.Count(); i++)
		{
			truck.SetOrigin(driving.m_aRoute[i]);
			driving.Step(1);
		}

		ctx.True(driving.m_bArrived, "the truck reached the target");
		array<vector> away = {start + Vector(LF_EventSceneSpawn.SIGHT_RANGE_M * 3, 15, 0)};
		ctx.True(operation.Advance(1, away), "the drive is over: the scene ends");
		ctx.True(operation.Crew() == null, "the crew is removed");
		ctx.True(LF_EventSceneDrive.Active() == null, "the drive is stopped");
		ctx.True(LF_EventSceneDrive.LastOver().Contains("reached the target"), "the drive over line says it arrived (" + LF_EventSceneDrive.LastOver() + ")");
		ctx.EqualStr("", LF_EventScenePlanner.RunningScene(), "the scene slot is free");

		// A truck that never moves: the scene's walk timeout ends the drive and the scene.
		LF_EventSceneOperation stuck = MakeOperation(true, 3);
		ctx.True(stuck.Run("test_event", "test_phase", null), "the second scene goes on stage");
		crew = stuck.Crew();
		ctx.NotNull(crew, "the second operation holds the crew");
		if (!crew)
			return;

		ctx.False(stuck.Advance(1), "the second scene starts its drive");
		driving = LF_EventSceneDrive.Active();
		ctx.NotNull(driving, "the second drive is active");
		if (driving)
		{
			for (int s = 0; s < 3; s++)
			{
				driving.Step(1);
			}

			ctx.False(driving.m_bOver, "the truck is not over: it only stood still for 3 s");
		}

		ctx.True(stuck.Advance(1, away), "the walk timeout ends the drive and the scene");
		ctx.True(LF_EventSceneDrive.Active() == null, "the timed-out drive is stopped");
		ctx.True(stuck.Crew() == null, "the second crew is removed");

		LF_EventScenePlanner.ResetStatics();
		LF_EventSceneSpawn.ResetStatics();
		LF_EventSceneWalk.ResetStatics();
		LF_EventSceneDrive.ResetStatics();
	}
}
