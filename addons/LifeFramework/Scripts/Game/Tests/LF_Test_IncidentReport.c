// red-proof: Observed 2026-11-09 with `--only`: with the cooldown check disabled in LF_IncidentFlags.Report, `a second report inside the cooldown is refused (expected 1, got 2)`; restored; green.

// tier: LOGIC
//! incident/report: a player report flags only the reporter's recent incidents, the cooldown
//! refuses a second report, an administrator marks a flag handled, the Game Master feed carries
//! flagged incidents first (even far away), and the flag command parses.
class LF_Test_IncidentReport : LF_Test
{
	override string GetName()
	{
		return "incident/report";
	}

	override void Run(LF_TestContext ctx)
	{
		array<ref LF_IncidentRecord> before = LF_IncidentLog.ExportAll();
		array<ref LF_IncidentFlagRow> flagsBefore = LF_IncidentFlags.ExportAll();
		LF_IncidentLog.ResetStatics();

		int now = 100000;
		LF_IncidentRecord shot = LF_IncidentLog.Append(LF_IncidentConfig.KIND_DAMAGE, now - 60, "rep", "bully", Vector(9000, 0, 9000), 30, "rifle", false);
		LF_IncidentRecord stale = LF_IncidentLog.Append(LF_IncidentConfig.KIND_DAMAGE, now - 5000, "rep", "bully", Vector(10, 0, 10), 30, "rifle", false);
		LF_IncidentRecord other = LF_IncidentLog.Append(LF_IncidentConfig.KIND_DEATH, now - 30, "bystander", "bully", Vector(10, 0, 10), 0, "rifle", true);
		LF_IncidentRecord asAttacker = LF_IncidentLog.Append(LF_IncidentConfig.KIND_DAMAGE, now - 20, "bully", "rep", Vector(10, 0, 10), 5, "fist", false);
		ctx.True(shot && stale && other && asAttacker, "four incidents were logged");
		if (!shot || !stale || !other || !asAttacker)
			return;

		int flagged;
		ctx.Equal(LF_IncidentFlags.REPORT_OK, LF_IncidentFlags.Report("rep", now, flagged), "the report is accepted");
		ctx.Equal(2, flagged, "two incidents of the reporter from the window are flagged");
		ctx.True(LF_IncidentFlags.IsFlagged(shot.m_iNumber), "the reporter's recent incident is flagged");
		ctx.True(LF_IncidentFlags.IsFlagged(asAttacker.m_iNumber), "an incident where they were the other party is flagged");
		ctx.False(LF_IncidentFlags.IsFlagged(stale.m_iNumber), "an incident older than the window is not");
		ctx.False(LF_IncidentFlags.IsFlagged(other.m_iNumber), "somebody else's incident is not");

		ctx.Equal(LF_IncidentFlags.REPORT_COOLDOWN, LF_IncidentFlags.Report("rep", now + 10, flagged), "a second report inside the cooldown is refused");
		ctx.Equal(0, flagged, "the refused report flags nothing");
		ctx.Equal(LF_IncidentFlags.REPORT_NOTHING, LF_IncidentFlags.Report("rep", now + 1000, flagged), "after the cooldown, nothing new in the window flags nothing");

		// The Game Master feed: the flagged far incident arrives first and marked, though it is
		// outside the radius of the camera.
		array<string> rows = LF_IncidentGm.Answer(true, 7, Vector(10, 0, 10), now);
		ctx.Equal(3, rows.Count(), "the feed has the near rows and the far flagged one");
		if (rows.Count() == 3)
		{
			array<string> first = {};
			rows[0].Split("|", first, false);
			ctx.Equal(12, first.Count(), "a row has twelve fields");
			ctx.EqualStr("1", first[11], "the far flagged incident comes first, marked flagged");
			ctx.EqualStr(shot.m_iNumber.ToString(), first[0], "and it is the one the reporter flagged");
		}

		ctx.True(LF_IncidentFlags.Handle(shot.m_iNumber, "admin"), "an administrator marks it handled");
		ctx.False(LF_IncidentFlags.IsFlagged(shot.m_iNumber), "the flag is gone");
		ctx.False(LF_IncidentFlags.Handle(shot.m_iNumber, "admin"), "handling it twice is refused");
		ctx.Equal(1, LF_IncidentFlags.Count(), "the other flag stays until handled");

		int number;
		ctx.True(LF_IncidentFlags.TryParseHandled("!handled 12", number), "the handled command parses");
		ctx.Equal(12, number, "with its number");
		ctx.False(LF_IncidentFlags.TryParseHandled("!handled twelve", number), "a word is not a number");
		ctx.False(LF_IncidentFlags.TryParseHandled("!wadd POLICE x", number), "another command is not it");

		LF_IncidentLog.ResetStatics();
		LF_IncidentFlags.ApplyAll(flagsBefore);
		LF_IncidentLog.ApplyAll(before);
	}
}
