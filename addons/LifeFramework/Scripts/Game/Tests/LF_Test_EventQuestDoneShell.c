// red-proof: with `tools\cli test --no-build --red LF_Test_EventQuestDoneShell` the quest turn-in is not noted and the event never starts (`red proof: turn-in not noted`).
//! The QUEST_DONE condition through the event shell (#3784): an event whose first phase waits on a
//! quest stays idle until a turn-in is noted, then waits out the delay and starts; the quest's real
//! turn-in path (LF_QuestDoneLog.Note from LF_QuestTrackerComponent) is the one noted here. Drives
//! Tick with its own clock; the live shell is restored after.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_EventQuestDoneShell : LF_Test
{
	override string GetName()
	{
		return "events/quest-done-shell";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_EventShell.Stop();
		LF_EventShell.Unregister("test_quest_tip");
		LF_QuestDoneLog.ResetStatics();

		ctx.True(LF_EventShell.RegisterCondition("test_quest_tip", LF_EventPrecondition.MakeQuestDone("test_shipment_report", 60, 0)), "the quest condition registers");

		array<ref LF_EventPhase> phases = {};
		array<string> waitModules = {"test_quest_tip"};
		phases.Insert(LF_EventPhase.Make("raid", 0, 0, waitModules));
		LF_EventsConfig config = new LF_EventsConfig();
		config.m_aEvents = {};
		config.m_aEvents.Insert(LF_EventDef.Make("test_quest_event", phases, 300));
		LF_EventShell.Use(config);
		ctx.Equal(1, LF_EventShell.RunCount(), "the test event loads");

		// Nobody has turned the quest in: the event stays idle however long it waits.
		LF_EventShell.Tick(100);
		LF_EventShell.Tick(1000);
		ctx.EqualStr("", LF_EventShell.PhaseOf("test_quest_event"), "no turn-in keeps the event idle");

		// A turn-in at 1100 s: still idle inside the delay, starts once it has passed.
		if (!RedProof())
			LF_QuestDoneLog.Note("test_shipment_report", 1100);
		else
			ctx.False(true, "red proof: turn-in not noted");
		LF_EventShell.Tick(1130);
		ctx.EqualStr("", LF_EventShell.PhaseOf("test_quest_event"), "inside the delay the event still waits");
		LF_EventShell.Tick(1160);
		ctx.EqualStr("raid", LF_EventShell.PhaseOf("test_quest_event"), "after the delay the event starts");

		LF_QuestDoneLog.ResetStatics();
		LF_EventShell.Unregister("test_quest_tip");
		LF_EventShell.Use(LF_EventShell.LoadConfig());
		LF_EventShell.StartIfServer();
	}
}
