// red-proof: observed 2026-11-16 with `tools\cli test --no-build --red LF_Test_EventPreconditions`: with the hook on the below-minimum player-count check is skipped and the test failed (`red proof: below-minimum check skipped`); `RED-OK events/preconditions`, `RED PROOF OK (all 1 listed test(s) fail with the hook on)`; without the hook green, `SUMMARY tier=only passed=1 failed=0`.
// tier: LOGIC
//! Event preconditions and aborts (#1949): an hour range that wraps midnight, a player count that
//! does not hold below the minimum, a cooldown condition, conditions AND-ed and aborts OR-ed, and
//! the two feeding LF_EventPhaseRules.Decide. No world.
class LF_Test_EventPreconditions : LF_Test
{
	override string GetName()
	{
		return "events/preconditions";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// An hour range across midnight: 22 to 5 holds at 23 and 3, not at 12 or 5.
		LF_EventPrecondition night = LF_EventPrecondition.MakeHourRange(22, 5);
		ctx.True(night.Holds(LF_EventContext.Make(23, 0, 0, 0)), "22 to 5 holds at 23:00");
		ctx.True(night.Holds(LF_EventContext.Make(3, 0, 0, 0)), "22 to 5 holds at 03:00");
		ctx.True(night.Holds(LF_EventContext.Make(22, 0, 0, 0)), "the window opens at its from hour");
		ctx.False(night.Holds(LF_EventContext.Make(5, 0, 0, 0)), "the window is closed at its to hour");
		ctx.False(night.Holds(LF_EventContext.Make(12, 0, 0, 0)), "22 to 5 does not hold at noon");

		LF_EventPrecondition day = LF_EventPrecondition.MakeHourRange(6, 18);
		ctx.True(day.Holds(LF_EventContext.Make(12, 0, 0, 0)), "6 to 18 holds at noon");
		ctx.False(day.Holds(LF_EventContext.Make(23, 0, 0, 0)), "6 to 18 does not hold at 23:00");
		ctx.True(day.Holds(LF_EventContext.Make(30, 0, 0, 0)), "an hour past 24 wraps (30 is 06:00)");
		ctx.True(LF_EventPrecondition.MakeHourRange(8, 8).Holds(LF_EventContext.Make(2, 0, 0, 0)), "equal bounds mean always");

		// A player count holds back the phase below its minimum.
		LF_EventPrecondition twoOnline = LF_EventPrecondition.MakePlayers(LF_EEventConditionKind.PLAYERS_ONLINE, 2);
		LF_EventPrecondition oneNearby = LF_EventPrecondition.MakePlayers(LF_EEventConditionKind.PLAYERS_NEARBY, 1);
		// Red-proof hook: with --red the below-minimum check is skipped, so the next assertion fails.
		if (!RedProof())
			ctx.False(twoOnline.Holds(LF_EventContext.Make(12, 1, 0, 0)), "one player online is below a minimum of two");
		else
			ctx.False(true, "red proof: below-minimum check skipped");
		ctx.True(twoOnline.Holds(LF_EventContext.Make(12, 2, 0, 0)), "two online meets a minimum of two");
		ctx.False(oneNearby.Holds(LF_EventContext.Make(12, 5, 0, 0)), "five online but none nearby does not hold");
		ctx.True(oneNearby.Holds(LF_EventContext.Make(12, 5, 1, 0)), "one nearby holds");

		// A cooldown: never ended is over, then it holds once the seconds pass.
		LF_EventPrecondition cool = LF_EventPrecondition.MakeCooldown(300);
		LF_EventContext fresh = LF_EventContext.Make(12, 0, 0, 100);
		ctx.True(cool.Holds(fresh), "an event that never ended is not cooling down");
		fresh.m_fLastEndedAt = 0;
		ctx.False(cool.Holds(fresh), "100 s after the end is still cooling down");
		fresh.m_fNow = 300;
		ctx.True(cool.Holds(fresh), "300 s after the end the cooldown is over");

		// Conditions are AND-ed.
		array<ref LF_EventPrecondition> all = {night, twoOnline};
		ctx.True(LF_EventPrecondition.AllHold(all, LF_EventContext.Make(23, 2, 0, 0)), "every condition holds");
		ctx.False(LF_EventPrecondition.AllHold(all, LF_EventContext.Make(23, 1, 0, 0)), "one condition short holds the phase");
		ctx.False(LF_EventPrecondition.AllHold(all, LF_EventContext.Make(12, 2, 0, 0)), "the other condition short holds the phase");
		ctx.True(LF_EventPrecondition.AllHold(null, LF_EventContext.Make(12, 0, 0, 0)), "no conditions holds");
		ctx.False(night.Holds(null), "no measurements hold nothing");

		// Aborts are OR-ed: any one ends the event.
		LF_EventAbort dawn = LF_EventAbort.MakeHourRange(5, 6);
		LF_EventAbort empty = LF_EventAbort.MakePlayers(LF_EEventAbortKind.PLAYERS_BELOW, 1);
		LF_EventAbort slow = LF_EventAbort.MakePhaseLimit(600);
		array<ref LF_EventAbort> aborts = {dawn, empty, slow};
		LF_EventContext live = LF_EventContext.Make(23, 3, 1, 100);
		ctx.False(LF_EventAbort.AnyFires(aborts, live), "no abort fires on a healthy event");
		ctx.True(LF_EventAbort.AnyFires(aborts, LF_EventContext.Make(5.5, 3, 1, 100)), "the dawn abort fires alone");
		ctx.True(LF_EventAbort.AnyFires(aborts, LF_EventContext.Make(23, 0, 0, 100)), "everyone leaving fires alone");
		live.m_fNow = 700;
		ctx.True(LF_EventAbort.AnyFires(aborts, live), "a phase that lasts too long fires alone");
		ctx.False(LF_EventAbort.AnyFires(null, live), "no aborts fires nothing");
		ctx.False(dawn.Fires(null), "no measurements fire nothing");

		// With the phase rules: any abort ends into cooldown, held conditions advance.
		array<ref LF_EventPhase> phases = {};
		phases.Insert(LF_EventPhase.Make("gather", 5, 0));
		phases.Insert(LF_EventPhase.Make("go", 5, 0));
		LF_EventDef def = LF_EventDef.Make("pre_test", phases, 300);
		LF_EventRunState state = new LF_EventRunState();
		LF_EventPhaseRules.Start(def, state, 0);

		LF_EventContext lone = LF_EventContext.Make(23, 0, 0, 10);
		LF_EEventPhaseDecision d = LF_EventPhaseRules.Decide(def, state, 10, LF_EventPrecondition.AllHold(all, lone), LF_EventAbort.AnyFires(aborts, lone));
		ctx.True(d == LF_EEventPhaseDecision.COOLDOWN, "an abort that fires ends the event");

		LF_EventContext ok = LF_EventContext.Make(23, 2, 1, 20);
		d = LF_EventPhaseRules.Decide(def, state, 20, LF_EventPrecondition.AllHold(all, ok), LF_EventAbort.AnyFires(aborts, ok));
		ctx.True(d == LF_EEventPhaseDecision.ADVANCE, "held conditions and no abort advance");

		array<ref LF_EventAbort> mild = {dawn, slow};
		LF_EventContext noon = LF_EventContext.Make(12, 2, 1, 30);
		d = LF_EventPhaseRules.Decide(def, state, 30, LF_EventPrecondition.AllHold(all, noon), LF_EventAbort.AnyFires(mild, noon));
		ctx.True(d == LF_EEventPhaseDecision.STAY, "an unmet hour condition holds the phase");
	}
}
