// red-proof: 2026-11-18 batched `--red` RED PROOF OK: with the hook on the watched walkers and the watched scene are never advanced past the force window, so "removed in sight" and "the scene goes once it has waited the force window" fail. Green with the fix.
// tier: WORLD
//! #2406: a player who keeps an event scene in sight cannot hold it forever. Past the plan's
//! m_fForceDespawnSeconds a timed-out walker and the scene's despawn go anyway (so the phase, the
//! scene slot and the AI budget come free), and the players' eyes are collected once per walk step,
//! not once per waiting walker. The grid cell `event-scene` has the spawn markers; the eyes are passed
//! in, like in the other scene tests.
class LF_Test_EventForceDespawn : LF_Test
{
	static const string SCENE = "grid_event_scene";
	static const float WALK_BUDGET_S = 60;
	static const float FORCE_S = 30;

	override string GetName()
	{
		return "events/force-despawn";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_EventScene BuildScene()
	{
		array<string> waypoints = {"grid_event_scene#0", "grid_event_scene#1", "grid_event_scene#2", "grid_event_scene#3"};
		LF_EventScene scene = LF_EventScene.Make(SCENE, waypoints, 3, "townsfolk");
		scene.m_fSpawnStaggerSeconds = 0;
		scene.m_fWalkTimeoutSeconds = WALK_BUDGET_S;
		scene.m_fDespawnDelaySeconds = 5;
		scene.m_fForceDespawnSeconds = FORCE_S;
		return scene;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// Only this feature's hooks; LF_StaticReset.ResetAll() would wipe every feature's statics.
		LF_EventScenePlanner.ResetStatics();
		LF_EventSceneSpawn.ResetStatics();
		LF_EventSceneWalk.ResetStatics();

		array<IEntity> spawns = {};
		ctx.True(LF_EventWaypointComponent.FindSpawns(SCENE, spawns) > 0, "the cell has a spawn marker");
		if (spawns.IsEmpty())
			return;

		array<vector> near = {spawns[0].GetOrigin() + "0 15 0"};
		array<vector> away = {spawns[0].GetOrigin() + Vector(LF_EventSceneSpawn.SIGHT_RANGE_M * 3, 15, 0)};

		// A watched, timed-out walker waits, then goes once the force window has passed.
		LF_EventSceneCrew crew = LF_EventSceneSpawn.Start(LF_EventScenePlanner.Plan(BuildScene(), 10));
		ctx.NotNull(crew, "the scene starts");
		if (!crew)
			return;

		ctx.EqualFloat(FORCE_S, crew.m_Plan.m_fForceDespawnSeconds, 0.001, "the plan carries the scene's force window");
		LF_EventSceneWalk walk = LF_EventSceneWalk.Begin(crew, false, false);
		ctx.NotNull(walk, "the walk starts");
		if (!walk)
		{
			Cleanup(crew, away);
			return;
		}

		walk.Step(0, near);
		walk.Step(WALK_BUDGET_S + 1, near);
		ctx.Equal(3, walk.CountTimedOut(), "the walkers stand still and time out");
		ctx.Equal(3, crew.m_aPeople.Count(), "a watched walker is not removed while in the force window");
		if (!RedProof())
			walk.Step(FORCE_S + 1, near);

		ctx.Equal(0, crew.m_aPeople.Count(), "past the force window the watched walkers are removed in sight");
		Cleanup(crew, away);

		// The players' eyes are read once per step, however many walkers wait.
		LF_EventScenePlanner.ResetStatics();
		LF_EventSceneSpawn.ResetStatics();
		LF_EventSceneWalk.ResetStatics();
		crew = LF_EventSceneSpawn.Start(LF_EventScenePlanner.Plan(BuildScene(), 10));
		ctx.NotNull(crew, "the second scene starts");
		if (!crew)
			return;

		walk = LF_EventSceneWalk.Begin(crew, false, false);
		if (!walk)
		{
			Cleanup(crew, away);
			return;
		}

		walk.Step(0, away);
		int scansBefore = LF_EventSceneSpawn.EyeScanCount();
		walk.Step(WALK_BUDGET_S + 1);
		ctx.Equal(3, walk.CountTimedOut(), "all three walkers time out in one step");
		ctx.Equal(0, crew.m_aPeople.Count(), "nobody is watching: all three are removed");
		ctx.Equal(scansBefore + 1, LF_EventSceneSpawn.EyeScanCount(), "the eyes were collected once for three walkers");
		Cleanup(crew, away);

		// The scene's own despawn: a watcher holds it only up to the force window.
		LF_EventScenePlanner.ResetStatics();
		LF_EventSceneSpawn.ResetStatics();
		LF_EventSceneWalk.ResetStatics();
		LF_EventSceneOperation operation = new LF_EventSceneOperation("test_force_despawn", BuildScene());
		operation.m_iFreeAiOverride = 10;
		operation.m_bDriveAi = false;
		operation.m_bTick = false;
		ctx.True(operation.Run("test_force_despawn_event", "walk", null), "the operation puts the scene on stage");
		LF_EventSceneCrew staged = operation.Crew();
		ctx.NotNull(staged, "the operation holds the crew");
		if (LF_EventSceneWalk.Active())
			LF_EventSceneWalk.Active().Stop();

		ctx.False(operation.Advance(LF_EventSceneOperation.DESPAWN_RETRY_S + 2, near), "a watched scene waits at first");
		ctx.True(LF_EventSceneSpawn.Running() == staged, "the scene still holds its slot");
		bool gone = false;
		if (!RedProof())
			gone = operation.Advance(FORCE_S + 1, near);

		ctx.True(gone, "the scene goes once it has waited the force window");
		ctx.True(LF_EventSceneSpawn.Running() == null, "and its slot is free");
		ctx.EqualStr("", LF_EventScenePlanner.RunningScene(), "the planner holds no scene");

		if (staged && LF_EventSceneSpawn.Running() == staged)
			LF_EventSceneSpawn.TryDespawn(staged, away);

		LF_EventScenePlanner.ResetStatics();
		LF_EventSceneSpawn.ResetStatics();
		LF_EventSceneWalk.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup(LF_EventSceneCrew crew, array<vector> away)
	{
		LF_EventSceneSpawn.TryDespawn(crew, away);
		LF_EventSceneWalk.ResetStatics();
		LF_EventSceneSpawn.ResetStatics();
		LF_EventScenePlanner.ResetStatics();
	}
}
