// red-proof: Observed 2026-11-16 with `tools/cli test --no-build --only LF_Test_EventWalkRouteWait` on the unfixed walk (route given at the first spawn): `walker 0 timed out: stuck 20 s` while it waited for its stagger, then walker 1 never joined (JoinNewWalkers matched by count after walker 0 was removed); fixed, passed=3 failed=0 with the other walk tests.

// tier: WORLD
//! Scene walk route timing (#2174 follow-up): like vanilla SCR_AIGroup.SpawnUnits (members first, then
//! AddWaypointsStatic), the walk hands out its route only once every planned walker has spawned. A
//! walker waiting at the spawn marker for the rest of its family is not stuck; once the route is out,
//! a walker that stands still times out as before. GridWorld cell `event-scene` (Grid_event_scene.layer).
class LF_Test_EventWalkRouteWait : LF_Test
{
	static const string SCENE = "grid_event_scene";
	static const float STAGGER_S = 30;

	override string GetName()
	{
		return "events/walk-route-wait";
	}

	override void Run(LF_TestContext ctx)
	{
		// Only this feature's hooks; LF_StaticReset.ResetAll() would wipe every feature's statics.
		LF_EventScenePlanner.ResetStatics();
		LF_EventSceneSpawn.ResetStatics();
		LF_EventSceneWalk.ResetStatics();

		array<string> waypoints = {"grid_event_scene#0", "grid_event_scene#1", "grid_event_scene#2", "grid_event_scene#3"};
		LF_EventScene scene = LF_EventScene.Make(SCENE, waypoints, 2, "townsfolk");
		scene.m_fSpawnStaggerSeconds = STAGGER_S;
		scene.m_fWalkTimeoutSeconds = 300;
		LF_EventScenePlan plan = LF_EventScenePlanner.Plan(scene, 10);
		LF_EventSceneCrew crew = LF_EventSceneSpawn.Start(plan);
		ctx.NotNull(crew, "the scene starts");
		if (!crew)
			return;

		LF_EventSceneWalk walk = LF_EventSceneWalk.Begin(crew, false, false);
		ctx.NotNull(walk, "the walk starts");
		if (!walk)
		{
			Cleanup(crew);
			return;
		}

		array<vector> farEyes = {walk.Target() + Vector(LF_EventSceneSpawn.SIGHT_RANGE_M * 3, 15, 0)};
		ctx.Equal(1, crew.m_aPeople.Count(), "only the first walker is out before its stagger");

		// The first walker waits longer than STUCK_SECONDS for the second: waiting is not being stuck.
		float waited = 0;
		while (waited < LF_EventSceneWalk.STUCK_SECONDS + 5)
		{
			walk.Step(1, farEyes);
			waited += 1;
		}
		ctx.Equal(1, walk.m_aWalkers.Count(), "one walker in the walk while the second waits for its stagger");
		ctx.False(walk.m_aWalkers[0].m_bTimedOut, "the first walker waiting for its family did not time out");

		// The second walker spawns; the route goes out. Standing still from here on is stuck.
		LF_EventSceneSpawn.SpawnDue(crew, STAGGER_S);
		ctx.True(crew.IsFullySpawned(), "every planned walker has spawned");
		float stood = 0;
		while (stood < LF_EventSceneWalk.STUCK_SECONDS + 2)
		{
			walk.Step(1, farEyes);
			stood += 1;
		}
		ctx.Equal(2, walk.m_aWalkers.Count(), "both walkers are in the walk");
		if (walk.m_aWalkers.Count() < 2)
		{
			walk.Stop();
			Cleanup(crew);
			return;
		}

		ctx.True(walk.m_aWalkers[0].m_bTimedOut, "once the route is out, a walker that stands still times out");
		ctx.True(walk.m_aWalkers[1].m_bTimedOut, "the second walker standing still times out too");

		walk.Stop();
		Cleanup(crew);
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup(LF_EventSceneCrew crew)
	{
		array<vector> away = {Vector(-5000, 50, -5000)};
		LF_EventSceneSpawn.TryDespawn(crew, away);
		LF_EventSceneWalk.ResetStatics();
		LF_EventSceneSpawn.ResetStatics();
		LF_EventScenePlanner.ResetStatics();
	}
}
