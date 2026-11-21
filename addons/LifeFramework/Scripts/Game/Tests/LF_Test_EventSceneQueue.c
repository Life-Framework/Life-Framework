// red-proof: observed 2026-11-19 (#3780) on main before the fix: `tools\cli test --no-build --only LF_Test_EventSceneQueue` failed (`the waiting event stays in its walk phase`, `the second scene goes on stage once the first has left`): the scene refused for the busy stage left nothing busy, the last phase ended the event into its cooldown and its scene never played.
//! Two events open the same dawn and each runs a scene (#3780): one scene is on stage at a time, so the second
//! must wait for the first and then play, not end into its cooldown unplayed. The event whose phase opens
//! first takes the stage; the other holds its walk phase (its idle clock restarts while it waits, so even a
//! short idle timeout cannot end it) and its scene goes on stage the cadence after the first has left. Runs the
//! real shell and scene operation on the grid cell `event-scene`; GridWorld has no AIWorld, so the test passes
//! the AI budget and steps the scenes' end by hand.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_EventSceneQueue : LF_Test
{
	static const string FIRST_ID = "test_queue_first";
	static const string SECOND_ID = "test_queue_second";
	static const ResourceName HOUR_CONF = "{D9FE49355E2B6E5B}Configs/Tests/LF_Test_EventModuleHour.conf";
	static const ResourceName SCENE_CONF = "{8F5F26A14F89DF35}Configs/Tests/LF_Test_EventModuleScene.conf";
	static const float IDLE_S = 100;	// the second event's walk idle timeout: shorter than the first scene's stay
	static const float LONG_IDLE_S = 1000;	// the first event's: its own scene stays well inside it

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "events/scene-queue";
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
	protected static LF_EventDef MakeEvent(string eventId, string gateModule, string sceneModule, float walkIdleSeconds)
	{
		array<string> gate = {gateModule};
		array<string> walk = {sceneModule};
		array<ref LF_EventPhase> phases = {};
		phases.Insert(LF_EventPhase.Make("gate", 0, 0, gate));
		phases.Insert(LF_EventPhase.Make("walk", 0, walkIdleSeconds, walk));
		return LF_EventDef.Make(eventId, phases, 300);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Setup(LF_EventSceneOperation operation)
	{
		if (!operation)
			return;

		operation.m_iFreeAiOverride = 10;
		operation.m_bDriveAi = false;
		operation.m_bTick = false;
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

		LF_EventsConfig config = new LF_EventsConfig();
		config.m_aEvents = {};
		config.m_aEvents.Insert(MakeEvent(FIRST_ID, "tq_gate_a", "tq_scene_a", LONG_IDLE_S));
		config.m_aEvents.Insert(MakeEvent(SECOND_ID, "tq_gate_b", "tq_scene_b", IDLE_S));
		config.m_aModules = {};
		config.m_aModules.Insert(Ref("tq_gate_a", HOUR_CONF));
		config.m_aModules.Insert(Ref("tq_gate_b", HOUR_CONF));
		config.m_aModules.Insert(Ref("tq_scene_a", SCENE_CONF));
		config.m_aModules.Insert(Ref("tq_scene_b", SCENE_CONF));

		LF_EventShell.Use(config);
		ctx.Equal(4, LF_EventModuleLoader.RegisterFrom(config), "both gates and both scenes register");
		LF_EventSceneOperation first = LF_EventSceneOperation.Cast(LF_EventShell.FindOperation("tq_scene_a"));
		LF_EventSceneOperation second = LF_EventSceneOperation.Cast(LF_EventShell.FindOperation("tq_scene_b"));
		ctx.NotNull(first, "the first scene registers as a scene operation");
		ctx.NotNull(second, "the second scene registers as a scene operation");
		Setup(first);
		Setup(second);
		if (!first || !second)
			return;

		// Both events open at the same dawn and reach their walk phase together.
		LF_EventShell.ForceHour(6);
		LF_EventShell.Tick(1000);
		ctx.EqualStr("gate", LF_EventShell.PhaseOf(FIRST_ID), "at dawn the first event starts");
		ctx.EqualStr("gate", LF_EventShell.PhaseOf(SECOND_ID), "at the same dawn the second event starts");
		LF_EventShell.Tick(1001);
		ctx.EqualStr("walk", LF_EventShell.PhaseOf(FIRST_ID), "the first event moves on to its walk");
		ctx.EqualStr("walk", LF_EventShell.PhaseOf(SECOND_ID), "the second event moves on to its walk");

		// The first takes the stage; the second finds it taken.
		LF_EventShell.Tick(1002);
		LF_EventSceneCrew firstCrew = first.Crew();
		ctx.NotNull(firstCrew, "the first scene goes on stage");
		ctx.True(second.Crew() == null, "the second scene finds the stage taken and does not go on");
		ctx.EqualStr("walk", LF_EventShell.PhaseOf(SECOND_ID), "the second event holds its walk phase");

		// The first scene stays longer than the walk phase's idle timeout; the waiting one is not timed out.
		LF_EventShell.Tick(1002 + IDLE_S + 50);
		ctx.EqualStr("walk", LF_EventShell.PhaseOf(FIRST_ID), "the first event is still on its walk");
		ctx.True(second.Crew() == null, "the second scene still waits");
		// The unfixed shell ended the waiting event at its first refused tick (red proof above).
		ctx.EqualStr("walk", LF_EventShell.PhaseOf(SECOND_ID), "the waiting event stays in its walk phase");

		// The first scene's walk ends and nobody sees the walkers leave.
		if (LF_EventSceneWalk.Active())
			LF_EventSceneWalk.Active().Stop();
		array<vector> farEyes = {Vector(0, 2000, 0)};
		ctx.True(first.Advance(LF_EventSceneOperation.DESPAWN_RETRY_S + 2, farEyes), "the first scene leaves out of sight");
		ctx.True(LF_EventSceneSpawn.Running() == null, "the stage is free");

		// The next cadence the second scene goes on stage, and the first event ends having played.
		LF_EventShell.Tick(1002 + IDLE_S + 51);
		LF_EventSceneCrew secondCrew = second.Crew();
		ctx.NotNull(secondCrew, "the second scene goes on stage once the first has left");
		ctx.EqualStr("walk", LF_EventShell.PhaseOf(SECOND_ID), "the second event is on its walk while its scene plays");
		ctx.EqualStr("", LF_EventShell.PhaseOf(FIRST_ID), "the first event has ended after its scene");

		if (LF_EventSceneWalk.Active())
			LF_EventSceneWalk.Active().Stop();
		ctx.True(second.Advance(LF_EventSceneOperation.DESPAWN_RETRY_S + 2, farEyes), "the second scene leaves out of sight");
		LF_EventShell.Tick(1002 + IDLE_S + 52);
		ctx.EqualStr("", LF_EventShell.PhaseOf(SECOND_ID), "the second event ends after its scene played");

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
