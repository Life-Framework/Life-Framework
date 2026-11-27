// red-proof: #5166 2026-11-27: `--only LF_Test_EvidenceLockerFill` failed alone on main with `- the cool-down is spent` (the game mode's convoy subscriber re-armed after a refused start); with `LF_EvidenceConvoy.ResetStatics()` at the start it passes alone. Observed 2026-11-19 with `tools/cli test --no-build --red LF_Test_EvidenceLockerFill` (RED PROOF OK): with the crossing row and the locker's break-in reset skipped it failed `- the locker is due once it crossed`, `- a break-in reset raises it early (expected 3, got 2)` and `- the event says why (expected 'break-in', got 'full')`. Without the hook it passes. Re-arm fix 2026-11-21: `--red LF_Test_EvidenceLockerFill,LF_Test_EvidenceConvoy` RED PROOF OK (skipping RearmNow fails "after the cool-down the still-full locker orders another convoy (expected 3, got 2)"; the convoy test fails "a refused start arms the re-order cool-down"), `--only` passed=3 failed=0.
//! The evidence locker knows how full it is (#3378): seized rows below the configured threshold
//! raise nothing; crossing it raises LF_OnEvidenceConvoyDue once; emptying the locker re-arms; a
//! break-in reset of the locker (LF_Breach.Reset on its target) raises it early, once.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_EvidenceLockerFill : LF_Test
{
	override string GetName()
	{
		return "investigation/locker-fill";
	}

	override void Run(LF_TestContext ctx)
	{
		array<ref LF_EvidenceLockerRow> before = LF_EvidenceLocker.ExportAll();
		LF_EvidenceLocker.ApplyAll(null);
		LF_EvidenceLockerFill.ResetStatics();
		// The game mode installs the real convoy subscriber; run alone it answers every order, its start is
		// refused in this world (no route) and it re-arms the cool-down (#5166). Only this test's listener hears the order.
		LF_EvidenceConvoy.ResetStatics();

		LF_EvidenceConvoyListener listener = new LF_EvidenceConvoyListener();
		LF_EventBus.Subscribe(LF_OnEvidenceConvoyDue, listener);

		int threshold = LF_EvidenceLockerFill.Threshold();
		ctx.True(threshold >= 2, "the configured threshold leaves room below it");
		if (threshold < 2)
		{
			LF_EventBus.Unsubscribe(LF_OnEvidenceConvoyDue, listener);
			LF_EvidenceLocker.ApplyAll(before);
			return;
		}

		for (int i = 0; i < threshold - 1; i++)
		{
			LF_EvidenceLocker.LogSeizure("{AAAAAAAAAAAAAAAA}Prefabs/Test/Seized.et", "Test Room", "fill-officer");
		}

		ctx.Equal(threshold - 1, LF_EvidenceLocker.Count(), "the rows below the threshold are on file");
		ctx.Equal(0, listener.m_iCount, "rows below the threshold raise nothing");
		ctx.False(LF_EvidenceLockerFill.IsDue(), "the locker is not due below the threshold");

		if (!RedProof())
			LF_EvidenceLocker.LogSeizure("{AAAAAAAAAAAAAAAA}Prefabs/Test/Seized.et", "Test Room", "fill-officer");

		ctx.Equal(1, listener.m_iCount, "crossing the threshold raises the convoy event once");
		ctx.Equal(threshold, listener.m_iRows, "the event carries the row count");
		ctx.EqualStr(LF_EvidenceLockerFill.REASON_FULL, listener.m_sReason, "the event says the locker is full");
		ctx.True(LF_EvidenceLockerFill.IsDue(), "the locker is due once it crossed");

		LF_EvidenceLocker.LogSeizure("{AAAAAAAAAAAAAAAA}Prefabs/Test/Seized.et", "Test Room", "fill-officer");
		ctx.Equal(1, listener.m_iCount, "more rows past the threshold raise nothing again");

		// Emptying re-arms; crossing again raises again.
		LF_EvidenceLocker.Raid("fill-raider");
		ctx.False(LF_EvidenceLockerFill.IsDue(), "an emptied locker is armed again");
		for (int j = 0; j < threshold; j++)
		{
			LF_EvidenceLocker.LogSeizure("{AAAAAAAAAAAAAAAA}Prefabs/Test/Seized.et", "Test Room", "fill-officer");
		}

		ctx.Equal(2, listener.m_iCount, "crossing again after emptying raises once more");

		// A convoy that never left (start refused, or called off) must not leave the order due for good:
		// the cool-down re-arms it, and the still-full locker orders another.
		LF_EvidenceLockerFill.ScheduleRearm();
		ctx.True(LF_EvidenceLockerFill.IsRearmPending(), "a refused or called-off convoy arms the cool-down");
		ctx.True(LF_EvidenceLockerFill.IsDue(), "the order stays due until the cool-down ends");
		ctx.Equal(2, listener.m_iCount, "nothing is ordered during the cool-down");
		if (!RedProof())
			LF_EvidenceLockerFill.RearmNow();

		ctx.Equal(3, listener.m_iCount, "after the cool-down the still-full locker orders another convoy");
		ctx.False(LF_EvidenceLockerFill.IsRearmPending(), "the cool-down is spent");
		LF_EvidenceLockerFill.ScheduleRearm();
		LF_EvidenceLocker.Raid("fill-raider");
		ctx.False(LF_EvidenceLockerFill.IsRearmPending(), "emptying the locker cancels the cool-down");
		LF_EvidenceLockerFill.RearmNow();
		ctx.Equal(3, listener.m_iCount, "a locker emptied during the cool-down orders nothing");
		for (int k = 0; k < threshold; k++)
		{
			LF_EvidenceLocker.LogSeizure("{AAAAAAAAAAAAAAAA}Prefabs/Test/Seized.et", "Test Room", "fill-officer");
		}

		ctx.Equal(4, listener.m_iCount, "filling it again orders a convoy");

		// A break-in reset raises it early, with the locker nearly empty.
		LF_EvidenceLocker.Raid("fill-raider");
		LF_EvidenceLocker.LogSeizure("{AAAAAAAAAAAAAAAA}Prefabs/Test/Seized.et", "Test Room", "fill-officer");
		ctx.Equal(4, listener.m_iCount, "one row raises nothing");
		LF_Breach.Reset("some-other-door");
		ctx.Equal(4, listener.m_iCount, "a reset of another target raises nothing");
		if (!RedProof())
			LF_Breach.Reset(LF_EvidenceLockerFill.LOCKER_TARGET_ID);

		ctx.Equal(5, listener.m_iCount, "a break-in reset raises it early");
		ctx.EqualStr(LF_EvidenceLockerFill.REASON_BREAK_IN, listener.m_sReason, "the event says why");
		LF_Breach.Reset(LF_EvidenceLockerFill.LOCKER_TARGET_ID);
		ctx.Equal(5, listener.m_iCount, "a second reset while the convoy is due raises nothing");

		LF_EventBus.Unsubscribe(LF_OnEvidenceConvoyDue, listener);
		LF_EvidenceLockerFill.ResetStatics();
		LF_EvidenceLocker.ApplyAll(before);
	}
}

//------------------------------------------------------------------------------------------------
//! Records LF_OnEvidenceConvoyDue for LF_Test_EvidenceLockerFill.
class LF_EvidenceConvoyListener : LF_EventSubscriber
{
	int m_iCount;
	int m_iRows;
	string m_sReason;

	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnEvidenceConvoyDue due = LF_OnEvidenceConvoyDue.Cast(busEvent);
		if (!due)
			return false;

		m_iCount++;
		m_iRows = due.m_iRows;
		m_sReason = due.m_sReason;
		return true;
	}
}
