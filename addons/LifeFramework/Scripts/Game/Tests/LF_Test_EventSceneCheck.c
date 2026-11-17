// red-proof: observed 2026-11-16 with `tools\cli test --no-build --red LF_Test_EventSceneCheck`: with the hook on the check is skipped and `a gap in the route breaks the scene (expected 1, got 0)` failed (20 assertions); `RED PROOF OK (all 1 listed test(s) fail with the hook on)`; without the hook `SUMMARY tier=only passed=1 failed=0`.
// tier: LOGIC
//! Event scene check (#2067): lists of marker orders and roles go in, one fault line per broken
//! scene comes out. A gap in the route, waypoints with no target and no spawn marker are each named
//! once; a clean scene reports clean; a bad scene never stops the others. No world.
class LF_Test_EventSceneCheck : LF_Test
{
	override string GetName()
	{
		return "events/scene-check";
	}

	//------------------------------------------------------------------------------------------------
	protected void Add(notnull array<ref LF_EventSceneMarker> markers, string scene, LF_EEventMarkerRole role, int order)
	{
		markers.Insert(LF_EventSceneMarker.Make(scene, role, order));
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_EventSceneMarker> markers = {};

		// A clean scene: route 0..2 (listed out of order), two spawns, one target.
		Add(markers, "clean", LF_EEventMarkerRole.WAYPOINT, 2);
		Add(markers, "clean", LF_EEventMarkerRole.WAYPOINT, 0);
		Add(markers, "clean", LF_EEventMarkerRole.WAYPOINT, 1);
		Add(markers, "clean", LF_EEventMarkerRole.SPAWN, 0);
		Add(markers, "clean", LF_EEventMarkerRole.SPAWN, 1);
		Add(markers, "clean", LF_EEventMarkerRole.TARGET, 0);

		array<string> faults = {};
		int broken = LF_EventSceneCheck.Check(markers, faults);
		ctx.Equal(0, broken, "a clean scene reports clean");
		ctx.Equal(0, faults.Count(), "a clean scene has no fault line");

		// A gap in the orders: 0, 2, 3 is missing 1.
		markers.Clear();
		Add(markers, "gappy", LF_EEventMarkerRole.WAYPOINT, 0);
		Add(markers, "gappy", LF_EEventMarkerRole.WAYPOINT, 2);
		Add(markers, "gappy", LF_EEventMarkerRole.WAYPOINT, 3);
		Add(markers, "gappy", LF_EEventMarkerRole.SPAWN, 0);
		Add(markers, "gappy", LF_EEventMarkerRole.TARGET, 0);
		faults.Clear();
		broken = 0;
		// Red-proof hook: with --red the check is skipped, so the fault assertions must fail.
		if (!RedProof())
			broken = LF_EventSceneCheck.Check(markers, faults);
		ctx.Equal(1, broken, "a gap in the route breaks the scene");
		ctx.Equal(1, faults.Count(), "one line for the scene");
		if (faults.Count() == 1)
		{
			ctx.True(faults[0].Contains("gappy"), "the line names the scene");
			ctx.True(faults[0].Contains("gap") && faults[0].Contains("missing 1"), "the line names the missing order");
			ctx.False(faults[0].Contains("no target"), "a placed target is not reported");
			ctx.False(faults[0].Contains("no spawn"), "a placed spawn is not reported");
		}

		// A route that does not start at 0 is a gap at 0.
		markers.Clear();
		Add(markers, "late", LF_EEventMarkerRole.WAYPOINT, 1);
		Add(markers, "late", LF_EEventMarkerRole.WAYPOINT, 2);
		Add(markers, "late", LF_EEventMarkerRole.SPAWN, 0);
		Add(markers, "late", LF_EEventMarkerRole.TARGET, 0);
		faults.Clear();
		LF_EventSceneCheck.Check(markers, faults);
		ctx.Equal(1, faults.Count(), "a route that starts at 1 is flagged");
		if (faults.Count() == 1)
			ctx.True(faults[0].Contains("missing 0"), "it names the missing 0");

		// Waypoints but no target.
		markers.Clear();
		Add(markers, "headless", LF_EEventMarkerRole.WAYPOINT, 0);
		Add(markers, "headless", LF_EEventMarkerRole.SPAWN, 0);
		faults.Clear();
		LF_EventSceneCheck.Check(markers, faults);
		ctx.Equal(1, faults.Count(), "waypoints without a target are flagged");
		if (faults.Count() == 1)
		{
			ctx.True(faults[0].Contains("no target"), "the line says there is no target");
			ctx.False(faults[0].Contains("gap"), "no gap is invented");
		}

		// No spawn marker.
		markers.Clear();
		Add(markers, "empty_ground", LF_EEventMarkerRole.WAYPOINT, 0);
		Add(markers, "empty_ground", LF_EEventMarkerRole.WAYPOINT, 1);
		Add(markers, "empty_ground", LF_EEventMarkerRole.TARGET, 0);
		faults.Clear();
		LF_EventSceneCheck.Check(markers, faults);
		ctx.Equal(1, faults.Count(), "a scene with no spawn marker is flagged");
		if (faults.Count() == 1)
			ctx.True(faults[0].Contains("no spawn"), "the line says there is no spawn marker");

		// Several faults in one scene stay one line, each named once; a second bad scene and a clean
		// one beside it are judged on their own.
		markers.Clear();
		Add(markers, "ruin", LF_EEventMarkerRole.WAYPOINT, 0);
		Add(markers, "ruin", LF_EEventMarkerRole.WAYPOINT, 2);
		Add(markers, "fine", LF_EEventMarkerRole.WAYPOINT, 0);
		Add(markers, "fine", LF_EEventMarkerRole.SPAWN, 0);
		Add(markers, "fine", LF_EEventMarkerRole.TARGET, 0);
		Add(markers, "spawnless", LF_EEventMarkerRole.TARGET, 0);
		faults.Clear();
		broken = LF_EventSceneCheck.Check(markers, faults);
		ctx.Equal(2, broken, "two scenes are broken");
		ctx.Equal(2, faults.Count(), "one line per broken scene");
		if (faults.Count() == 2)
		{
			ctx.True(faults[0].Contains("ruin") && faults[0].Contains("gap") && faults[0].Contains("no target") && faults[0].Contains("no spawn"), "the first scene names all three faults");
			ctx.True(faults[1].Contains("spawnless") && faults[1].Contains("no spawn"), "the second scene names its fault");
			ctx.False(faults[1].Contains("no target"), "a target-only scene has a target");
		}

		// Nothing placed is nothing to check; a marker with no scene id is not a scene.
		markers.Clear();
		faults.Clear();
		ctx.Equal(0, LF_EventSceneCheck.Check(markers, faults), "no markers, no fault");
		Add(markers, "", LF_EEventMarkerRole.WAYPOINT, 0);
		ctx.Equal(0, LF_EventSceneCheck.Check(markers, faults), "a marker with no scene id is skipped");
	}
}
