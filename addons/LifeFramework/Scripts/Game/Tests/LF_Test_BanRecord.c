// red-proof: Observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_BanRecord`: with the separator cleaning and the V1 check removed from LF_BanRecord, it failed `the file reads back the two rows and skips the legacy and broken ones (expected 2, got 1)`; restored.
// tier: PERSISTENCE
//! moderation/ban-record (#1285): a ban and an unban each append one versioned row to the ban
//! record file; reading the file back returns both with reason, staff uid and the target's recent
//! incident ids; a legacy or malformed line is skipped with a warning, never thrown.
class LF_Test_BanRecord : LF_Test
{
	static const string PATH = "$profile:LifeFramework/test_ban_record.txt";

	override string GetName()
	{
		return "moderation/ban-record";
	}

	override void Run(LF_TestContext ctx)
	{
		array<ref LF_IncidentRecord> before = LF_IncidentLog.ExportAll();
		LF_IncidentLog.ResetStatics();
		LF_BanRecord.SetSavePath(PATH);
		FileIO.DeleteFile(PATH);

		string target = "test-ban-target";
		string staff = "test-ban-admin";
		int now = System.GetUnixTime();
		LF_IncidentRecord killed = LF_IncidentLog.Append(LF_IncidentConfig.KIND_DEATH, now - 60, "test-ban-victim", target, vector.Zero, 0, "rifle", true);
		LF_IncidentLog.Append(LF_IncidentConfig.KIND_DEATH, now - 30, "someone-else", "nobody", vector.Zero, 0, "rifle", false);

		ctx.True(LF_BanRecord.Record(LF_BanRecord.ACTION_BAN, target, staff, "RDM | twice\nin town", 3600, now), "the ban writes a row");
		ctx.True(LF_BanRecord.Record(LF_BanRecord.ACTION_UNBAN, target, staff, "appeal accepted", 0, now + 10), "the unban writes a row");
		ctx.False(LF_BanRecord.Record(LF_BanRecord.ACTION_BAN, "", staff, "no target", 0, now), "a row with no target is refused");

		// A legacy row (no version) and a broken row, as an older build or a hand edit leaves them.
		FileHandle handle = FileIO.OpenFile(PATH, FileMode.APPEND);
		if (handle)
		{
			handle.WriteLine("BAN|" + target + "|123|old-admin|reason");
			handle.WriteLine("V1|BAN|" + target + "|not enough fields");
			handle.Close();
		}

		array<ref LF_BanRow> rows = LF_BanRecord.ReadFor(target);
		ctx.Equal(2, rows.Count(), "the file reads back the two rows and skips the legacy and broken ones");
		if (rows.Count() == 2)
		{
			ctx.EqualStr(LF_BanRecord.ACTION_BAN, rows[0].m_sAction, "the first row is the ban");
			ctx.EqualStr(staff, rows[0].m_sStaffUid, "the ban names the staff uid");
			ctx.EqualStr("RDM / twice in town", rows[0].m_sReason, "the reason survives with its separators cleaned");
			ctx.Equal(3600, rows[0].m_iDuration, "the duration is kept");
			ctx.Equal(now, rows[0].m_iTime, "the time is kept");
			ctx.Equal(1, rows[0].m_aIncidentIds.Count(), "the ban names the target's one recent incident");
			if (rows[0].m_aIncidentIds.Count() == 1 && killed)
				ctx.Equal(killed.m_iNumber, rows[0].m_aIncidentIds[0], "the incident id is the killing");
			ctx.EqualStr(LF_BanRecord.ACTION_UNBAN, rows[1].m_sAction, "the second row is the unban");
			ctx.EqualStr("appeal accepted", rows[1].m_sReason, "the unban keeps its reason");
		}

		ctx.Equal(0, LF_BanRecord.ReadFor("someone-else").Count(), "another account has no rows");
		ctx.True(LF_BanRecord.Describe(target, rows).Contains("appeal accepted"), "the admin listing shows the rows");

		FileIO.DeleteFile(PATH);
		LF_BanRecord.SetSavePath("");
		LF_IncidentLog.ResetStatics();
		LF_IncidentLog.ApplyAll(before);
	}
}
