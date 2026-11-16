// red-proof: Observed 2026-11-16 with `tools/cli test --no-build --red LF_Test_PersistLastPosition` (RED-OK spawn/last-position): with the payload read skipped, failed "the last position is back (load 1)"; green on the real code.
// tier: PERSISTENCE
//! #1777: a character's last position (position, heading, time taken) survives the "lastpos" payload
//! and a restart. A save from before the payload (version 34, and the version just before "lastpos")
//! loads with no rows; loading twice changes nothing.
//! Across a restart (`tools\cli wt e2e`): the save boot records E2E_ID, the shutdown save carries it,
//! and the reload boot finds the same position, heading and time.
class LF_Test_PersistLastPosition : LF_Test
{
	protected static const string CHAR_ID = "lastpos-persist-character";
	protected static const string OTHER_ID = "lastpos-persist-other";
	protected static const string E2E_ID = "lastpos-e2e-character";
	protected static const int TAKEN = 1700000000;
	protected static const int E2E_TAKEN = 1700000500;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "spawn/last-position";
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
		// The reload boot: the row the save boot left came back from the real save.
		LF_LastPositionRow restarted = LF_LastPosition.Get(E2E_ID);
		ctx.NotNull(restarted, "the last position is kept after the restart");
		if (restarted)
		{
			ctx.True(vector.Distance(restarted.m_vPosition, "1234.5 56.25 4321.75") < 0.01, "the position is the one saved before the restart");
			ctx.True(Math.AbsFloat(restarted.m_fYaw - 135.5) < 0.01, "the heading is the one saved before the restart");
			ctx.Equal(E2E_TAKEN, restarted.m_iTakenUnix, "the time is the one saved before the restart");
		}
		return;
#endif

		array<ref LF_LastPositionRow> before = LF_LastPosition.ExportAll();

		RoundTrip(ctx, persistence);

		LF_TestLegacySnapshot kept = LF_TestLegacySnapshot.Take();
		CheckLegacy(ctx, persistence, 34);
		CheckLegacy(ctx, persistence, LF_PersistenceComponentSerializer.PayloadSinceVersion("lastpos") - 1);
		kept.Restore();

		LF_LastPosition.ApplyAll(before);
#ifdef LF_E2E_HOST_STOP
		// The save boot: one row carried by the shutdown save.
		ctx.True(LF_LastPosition.Record(E2E_ID, "1234.5 56.25 4321.75", 135.5, E2E_TAKEN), "the save boot records the e2e character");
#endif
	}

	//------------------------------------------------------------------------------------------------
	protected void RoundTrip(LF_TestContext ctx, LF_PersistenceComponent persistence)
	{
		LF_LastPosition.ApplyAll(null);
		ctx.True(LF_LastPosition.Record(CHAR_ID, "100.5 20.25 300.75", 270.5, TAKEN), "the position is recorded");
		ctx.True(LF_LastPosition.Record(OTHER_ID, "1 2 3", 10, TAKEN - 60), "a second character is recorded");
		ctx.False(LF_LastPosition.Record("", "1 2 3", 10, TAKEN), "a character with no id is not recorded");

		JsonSaveContext saveContext = new JsonSaveContext(false);
		LF_PersistenceComponentSerializer.WritePersistencePayload(persistence, saveContext, "lastpos");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(CHAR_ID), "the save carries the character's last position");

		// A fresh session, then the payload read back.
		LF_LastPosition.ApplyAll(null);
		ctx.Equal(0, LF_LastPosition.Count(), "the restarted session starts with no last positions");

		for (int pass = 1; pass <= 2; pass++)
		{
			string tag = string.Format(" (load %1)", pass);
			JsonLoadContext loadContext = new JsonLoadContext(false);
			ctx.True(loadContext.LoadFromString(saved), "the save parses" + tag);
			if (!RedProof())
				LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, loadContext, "lastpos");

			LF_LastPositionRow loaded = LF_LastPosition.Get(CHAR_ID);
			ctx.NotNull(loaded, "the last position is back" + tag);
			if (!loaded)
				return;

			ctx.True(vector.Distance(loaded.m_vPosition, "100.5 20.25 300.75") < 0.01, "the position round-trips" + tag);
			ctx.True(Math.AbsFloat(loaded.m_fYaw - 270.5) < 0.01, "the heading round-trips" + tag);
			ctx.Equal(TAKEN, loaded.m_iTakenUnix, "the time taken round-trips" + tag);
			ctx.Equal(2, LF_LastPosition.Count(), "both characters and no extra rows" + tag);
		}

		// A newer record replaces the older one; an older duplicate on load does not win.
		array<ref LF_LastPositionRow> rows = LF_LastPosition.ExportAll();
		rows.Insert(LF_LastPositionRow.Create(CHAR_ID, "9 9 9", 0, TAKEN - 1));
		LF_LastPosition.ApplyAll(rows);
		LF_LastPositionRow newest = LF_LastPosition.Get(CHAR_ID);
		ctx.True(newest && newest.m_iTakenUnix == TAKEN, "an older duplicate row does not replace the newer one");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckLegacy(LF_TestContext ctx, LF_PersistenceComponent persistence, int version)
	{
		// A live row exists while saving: the old format cannot carry it.
		LF_LastPosition.ApplyAll(null);
		LF_LastPosition.Record(CHAR_ID, "5 6 7", 45, TAKEN);

		JsonSaveContext saveContext = new JsonSaveContext(false);
		LF_PersistenceComponentSerializer.SerializeStoreAsVersion(persistence, saveContext, version);
		string saved = saveContext.SaveToString();
		ctx.False(saved.Contains(CHAR_ID), string.Format("the version %1 save has no last-position rows", version));

		// A fresh boot loads it, twice.
		LF_LastPosition.ApplyAll(null);
		for (int pass = 1; pass <= 2; pass++)
		{
			string tag = string.Format(" (version %1 load %2)", version, pass);
			JsonLoadContext loadContext = new JsonLoadContext(false);
			ctx.True(loadContext.LoadFromString(saved), "the old save parses" + tag);
			ctx.True(LF_PersistenceComponentSerializer.DeserializeStore(persistence, loadContext), "the old save loads" + tag);
			ctx.Equal(0, LF_LastPosition.Count(), "no last position is kept" + tag);
		}

	}
}
