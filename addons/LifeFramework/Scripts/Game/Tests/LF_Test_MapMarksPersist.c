// red-proof: 2026-11-17, cli test --no-build --red LF_Test_MapMarksPersist: RED PROOF OK (with the payload read skipped, "re-applying the rows changes nothing (expected 3, got 0)")
// tier: PERSISTENCE
//! map/marks-persist (#2236): the "mapmarks" payload round trips a character's personal marks, the
//! 21st mark is refused (D-M11), a networked marker add is refused (D-M7), and a re-applied load is
//! idempotent. Across a restart (`tools\cli wt e2e`): the save boot gives E2E_CHARACTER two marks;
//! the reload boot finds both with their icon and note.
class LF_Test_MapMarksPersist : LF_Test
{
	protected static const string CHARACTER = "mapmarks-persist-character";
	protected static const string OTHER = "mapmarks-persist-other";
	protected static const string E2E_CHARACTER = "mapmarks-e2e-character";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "map/marks-persist";
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_MapMarkRow Mark(int x, int icon, string text)
	{
		return LF_MapMarkRow.Make("", SCR_EMapMarkerType.PLACED_CUSTOM, x, 2000 + x, icon, 1, 45, -1, 0, text);
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
		// The reload boot: the marks the save boot placed came back from the real save.
		ctx.Equal(2, LF_MapMarks.CountOf(E2E_CHARACTER), "both marks are back after the restart");
		LF_MapMarkRow e2eMark = LF_MapMarks.MarkOf(E2E_CHARACTER, 1);
		ctx.NotNull(e2eMark, "the second mark is back");
		if (e2eMark)
		{
			ctx.Equal(4, e2eMark.m_iIcon, "the second mark keeps its pencil icon");
			ctx.EqualStr("e2e-note", e2eMark.m_sText, "the second mark keeps its note");
		}
		return;
#endif

		ctx.True(LF_PersistenceComponentSerializer.PayloadSinceVersion("mapmarks") > LF_PersistenceComponentSerializer.PayloadSinceVersion("mapknown"), "mapmarks is registered after mapknown");

		array<ref LF_MapMarkRow> before = LF_MapMarks.ExportAll();
		LF_MapMarks.ApplyAll(null);

		// Three marks round trip.
		ctx.True(LF_MapMarks.Add(CHARACTER, Mark(100, 0, "camp")), "the first mark is kept");
		ctx.True(LF_MapMarks.Add(CHARACTER, Mark(200, 2, "")), "the second mark is kept");
		ctx.True(LF_MapMarks.Add(CHARACTER, Mark(300, 4, "meet here")), "the third mark is kept");
		ctx.True(LF_MapMarks.Add(OTHER, Mark(900, 1, "theirs")), "another character's mark is kept");

		JsonSaveContext saveContext = new JsonSaveContext(false);
		LF_PersistenceComponentSerializer.WritePersistencePayload(persistence, saveContext, "mapmarks");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains("meet here"), "the save carries the third mark's note");

		// A fresh session, then the payload read back.
		LF_MapMarks.ApplyAll(null);
		ctx.Equal(0, LF_MapMarks.CountOf(CHARACTER), "the restarted session starts with no marks");

		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the save parses");
		if (!RedProof())
			LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, loadContext, "mapmarks");

		ctx.Equal(3, LF_MapMarks.CountOf(CHARACTER), "three marks after the load");
		ctx.Equal(1, LF_MapMarks.CountOf(OTHER), "the other character keeps its own mark");
		LF_MapMarkRow third = LF_MapMarks.MarkOf(CHARACTER, 2);
		ctx.NotNull(third, "the third mark is back");
		if (third)
		{
			ctx.Equal(300, third.m_iPosX, "the third mark keeps its x");
			ctx.Equal(2300, third.m_iPosY, "the third mark keeps its y");
			ctx.Equal(4, third.m_iIcon, "the third mark keeps its icon");
			ctx.Equal(45, third.m_iRotation, "the third mark keeps its rotation");
			ctx.EqualStr("meet here", third.m_sText, "the third mark keeps its note");
		}

		// A reload of the same rows is idempotent.
		LF_MapMarks.ApplyAll(LF_MapMarks.ExportAll());
		LF_MapMarks.ApplyAll(LF_MapMarks.ExportAll());
		ctx.Equal(3, LF_MapMarks.CountOf(CHARACTER), "re-applying the rows changes nothing");
		ctx.Equal(1, LF_MapMarks.CountOf(OTHER), "re-applying keeps the other character at one");

		// The 21st mark is refused, by Add and by a client's upload.
		for (int i = LF_MapMarks.CountOf(CHARACTER); i < LF_MapMarks.MAX_PER_CHARACTER; i++)
		{
			LF_MapMarks.Add(CHARACTER, Mark(1000 + i, 0, ""));
		}
		ctx.Equal(20, LF_MapMarks.CountOf(CHARACTER), "twenty marks fit");
		ctx.False(LF_MapMarks.Add(CHARACTER, Mark(5000, 0, "one too many")), "the 21st mark is refused");
		ctx.Equal(20, LF_MapMarks.CountOf(CHARACTER), "still twenty after the refusal");

		array<ref LF_MapMarkRow> upload = new array<ref LF_MapMarkRow>();
		for (int u = 0; u < 21; u++)
		{
			upload.Insert(Mark(u, 0, ""));
		}
		ctx.Equal(20, LF_MapMarks.SetCharacter(CHARACTER, upload), "an upload of 21 keeps the first 20");

		// A networked marker add is refused: the server's sync component never adds it, and the
		// manager places nothing networked.
		SCR_MapMarkerManagerComponent manager = SCR_MapMarkerManagerComponent.GetInstance();
		ctx.NotNull(manager, "the game mode carries the marker manager");
		if (manager)
		{
			int refusedBefore = LF_MapMarks.NetworkedRefusedCount();
			int countBefore = manager.GetStaticMarkers().Count();
			SCR_MapMarkerBase networked = new SCR_MapMarkerBase();
			networked.SetType(SCR_EMapMarkerType.PLACED_CUSTOM);
			networked.SetWorldPos(10, 10);
			manager.InsertStaticMarker(networked, false);
			ctx.Equal(refusedBefore + 1, LF_MapMarks.NetworkedRefusedCount(), "the networked add is refused");
			ctx.Equal(-1, networked.GetMarkerID(), "the marker never got a network id");
			foreach (SCR_MapMarkerBase marker : manager.GetStaticMarkers())
			{
				if (marker && marker.GetMarkerID() != -1 && marker.GetMarkerOwnerID() != -1)
					ctx.Fail("a player marker is networked");
			}

			// A host with a local player gets it as a local mark instead; take it away again.
			if (manager.GetStaticMarkers().Count() > countBefore)
				manager.RemoveStaticMarker(networked);
		}

		LF_MapMarks.ApplyAll(before);
#ifdef LF_E2E_HOST_STOP
		// The save boot: the shutdown save carries two marks of the e2e character.
		LF_MapMarks.Add(E2E_CHARACTER, Mark(10, 0, "e2e-first"));
		LF_MapMarks.Add(E2E_CHARACTER, Mark(20, 4, "e2e-note"));
		ctx.Equal(2, LF_MapMarks.CountOf(E2E_CHARACTER), "the save boot placed the e2e marks");
#endif
	}
}
