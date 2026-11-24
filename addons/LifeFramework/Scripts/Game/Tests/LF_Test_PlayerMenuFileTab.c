// red-proof: observed 2026-11-24 `tools/cli test --no-build --red LF_Test_PlayerMenuFileTab`: RED PROOF OK (with the hook on the account has no ticket, booking or ban: the booking count, its age and the seized licence all fail). Green with `--only`.
//! The F2 My file tab (#4711, police PO-P10), no world: an account with one open ticket, one booking and a
//! seized driving licence gets one line each, the licence with the days left that the licence desk quotes;
//! the answer round-trips on its wire; a clean account gets the three "nothing" lines; a paid ticket and
//! another account's ticket, booking and ban never show. The test puts back every store it touched.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_PlayerMenuFileTab : LF_Test
{
	protected static const string UID = "filetab-test-uid";
	protected static const string OTHER = "filetab-test-other";
	protected static const string CLEAN = "filetab-test-clean";
	protected static const int TODAY = 40000;
	protected static const int NOW_MINUTES = 5000000;
	protected static const int BAN_DAYS = 3;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "playermenu/file-tab";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_FineTicketRecord> keptTickets = LF_FineTickets.ExportAll();
		array<ref LF_ConvictionRow> keptConvictions = LF_ConvictionRecord.ExportAll();
		int keptNow = LF_LicenceBans.Now();
		array<ref LF_LicenceBanRow> keptBans = LF_LicenceBans.ExportAll(keptNow);

		LF_FineTickets.ApplyAll(null);
		LF_ConvictionRecord.ApplyAll(null);
		LF_LicenceBans.ApplyAll(new array<ref LF_LicenceBanRow>(), NOW_MINUTES);
		LF_LicenceBans.SetNowForTest(NOW_MINUTES);
		LF_ConvictionRecord.SetTodayForTest(TODAY);

		// RedProof(): the account has no ticket, booking or ban on file (`--red`).
		if (!RedProof())
			Seed();

		CheckFile(ctx);
		CheckClean(ctx);

		LF_ConvictionRecord.SetTodayForTest(-1);
		LF_LicenceBans.SetNowForTest(-1);
		LF_FineTickets.ApplyAll(keptTickets);
		LF_ConvictionRecord.ApplyAll(keptConvictions);
		LF_LicenceBans.ApplyAll(keptBans, keptNow);
	}

	//------------------------------------------------------------------------------------------------
	//! UID: one open ticket, one paid ticket, a booking two days ago and a driving ban. OTHER: the same
	//! kinds of record, which must never show on UID's file.
	protected static void Seed()
	{
		LF_FineTicketRecord open = LF_FineTickets.Issue(UID, "Ada Quill", "off-1", "Cmdr Vale", 3, "#LF-Offence_UnpaidFine", 150, "01/06/1989 10:00");
		LF_FineTicketRecord paid = LF_FineTickets.Issue(UID, "Ada Quill", "off-1", "Cmdr Vale", 3, "#LF-Offence_UnpaidFine", 90, "01/06/1989 11:00");
		if (paid)
			paid.m_bPaid = true;

		LF_FineTickets.Issue(OTHER, "Rex Rival", "off-1", "Cmdr Vale", 3, "#LF-Offence_UnpaidFine", 400, "01/06/1989 12:00");

		LF_ConvictionRecord.SetTodayForTest(TODAY - 2);
		LF_ConvictionRecord.Record(UID);
		LF_ConvictionRecord.SetTodayForTest(TODAY);
		LF_ConvictionRecord.Record(OTHER);
		LF_ConvictionRecord.Record(OTHER);

		LF_LicenceBans.Ban(UID, LF_ELicenseType.DRIVING_LICENCE, BAN_DAYS);
		LF_LicenceBans.Ban(OTHER, LF_ELicenseType.FIREARM_PERMIT, 9);
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_UIPart Find(notnull array<ref LF_UIPart> parts, string name)
	{
		foreach (LF_UIPart part : parts)
		{
			if (part.GetName() == name)
				return part;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckFile(LF_TestContext ctx)
	{
		LF_PlayerMenuFileData data = LF_PlayerMenuFileTab.Gather(UID, TODAY);
		ctx.Equal(1, data.m_aTicketNumbers.Count(), "one open ticket: the paid one and the other player's are not listed");
		ctx.Equal(1, data.m_iBookings, "one booking on this account, not the other player's two");
		ctx.Equal(2, data.m_iLastBookingDaysAgo, "the latest booking was two in-game days ago");
		ctx.Equal(1, data.m_aLicences.Count(), "one seized licence: the other player's firearm ban is not here");

		if (data.m_aTicketNumbers.IsEmpty() || data.m_aLicences.IsEmpty())
			return;

		// The wire carries the same facts and none of the other account's.
		string wire = data.Encode();
		ctx.False(wire.Contains("Rex Rival") || wire.Contains("400") || wire.Contains("FIREARM"), "another account's records are not on the wire");
		LF_PlayerMenuFileData back = LF_PlayerMenuFileData.Decode(wire);
		ctx.NotNull(back, "the wire decodes");
		if (!back)
			return;

		ctx.Equal(150, back.m_aTicketAmounts[0], "the ticket's amount survives the wire");
		ctx.Equal(2, back.m_iLastBookingDaysAgo, "the booking's age survives the wire");
		ctx.EqualStr("DRIVING_LICENCE", back.m_aLicences[0], "the licence survives the wire");
		ctx.Equal(BAN_DAYS, back.m_aLicenceDays[0], "the days left survive the wire, the same count the licence desk quotes");
		ctx.Equal(LF_LicenceSeizure.DaysLeft(UID, LF_ELicenseType.DRIVING_LICENCE), back.m_aLicenceDays[0], "the tab and the licence desk agree on the days left");

		array<ref LF_UIPart> parts = {};
		LF_PlayerMenuFileTab.Build(parts, back);
		LF_UIPart ticket = Find(parts, LF_PlayerMenuFileTab.ROW_TICKET + "0");
		LF_UIPart count = Find(parts, LF_PlayerMenuFileTab.ROW_BOOKING + "count");
		LF_UIPart last = Find(parts, LF_PlayerMenuFileTab.ROW_BOOKING + "last");
		LF_UIPart licence = Find(parts, LF_PlayerMenuFileTab.ROW_LICENCE + "0");
		ctx.True(ticket != null, "one line for the open ticket");
		ctx.True(Find(parts, LF_PlayerMenuFileTab.ROW_TICKET + "1") == null, "no line for the paid ticket");
		ctx.True(count != null && count.GetStatus() == "1", "one line for the booking, counting one");
		ctx.True(last != null, "a line for when the latest booking was");
		ctx.True(licence != null && licence.GetText() == "#LF-PlayerMenuFile_LicenceDriving", "one line for the seized driving licence");
		ctx.True(Find(parts, LF_PlayerMenuFileTab.ROW_TICKET + "none") == null && Find(parts, LF_PlayerMenuFileTab.ROW_BOOKING + "none") == null && Find(parts, LF_PlayerMenuFileTab.ROW_LICENCE + "none") == null, "no 'nothing' line when there is something");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckClean(LF_TestContext ctx)
	{
		LF_PlayerMenuFileData data = LF_PlayerMenuFileTab.Gather(CLEAN, TODAY);
		array<ref LF_UIPart> parts = {};
		LF_PlayerMenuFileTab.Build(parts, data);
		ctx.True(Find(parts, LF_PlayerMenuFileTab.ROW_TICKET + "none") != null, "a clean account says it has no tickets");
		ctx.True(Find(parts, LF_PlayerMenuFileTab.ROW_BOOKING + "none") != null, "a clean account says it has no bookings");
		ctx.True(Find(parts, LF_PlayerMenuFileTab.ROW_LICENCE + "none") != null, "a clean account says it has no seized licence");

		array<ref LF_UIPart> waiting = {};
		LF_PlayerMenuFileTab.Build(waiting, null);
		ctx.True(Find(waiting, LF_PlayerMenuFileTab.PART_WAITING) != null, "no answer yet prints the waiting line");
		ctx.True(LF_PlayerMenuFileData.Decode("V|99") == null, "a wire of another version is refused");
	}
}
