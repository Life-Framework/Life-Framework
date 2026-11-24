// red-proof: RED-OK law/booking-records-persist, observed 2026-11-24 with `node tools/cli.mjs test --no-build --red LF_Test_BookingRecords,LF_Test_BookingRecordsPersist`: with the section read skipped, `the booking is back after the load (expected 1, got 0)`. Green with `--only`.
//! The booking records section (#4711 follow-up): a settled booking is written in the "booking_records"
//! save section and read back with every field, loading twice ends in the same state, and a save from
//! before the section loads with no records. Restart: the e2e account's booking survives `wt e2e`.
[LF_TestInfo(LF_TestTier.PERSISTENCE)]
class LF_Test_BookingRecordsPersist : LF_Test
{
	static const string UID = "test-bookrec-persist";
	static const string E2E_UID = "e2e-booking-record";
	static const string CODES = "#LF-Ticket_OffenceRecklessDriving,#LF-Ticket_OffenceBrandishing";
	static const int DAY = 740123;

	override string GetName()
	{
		return "law/booking-records-persist";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		LF_PersistenceComponent persistence;
		if (gameMode)
			persistence = LF_PersistenceComponent.Cast(gameMode.FindComponent(LF_PersistenceComponent));

		ctx.NotNull(persistence, "the game mode carries LF_PersistenceComponent");
		if (!persistence)
			return;

#ifdef LF_E2E_LOAD_BOOT
		if (LF_E2E.RequiresSection(LF_BookingRecordsSavePayload.NAME))
		{
			array<ref LF_BookingRecordRow> kept = {};
			LF_BookingRecords.CollectFor(E2E_UID, kept);
			ctx.Equal(1, kept.Count(), "the e2e account's booking survives the restart");
			if (kept.Count() == 1)
				CheckRow(ctx, kept[0], "after the restart");
		}

		return;
#endif

		ctx.NotNull(LF_SaveSections.Get(LF_BookingRecordsSavePayload.NAME), "booking_records is a registered save section");
		array<ref LF_BookingRecordRow> before = LF_BookingRecords.ExportAll();

		array<ref LF_BookingRecordRow> seed = {};
		seed.Insert(MakeRow(UID));
		LF_BookingRecords.ApplyAll(seed);

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(LF_SaveSections.OBJECT_PREFIX + LF_BookingRecordsSavePayload.NAME), "the save carries the booking_records section");
		ctx.Equal(0, LF_PersistenceComponentSerializer.PayloadSinceVersion(LF_BookingRecordsSavePayload.NAME), "booking_records is not a legacy positional payload");

		// Restart: the session holds no record.
		LF_BookingRecords.ApplyAll(null);
		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the save parses");
		// RedProof(): the load is skipped, so every read-back assertion fails (`--red`).
		if (!RedProof())
			ReadSection(loadContext, ctx);

		array<ref LF_BookingRecordRow> rows = {};
		LF_BookingRecords.CollectFor(UID, rows);
		ctx.Equal(1, rows.Count(), "the booking is back after the load");
		if (rows.Count() == 1)
			CheckRow(ctx, rows[0], "after the load");

		JsonLoadContext again = new JsonLoadContext(false);
		again.LoadFromString(saved);
		ReadSection(again, ctx);
		ctx.Equal(1, LF_BookingRecords.CountFor(UID), "loading twice keeps one record");

		// A save from before the section: no records.
		JsonSaveContext oldContext = new JsonSaveContext(false);
		array<string> omit = {LF_BookingRecordsSavePayload.NAME};
		LF_PersistenceComponentSerializer.SerializeStoreOmitting(persistence, oldContext, omit);
		ctx.False(oldContext.SaveToString().Contains(LF_SaveSections.OBJECT_PREFIX + LF_BookingRecordsSavePayload.NAME), "the older save has no booking_records section");
		// A boot that loads it starts empty; OnAbsent adds nothing.
		LF_BookingRecords.ApplyAll(null);
		LF_SaveSections.Get(LF_BookingRecordsSavePayload.NAME).OnAbsent();
		ctx.Equal(0, LF_BookingRecords.CountFor(UID), "a save from before the section holds no booking record");

		// Every non-reload run (the e2e save boot among them) leaves the e2e booking in the store.
		LF_BookingRecords.ApplyAll(before);
		array<ref LF_BookingRecordRow> withE2e = LF_BookingRecords.ExportAll();
		withE2e.Insert(MakeRow(E2E_UID));
		LF_BookingRecords.ApplyAll(withE2e);
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_BookingRecordRow MakeRow(string uid)
	{
		array<string> codes = {};
		LF_BookingRecordRow.Split(CODES, codes);
		array<string> licences = {"DRIVING_LICENCE"};
		return LF_BookingRecordRow.Create(uid, DAY, codes, 450, LF_EBookingSettled.TIME_SERVED, 480, licences, "Ann Officer");
	}

	//------------------------------------------------------------------------------------------------
	protected static void CheckRow(LF_TestContext ctx, notnull LF_BookingRecordRow row, string when)
	{
		ctx.Equal(DAY, row.m_iDay, when + ": the in-game day");
		ctx.EqualStr(CODES, row.m_sCodes, when + ": the charge codes");
		ctx.Equal(450, row.m_iFine, when + ": the fine");
		ctx.Equal(LF_EBookingSettled.TIME_SERVED, row.m_iSettled, when + ": settled as time served");
		ctx.Equal(480, row.m_iServedSeconds, when + ": the time served");
		ctx.EqualStr("DRIVING_LICENCE", row.m_sLicences, when + ": the licence seized");
		ctx.EqualStr("Ann Officer", row.m_sOfficer, when + ": the booking officer's name");
	}

	//------------------------------------------------------------------------------------------------
	protected static void ReadSection(notnull JsonLoadContext context, LF_TestContext ctx)
	{
		LF_SavePayload section = LF_SaveSections.Get(LF_BookingRecordsSavePayload.NAME);
		if (!section)
			return;

		ctx.True(context.StartObject(LF_SaveSections.OBJECT_PREFIX + LF_BookingRecordsSavePayload.NAME), "the booking_records section opens");
		int savedVersion;
		context.ReadValue(LF_SaveSections.VERSION_KEY, savedVersion);
		ctx.Equal(section.Version(), savedVersion, "the section carries its own version");
		ctx.True(section.ReadSection(context, savedVersion), "the booking_records section reads");
		context.EndObject();
	}
}
