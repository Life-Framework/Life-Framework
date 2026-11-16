// red-proof: Observed 2026-11-16 with `tools/cli test --no-build --red LF_Test_EventWalkArrival`: `RED-OK events/walk-arrival` (with the group-done flag off, "a formation member 4.4 m out arrived with its group" and "a member 10.6 m out that followed the leader arrived" fail); green with `--only` (passed=2 failed=0 with LF_Test_EventSceneWalk).

// tier: LOGIC
//! The scene walk's arrival rule (#2174): a walker that passed every waypoint arrives near the target;
//! once the AI group has finished its route, a member standing in the group's formation around the
//! target has arrived too, instead of timing out as stuck.
class LF_Test_EventWalkArrival : LF_Test
{
	override string GetName()
	{
		return "events/walk-arrival";
	}

	override void Run(LF_TestContext ctx)
	{
		// While the group walks: only the walker that passed every waypoint and stands by the target.
		ctx.True(LF_EventSceneWalk.HasArrived(true, 2, false), "a walker 2 m from the target after the last waypoint arrived");
		ctx.False(LF_EventSceneWalk.HasArrived(false, 2, false), "standing near the target without walking the route is not arriving");
		ctx.False(LF_EventSceneWalk.HasArrived(true, 7, false), "7 m out while the group still walks is not there yet");

		// The group finished its route: the formation stands 1 to 11 m around the leader (the #2174 run).
		bool groupDone = true;
		if (RedProof())
			groupDone = false;
		ctx.True(LF_EventSceneWalk.HasArrived(true, 4.4, groupDone), "a formation member 4.4 m out arrived with its group");
		ctx.True(LF_EventSceneWalk.HasArrived(false, 10.6, groupDone), "a member 10.6 m out that followed the leader arrived");
		ctx.False(LF_EventSceneWalk.HasArrived(true, LF_EventSceneWalk.FORMATION_ARRIVE_M + 5, true), "a straggler beyond the formation's spread has not arrived");
	}
}
