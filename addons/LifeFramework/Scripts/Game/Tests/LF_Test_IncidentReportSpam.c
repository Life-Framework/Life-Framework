// red-proof: RED-OK 2026-11-17, `tools/cli test --no-build --red LF_Test_IncidentReportSpam,...` (hook reports past the cooldown, as the unfixed code let every empty report through): `the empty report started the cooldown, so the next one inside it is refused` failed; restored, green.

// tier: LOGIC
//! incident/report-spam (#2414): a /report with nothing to flag starts the cooldown too, so it cannot be
//! repeated every frame (each call ran a log query, wrote a line and sent a notification).
class LF_Test_IncidentReportSpam : LF_Test
{
	override string GetName()
	{
		return "incident/report-spam";
	}

	override void Run(LF_TestContext ctx)
	{
		array<ref LF_IncidentRecord> before = LF_IncidentLog.ExportAll();
		array<ref LF_IncidentFlagRow> flagsBefore = LF_IncidentFlags.ExportAll();
		LF_IncidentLog.ResetStatics();
		LF_IncidentFlags.ApplyAll(null);

		int now = 300000;
		int flagged;
		ctx.Equal(LF_IncidentFlags.REPORT_NOTHING, LF_IncidentFlags.Report("spam-rep", now, flagged), "a report with nothing to flag says so");

		int later = now + 5;
		if (RedProof())
			later = now + LF_IncidentConfig.ReportCooldownSeconds() + 1;
		ctx.Equal(LF_IncidentFlags.REPORT_COOLDOWN, LF_IncidentFlags.Report("spam-rep", later, flagged), "the empty report started the cooldown, so the next one inside it is refused");

		ctx.Equal(LF_IncidentFlags.REPORT_NOTHING, LF_IncidentFlags.Report("spam-rep", now + LF_IncidentConfig.ReportCooldownSeconds() + 1, flagged), "after the cooldown a report is answered again");

		// Something to flag still works, and an empty report from someone else is not affected.
		LF_IncidentLog.Append(LF_IncidentConfig.KIND_DAMAGE, now + 1000, "spam-other", "spam-bully", vector.Zero, 10, "fist", false);
		ctx.Equal(LF_IncidentFlags.REPORT_OK, LF_IncidentFlags.Report("spam-other", now + 1001, flagged), "another player's report with something to flag is accepted");
		ctx.Equal(1, flagged, "and flags it");

		LF_IncidentLog.ResetStatics();
		LF_IncidentFlags.ApplyAll(flagsBefore);
		LF_IncidentLog.ApplyAll(before);
	}
}
