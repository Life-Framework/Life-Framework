// red-proof: Observed 2026-11-16 with `tools/cli test --no-build --red LF_Test_RaceRecords` (RED PROOF OK): with the payload read skipped, failed "the other course loads (expected 70000, got -1)"; green on the real code.
// tier: PERSISTENCE
//! #1925: the record board keeps the best race times. A faster finish replaces a character's record
//! and a slower one does not; a finished race (LF_OnRaceFinished) records the winner; the
//! "racerecords" payload survives a save round trip and a second load; the board card lists the top
//! five of a course, fastest first.
//! Across a restart (`tools\cli wt e2e`): the save boot records E2E_UID on E2E_COURSE, the shutdown
//! save carries it, and the reload boot finds the same time.
class LF_Test_RaceRecords : LF_Test
{
	protected static const string COURSE = "test_records_loop";
	protected static const string OTHER_COURSE = "test_records_other";
	protected static const string E2E_COURSE = "test_records_e2e";
	protected static const string E2E_UID = "racerecords-e2e-uid";
	protected static const int E2E_MS = 83400;

	override string GetName()
	{
		return "racing/race-records";
	}

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
		// The reload boot: the record the save boot set came back from the real save.
		ctx.Equal(E2E_MS, LF_RaceRecords.BestOf(E2E_COURSE, E2E_UID), "the record set before the restart is there after it");
		return;
#endif

		array<ref LF_RaceRecordRow> before = LF_RaceRecords.ExportAll();
		LF_RaceRecords.ApplyAll(null);

		// A first time is the record; a slower one is not; a faster one replaces it.
		ctx.True(LF_RaceRecords.Offer(COURSE, "uid-a", "Anna Sold", 90000), "a first finish sets the record");
		ctx.False(LF_RaceRecords.Offer(COURSE, "uid-a", "Anna Sold", 95000), "a slower finish does not change it");
		ctx.Equal(90000, LF_RaceRecords.BestOf(COURSE, "uid-a"), "the record stays at the faster time");
		ctx.True(LF_RaceRecords.Offer(COURSE, "uid-a", "Anna Sold", 85000), "a faster finish replaces it");
		ctx.Equal(85000, LF_RaceRecords.BestOf(COURSE, "uid-a"), "the record is the new time");
		ctx.False(LF_RaceRecords.Offer(COURSE, "uid-a", "Anna Sold", 85000), "an equal time is not a new record");
		ctx.Equal(-1, LF_RaceRecords.BestOf(OTHER_COURSE, "uid-a"), "a record is per course");
		ctx.False(LF_RaceRecords.Offer(COURSE, "uid-a", "Anna Sold", 0), "a time of 0 is refused");

		// A finished race records its winner through the bus.
		LF_RaceRecords.EnsureSubscribed();
		LF_EventBus.Publish(LF_OnRaceFinished.Create(OTHER_COURSE, "uid-b", 300, 2, 70000));
		ctx.Equal(70000, LF_RaceRecords.BestOf(OTHER_COURSE, "uid-b"), "a finished race records the winner's time");

		// Six racers on one course: the board shows the five fastest, fastest first.
		LF_RaceRecords.Offer(COURSE, "uid-c", "Carl Marsh", 99000);
		LF_RaceRecords.Offer(COURSE, "uid-d", "Dita Novak", 80000);
		LF_RaceRecords.Offer(COURSE, "uid-e", "Emil Brook", 92000);
		LF_RaceRecords.Offer(COURSE, "uid-f", "Fran Kell", 88000);
		LF_RaceRecords.Offer(COURSE, "uid-g", "", 87000);
		array<ref LF_RaceRecordRow> top = {};
		LF_RaceRecords.Top(COURSE, LF_RaceRecords.BOARD_ROWS, top);
		ctx.Equal(5, top.Count(), "the board keeps five rows of six racers");
		if (top.Count() == 5)
		{
			ctx.EqualStr("uid-d", top[0].m_sUid, "the fastest is first");
			ctx.EqualStr("uid-e", top[4].m_sUid, "the fifth is the fifth fastest");
		}

		LF_BoardAnswer answer = LF_BoardProviders.Answer(LF_RaceBoard.KIND, 0);
		ctx.NotNull(answer, "the record board answers a read through its provider");
		if (answer)
		{
			array<ref LF_UIPart> parts = LF_BoardProviders.Paint(LF_RaceBoard.KIND, answer);
			string card = "";
			foreach (LF_UIPart part : parts)
			{
				card += part.GetText() + "\n";
			}

			ctx.True(card.Contains(LF_RaceBoard.RowText(1, "Dita Novak", 80000)), "the card's first row is the record holder: " + card);
			ctx.True(card.Contains(LF_RaceBoard.RowText(5, "Emil Brook", 92000)), "the card lists the fifth");
			ctx.False(card.Contains("Carl Marsh"), "the sixth is not on the card");
			ctx.True(card.Contains(LF_RaceBoard.RowText(1, "", 70000)), "the other course has its own rows; a nameless winner shows the stand-in");
			ctx.True(card.Contains(OTHER_COURSE), "the other course is headed by its id");
		}

		// The payload round trip: write, a fresh session, read back, read again.
		JsonSaveContext saveContext = new JsonSaveContext(false);
		LF_PersistenceComponentSerializer.WritePersistencePayload(persistence, saveContext, "racerecords");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains("uid-d"), "the save carries the records");

		LF_RaceRecords.ApplyAll(null);
		ctx.Equal(0, LF_RaceRecords.Count(), "the restarted session starts with no records");

		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the save parses");
		if (!RedProof())
			LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, loadContext, "racerecords");
		ctx.Equal(85000, LF_RaceRecords.BestOf(COURSE, "uid-a"), "the beaten record loads at its best time");
		ctx.Equal(70000, LF_RaceRecords.BestOf(OTHER_COURSE, "uid-b"), "the other course loads");
		ctx.Equal(7, LF_RaceRecords.Count(), "no extra rows");

		LF_RaceRecords.ApplyAll(LF_RaceRecords.ExportAll());
		ctx.Equal(7, LF_RaceRecords.Count(), "re-applying the rows changes nothing");

		LF_RaceRecords.ApplyAll(before);
#ifdef LF_E2E_HOST_STOP
		// The save boot: one record the shutdown save carries.
		LF_RaceRecords.Offer(E2E_COURSE, E2E_UID, "E2E Racer", E2E_MS);
		ctx.Equal(E2E_MS, LF_RaceRecords.BestOf(E2E_COURSE, E2E_UID), "the save boot set the e2e record");
#endif
	}
}
