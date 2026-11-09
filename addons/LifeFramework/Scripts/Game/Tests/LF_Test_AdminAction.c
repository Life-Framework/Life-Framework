// red-proof: Observed 2026-11-10 with `toolscli test --no-build --only LF_Test_AdminAction`: asserting two rows instead of one failed `!wadd writes one admin row (expected 2, got 1)`; restored.

// tier: LOGIC
//! incident/admin-action: an admin action (here !wadd and !handled) publishes one event that the
//! incident log records as kind admin (admin as subject, target as other party, action in how); a
//! missing admin id records nothing.
class LF_Test_AdminAction : LF_Test
{
	override string GetName()
	{
		return "incident/admin-action";
	}

	override void Run(LF_TestContext ctx)
	{
		array<ref LF_IncidentRecord> before = LF_IncidentLog.ExportAll();
		LF_IncidentLog.ResetStatics();
		LF_IncidentLog.Install();

		LF_AdminAction.ReportWhitelistAdd("admin-1", LF_WhitelistType.POLICE, "target-1");
		ctx.Equal(1, LF_IncidentLog.Count(), "!wadd writes one admin row");
		array<ref LF_IncidentRecord> rows = LF_IncidentLog.Query(vector.Zero, -1, 0, -1, LF_IncidentConfig.KIND_ADMIN);
		ctx.Equal(1, rows.Count(), "the row is an admin row");
		if (rows.Count() == 1)
		{
			ctx.EqualStr("admin-1", rows[0].m_sSubjectUid, "the admin is the subject");
			ctx.EqualStr("target-1", rows[0].m_sOtherUid, "the whitelisted player is the other party");
			ctx.True(rows[0].m_sHow.StartsWith("!wadd"), "the action names the command");
		}

		LF_AdminAction.ReportHandled("admin-1", 7);
		ctx.Equal(2, LF_IncidentLog.Count(), "!handled writes an admin row too");

		LF_AdminAction.Report("", "target-1", "teleport", vector.Zero);
		ctx.Equal(2, LF_IncidentLog.Count(), "an action with no admin id writes nothing");

		LF_IncidentLog.ResetStatics();
		LF_IncidentLog.ApplyAll(before);
	}
}
