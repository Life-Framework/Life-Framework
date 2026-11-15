// red-proof: Observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_BanRecord`: with the separator cleaning and the V1 check removed from LF_BanRecord, it failed `the file reads back the two rows and skips the legacy and broken ones (expected 2, got 1)`; restored. Review fixes, observed 2026-11-14 with the success check in LF_BanRecord.Complete and the torn-line guard in Append disabled: `the failed ban left no row (expected 0, got 2)` and `the row after a torn line reads back (expected 1, got 0)`; restored. Re-observed 2026-11-15 (Complete success check disabled): `the failed ban left no row (expected 0, got 2)`, `the confirmed ban is on file (expected 1, got 3)`; restored.
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

		// M1: a ban is recorded only when the backend's final answer is OK on a create or remove.
		string failed = "test-ban-failed";
		ScrServerCmdResult pendingResult = new ScrServerCmdResult("", EServerCmdResultType.PENDING);
		array<string> createArgv = {"ban", "create", failed, "600"};
		LF_BanPending pending = LF_BanRecord.Prepare(createArgv, staff, pendingResult);
		ctx.True(pending != null, "a sent #ban create is held until the backend answers");
		if (pending)
		{
			ctx.EqualStr(LF_BanRecord.NO_REASON, pending.m_sReason, "an empty reason is stored as vanilla sends it");
			ctx.False(LF_BanRecord.Complete(pending, SCR_EBanSubcommandArg.EBSA_CREATE, EServerCmdResultType.ERR, now), "a ban the backend refused writes nothing");
			ctx.False(LF_BanRecord.Complete(pending, SCR_EBanSubcommandArg.EBSA_LIST_FILTERED, EServerCmdResultType.OK, now), "a multiple-match list writes nothing");
			ctx.Equal(0, LF_BanRecord.ReadFor(failed).Count(), "the failed ban left no row");
			ctx.True(LF_BanRecord.Complete(pending, SCR_EBanSubcommandArg.EBSA_CREATE, EServerCmdResultType.OK, now), "a confirmed ban writes its row");
			ctx.Equal(1, LF_BanRecord.ReadFor(failed).Count(), "the confirmed ban is on file");
		}
		ScrServerCmdResult errResult = new ScrServerCmdResult("Player not found", EServerCmdResultType.ERR);
		ctx.True(LF_BanRecord.Prepare(createArgv, staff, errResult) == null, "a refused command is not held");

		// L2: #ban remove keeps a digits-only id as typed (vanilla sends it to the backend as an identity).
		array<string> removeArgv = {"ban", "remove", "12345"};
		LF_BanPending unban = LF_BanRecord.Prepare(removeArgv, staff, pendingResult);
		if (unban)
			ctx.EqualStr("12345", unban.m_sTargetUid, "a digits-only unban id stays as typed");
		else
			ctx.True(false, "a sent #ban remove is held");

		// L1: a crash-torn last line does not swallow the next row.
		FileHandle torn = FileIO.OpenFile(PATH, FileMode.APPEND);
		if (torn)
		{
			torn.Write("V1|BAN|torn-row");
			torn.Close();
		}
		ctx.True(LF_BanRecord.Record(LF_BanRecord.ACTION_BAN, "test-ban-after-torn", staff, "after a torn row", 60, now), "a row after a torn line is written");
		ctx.Equal(1, LF_BanRecord.ReadFor("test-ban-after-torn").Count(), "the row after a torn line reads back");
		ctx.True(LF_BanRecord.Describe(target, rows).Contains("appeal accepted"), "the admin listing shows the rows");

		FileIO.DeleteFile(PATH);
		LF_BanRecord.SetSavePath("");
		LF_IncidentLog.ResetStatics();
		LF_IncidentLog.ApplyAll(before);
	}
}
