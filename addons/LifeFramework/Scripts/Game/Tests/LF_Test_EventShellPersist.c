// red-proof: observed 2026-11-17 with `tools\cli test --no-build --red LF_Test_EventShellPersist`: with the hook on ApplySaved is skipped and the test failed (`A's int bag survives (expected 7, got 0)`, `B saved mid-phase loads into its full cooldown (expected 300, got 0)`), `RED PROOF OK`; without the hook `SUMMARY tier=only passed=2 failed=0`.
// tier: WORLD
//! Events survive a restart by phase name (#1951): the "worldevents" rows round trip a cooldown and
//! the int bag onto a fresh shell clock; an event saved mid-phase loads into its full cooldown, not
//! back into the phase; a phase name the event no longer has loads idle with a warning; an event id
//! this boot does not run is skipped with a warning. Applying the same rows twice gives the same state.
class LF_Test_EventShellPersist : LF_Test
{
	override string GetName()
	{
		return "events/shell-persist";
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_EventsConfig MakeConfig(bool withHot)
	{
		array<ref LF_EventPhase> phases = {};
		phases.Insert(LF_EventPhase.Make("prep", 0, 0));
		if (withHot)
			phases.Insert(LF_EventPhase.Make("hot", 0, 0));
		LF_EventsConfig config = new LF_EventsConfig();
		config.m_aEvents = {};
		config.m_aEvents.Insert(LF_EventDef.Make("test_persist_a", phases, 300));
		config.m_aEvents.Insert(LF_EventDef.Make("test_persist_b", phases, 300));
		return config;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_EventShell.Stop();

		// Session one: A runs and ends (cooling, 300 s from t=12); B is mid-phase in "hot".
		LF_EventShell.Use(MakeConfig(true));
		ctx.Equal(2, LF_EventShell.RunCount(), "both test events run");
		LF_EventShell.Tick(10);
		LF_EventShell.Tick(11);
		LF_EventShell.Tick(12);
		ctx.EqualStr("", LF_EventShell.PhaseOf("test_persist_a"), "A ended into cooldown");
		ctx.True(LF_EventShell.SetInt("test_persist_a", "spawned", 7), "A's int bag takes a value");
		ctx.False(LF_EventShell.SetInt("test_unknown", "spawned", 7), "an unknown event has no bag");

		// Save at t=112: A has 200 s of cooldown left.
		array<ref LF_EventShellRow> rows = LF_EventShell.ExportSaved(112);
		ctx.Equal(2, rows.Count(), "one row per event");

		// Put B mid-phase by hand in its row (the ticks above ended both events).
		LF_EventShellRow rowB;
		foreach (LF_EventShellRow row : rows)
		{
			if (row.m_sEventId == "test_persist_b")
				rowB = row;
		}
		ctx.True(rowB != null, "B has a row");
		if (rowB)
		{
			rowB.m_sPhaseName = "hot";
			rowB.m_fCooldownLeft = 0;
		}
		LF_EventShellRow ghost = new LF_EventShellRow();
		ghost.m_iVersion = LF_EventShellRow.VERSION;
		ghost.m_sEventId = "test_persist_removed";
		rows.Insert(ghost);

		// Session two: a fresh shell on a clock that restarted at 0; load at t=5.
		LF_EventShell.Use(MakeConfig(true));
		int applied;
		if (!RedProof())
			applied = LF_EventShell.ApplySaved(rows, 5);
		ctx.Equal(2, applied, "both known events apply, the removed id is skipped");
		ctx.EqualFloat(200, LF_EventShell.CooldownLeftOf("test_persist_a", 5), 0.01, "A keeps its 200 s of cooldown on the new clock");
		ctx.Equal(7, LF_EventShell.GetInt("test_persist_a", "spawned"), "A's int bag survives");
		ctx.EqualStr("", LF_EventShell.PhaseOf("test_persist_b"), "B saved mid-phase is not resumed");
		ctx.EqualFloat(300, LF_EventShell.CooldownLeftOf("test_persist_b", 5), 0.01, "B saved mid-phase loads into its full cooldown");
		ctx.Equal(1, LF_EventShell.LoadWarningCount(), "the removed event id warns once");

		// Idempotent: applying the same rows again gives the same state.
		LF_EventShell.ApplySaved(rows, 5);
		ctx.EqualFloat(200, LF_EventShell.CooldownLeftOf("test_persist_a", 5), 0.01, "a second apply keeps A's cooldown");
		ctx.EqualFloat(300, LF_EventShell.CooldownLeftOf("test_persist_b", 5), 0.01, "a second apply keeps B's cooldown");

		// A cooling event does not restart early on the new clock.
		LF_EventShell.Tick(100);
		ctx.EqualStr("", LF_EventShell.PhaseOf("test_persist_a"), "A still cools down after the load");

		// Session three: B's "hot" phase was removed by a pack: B loads idle, no cooldown, with a warning.
		LF_EventShell.Use(MakeConfig(false));
		LF_EventShell.ApplySaved(rows, 5);
		ctx.EqualFloat(0, LF_EventShell.CooldownLeftOf("test_persist_b", 5), 0.01, "B in a removed phase loads idle with no cooldown");
		ctx.EqualStr("", LF_EventShell.PhaseOf("test_persist_b"), "B in a removed phase has no phase");
		ctx.Equal(2, LF_EventShell.LoadWarningCount(), "the removed phase name and the removed id each warn");

		// Restore the live shell.
		LF_EventShell.Use(LF_EventShell.LoadConfig());
		LF_EventShell.StartIfServer();
	}
}
