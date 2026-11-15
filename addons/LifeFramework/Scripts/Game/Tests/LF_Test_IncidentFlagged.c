// red-proof: Observed 2026-11-15 with `tools\cli test --no-build --only LF_Test_IncidentFlagged` before LF_IncidentFlags.Report published: `a report that flagged publishes one event (expected 1, got 0)`; green after.
// tier: LOGIC
//! incident/flagged-event: a report that flags incidents publishes one LF_OnIncidentFlagged with the
//! reporter and the flagged count, after the rows exist; a refused or empty report publishes none.
class LF_Test_IncidentFlaggedSubscriber : LF_EventSubscriber
{
	ref array<ref LF_OnIncidentFlagged> m_aSeen = new array<ref LF_OnIncidentFlagged>();

	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnIncidentFlagged flagged = LF_OnIncidentFlagged.Cast(busEvent);
		if (!flagged)
			return false;

		// The rows must already be in: the event follows the flag, never leads it.
		if (LF_IncidentFlags.Count() < flagged.m_iFlagged)
			return false;

		m_aSeen.Insert(flagged);
		return true;
	}
}

class LF_Test_IncidentFlagged : LF_Test
{
	override string GetName()
	{
		return "incident/flagged-event";
	}

	override void Run(LF_TestContext ctx)
	{
		array<ref LF_IncidentRecord> before = LF_IncidentLog.ExportAll();
		array<ref LF_IncidentFlagRow> flagsBefore = LF_IncidentFlags.ExportAll();
		LF_IncidentLog.ResetStatics();
		LF_IncidentFlags.ApplyAll(null);

		LF_Test_IncidentFlaggedSubscriber sub = new LF_Test_IncidentFlaggedSubscriber();
		LF_EventBus.Subscribe(LF_OnIncidentFlagged, sub);

		int now = 200000;
		LF_IncidentRecord one = LF_IncidentLog.Append(LF_IncidentConfig.KIND_DAMAGE, now - 30, "flag-rep", "flag-bully", Vector(10, 0, 10), 30, "rifle", false);
		LF_IncidentRecord two = LF_IncidentLog.Append(LF_IncidentConfig.KIND_DAMAGE, now - 20, "flag-bully", "flag-rep", Vector(10, 0, 10), 5, "fist", false);
		ctx.True(one && two, "two incidents were logged");

		int flagged;
		ctx.Equal(LF_IncidentFlags.REPORT_NOTHING, LF_IncidentFlags.Report("flag-stranger", now, flagged), "a stranger has nothing to flag");
		ctx.Equal(0, sub.m_aSeen.Count(), "an empty report publishes nothing");

		ctx.Equal(LF_IncidentFlags.REPORT_OK, LF_IncidentFlags.Report("flag-rep", now, flagged), "the report is accepted");

		ctx.Equal(1, sub.m_aSeen.Count(), "a report that flagged publishes one event");
		if (sub.m_aSeen.Count() == 1)
		{
			ctx.EqualStr("flag-rep", sub.m_aSeen[0].m_sReporterUid, "the event names the reporter");
			ctx.Equal(2, sub.m_aSeen[0].m_iFlagged, "and how many incidents were flagged");
		}

		ctx.Equal(LF_IncidentFlags.REPORT_COOLDOWN, LF_IncidentFlags.Report("flag-rep", now + 5, flagged), "a second report is refused");
		ctx.Equal(1, sub.m_aSeen.Count(), "a refused report publishes nothing");

		LF_EventBus.Unsubscribe(LF_OnIncidentFlagged, sub);
		LF_IncidentLog.ResetStatics();
		LF_IncidentFlags.ApplyAll(flagsBefore);
		LF_IncidentLog.ApplyAll(before);
	}
}
