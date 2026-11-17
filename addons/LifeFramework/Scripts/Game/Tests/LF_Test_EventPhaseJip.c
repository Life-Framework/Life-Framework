// red-proof: observed 2026-11-16 with `tools\cli test --no-build --red LF_Test_EventPhaseJip`: with the hook on no pairs are collected and the board stays empty (`one running event makes one id and phase pair (expected 2, got 0)`), `RED PROOF OK (all 1 listed test(s) fail with the hook on)`; without the hook `SUMMARY tier=only passed=2 failed=0`.
// tier: WORLD
//! A late joiner gets the running event phases (#2106): a test event started by the shell is
//! collected into the JIP pairs, a fresh board reads them, and holds the same id and phase. An idle
//! event is not sent; odd trailing data is ignored.
class LF_Test_EventPhaseJip : LF_Test
{
	override string GetName()
	{
		return "events/phase-jip";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_EventShell.Stop();
		LF_EventShell.ForceHour(20);

		array<ref LF_EventPhase> phases = {};
		phases.Insert(LF_EventPhase.Make("prep", 0, 0));
		phases.Insert(LF_EventPhase.Make("hot", 0, 0));
		LF_EventShell.Unregister("test_jip_night");
		LF_EventShell.RegisterCondition("test_jip_night", LF_EventPrecondition.MakeHourRange(1, 2));
		array<string> nightModules = {"test_jip_night"};
		array<ref LF_EventPhase> later = {};
		later.Insert(LF_EventPhase.Make("open", 0, 0, nightModules));
		LF_EventsConfig config = new LF_EventsConfig();
		config.m_aEvents = {};
		config.m_aEvents.Insert(LF_EventDef.Make("test_jip_a", phases, 300));
		config.m_aEvents.Insert(LF_EventDef.Make("test_jip_idle", later, 300));
		LF_EventShell.Use(config);

		// test_jip_a starts in prep, then advances to hot; test_jip_idle waits for a night hour and stays idle.
		LF_EventShell.Tick(10);
		LF_EventShell.Tick(11);
		ctx.EqualStr("hot", LF_EventShell.PhaseOf("test_jip_a"), "the test event runs in hot");

		array<string> ids = {"test_jip_a", "test_jip_idle", "test_jip_unknown"};
		array<string> pairs = {};
		if (!RedProof())
			pairs = LF_EventPhaseJip.Collect(ids);
		ctx.Equal(2, pairs.Count(), "one running event makes one id and phase pair");

		// A fresh board: what a late joiner holds before the snapshot arrives.
		LF_EventPhaseBoard.ResetStatics();
		ctx.EqualStr("", LF_EventPhaseBoard.PhaseOf("test_jip_a"), "the fresh board knows nothing");
		ctx.Equal(1, LF_EventPhaseJip.Apply(pairs), "one event applied from the snapshot");
		ctx.EqualStr("hot", LF_EventPhaseBoard.PhaseOf("test_jip_a"), "the late joiner's board holds the running phase");
		ctx.EqualStr("", LF_EventPhaseBoard.PhaseOf("test_jip_idle"), "an idle event is not on the board");

		array<string> odd = {"test_jip_b", "open", "dangling"};
		ctx.Equal(1, LF_EventPhaseJip.Apply(odd), "a dangling id without a phase is ignored");
		ctx.Equal(0, LF_EventPhaseJip.Collect(null).Count(), "no ids, no pairs");

		LF_EventPhaseBoard.ResetStatics();
		LF_EventShell.Unregister("test_jip_night");
		LF_EventShell.ForceHour(-1);
		LF_EventShell.Use(LF_EventShell.LoadConfig());
		LF_EventShell.StartIfServer();
	}
}
