// red-proof: `test --no-build --red LF_Test_EventSceneHarness` -> RED PROOF OK (2026-11-16): with the hook on the fixture is never applied, so `the fixture is recorded for status` and `at the forced dawn hour the shell starts the event` fail; without it `SUMMARY tier=only passed=1 failed=0`.
// tier: WORLD
//! The `eventscene` harness verb (#2173): a fixture conf laid over the event list turns on an event
//! the files keep off and registers its modules; with the hour forced to dawn the shell starts it.
//! A missing conf, no conf and a conf of another root class are refused and change nothing. `Stop`
//! puts the files' own list back. Only this feature's statics are touched, never ResetAll().
class LF_Test_EventSceneHarness : LF_Test
{
	static const ResourceName FIXTURE = "{901C603717D05377}Configs/Tests/LF_Test_EventSceneFixture.conf";
	static const ResourceName SCENE_CONF = "{8F5F26A14F89DF35}Configs/Tests/LF_Test_EventModuleScene.conf";
	static const string EVENT_ID = "test_eventscene";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "events/scene-harness";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_EventShell.Stop();
		LF_EventSceneHarness.ResetStatics();
		// Built at run time: a conf that does not exist on purpose (the resource-ref lint reads literals).
		ResourceName missing = "{0000000000000001}Configs/Tests/" + "LF_Test_EventSceneMissing.conf";

		ctx.EqualStr(LF_EventSceneHarness.REFUSED_NO_CONF, LF_EventSceneHarness.Start(""), "no conf is refused");
		ctx.EqualStr(LF_EventSceneHarness.REFUSED_NOT_LOADED, LF_EventSceneHarness.Start(missing, 6), "a missing conf is refused");
		ctx.EqualStr(LF_EventSceneHarness.REFUSED_WRONG_ROOT, LF_EventSceneHarness.Start(SCENE_CONF, 6), "a conf that is no event list is refused");
		ctx.True(LF_EventOverlay.Find(EVENT_ID) == null, "a refusal leaves the event list as the files have it");
		ctx.EqualStr("", LF_EventSceneHarness.Fixture(), "and records no fixture");

		string result;
		// Red-proof hook: with --red the fixture is never applied.
		if (!RedProof())
			result = LF_EventSceneHarness.Start(FIXTURE, 6);

		ctx.True(result.StartsWith("ok:"), "the fixture applies: " + result);
		LF_EventDef def = LF_EventOverlay.Find(EVENT_ID);
		ctx.NotNull(def, "the fixture's event is in the list");
		if (def)
			ctx.True(def.m_bEnabled, "and it is on");
		ctx.True(LF_EventShell.IsRegistered("test_es_dawn"), "the fixture's module registers");
		ctx.EqualStr(FIXTURE, LF_EventSceneHarness.Fixture(), "the fixture is recorded for status");

		float now = 100000;
		if (GetGame() && GetGame().GetWorld())
			now += GetGame().GetWorld().GetWorldTime() / 1000;
		LF_EventShell.Tick(now);
		ctx.EqualStr("gate", LF_EventShell.PhaseOf(EVENT_ID), "at the forced dawn hour the shell starts the event");
		ctx.True(LF_EventSceneHarness.Status(EVENT_ID).Contains("phase 'gate'"), "status names the phase: " + LF_EventSceneHarness.Status(EVENT_ID));

		LF_EventSceneHarness.Stop();
		ctx.True(LF_EventOverlay.Find(EVENT_ID) == null, "stop puts the files' own list back");
		ctx.EqualStr("", LF_EventShell.PhaseOf(EVENT_ID), "and the fixture event no longer runs");
		ctx.False(LF_EventShell.IsRegistered("test_es_dawn"), "and its module is gone");
	}
}
