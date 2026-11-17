// red-proof: `test --no-build --red LF_Test_EventSceneSpawn` -> RED PROOF OK (2026-11-17): with the hook on Start is skipped, so `the scene starts` fails (5 assertions); without it `SUMMARY tier=only passed=1 failed=0`.
// tier: WORLD
//! Event scene spawn (#1955): a plan of the grid cell `event-scene` (Grid_event_scene.layer) spawns its
//! walkers at the scene's spawn markers in the role's outfit, the NPC cap holds the count, a second
//! scene cannot start while it runs, and the despawn waits while a test eye has line of sight and
//! removes everyone once nobody sees them.
class LF_Test_EventSceneSpawn : LF_Test
{
	static const string SCENE = "grid_event_scene";

	override string GetName()
	{
		return "events/scene";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_EventScenePlan MakePlan(string sceneId, int npcCount, int cap)
	{
		array<string> waypoints = {"grid_event_scene#0", "grid_event_scene#1"};
		LF_EventScene scene = LF_EventScene.Make(sceneId, waypoints, npcCount, "townsfolk");
		scene.m_fSpawnStaggerSeconds = 0;
		return LF_EventScenePlanner.Plan(scene, 10, cap);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// Only this feature's hooks; LF_StaticReset.ResetAll() would wipe every feature's statics.
		LF_EventScenePlanner.ResetStatics();
		LF_EventSceneSpawn.ResetStatics();

		ctx.True(LF_EventSceneSpawn.FreeAiSlots() >= 0, "the AI budget reader never goes below 0");

		array<IEntity> spawns = {};
		ctx.Equal(2, LF_EventWaypointComponent.FindSpawns(SCENE, spawns), "the cell holds two spawn markers");
		if (spawns.Count() < 2)
			return;

		// Four asked for, a cap of three: three spawn.
		LF_EventScenePlan plan = MakePlan(SCENE, 4, 3);
		ctx.True(plan.m_bGo, "the plan goes");
		LF_EventSceneCrew crew;
		// Red-proof hook: with --red nothing is started, so every spawn assertion fails.
		if (!RedProof())
			crew = LF_EventSceneSpawn.Start(plan);
		ctx.NotNull(crew, "the scene starts");
		if (!crew)
			return;

		array<IEntity> people = {};
		ctx.Equal(3, crew.GetEntities(people), "the cap holds: three walkers, not four");
		ctx.True(LF_EventSceneSpawn.Running() == crew, "the crew is the running scene");
		ctx.EqualStr(SCENE, LF_EventScenePlanner.RunningScene(), "the planner holds the scene slot");
		array<EntityID> ids = {};
		foreach (int i, IEntity person : people)
		{
			ids.Insert(person.GetID());
			ctx.EqualStr("townsfolk", LF_Roles.RoleOf(person), "walker " + i + " wears the role's outfit");
			float nearest = 1000000;
			foreach (IEntity marker : spawns)
				nearest = Math.Min(nearest, vector.DistanceXZ(person.GetOrigin(), marker.GetOrigin()));
			ctx.True(nearest <= LF_EventSceneSpawn.SPACING_M + 0.1, "walker " + i + " stands at a spawn marker (" + nearest + " m)");
		}

		// One scene at a time.
		ctx.True(LF_EventSceneSpawn.Start(MakePlan(SCENE, 2, 3)) == null, "a second scene cannot start while one runs");

		// An eye 15 m over the first marker sees the crowd: the despawn waits.
		vector over = spawns[0].GetOrigin() + "0 15 0";
		array<vector> near = {over};
		ctx.False(LF_EventSceneSpawn.TryDespawn(crew, near), "despawn waits while the observer has line of sight");
		array<IEntity> still = {};
		ctx.Equal(3, crew.GetEntities(still), "nobody is removed in sight");
		ctx.EqualStr(SCENE, LF_EventScenePlanner.RunningScene(), "the scene still runs");

		// The same eye far out of range: everyone goes and the slot frees.
		vector far = spawns[0].GetOrigin() + Vector(LF_EventSceneSpawn.SIGHT_RANGE_M * 3, 15, 0);
		array<vector> away = {far};
		ctx.True(LF_EventSceneSpawn.TryDespawn(crew, away), "despawn goes once nobody sees them");
		BaseWorld world = GetGame().GetWorld();
		int left = 0;
		foreach (EntityID id : ids)
		{
			if (world.FindEntityByID(id))
				left++;
		}
		ctx.Equal(0, left, "every walker is removed");
		ctx.EqualStr("", LF_EventScenePlanner.RunningScene(), "the scene slot is free");
		ctx.True(LF_EventSceneSpawn.Running() == null, "no crew runs");

		// A scene with no spawn marker placed spawns nobody and holds no slot.
		ctx.True(LF_EventSceneSpawn.Start(MakePlan("no_such_scene", 2, 3)) == null, "a scene without spawn markers does not start");
		ctx.EqualStr("", LF_EventScenePlanner.RunningScene(), "and holds no slot");

		// The static reset drops a running crew.
		LF_EventSceneCrew again = LF_EventSceneSpawn.Start(MakePlan(SCENE, 1, 3));
		ctx.NotNull(again, "a new scene starts after the first ended");
		LF_EventSceneSpawn_StaticResetHook hook = new LF_EventSceneSpawn_StaticResetHook();
		if (again)
			LF_EventSceneSpawn.TryDespawn(again, away);
		hook.Apply();
		ctx.True(LF_EventSceneSpawn.Running() == null, "a world reset drops the running crew");

		LF_EventScenePlanner.ResetStatics();
		LF_EventSceneSpawn.ResetStatics();
	}
}
