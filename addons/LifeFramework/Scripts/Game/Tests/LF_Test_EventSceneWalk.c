// red-proof: `test --no-build --red LF_Test_EventSceneWalk` -> RED PROOF OK (2026-11-17): with the hook on nobody moves, so `walker 1 reached the target` and `passed every waypoint first` fail; without it `SUMMARY tier=only passed=1 failed=0`.
// tier: WORLD
//! Event scene walk (#1956): the walkers of the grid cell `event-scene` (Grid_event_scene.layer) follow
//! the plan's waypoints in order to the target. GridWorld has no AIWorld, so the test moves the
//! walkers toward the point the walk names for each (what the AI group does on a real map) and checks
//! the books: every moving walker reaches the target inside the plan's walk budget, and a walker that
//! stands still times out with a logged `[LFDebug:Events]` line and is removed out of sight.
class LF_Test_EventSceneWalk : LF_Test
{
	static const string SCENE = "grid_event_scene";
	static const float WALK_BUDGET_S = 60;
	static const float SPEED_MPS = 1.4;

	override string GetName()
	{
		return "events/scene-walk";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// Only this feature's hooks; LF_StaticReset.ResetAll() would wipe every feature's statics.
		LF_EventScenePlanner.ResetStatics();
		LF_EventSceneSpawn.ResetStatics();
		LF_EventSceneWalk.ResetStatics();

		array<string> waypoints = {"grid_event_scene#0", "grid_event_scene#1", "grid_event_scene#2", "grid_event_scene#3"};
		LF_EventScene scene = LF_EventScene.Make(SCENE, waypoints, 3, "townsfolk");
		scene.m_fSpawnStaggerSeconds = 0;
		scene.m_fWalkTimeoutSeconds = WALK_BUDGET_S;
		LF_EventScenePlan plan = LF_EventScenePlanner.Plan(scene, 10);
		LF_EventSceneCrew crew = LF_EventSceneSpawn.Start(plan);
		ctx.NotNull(crew, "the scene starts");
		if (!crew)
			return;

		ctx.Equal(3, crew.m_aPeople.Count(), "three walkers spawned");
		LF_EventSceneWalk walk = LF_EventSceneWalk.Begin(crew, false, false);
		ctx.NotNull(walk, "the walk starts");
		if (!walk)
		{
			Cleanup(crew);
			return;
		}

		ctx.True(LF_EventSceneWalk.Begin(crew, false, false) == null, "a second walk is refused");
		ctx.Equal(5, walk.m_aRoute.Count(), "the route is the four waypoints and the target");
		IEntity first = LF_Places.Find(LF_EventWaypointComponent.KIND_WAYPOINT, "grid_event_scene#0");
		IEntity target = LF_EventWaypointComponent.FindTarget(SCENE);
		if (first && target)
		{
			ctx.True(vector.Distance(first.GetOrigin(), walk.m_aRoute[0]) < 0.01, "the route starts at waypoint #0");
			ctx.True(vector.Distance(target.GetOrigin(), walk.Target()) < 0.01, "the route ends at the target");
		}

		// Walkers 0 and 1 walk; walker 2 is blocked and stands where it spawned.
		BaseWorld world = GetGame().GetWorld();
		array<vector> farEyes = {walk.Target() +Vector(LF_EventSceneSpawn.SIGHT_RANGE_M * 3, 15, 0)};
		walk.Step(0, farEyes);
		ctx.Equal(3, walk.m_aWalkers.Count(), "every spawned walker joins the walk");
		// Walker 1 spawns 3 m from the target: standing there is not the walk, the route comes first.
		ctx.False(walk.m_aWalkers[1].m_bArrived, "a walker spawned beside the target has not arrived");

		bool inOrder = true;
		array<int> lastNext = {0, 0};
		float seconds = 0;
		while (seconds < LF_EventSceneWalk.STUCK_SECONDS + 10)
		{
			for (int i = 0; i < 2; i++)
			{
				IEntity person = world.FindEntityByID(walk.m_aWalkers[i].m_Id);
				if (!person || walk.m_aWalkers[i].m_bArrived)
					continue;

				// Red-proof hook: with --red nobody moves, so nobody reaches the target.
				if (!RedProof())
					MoveToward(person, walk.NextPoint(i), SPEED_MPS);
			}

			seconds += 1;
			walk.Step(1, farEyes);
			for (int j = 0; j < 2; j++)
			{
				if (walk.m_aWalkers[j].m_iNext < lastNext[j])
					inOrder = false;
				lastNext[j] = walk.m_aWalkers[j].m_iNext;
			}
		}

		for (int k = 0; k < 2; k++)
		{
			LF_EventSceneWalker walker = walk.m_aWalkers[k];
			ctx.True(walker.m_bArrived, "walker " + k + " reached the target");
			ctx.Equal(4, walker.m_iNext, "walker " + k + " passed every waypoint first");
			ctx.True(walker.m_fWalked <= WALK_BUDGET_S, "walker " + k + " arrived within the walk budget (" + walker.m_fWalked + " s)");
		}

		ctx.True(inOrder, "the walkers pass the waypoints in order");
		LF_EventSceneWalker blocked = walk.m_aWalkers[2];
		ctx.False(blocked.m_bArrived, "the blocked walker did not arrive");
		ctx.True(blocked.m_bTimedOut, "the blocked walker timed out");
		ctx.True(walk.m_sLastTimeout.Contains("walker 2 timed out"), "the time out is logged: " + walk.m_sLastTimeout);
		ctx.True(world.FindEntityByID(blocked.m_Id) == null, "the blocked walker is removed out of sight");
		ctx.Equal(2, crew.m_aPeople.Count(), "the crew keeps the two who arrived");
		ctx.True(walk.IsDone(), "the walk is over");

		walk.Stop();
		ctx.True(LF_EventSceneWalk.Active() == null, "a stopped walk is no longer active");
		Cleanup(crew);
	}

	//------------------------------------------------------------------------------------------------
	protected void MoveToward(IEntity person, vector point, float metres)
	{
		vector at = person.GetOrigin();
		vector flat = Vector(point[0] - at[0], 0, point[2] - at[2]);
		float distance = flat.Length();
		if (distance > metres)
			flat = flat * (metres / distance);

		person.SetOrigin(at + flat);
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
