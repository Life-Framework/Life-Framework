// red-proof: `test --no-build --red LF_Test_EventWaypoints` -> RED PROOF OK (2026-11-16): with the hook on, FindWaypoints is skipped, so
// `the cell holds four waypoints (expected 4, got 0)` and `all four come back (expected 4, got 0)` fail; 24 assertions go red.
// tier: WORLD
//! Event scene markers (#1954): the test route in grid cell `event-scene` (Grid_event_scene.layer) is
//! found by component through LF_Places, never by entity name. The four waypoints come back in their
//! order though the layer lists them out of order, the two spawn markers likewise, the target resolves
//! by scene id, and an unknown or empty scene finds nothing.
class LF_Test_EventWaypoints : LF_Test
{
	static const string SCENE = "grid_event_scene";

	override string GetName()
	{
		return "events/waypoints";
	}

	override void Run(LF_TestContext ctx)
	{
		array<IEntity> waypoints = {};
		int foundWaypoints = 0;
		if (!RedProof())
			foundWaypoints = LF_EventWaypointComponent.FindWaypoints(SCENE, waypoints);

		ctx.Equal(4, foundWaypoints, "the cell holds four waypoints");
		ctx.Equal(4, waypoints.Count(), "all four come back");
		CheckOrdered(ctx, waypoints, "waypoint", LF_EEventMarkerRole.WAYPOINT);

		// The layer lists them 2, 0, 3, 1; the cell's entity names follow the route, so a result in
		// route order is the names R0_1 to R0_4.
		if (waypoints.Count() == 4)
		{
			ctx.EqualStr("Grid_event_scene_R0_1", waypoints[0].GetName(), "the route starts at the order-0 marker");
			ctx.EqualStr("Grid_event_scene_R0_2", waypoints[1].GetName(), "then order 1");
			ctx.EqualStr("Grid_event_scene_R0_3", waypoints[2].GetName(), "then order 2");
			ctx.EqualStr("Grid_event_scene_R0_4", waypoints[3].GetName(), "and ends at order 3");
		}

		array<IEntity> spawns = {};
		ctx.Equal(2, LF_EventWaypointComponent.FindSpawns(SCENE, spawns), "the cell holds two spawn markers");
		CheckOrdered(ctx, spawns, "spawn", LF_EEventMarkerRole.SPAWN);

		IEntity target = LF_EventWaypointComponent.FindTarget(SCENE);
		ctx.NotNull(target, "the target resolves by scene id");
		if (target)
		{
			ctx.True(target == LF_Places.Find(LF_EventWaypointComponent.KIND_TARGET, SCENE), "it is the LF_Places entry of kind event_target");
			LF_EventWaypointComponent marker = LF_EventWaypointComponent.Cast(target.FindComponent(LF_EventWaypointComponent));
			ctx.NotNull(marker, "the target carries the marker component");
			if (marker)
				ctx.True(marker.GetRole() == LF_EEventMarkerRole.TARGET, "and its role is TARGET");

			vector origin;
			ctx.True(LF_Places.FindOrigin(LF_EventWaypointComponent.KIND_TARGET, SCENE, origin), "its position is read through LF_Places");
		}

		array<IEntity> none = {};
		ctx.Equal(0, LF_EventWaypointComponent.FindWaypoints("no_such_scene", none), "an unknown scene has no waypoints");
		ctx.Equal(0, LF_EventWaypointComponent.FindWaypoints("", none), "an empty scene id finds nothing");
		ctx.Equal(0, LF_EventWaypointComponent.FindSpawns("no_such_scene", none), "an unknown scene has no spawns");
		ctx.True(LF_EventWaypointComponent.FindTarget("no_such_scene") == null, "an unknown scene has no target");
		ctx.True(LF_EventWaypointComponent.FindTarget("") == null, "an empty scene id has no target");
		ctx.Equal(0, none.Count(), "nothing was added for them");
	}

	//------------------------------------------------------------------------------------------------
	//! Every marker in the list belongs to the scene and has the role, and the orders run 0, 1, 2 ...
	protected void CheckOrdered(LF_TestContext ctx, array<IEntity> markers, string what, LF_EEventMarkerRole role)
	{
		foreach (int i, IEntity entity : markers)
		{
			LF_EventWaypointComponent marker = LF_EventWaypointComponent.Cast(entity.FindComponent(LF_EventWaypointComponent));
			ctx.NotNull(marker, what + " " + i + " carries the marker component");
			if (!marker)
				continue;

			ctx.EqualStr(SCENE, marker.GetSceneId(), what + " " + i + " belongs to the scene");
			ctx.True(marker.GetRole() == role, what + " " + i + " has the role");
			ctx.Equal(i, marker.GetOrder(), what + " " + i + " is in order");
		}
	}
}
