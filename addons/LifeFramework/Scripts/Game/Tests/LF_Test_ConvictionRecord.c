// red-proof: Observed 2026-11-16 with `tools/cli test --no-build --only LF_Test_ConvictionRecord` before the "convictions" payload was registered: failed "the conviction is back, no extra rows (expected 1, got 0)". Green once the payload was wired. 2026-11-16: in a full suite an earlier test resets the event bus and this one found "no subscribers" (expected 1, got 0, run on #2001 + the hook fix); the ResetStatics above is the fix, run 611/611.
// tier: PERSISTENCE
//! #1914: an arrest published on the bus stamps the account with the in-game day; IsRecent answers
//! the clean-record question for a window of days; the stamp survives the "convictions" payload.
//! Across a restart (`tools\cli wt e2e`): the save boot leaves one conviction on today's clock day
//! for E2E_UID, so the shutdown save carries it; the reload boot finds it back and still recent.
class LF_Test_ConvictionRecord : LF_Test
{
	protected static const string UID = "conviction-test-uid";
	protected static const int STAMP_DAY = 726000;
	protected static const int WINDOW = 3;
	protected static const string E2E_UID = "conviction-e2e-uid";

	override string GetName()
	{
		return "police/conviction-record";
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
		// The reload boot: the stamp the save boot left came back from the real save.
		ctx.Equal(1, LF_ConvictionRecord.Count(E2E_UID), "the save boot's conviction is back after the restart");
		ctx.True(LF_ConvictionRecord.IsRecent(E2E_UID, WINDOW), "the restored conviction is still recent on the restored clock");
		return;
#endif

		array<ref LF_ConvictionRow> before = LF_ConvictionRecord.ExportAll();
		// A test earlier in the suite resets the event bus; a subscriber kept from before would never hear the arrest.
		LF_ConvictionRecord.ResetStatics();
		LF_ConvictionRecord.ApplyAll(null);
		LF_ConvictionRecord.SetTodayForTest(STAMP_DAY);
		ctx.True(LF_ConvictionRecord.EnsureSubscribed(), "the record subscribes to LF_OnArrested");

		ctx.False(LF_ConvictionRecord.IsRecent(UID, WINDOW), "no conviction before an arrest");

		if (!RedProof())
			LF_EventBus.Publish(LF_OnArrested.Create(UID, 300, 2));

		ctx.Equal(1, LF_ConvictionRecord.Count(UID), "the arrest stamped one conviction");
		ctx.True(LF_ConvictionRecord.IsRecent(UID, WINDOW), "recent on the day of the arrest");
		LF_ConvictionRecord.SetTodayForTest(STAMP_DAY + WINDOW - 1);
		ctx.True(LF_ConvictionRecord.IsRecent(UID, WINDOW), "still recent on the last day of the window");
		LF_ConvictionRecord.SetTodayForTest(STAMP_DAY + WINDOW);
		ctx.False(LF_ConvictionRecord.IsRecent(UID, WINDOW), "not recent once the day moves past the window");
		ctx.False(LF_ConvictionRecord.IsRecent(UID, 0), "a zero window never matches");

		// Month and year ends are one day like any other.
		ctx.Equal(1, LF_ConvictionRecord.DayOrdinal(1989, 3, 1) - LF_ConvictionRecord.DayOrdinal(1989, 2, 28), "28 Feb to 1 Mar 1989 is one day");
		ctx.Equal(1, LF_ConvictionRecord.DayOrdinal(1990, 1, 1) - LF_ConvictionRecord.DayOrdinal(1989, 12, 31), "New Year's Eve to New Year's Day is one day");

		// Save round trip through the payload, then a fresh session reads it back.
		LF_ConvictionRecord.SetTodayForTest(STAMP_DAY + 1);
		JsonSaveContext saveContext = new JsonSaveContext(false);
		LF_PersistenceComponentSerializer.WritePersistencePayload(persistence, saveContext, "convictions");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(UID), "the save carries the convictions payload");

		LF_ConvictionRecord.ApplyAll(null);
		ctx.False(LF_ConvictionRecord.IsRecent(UID, WINDOW), "the restarted session starts empty");

		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the save parses");
		LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, loadContext, "convictions");
		ctx.Equal(1, LF_ConvictionRecord.Count(UID), "the conviction is back, no extra rows");
		ctx.True(LF_ConvictionRecord.IsRecent(UID, WINDOW), "the loaded stamp keeps its day");
		LF_ConvictionRecord.SetTodayForTest(STAMP_DAY + WINDOW);
		ctx.False(LF_ConvictionRecord.IsRecent(UID, WINDOW), "the loaded stamp still ages out");

		// Loading the same rows twice is idempotent.
		LF_ConvictionRecord.ApplyAll(LF_ConvictionRecord.ExportAll());
		ctx.Equal(1, LF_ConvictionRecord.Count(UID), "re-applying the record changes nothing");

		LF_ConvictionRecord.SetTodayForTest(-1);
		LF_ConvictionRecord.ApplyAll(before);
#ifdef LF_E2E_HOST_STOP
		// The save boot: one arrest on the real clock's day, carried by the shutdown save.
		LF_EventBus.Publish(LF_OnArrested.Create(E2E_UID, 300, 2));
		ctx.Equal(1, LF_ConvictionRecord.Count(E2E_UID), "the save boot stamped the e2e conviction");
#endif
	}
}
