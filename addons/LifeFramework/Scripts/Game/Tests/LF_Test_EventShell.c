// red-proof: observed 2026-11-16 with `tools\cli test --no-build --red LF_Test_EventShell`: with the hook on the forced-hour ticks are skipped and the test failed (`one LF_OnEventPhase per change (expected 3, got 0)`), `RED PROOF OK (all 1 listed test(s) fail with the hook on)`; without the hook `SUMMARY tier=only passed=1 failed=0`.
// tier: WORLD
//! The event shell (#1950): a test event with a forced in-game hour starts in prep, advances to hot
//! and ends into cooldown, publishing one LF_OnEventPhase per change; outside its hour a failing
//! start condition keeps it idle; a cooling event does not restart; an event that is off
//! (the shipped core example) never runs. Drives Tick with its own clock.
class LF_Test_EventShell_Listener : LF_EventSubscriber
{
	ref array<string> m_aSeen = {};

	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnEventPhase e = LF_OnEventPhase.Cast(busEvent);
		if (e && e.m_sEventId == "test_shell")
			m_aSeen.Insert(string.Format("%1:%2:%3", e.m_sKind, e.m_sToPhase, e.m_sReason));
		return true;
	}
}

class LF_Test_EventShell : LF_Test
{
	override string GetName()
	{
		return "events/shell";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// The core example ships off: a live server never runs it unless a pack turns it on.
		LF_EventDef example = LF_EventOverlay.Find("example");
		ctx.True(example != null, "the core example event loads");
		if (example)
			ctx.False(example.m_bEnabled, "the core example event is off by default");

		// Stop the live ticker and swap in the test event; other modules' registrations stay.
		LF_EventShell.Stop();
		LF_EventShell.Unregister("test_shell_evening");
		LF_Test_EventShell_Listener listener = new LF_Test_EventShell_Listener();
		LF_EventBus.Subscribe(LF_OnEventPhase, listener);

		ctx.True(LF_EventShell.RegisterCondition("test_shell_evening", LF_EventPrecondition.MakeHourRange(18, 22)), "the hour condition registers");
		ctx.False(LF_EventShell.RegisterCondition("test_shell_evening", LF_EventPrecondition.MakeHourRange(1, 2)), "a taken module id is refused");

		array<ref LF_EventPhase> phases = {};
		array<string> prepModules = {"test_shell_evening"};
		phases.Insert(LF_EventPhase.Make("prep", 0, 0, prepModules));
		phases.Insert(LF_EventPhase.Make("hot", 0, 0));
		LF_EventsConfig config = new LF_EventsConfig();
		config.m_aEvents = {};
		config.m_aEvents.Insert(LF_EventDef.Make("test_shell", phases, 300));
		config.m_aEvents.Insert(LF_EventDef.Make("test_shell", phases, 300));
		LF_EventDef off = LF_EventDef.Make("test_shell_off", phases, 300);
		off.m_bEnabled = false;
		config.m_aEvents.Insert(off);
		array<ref LF_EventPhase> none = {};
		config.m_aEvents.Insert(LF_EventDef.Make("test_broken", none, 300));
		LF_EventShell.Use(config);
		ctx.Equal(1, LF_EventShell.RunCount(), "a repeated id, an event with no phases and an event that is off are skipped");

		// Noon is outside 18 to 22: the start condition fails and the event stays idle.
		LF_EventShell.ForceHour(12);
		LF_EventShell.Tick(0);
		LF_EventShell.Tick(1);
		ctx.Equal(0, listener.m_aSeen.Count(), "a failing precondition keeps the event idle");
		ctx.EqualStr("", LF_EventShell.PhaseOf("test_shell"), "no phase while idle");

		// 20:00 is inside: start into prep, advance to hot, end into cooldown, one event per change.
		LF_EventShell.ForceHour(20);
		if (!RedProof())
		{
			LF_EventShell.Tick(10);
			ctx.EqualStr("prep", LF_EventShell.PhaseOf("test_shell"), "the event starts in prep");
			LF_EventShell.Tick(11);
			ctx.EqualStr("hot", LF_EventShell.PhaseOf("test_shell"), "prep advances to hot");
			LF_EventShell.Tick(12);
		}
		ctx.Equal(3, listener.m_aSeen.Count(), "one LF_OnEventPhase per change");
		if (listener.m_aSeen.Count() == 3)
		{
			ctx.EqualStr("start:prep:", listener.m_aSeen[0], "first: start into prep");
			ctx.EqualStr("advance:hot:", listener.m_aSeen[1], "second: advance to hot");
			ctx.EqualStr("end::done", listener.m_aSeen[2], "third: the last phase ends into cooldown");
		}

		ctx.EqualStr("", LF_EventShell.PhaseOf("test_shell_off"), "an event that is off never starts, even inside its hour");

		// Cooling down: the hour still holds but the event does not restart before 300 s.
		LF_EventShell.Tick(13);
		LF_EventShell.Tick(200);
		ctx.Equal(3, listener.m_aSeen.Count(), "a cooling event does not restart");
		LF_EventShell.Tick(400);
		ctx.EqualStr("prep", LF_EventShell.PhaseOf("test_shell"), "after the cooldown it starts again");

		LF_EventBus.Unsubscribe(LF_OnEventPhase, listener);
		LF_EventShell.Unregister("test_shell_evening");
		LF_EventShell.ForceHour(-1);
		LF_EventShell.Use(LF_EventShell.LoadConfig());
		LF_EventShell.StartIfServer();
	}
}
