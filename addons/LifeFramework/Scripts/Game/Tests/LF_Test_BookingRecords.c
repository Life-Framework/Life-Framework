// red-proof: RED-OK law/booking-records, observed 2026-11-24 with `node tools/cli.mjs test --no-build --red LF_Test_BookingRecords,LF_Test_BookingRecordsPersist`: with the record writes skipped, `pay now writes one record (expected 1, got 0)` and `the wire carries the one record`. Green: `--only` passed with LF_Test_PlayerMenuFileTab and LF_Test_SheetLicenceSeizure.
//! Booking records (#4711 follow-up): a settled desk offer writes one row on the suspect's record with the
//! in-game day, the sheet's codes, the fine, how it was settled, the licences the sheet took and the cell
//! time; a paid offer reads as paid, a payable offer still payable after PayNow as time served; each account
//! keeps its newest KEEP; My file's server build lists the asking account's rows only, newest first, and the
//! wire carries them to the card.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_BookingRecords : LF_Test
{
	protected static const string UID_PAY = "test-bookrec-pay";
	protected static const string UID_CELL = "test-bookrec-cell";
	protected static const string UID_CAP = "test-bookrec-cap";
	protected static const string UID_OTHER = "test-bookrec-other";
	protected static const string RECKLESS = "#LF-Ticket_OffenceRecklessDriving";
	protected static const string BRANDISHING = "#LF-Ticket_OffenceBrandishing";
	protected static const int DAY = 740000;

	override string GetName()
	{
		return "law/booking-records";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_BookingRecordRow> before = LF_BookingRecords.ExportAll();
		LF_BookingRecords.ApplyAll(null);
		LF_ConvictionRecord.SetTodayForTest(DAY);

		RunPay(ctx);
		RunCell(ctx);
		RunCap(ctx);
		RunOwnOnly(ctx);

		LF_ConvictionRecord.SetTodayForTest(-1);
		LF_BookingRecords.ApplyAll(before);
	}

	//------------------------------------------------------------------------------------------------
	//! Pay now: the offer was spent (LF_ChargePayment clears m_bPayable), so it settles as paid.
	protected void RunPay(LF_TestContext ctx)
	{
		LF_BookingOffer offer = MakeOffer(UID_PAY, 400, 0);
		offer.m_aCodes.Insert(RECKLESS);
		offer.m_bPayable = false;
		// RedProof(): no record is written, so every field assertion fails (`--red`).
		if (!RedProof())
			LF_BookingRecords.Note(offer, LF_BookingDesk.SettledBy(offer), 400);

		ctx.Equal(1, LF_BookingRecords.CountFor(UID_PAY), "pay now writes one record");
		LF_BookingRecordRow row = Newest(UID_PAY);
		ctx.NotNull(row, "pay now: the record is there");
		if (!row)
			return;

		ctx.Equal(DAY, row.m_iDay, "pay now: the in-game day");
		ctx.EqualStr(RECKLESS, row.m_sCodes, "pay now: the sheet's code");
		ctx.Equal(400, row.m_iFine, "pay now: the fine as offered");
		ctx.Equal(LF_EBookingSettled.PAID, row.m_iSettled, "pay now: settled as paid");
		ctx.Equal(0, row.m_iServedSeconds, "pay now: no cell time");
		array<string> expected = {};
		LF_BookingRecords.SeizedBy(offer.m_aCodes, expected);
		ctx.EqualStr(LF_BookingRecordRow.Join(expected), row.m_sLicences, "pay now: the licences the sheet's rule takes");
		if (LF_Systems.IsEnabled(LF_LicenseGate.SYSTEM))
			ctx.EqualStr("DRIVING_LICENCE", row.m_sLicences, "pay now: reckless driving takes the driving licence");
	}

	//------------------------------------------------------------------------------------------------
	//! The cell: "take the time" and a short PayNow (still payable) both settle as time served.
	protected void RunCell(LF_TestContext ctx)
	{
		LF_BookingOffer offer = MakeOffer(UID_CELL, 250, 420);
		offer.m_aCodes.Insert(BRANDISHING);
		offer.m_aCodes.Insert(RECKLESS);
		offer.m_bPayable = true;
		ctx.Equal(LF_EBookingSettled.TIME_SERVED, LF_BookingDesk.SettledBy(offer), "a payable offer after PayNow went to the cell short");
		if (!RedProof())
			LF_BookingRecords.Note(offer, LF_EBookingSettled.TIME_SERVED, 250);

		ctx.Equal(1, LF_BookingRecords.CountFor(UID_CELL), "the cell writes one record");
		LF_BookingRecordRow row = Newest(UID_CELL);
		ctx.NotNull(row, "cell: the record is there");
		if (!row)
			return;

		array<string> codes = {};
		row.Codes(codes);
		ctx.Equal(2, codes.Count(), "cell: both sheet codes");
		ctx.True(codes.Contains(BRANDISHING) && codes.Contains(RECKLESS), "cell: the sheet's codes as signed");
		ctx.Equal(LF_EBookingSettled.TIME_SERVED, row.m_iSettled, "cell: settled as time served");
		ctx.Equal(420, row.m_iServedSeconds, "cell: the time the offer served");
		ctx.Equal(DAY, row.m_iDay, "cell: the in-game day");
		ctx.EqualStr(string.Empty, row.m_sOfficer, "cell: an officer with no account leaves the name empty, never an id");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunCap(LF_TestContext ctx)
	{
		int total = LF_BookingRecords.KEEP + 3;
		for (int i = 0; i < total; i++)
		{
			LF_ConvictionRecord.SetTodayForTest(DAY + i);
			LF_BookingOffer offer = MakeOffer(UID_CAP, 100 + i, 60);
			offer.m_aCodes.Insert(RECKLESS);
			if (!RedProof())
				LF_BookingRecords.Note(offer, LF_EBookingSettled.TIME_SERVED, 100 + i);
		}

		LF_ConvictionRecord.SetTodayForTest(DAY);
		ctx.Equal(LF_BookingRecords.KEEP, LF_BookingRecords.CountFor(UID_CAP), "an account keeps its newest KEEP records");
		array<ref LF_BookingRecordRow> rows = {};
		LF_BookingRecords.CollectFor(UID_CAP, rows);
		if (rows.Count() != LF_BookingRecords.KEEP)
			return;

		ctx.Equal(100 + total - 1, rows[0].m_iFine, "the newest record comes first");
		ctx.Equal(100 + total - LF_BookingRecords.KEEP, rows[LF_BookingRecords.KEEP - 1].m_iFine, "the oldest records went");
		ctx.Equal(1, LF_BookingRecords.CountFor(UID_PAY), "the cap trims only its own account");

		// A save holding more than KEEP for one account loads its newest KEEP.
		array<ref LF_BookingRecordRow> saved = LF_BookingRecords.ExportAll();
		saved.Insert(LF_BookingRecordRow.Create(UID_CAP, DAY + 99, null, 999, LF_EBookingSettled.PAID, 0, null, ""));
		saved.Insert(LF_BookingRecordRow.Create("", DAY, null, 1, LF_EBookingSettled.PAID, 0, null, ""));
		saved.Insert(LF_BookingRecordRow.Create(UID_OTHER, DAY, null, 1, 0, 0, null, ""));
		LF_BookingRecords.ApplyAll(saved);
		ctx.Equal(LF_BookingRecords.KEEP, LF_BookingRecords.CountFor(UID_CAP), "a load keeps the cap");
		ctx.Equal(999, Newest(UID_CAP).m_iFine, "a load keeps the newest");
		ctx.Equal(0, LF_BookingRecords.CountFor(UID_OTHER), "a row with no settlement is skipped");
	}

	//------------------------------------------------------------------------------------------------
	//! My file's server build lists the asking account's records only, and the wire carries them.
	protected void RunOwnOnly(LF_TestContext ctx)
	{
		LF_BookingOffer offer = MakeOffer(UID_OTHER, 700, 0);
		offer.m_aCodes.Insert(BRANDISHING);
		LF_BookingRecords.Note(offer, LF_EBookingSettled.PAID, 700);

		LF_ConvictionRecord.SetTodayForTest(DAY + 2);
		LF_PlayerMenuFileData data = LF_PlayerMenuFileTab.Gather(UID_PAY, DAY + 2);
		ctx.Equal(1, data.m_aRecords.Count(), "My file lists the asker's one record");
		ctx.Equal(1, data.m_iBookings, "a fine paid at the desk counts as a booking on record");
		ctx.Equal(2, data.m_iLastBookingDaysAgo, "the paid booking is the latest");
		foreach (LF_BookingRecordRow row : data.m_aRecords)
			ctx.True(row.m_iFine != 700, "another account's record never shows");

		LF_PlayerMenuFileData read = LF_PlayerMenuFileData.Decode(data.Encode());
		ctx.NotNull(read, "the file wire decodes");
		if (!read || read.m_aRecords.Count() != 1)
		{
			ctx.Fail("the wire carries the one record");
			return;
		}

		LF_BookingRecordRow wired = read.m_aRecords[0];
		ctx.Equal(2, wired.m_iDay, "the wire carries days ago");
		ctx.EqualStr(RECKLESS, wired.m_sCodes, "the wire carries the codes");
		ctx.Equal(400, wired.m_iFine, "the wire carries the fine");
		ctx.Equal(LF_EBookingSettled.PAID, wired.m_iSettled, "the wire carries the settlement");

		LF_BookingRecordRow named = LF_BookingRecordRow.Create(UID_PAY, DAY, null, 0, LF_EBookingSettled.TIME_SERVED, 300, null, "Ann Officer");
		ctx.True(LF_PlayerMenuFileTab.RecordNote(named).Contains("Ann Officer"), "the card names the booking officer");
		ctx.True(LF_PlayerMenuFileTab.SettledText(named).Contains("5"), "time served reads in minutes");

		array<ref LF_UIPart> parts = {};
		LF_PlayerMenuFileTab.Build(parts, read);
		bool listed;
		foreach (LF_UIPart part : parts)
		{
			if (part && part.GetName() == LF_PlayerMenuFileTab.ROW_RECORD + "0")
				listed = true;
		}

		ctx.True(listed, "the Bookings card lists the past booking");
		LF_ConvictionRecord.SetTodayForTest(DAY);
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_BookingRecordRow Newest(string uid)
	{
		array<ref LF_BookingRecordRow> rows = {};
		LF_BookingRecords.CollectFor(uid, rows);
		if (rows.IsEmpty())
			return null;

		return rows[0];
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_BookingOffer MakeOffer(string uid, int fine, int serveSeconds)
	{
		LF_BookingOffer offer = new LF_BookingOffer();
		offer.m_sSuspectUid = uid;
		offer.m_sOfficerUid = "test-bookrec-officer";
		offer.m_iFine = fine;
		offer.m_iServeSeconds = serveSeconds;
		return offer;
	}
}
