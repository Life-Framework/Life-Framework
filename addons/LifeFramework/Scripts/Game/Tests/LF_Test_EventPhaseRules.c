// red-proof: observed 2026-11-16 with `tools\cli test --no-build --red LF_Test_EventPhaseRules`: with the hook on the advance and idle-timeout acts are skipped and the test failed (`an idle timeout changes the state`, `an idle timeout ends the event`, `an idle timeout ends it into cooldown`, plus the phase-advance assertions); `RED PROOF OK (all 1 listed test(s) fail with the hook on)`; without the hook green, `SUMMARY tier=only passed=1 failed=0`.
// tier: LOGIC
//! Event phase rules (#1948): phases advance in order when their conditions hold, an idle timeout
//! or an abort ends the event into cooldown, a cooldown blocks a restart until it runs out, and a
//! malformed definition is refused. No world.
class LF_Test_EventPhaseRules : LF_Test
{
	override string GetName()
	{
		return "events/phase-rules";
	}

	//------------------------------------------------------------------------------------------------
	//! prep (idle timeout 60 s) then hot (never times out) then wrap; a 300 s cooldown.
	protected LF_EventDef MakeDef()
	{
		array<string> modules = {"mod_a", "mod_b"};
		array<ref LF_EventPhase> phases = {};
		phases.Insert(LF_EventPhase.Make("prep", 5, 60, modules));
		phases.Insert(LF_EventPhase.Make("hot", 1, 0));
		phases.Insert(LF_EventPhase.Make("wrap", 5, 0));
		return LF_EventDef.Make("test_event", phases, 300);
	}

