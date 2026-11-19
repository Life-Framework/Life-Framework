// red-proof: `test --no-build --red LF_Test_EventSceneDrive` -> RED PROOF OK (2026-11-19): with the hook on the truck is never moved and the stuck timeout never fires, so `the truck reached the target` and `a blocked truck is moved on after the stuck timeout` fail; without it `PASS events/scene-drive (17 assertions)`.
//! Event scene drive (#3380): the parked truck of the grid cell `scene-drive` (Grid_scene_drive.layer,
//! three waypoints and a target along one lane) is driven along the scene's route. GridWorld has no
//! AIWorld, so the test moves the truck toward the point the drive names (what the AI crew does on a
//! real map) and checks the books: a truck that follows the lane reaches the target without being
//! moved on, and a truck that stands still for the stuck timeout is moved to the next route point and
//! the recovery is logged (`[LFDebug:Events]` warn, kept in `m_sLastRecovery`).
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_EventSceneDrive : LF_Test
{
	static const string SCENE = "grid_scene_drive";
	//! Vanilla civilian truck, given as a prefab (a core slot names no vehicle).
	static const string TRUCK = "{A2E23CB8DDCFE82E}Prefabs/Vehicles/Wheeled/Ural4320/Ural4320_transport_CIV_base.et";

	override string GetName()
	{
		return "events/scene-drive";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_EventSceneCrew StartScene(notnull LF_TestContext ctx)
	{
		array<string> waypoints = {"grid_scene_drive#0", "grid_scene_drive#1", "grid_scene_drive#2"};
		LF_EventScene scene = LF_EventScene.Make(SCENE, waypoints, 1, "townsfolk");
		scene.m_fSpawnStaggerSeconds = 0;
		scene.m_sVehicleSlot = TRUCK;
		LF_EventSceneCrew crew = LF_EventSceneSpawn.Start(LF_EventScenePlanner.Plan(scene, 10));
		ctx.NotNull(crew, "the scene starts");
		return crew;
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup(LF_EventSceneCrew crew, vector near)
	{
		if (!crew)
			return;

		array<vector> away = {near + Vector(LF_EventSceneSpawn.SIGHT_RANGE_M * 3, 15, 0)};
		LF_EventSceneSpawn.TryDespawn(crew, away);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// Only this feature's hooks; LF_StaticReset.ResetAll() would wipe every feature's statics.
		LF_EventScenePlanner.ResetStatics();
		LF_EventSceneSpawn.ResetStatics();
		LF_EventSceneDrive.ResetStatics();

		BaseWorld world = GetGame().GetWorld();

		// A truck that follows the lane reaches the last point.
		LF_EventSceneCrew crew = StartScene(ctx);
		if (!crew)
			return;

		IEntity truck = world.FindEntityByID(crew.m_VehicleId);
		ctx.NotNull(truck, "the scene parked its truck");
		LF_EventSceneDrive drive = LF_EventSceneDrive.Begin(crew, false, false);
		ctx.NotNull(drive, "the drive starts");
		if (!truck || !drive)
		{
			Cleanup(crew, vector.Zero);
			return;
		}

		ctx.True(LF_EventSceneDrive.Begin(crew, false, false) == null, "a second drive is refused");
		ctx.Equal(4, drive.m_aRoute.Count(), "the route is the three waypoints and the target");
		vector start = truck.GetOrigin();
		int order = 0;
		if (!RedProof())
		{
			for (int i = 0; i < drive.m_aRoute.Count(); i++)
			{
				vector point = drive.m_aRoute[i];
				truck.SetOrigin(point);
				drive.Step(1);
				order = drive.m_iNext;
			}
		}

		ctx.True(drive.m_bArrived, "the truck reached the target");
		ctx.Equal(0, drive.m_iRecoveries, "a truck that follows the lane is never moved on");
		ctx.Equal(3, order, "it passed the three waypoints in order");
		ctx.True(LF_EventSceneDrive.LastOver().Contains("reached the target"), "the drive over line says it arrived (" + LF_EventSceneDrive.LastOver() + ")");
		drive.Stop();
		Cleanup(crew, start);

		// A blocked truck stands still: after the stuck timeout it is moved on and the warning is logged.
		crew = StartScene(ctx);
		if (!crew)
			return;

		truck = world.FindEntityByID(crew.m_VehicleId);
		drive = LF_EventSceneDrive.Begin(crew, false, false);
		ctx.NotNull(drive, "the second drive starts");
		if (!truck || !drive)
		{
			Cleanup(crew, vector.Zero);
			return;
		}

		start = truck.GetOrigin();
		drive.Step(1);
		for (int s = 0; s < LF_EventSceneDrive.STUCK_SECONDS - 2; s++)
		{
			drive.Step(1);
		}

		ctx.Equal(0, drive.m_iRecoveries, "a blocked truck is not moved before the stuck timeout");
		ctx.True(drive.m_sLastRecovery.IsEmpty(), "and nothing is logged before it");
		if (!RedProof())
		{
			drive.Step(1);
			drive.Step(1);
		}

		ctx.Equal(1, drive.m_iRecoveries, "a blocked truck is moved on after the stuck timeout");
		ctx.True(drive.m_sLastRecovery.Contains("stuck"), "the recovery is logged (" + drive.m_sLastRecovery + ")");
		ctx.Equal(1, drive.m_iNext, "it now heads for the second route point");
		if (drive.m_aRoute.Count() > 1)
			ctx.True(vector.DistanceXZ(truck.GetOrigin(), drive.m_aRoute[1]) < 1, "the truck stands on the next waypoint");

		drive.Stop();
		Cleanup(crew, start);
		LF_EventScenePlanner.ResetStatics();
		LF_EventSceneSpawn.ResetStatics();
		LF_EventSceneDrive.ResetStatics();
	}
}
