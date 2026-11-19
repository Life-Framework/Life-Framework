// red-proof: observed 2026-11-19 (#3780) on main before the fix: `tools\cli test --no-build --only LF_Test_EveronDawnQueue` failed (`the shipment holds its unload phase while the arrival is on stage`, `the shipment docks once the arrival has left`): the shipment's scene was refused for the busy stage, nothing held its last phase and the event ended into its week-long cooldown unplayed.
//! #3780 under the Everon 1989 header (the pack leg of `tools\cli test`): the relocated arrival and the shipment
//! docking both open at dawn and only one scene runs at a time. Both pack events run through the real shell
//! from the merged event table, at an hour inside both dawn windows, each with its real module confs and phase
//! cadences. The two scenes are pointed at the grid cell `event-scene` (the test world holds no island markers
//! and the AI budget is passed in), and the test steps the arrival's end by hand. The arrival takes the stage,
//! the shipment waits well past a game-morning's worth of ticks without timing out, and goes on stage the cadence
//! after the arrival has left: both play in one morning.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronDawnQueue : LF_Test
{
	static const string ARRIVAL_ID = "relocated_arrival";
	static const string ARRIVAL_SCENE_MODULE = "relocated_arrival_scene";
	static const string SHIPMENT_ID = "shipment_docks";
	static const string SHIPMENT_SCENE_MODULE = "shipment_docks_scene";
	static const string GRID_SCENE = "grid_event_scene";
	static const float MORNING_HOUR = 6.5;	// inside both dawn windows
	static const float STEP_S = 60;			// a wait of this many seconds between ticks, as the shipment's idle clock sees it

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/dawn-queue";
	}

	//------------------------------------------------------------------------------------------------
	//! Points a pack scene at the grid cell's markers and drops what the test world cannot hold, keeping the
	//! scene's own walkers, count and timings.
	protected static LF_EventSceneOperation Prepare(string moduleId)
	{
		LF_EventSceneOperation operation = LF_EventSceneOperation.Cast(LF_EventShell.FindOperation(moduleId));
		if (!operation || !operation.m_Scene)
			return null;

		operation.m_Scene.m_sSceneId = GRID_SCENE;
		operation.m_Scene.m_aWaypointIds = {};
		for (int i = 0; i < 4; i++)
			operation.m_Scene.m_aWaypointIds.Insert(LF_Places.OrderedId(GRID_SCENE, i));
		operation.m_Scene.m_sVehicleSlot = "";
		operation.m_Scene.m_fSpawnStaggerSeconds = 0;	// every walker spawns at once, so the scene is fully on stage in one tick
		operation.m_iFreeAiOverride = 20;
		operation.m_bDriveAi = false;
		operation.m_bTick = false;
		return operation;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// Only this feature's statics; LF_StaticReset.ResetAll() would wipe every feature's.
		LF_EventShell.Stop();
		LF_EventModuleLoader.UnregisterLoaded();
		LF_EventScenePlanner.ResetStatics();
		LF_EventSceneSpawn.ResetStatics();
		LF_EventSceneWalk.ResetStatics();

		LF_EventsConfig events = LF_EventShell.LoadConfig();
		ctx.NotNull(events, "the merged event table loads");
		if (!events)
			return;

		LF_EventShell.Use(events);
		LF_EventModuleLoader.RegisterFrom(events);
		LF_EventSceneOperation arrival = Prepare(ARRIVAL_SCENE_MODULE);
		LF_EventSceneOperation shipment = Prepare(SHIPMENT_SCENE_MODULE);
		ctx.NotNull(arrival, "the arrival's scene registers as a scene operation");
		ctx.NotNull(shipment, "the shipment's scene registers as a scene operation");
		if (!arrival || !shipment)
			return;

		float now = 5000;
		LF_EventShell.ForceHour(MORNING_HOUR);
		LF_EventShell.Tick(now);
		ctx.EqualStr("dawn", LF_EventShell.PhaseOf(ARRIVAL_ID), "at dawn the arrival event starts");
		ctx.EqualStr("dawn", LF_EventShell.PhaseOf(SHIPMENT_ID), "at the same dawn the shipment event starts");

		// Their dawn gates pass on the next cadence and both reach their scene phase.
		now += 10;
		LF_EventShell.Tick(now);
		ctx.EqualStr("walk", LF_EventShell.PhaseOf(ARRIVAL_ID), "the arrival moves on to its walk");
		ctx.EqualStr("unload", LF_EventShell.PhaseOf(SHIPMENT_ID), "the shipment moves on to its unload");

		// The arrival is first in the table and takes the stage; the shipment finds it taken.
		now += 10;
		LF_EventShell.Tick(now);
		LF_EventSceneCrew arrivalCrew = arrival.Crew();
		ctx.NotNull(arrivalCrew, "the arrival goes on stage");
		ctx.True(shipment.Crew() == null, "the shipment finds the stage taken and does not go on");
		ctx.EqualStr("unload", LF_EventShell.PhaseOf(SHIPMENT_ID), "the shipment holds its unload phase while the arrival is on stage");

		// Ten minutes on, the arrival is still walking and the shipment is still waiting, not timed out.
		for (int i = 0; i < 10; i++)
		{
			now += STEP_S;
			LF_EventShell.Tick(now);
		}
		ctx.True(shipment.Crew() == null, "the shipment still waits for the stage");
		ctx.EqualStr("unload", LF_EventShell.PhaseOf(SHIPMENT_ID), "the waiting shipment is not timed out into its cooldown");

		// The arrival's walk ends and nobody sees the walkers leave.
		if (LF_EventSceneWalk.Active())
			LF_EventSceneWalk.Active().Stop();
		array<vector> farEyes = {Vector(0, 2000, 0)};
		ctx.True(arrival.Advance(LF_EventSceneOperation.DESPAWN_RETRY_S + 100, farEyes), "the arrival's walkers leave out of sight");
		ctx.True(LF_EventSceneSpawn.Running() == null, "the stage is free");

		// The next cadences: the arrival ends, the shipment docks.
		now += 10;
		LF_EventShell.Tick(now);
		LF_EventSceneCrew shipmentCrew = shipment.Crew();
		ctx.NotNull(shipmentCrew, "the shipment docks once the arrival has left");
		ctx.EqualStr("", LF_EventShell.PhaseOf(ARRIVAL_ID), "the arrival has ended after its scene");
		ctx.EqualStr("unload", LF_EventShell.PhaseOf(SHIPMENT_ID), "the shipment is on its unload while its scene plays");

		if (LF_EventSceneWalk.Active())
			LF_EventSceneWalk.Active().Stop();
		ctx.True(shipment.Advance(LF_EventSceneOperation.DESPAWN_RETRY_S + 100, farEyes), "the shipment's crew leaves out of sight");
		now += 10;
		LF_EventShell.Tick(now);
		ctx.EqualStr("", LF_EventShell.PhaseOf(SHIPMENT_ID), "the shipment ends after its scene played");

		// Leave the live shell as the game had it: its own config and modules, ticking.
		LF_EventModuleLoader.UnregisterLoaded();
		LF_EventScenePlanner.ResetStatics();
		LF_EventSceneSpawn.ResetStatics();
		LF_EventSceneWalk.ResetStatics();
		LF_EventShell.ForceHour(-1);
		LF_EventsConfig live = LF_EventShell.LoadConfig();
		LF_EventShell.Use(live);
		LF_EventModuleLoader.RegisterFrom(live);
		LF_EventShell.StartIfServer();
	}
}
