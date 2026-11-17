// red-proof: 2026-11-17, cli test --no-build --red LF_Test_MapKnown,LF_Test_MapKnownPersist: RED PROOF OK (with the payload read skipped, "no extra rows (expected 3, got 0)"); the first run also failed the same way on the real code while the mapknown read branch was misnested, green after the fix.
// tier: PERSISTENCE
//! map/known-persist (#2241): the "mapknown" payload round trips each character's heard-of and
//! visited places, keeps the row of a place that is not live (a dark chapter's place), and a
//! re-applied load is idempotent. Across a restart (`tools\cli wt e2e`): the save boot records
//! E2E_CHARACTER visited E2E_PLACE and heard of E2E_DARK; the reload boot finds both.
class LF_Test_MapKnownPersist : LF_Test
{
	protected static const string CHARACTER = "mapknown-persist-character";
	protected static const string OTHER = "mapknown-persist-other";
	protected static const string DARK = "mapknown-persist-dark-place";
	protected static const string E2E_CHARACTER = "mapknown-e2e-character";
	protected static const string E2E_PLACE = "mapknown-e2e-place";
	protected static const string E2E_DARK = "mapknown-e2e-dark";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "map/known-persist";
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
		// The reload boot: what the save boot recorded came back from the real save.
		ctx.True(LF_MapKnown.KnownOf(E2E_CHARACTER, E2E_PLACE) == LF_EMapPoiKnown.VISITED, "the visited place is still visited after the restart");
		ctx.True(LF_MapKnown.KnownOf(E2E_CHARACTER, E2E_DARK) == LF_EMapPoiKnown.HEARD, "the dark place is still heard of after the restart");
		return;
#endif

		ctx.True(LF_PersistenceComponentSerializer.PayloadSinceVersion("mapknown") > 0, "mapknown is a registered payload");

		array<ref LF_MapKnownRow> before = LF_MapKnown.ExportAll();
		LF_MapKnown.ApplyAll(null);

		LF_MapKnown.HearCharacter(CHARACTER, "mkp-bank");
		LF_MapKnown.VisitAsk(990242, CHARACTER, "mkp-bank", true, 3, 0, 5000);
		LF_MapKnown.HearCharacter(CHARACTER, "mkp-shop");
		LF_MapKnown.HearCharacter(CHARACTER, DARK);
		LF_MapKnown.HearCharacter(OTHER, "mkp-shop");

		JsonSaveContext saveContext = new JsonSaveContext(false);
		LF_PersistenceComponentSerializer.WritePersistencePayload(persistence, saveContext, "mapknown");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(DARK), "the save carries the dark place's row");

		// A fresh session, then the payload read back.
		LF_MapKnown.ApplyAll(null);
		ctx.Equal(0, LF_MapKnown.CountOf(CHARACTER), "the restarted session starts with a blank map");

		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the save parses");
		if (!RedProof())
			LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, loadContext, "mapknown");

		ctx.True(LF_MapKnown.KnownOf(CHARACTER, "mkp-bank") == LF_EMapPoiKnown.VISITED, "the visited place is visited after the load");
		ctx.True(LF_MapKnown.KnownOf(CHARACTER, "mkp-shop") == LF_EMapPoiKnown.HEARD, "the heard-of place is heard of after the load");
		ctx.True(LF_MapKnown.KnownOf(CHARACTER, DARK) == LF_EMapPoiKnown.HEARD, "the dark place's row survives");
		ctx.True(LF_MapKnown.KnownOf(OTHER, "mkp-shop") == LF_EMapPoiKnown.HEARD, "the other character's row is its own");
		ctx.True(LF_MapKnown.KnownOf(OTHER, "mkp-bank") == LF_EMapPoiKnown.NONE, "knowledge is not shared between characters");
		ctx.Equal(3, LF_MapKnown.CountOf(CHARACTER), "no extra rows");

		// Loading the same rows twice is idempotent.
		LF_MapKnown.ApplyAll(LF_MapKnown.ExportAll());
		ctx.Equal(3, LF_MapKnown.CountOf(CHARACTER), "re-applying the rows changes nothing");

		// A broken row is skipped, the rest load.
		array<ref LF_MapKnownRow> rows = LF_MapKnown.ExportAll();
		rows.Insert(LF_MapKnownRow.Make("", "mkp-x", 1));
		rows.Insert(LF_MapKnownRow.Make(CHARACTER, "mkp-y", 7));
		LF_MapKnown.ApplyAll(rows);
		ctx.Equal(3, LF_MapKnown.CountOf(CHARACTER), "rows with no key or an unknown state are skipped");

		LF_MapKnown.ApplyAll(before);
#ifdef LF_E2E_HOST_STOP
		// The save boot: the shutdown save carries one visited and one dark heard-of place.
		LF_MapKnown.VisitAsk(990243, E2E_CHARACTER, E2E_PLACE, true, 0, 0, 5000);
		LF_MapKnown.HearCharacter(E2E_CHARACTER, E2E_DARK);
		ctx.True(LF_MapKnown.KnownOf(E2E_CHARACTER, E2E_PLACE) == LF_EMapPoiKnown.VISITED, "the save boot recorded the e2e visit");
#endif
	}
}