	//------------------------------------------------------------------------------------------------
	protected bool Tick(LF_EventDef def, LF_EventRunState state, float now, bool conditionsHold, bool abort)
	{
		LF_EEventPhaseDecision decision = LF_EventPhaseRules.Decide(def, state, now, conditionsHold, abort);
		return LF_EventPhaseRules.Apply(def, state, decision, now);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_EventDef def = MakeDef();
		ctx.True(LF_EventPhaseRules.Validate(def) == "", "the test definition is valid");

		// Phases advance in order, only when the conditions hold.
		LF_EventRunState state = new LF_EventRunState();
		ctx.True(LF_EventPhaseRules.CanStart(def, state, 0), "a fresh event may start");
		ctx.True(LF_EventPhaseRules.Start(def, state, 0), "start opens the event");
		ctx.True(state.m_sPhaseName == "prep", "the first phase runs first");
		ctx.False(LF_EventPhaseRules.Start(def, state, 1), "a running event cannot be started again");

		ctx.False(Tick(def, state, 10, false, false), "conditions that do not hold keep the phase");
		ctx.True(state.m_sPhaseName == "prep", "still in the first phase");

		// Red-proof hook: with --red the advance is skipped, so the next assertions must fail.
		if (!RedProof())
			Tick(def, state, 20, true, false);
		ctx.True(state.m_sPhaseName == "hot", "held conditions advance to the second phase");
		ctx.True(state.m_fPhaseStartedAt == 20, "the new phase records when it began");

		Tick(def, state, 30, true, false);
		ctx.True(state.m_sPhaseName == "wrap", "held conditions advance to the third phase");

		// Finishing the last phase ends the event into cooldown.
		Tick(def, state, 40, true, false);
		ctx.False(LF_EventPhaseRules.IsRunning(state), "finishing the last phase ends the event");
		ctx.True(state.m_fCooldownUntil == 340, "the cooldown starts when the event ends");

		// A cooldown blocks a restart until it runs out.
		ctx.False(LF_EventPhaseRules.CanStart(def, state, 100), "a cooldown blocks a restart");
		ctx.False(LF_EventPhaseRules.Start(def, state, 339), "a start one second early is refused");
		ctx.False(LF_EventPhaseRules.IsRunning(state), "a refused start changes nothing");
		ctx.True(LF_EventPhaseRules.Start(def, state, 340), "the cooldown has run out");
		ctx.True(state.m_sPhaseName == "prep", "a restart begins at the first phase again");
		ctx.True(state.m_fCooldownUntil == 0, "a restart clears the cooldown");

		// An idle timeout ends the event into cooldown; activity restarts the idle clock.
		LF_EventRunState idle = new LF_EventRunState();
		LF_EventPhaseRules.Start(def, idle, 1000);
		Tick(def, idle, 1050, false, false);
		ctx.True(idle.m_sPhaseName == "prep", "inside the idle timeout the phase stays");
		LF_EventPhaseRules.NoteActivity(idle, 1050);
		Tick(def, idle, 1100, false, false);
		ctx.True(idle.m_sPhaseName == "prep", "activity restarts the idle clock");
		bool timedOut = false;
		if (!RedProof())
			timedOut = Tick(def, idle, 1110, false, false);
		ctx.True(timedOut, "an idle timeout changes the state");
		ctx.False(LF_EventPhaseRules.IsRunning(idle), "an idle timeout ends the event");
		ctx.True(LF_EventPhaseRules.InCooldown(idle, 1111), "an idle timeout ends it into cooldown");
		ctx.False(LF_EventPhaseRules.InCooldown(idle, 1410), "the cooldown ends on time");

		// A phase with no timeout never times out.
		LF_EventRunState hot = new LF_EventRunState();
		hot.m_sPhaseName = "hot";
		Tick(def, hot, 100000, false, false);
		ctx.True(hot.m_sPhaseName == "hot", "a phase with no idle timeout stays");

		// One abort ends the event even when the conditions hold.
		LF_EventRunState aborted = new LF_EventRunState();
		LF_EventPhaseRules.Start(def, aborted, 0);
		ctx.True(Tick(def, aborted, 1, true, true), "an abort changes the state");
		ctx.True(LF_EventPhaseRules.InCooldown(aborted, 2), "an abort ends the event into cooldown");

		// Conditions are AND-ed, aborts OR-ed.
		array<bool> held = {true, true};
		array<bool> mixed = {true, false};
		array<bool> none = {};
		ctx.True(LF_EventPhaseRules.AllHold(held), "all held conditions hold");
		ctx.False(LF_EventPhaseRules.AllHold(mixed), "one failed condition blocks the advance");
		ctx.True(LF_EventPhaseRules.AllHold(none), "no conditions hold at once");
		ctx.True(LF_EventPhaseRules.AnyHolds(mixed), "one abort is enough");
		ctx.False(LF_EventPhaseRules.AnyHolds(none), "no aborts never abort");

		// Cadence.
		LF_EventPhase prep = def.m_aPhases[0];
		ctx.False(LF_EventPhaseRules.CadenceDue(prep, 14, 10), "inside the cadence the modules wait");
		ctx.True(LF_EventPhaseRules.CadenceDue(prep, 15, 10), "after the cadence the modules run");
		ctx.True(LF_EventPhaseRules.CadenceDue(LF_EventPhase.Make("x", 0, 0), 10, 10), "a cadence of 0 is always due");

		// A phase name that is not in the definition ends the event; it is the save key.
		LF_EventRunState lost = new LF_EventRunState();
		lost.m_sPhaseName = "renamed";
		Tick(def, lost, 500, true, false);
		ctx.False(LF_EventPhaseRules.IsRunning(lost), "an unknown saved phase ends the event");
		ctx.True(def.FindPhase("hot") == 1 && def.FindPhase("renamed") == -1, "phases are found by name");

		// Malformed definitions are refused.
		array<ref LF_EventPhase> dup = {};
		dup.Insert(LF_EventPhase.Make("a", 1, 0));
		dup.Insert(LF_EventPhase.Make("a", 1, 0));
		ctx.True(LF_EventPhaseRules.Validate(LF_EventDef.Make("e", dup, 1)) != "", "a duplicate phase name is refused");
		array<ref LF_EventPhase> empty = {};
		ctx.True(LF_EventPhaseRules.Validate(LF_EventDef.Make("e", empty, 1)) != "", "an event with no phases is refused");
		ctx.True(LF_EventPhaseRules.Validate(LF_EventDef.Make("", dup, 1)) != "", "an empty event id is refused");
		array<ref LF_EventPhase> unnamed = {};
		unnamed.Insert(LF_EventPhase.Make("", 1, 0));
		ctx.True(LF_EventPhaseRules.Validate(LF_EventDef.Make("e", unnamed, 1)) != "", "an unnamed phase is refused");
		array<ref LF_EventPhase> good = {};
		good.Insert(LF_EventPhase.Make("a", 1, 0));
		ctx.True(LF_EventPhaseRules.Validate(LF_EventDef.Make("e", good, -1)) != "", "a negative cooldown is refused");
		ctx.False(LF_EventPhaseRules.CanStart(LF_EventDef.Make("e", empty, 1), new LF_EventRunState(), 0), "a bad definition never starts");
		ctx.True(LF_EventPhaseRules.Validate(null) != "", "no definition is refused");
	}
}
