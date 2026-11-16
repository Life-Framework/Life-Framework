// red-proof: observed 2026-11-16 with `tools\cli test --no-build --red LF_Test_EventScenePlan`: with the hook on the cap is lifted and `the cap trims the NPC count` failed; `RED PROOF OK (all 1 listed test(s) fail with the hook on)`; without the hook green, `SUMMARY tier=only passed=1 failed=0`.
// tier: LOGIC
//! Event scene plan (#1953): waypoint order follows the config, the NPC cap trims the count, a
//! second scene is refused while one runs, and a scene is skipped with a logged reason when the AI
//! budget cannot hold it. No world.
class LF_Test_EventScenePlan : LF_Test
{
	override string GetName()
	{
		return "events/scene-plan";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_EventScene MakeScene(string id, int npcCount)
	{
		array<string> waypoints = {"wp_gate", "wp_road", "wp_square"};
		array<string> dialogue = {"d_elder", "", "d_child", "d_extra"};
		LF_EventScene scene = LF_EventScene.Make(id, waypoints, npcCount, "relocated_family", dialogue);
		scene.m_fSpawnStaggerSeconds = 3;
		scene.m_sVehicleSlot = "slot_cart";
		return scene;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_EventScenePlanner.ResetStatics();

		// Waypoint order follows the config.
		LF_EventScenePlan plan = LF_EventScenePlanner.Plan(MakeScene("arrival", 3), 10);
		ctx.True(plan.m_bGo, "a scene that fits the budget goes");
		ctx.True(plan.m_aWaypointIds.Count() == 3, "every waypoint is kept");
		ctx.True(plan.m_aWaypointIds.Count() == 3 && plan.m_aWaypointIds[0] == "wp_gate" && plan.m_aWaypointIds[1] == "wp_road" && plan.m_aWaypointIds[2] == "wp_square", "waypoints keep the configured order");
		ctx.True(plan.m_aNpcs.Count() == 3, "the NPC count is as configured under the cap");
		if (plan.m_aNpcs.Count() < 3)
			return;
		ctx.True(plan.m_aNpcs[0].m_sRole == "relocated_family", "every walker gets the scene's role");
		ctx.True(plan.m_aNpcs[0].m_sDialogueId == "d_elder" && plan.m_aNpcs[1].m_sDialogueId == "" && plan.m_aNpcs[2].m_sDialogueId == "d_child", "dialogue ids go to NPCs by index, a blank one means none");
		ctx.True(plan.m_aNpcs[2].m_fSpawnDelaySeconds == 6, "walkers appear one stagger apart");
		ctx.True(plan.m_sVehicleSlot == "slot_cart", "the parked vehicle slot is carried");

		// The cap trims the count (and the dialogue beyond it).
		// Red-proof hook: with --red the cap is lifted, so the trim assertions must fail.
		int cap = 2;
		if (RedProof())
			cap = LF_EventScenePlanner.HARD_NPC_CAP;
		LF_EventScenePlan trimmed = LF_EventScenePlanner.Plan(MakeScene("arrival", 4), 10, cap);
		ctx.True(trimmed.m_bGo, "a trimmed scene still goes");
		ctx.True(trimmed.m_aNpcs.Count() == 2, "the cap trims the NPC count");
		ctx.True(trimmed.m_iRequestedCount == 4, "the plan remembers how many were asked for");
		LF_EventScenePlan huge = LF_EventScenePlanner.Plan(MakeScene("arrival", 500), 1000);
		ctx.True(huge.m_aNpcs.Count() == LF_EventScenePlanner.HARD_NPC_CAP, "a scene never passes the hard cap");

		// A second scene is refused while one runs; finishing frees the slot.
		ctx.True(LF_EventScenePlanner.Begin(plan), "the first scene starts");
		ctx.True(LF_EventScenePlanner.RunningScene() == "arrival", "the running scene is recorded");
		LF_EventScenePlan second = LF_EventScenePlanner.Plan(MakeScene("parade", 2), 10);
		ctx.False(second.m_bGo, "a second scene is refused while one runs");
		ctx.True(second.m_sSkipReason.Contains("arrival"), "the refusal names the scene that runs");
		ctx.False(LF_EventScenePlanner.Begin(trimmed), "a plan made earlier cannot start a second scene");
		LF_EventScenePlanner.Finish("parade");
		ctx.True(LF_EventScenePlanner.RunningScene() == "arrival", "only the running scene can end itself");
		LF_EventScenePlanner.Finish("arrival");
		ctx.True(LF_EventScenePlanner.RunningScene() == "", "finishing frees the slot");
		ctx.True(LF_EventScenePlanner.Plan(MakeScene("parade", 2), 10).m_bGo, "the next scene may go once the first finished");

		// A used-up AI budget skips the scene with a reason.
		LF_EventScenePlan broke = LF_EventScenePlanner.Plan(MakeScene("arrival", 3), 0);
		ctx.False(broke.m_bGo, "no free AI budget skips the scene");
		ctx.True(broke.m_sSkipReason.Contains("AI budget"), "the skip reason says the AI budget is used");
		ctx.True(broke.m_aNpcs.Count() == 0, "a skipped scene places nobody");
		LF_EventScenePlan tight = LF_EventScenePlanner.Plan(MakeScene("arrival", 3), 2);
		ctx.False(tight.m_bGo, "a budget that cannot hold every walker skips the scene");
		ctx.False(LF_EventScenePlanner.Begin(broke), "a skipped plan never starts");
		ctx.True(LF_EventScenePlanner.RunningScene() == "", "a skipped scene does not hold the slot");

		// A restart does not resume a scene: the static reset drops it.
		LF_EventScenePlanner.Begin(LF_EventScenePlanner.Plan(MakeScene("arrival", 1), 5));
		// Only this feature's hook: LF_StaticReset.ResetAll() would wipe every feature's statics
		// (placed-object registries) for the tests that run after this one.
		LF_EventScenePlanner_StaticResetHook hook = new LF_EventScenePlanner_StaticResetHook();
		hook.Apply();
		ctx.True(LF_EventScenePlanner.RunningScene() == "", "a world reset drops the running scene");

		// A malformed scene is refused.
		array<string> none = {};
		array<string> blank = {"wp_a", ""};
		array<string> one = {"wp_a"};
		ctx.False(LF_EventScenePlanner.Plan(LF_EventScene.Make("s", none, 2, "r"), 5).m_bGo, "a scene with no waypoints is refused");
		ctx.False(LF_EventScenePlanner.Plan(LF_EventScene.Make("s", blank, 2, "r"), 5).m_bGo, "an empty waypoint id is refused");
		ctx.False(LF_EventScenePlanner.Plan(LF_EventScene.Make("", one, 2, "r"), 5).m_bGo, "an empty scene id is refused");
		ctx.False(LF_EventScenePlanner.Plan(LF_EventScene.Make("s", one, 0, "r"), 5).m_bGo, "a scene with no NPCs is refused");
		ctx.False(LF_EventScenePlanner.Plan(null, 5).m_bGo, "no scene is refused");
		ctx.False(LF_EventScenePlanner.Plan(MakeScene("s", 2), 5, 0).m_bGo, "a cap of zero skips the scene");

		LF_EventScenePlanner.ResetStatics();
	}
}
