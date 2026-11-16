// red-proof: `test --no-build --red LF_Test_EventModuleLoader` -> RED PROOF OK (2026-11-16): with the hook on nothing registers, so `the walk phase puts the scene on stage` and `the busy scene holds its phase` fail; without it `SUMMARY tier=only passed=1 failed=0`.
// tier: WORLD
//! Event module loader (#2137): an event config's module refs register by the root class of their
//! .conf (an hour window and a players-nearby condition, a phase-length abort, a scene) and a ref of
//! the wrong class is refused. Then an event that names them runs through the shell: outside the hour
//! it stays idle, at dawn it starts, advances to its walk phase, whose scene operation spawns the
//! walkers on the grid cell `event-scene` and starts their walk. The busy scene holds the phase; once
//! the walk is over and nobody sees the walkers they are removed and the event ends into cooldown.
//! GridWorld has no AIWorld, so the test passes the AI budget and steps the scene's end by hand.
class LF_Test_EventModuleLoader : LF_Test
{
	static const string EVENT_ID = "test_module_loader";
	static const ResourceName HOUR_CONF = "{D9FE49355E2B6E5B}Configs/Tests/LF_Test_EventModuleHour.conf";
	static const ResourceName NEARBY_CONF = "{813977DD05AA8F8C}Configs/Tests/LF_Test_EventModuleNearby.conf";
	static const ResourceName ABORT_CONF = "{1A437E9D4EB96AA7}Configs/Tests/LF_Test_EventModuleAbort.conf";
	static const ResourceName SCENE_CONF = "{8F5F26A14F89DF35}Configs/Tests/LF_Test_EventModuleScene.conf";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "events/module-loader";
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_EventModuleRef Ref(string id, ResourceName config)
	{
		LF_EventModuleRef entry = new LF_EventModuleRef();
		entry.m_sId = id;
		entry.m_Config = config;
		return entry;
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

		array<string> gate = {"test_ml_dawn", "test_ml_nearby"};
		array<string> walkModules = {"test_ml_scene", "test_ml_too_long"};
		array<ref LF_EventPhase> phases = {};
		phases.Insert(LF_EventPhase.Make("gate", 0, 0, gate));
		phases.Insert(LF_EventPhase.Make("walk", 0, 0, walkModules));
		LF_EventsConfig config = new LF_EventsConfig();
		config.m_aEvents = {};
		config.m_aEvents.Insert(LF_EventDef.Make(EVENT_ID, phases, 300));
		config.m_aModules = {};
		config.m_aModules.Insert(Ref("test_ml_dawn", HOUR_CONF));
		config.m_aModules.Insert(Ref("test_ml_nearby", NEARBY_CONF));
		config.m_aModules.Insert(Ref("test_ml_too_long", ABORT_CONF));
		config.m_aModules.Insert(Ref("test_ml_scene", SCENE_CONF));
		// An event list is not a module: refused, logged, and the rest still register.
		config.m_aModules.Insert(Ref("test_ml_wrong", LF_EventOverlay.CONFIG_PATH));

		LF_EventShell.Use(config);
		int registered;
		// Red-proof hook: with --red nothing registers, so nobody runs the event.
		if (!RedProof())
			registered = LF_EventModuleLoader.RegisterFrom(config);

		ctx.Equal(4, registered, "the condition, players-nearby, abort and scene refs register");
		ctx.True(LF_EventShell.IsRegistered("test_ml_dawn"), "the hour window is a module");
		ctx.True(LF_EventShell.IsRegistered("test_ml_nearby"), "the players-nearby condition is a module");
		ctx.True(LF_EventShell.IsRegistered("test_ml_too_long"), "the abort is a module");
		ctx.False(LF_EventShell.IsRegistered("test_ml_wrong"), "a conf that is no module is refused");

		LF_EventSceneOperation operation = LF_EventSceneOperation.Cast(LF_EventShell.FindOperation("test_ml_scene"));
		ctx.NotNull(operation, "the scene conf registers as a scene operation");
		if (operation)
		{
			operation.m_iFreeAiOverride = 10;
			operation.m_bDriveAi = false;
			operation.m_bTick = false;
		}

		LF_EventShell.ForceHour(12);
		LF_EventShell.Tick(1000);
		ctx.EqualStr("", LF_EventShell.PhaseOf(EVENT_ID), "at noon the dawn event stays idle");

		LF_EventShell.ForceHour(6);
		LF_EventShell.Tick(1001);
		ctx.EqualStr("gate", LF_EventShell.PhaseOf(EVENT_ID), "at dawn the event starts");
		LF_EventShell.Tick(1002);
		ctx.EqualStr("walk", LF_EventShell.PhaseOf(EVENT_ID), "its gate holds and it moves on to the walk");

		LF_EventShell.Tick(1003);
		LF_EventSceneCrew crew;
		if (operation)
			crew = operation.Crew();
		ctx.NotNull(crew, "the walk phase puts the scene on stage");
		if (crew)
		{
			ctx.Equal(2, crew.m_aPeople.Count(), "both walkers spawned");
			ctx.True(LF_EventSceneSpawn.Running() == crew, "the scene is the running one");
			LF_EventSceneWalk walk = LF_EventSceneWalk.Active();
			ctx.True(walk && walk.m_Crew == crew, "the walkers walk the scene's route");
		}

		LF_EventShell.Tick(1004);
		ctx.EqualStr("walk", LF_EventShell.PhaseOf(EVENT_ID), "the busy scene holds its phase");
		if (operation)
			ctx.True(operation.Crew() == crew, "a second run does not stage the scene again");

		// The walk ends (the AI would bring them to the target), then nobody is near to see them go.
		if (LF_EventSceneWalk.Active())
			LF_EventSceneWalk.Active().Stop();
		array<vector> farEyes = {Vector(0, 2000, 0)};
		bool gone;
		if (operation)
			gone = operation.Advance(LF_EventSceneOperation.DESPAWN_RETRY_S + 2, farEyes);
		ctx.True(gone, "after the despawn delay the walkers leave out of sight");
		ctx.True(LF_EventSceneSpawn.Running() == null, "no scene is running any more");

		LF_EventShell.Tick(1005);
		ctx.EqualStr("", LF_EventShell.PhaseOf(EVENT_ID), "with the scene gone the event ends");
		LF_EventShell.Tick(1006);
		ctx.EqualStr("", LF_EventShell.PhaseOf(EVENT_ID), "and stays in cooldown: the scene does not play twice");
		if (operation)
			ctx.True(operation.Crew() == null, "nothing is on stage after the end");

		// Leave the live shell as the game had it: its own config and modules, ticking.
		if (crew)
			LF_EventSceneSpawn.TryDespawn(crew, farEyes);
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
