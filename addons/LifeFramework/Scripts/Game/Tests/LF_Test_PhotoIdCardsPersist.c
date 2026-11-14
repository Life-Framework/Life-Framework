// red-proof: Observed 2026-11-13 with `tools\cli test --no-build --only LF_Test_PhotoIdCardsPersist`:
// the "idcards" load skipped LF_PhotoIdCard.ApplyAll; got `[LFTEST] FAIL persist/photo-id-cards`,
// `- the three cards are back (load 1) (expected 3, got 0)`,
// `- the next card is numbered after the loaded ones (expected 'EV 000004', got 'EV 000001')`. Restored; green.
// tier: PERSISTENCE
//! The identity card registry (#902) rides the real save path: the "idcards" payload, save version
//! 30. After the registry is wiped the rows come back from the save, twice the same (idempotent),
//! with the replaced card still void and the photo and the date of issue intact; numbering goes on
//! after the highest number loaded. A version 29 save has no rows and leaves the registry alone.
//! The test puts back what it found.
class LF_Test_PhotoIdCardsPersist : LF_Test
{
	protected static const string HOLDER = "test-idcards-holder";
	protected static const string OTHER = "test-idcards-other";
	protected static const string LOOK = "9B5BB216CC7FF18E|1=9F546CCA2582D16F";

	override string GetName()
	{
		return "persist/photo-id-cards";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		ctx.NotNull(gameMode, "the DebugWorld game mode resolves");
		if (!gameMode)
			return;

		LF_PersistenceComponent persistence = LF_PersistenceComponent.Cast(gameMode.FindComponent(LF_PersistenceComponent));
		ctx.NotNull(persistence, "the game mode carries LF_PersistenceComponent");
		if (!persistence)
			return;

		LF_TestLegacySnapshot kept = LF_TestLegacySnapshot.Take();
		LF_PhotoIdCard.ResetStatics();
		LF_PhotoIdRow first = LF_PhotoIdCard.Issue(HOLDER, "Ada", "Novak", 31, "01/06/1989", "");
		LF_PhotoIdCard.Issue(OTHER, "Bob", "Kral", 45, "02/06/1989", "");
		LF_PhotoIdRow replacement = LF_PhotoIdCard.Issue(HOLDER, "Ada", "Novak", 31, "09/06/1989", LOOK);

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(replacement.m_sNumber) && saved.Contains("09/06/1989"), "the save carries the card rows");

		for (int pass = 1; pass <= 2; pass++)
		{
			LF_PhotoIdCard.ResetStatics();
			JsonLoadContext loadContext = new JsonLoadContext(false);
			ctx.True(loadContext.LoadFromString(saved), "the save parses");
			int version;
			loadContext.ReadValue("version", version);
			ctx.Equal(32, version, "the save is written at version 32 (idcards is payload 30)");
			ctx.True(LF_PersistenceComponentSerializer.DeserializeStore(persistence, loadContext), "the save loads");
			string tag = string.Format(" (load %1)", pass);
			ctx.Equal(3, LF_PhotoIdCard.Count(), "the three cards are back" + tag);
			LF_PhotoIdRow current = LF_PhotoIdCard.Current(HOLDER);
			ctx.True(current != null && current.m_sNumber == replacement.m_sNumber, "the replacement is the holder's card" + tag);
			ctx.False(LF_PhotoIdCard.IsValid(first.m_sNumber), "the replaced card is still void" + tag);
			if (current)
			{
				ctx.EqualStr(LOOK, current.m_sLook, "the photo survives" + tag);
				ctx.EqualStr("09/06/1989", current.m_sIssued, "the date of issue survives" + tag);
				ctx.Equal(31, current.m_iAge, "the printed age survives" + tag);
			}
		}

		ctx.EqualStr(LF_PhotoIdCard.FormatNumber(4), LF_PhotoIdCard.Issue(OTHER, "Bob", "Kral", 45, "10/06/1989", "").m_sNumber, "the next card is numbered after the loaded ones");

		// A version 29 save has no "idcards": the registry is left as it is.
		JsonSaveContext oldContext = new JsonSaveContext(false);
		LF_PersistenceComponentSerializer.SerializeStoreAsVersion(persistence, oldContext, 29);
		string oldSave = oldContext.SaveToString();
		ctx.False(oldSave.Contains("09/06/1989"), "a version 29 save carries no card rows");
		JsonLoadContext oldLoad = new JsonLoadContext(false);
		ctx.True(oldLoad.LoadFromString(oldSave) && LF_PersistenceComponentSerializer.DeserializeStore(persistence, oldLoad), "the version 29 save loads");
		ctx.Equal(4, LF_PhotoIdCard.Count(), "a version 29 load leaves the registry alone");

		kept.Restore();
	}
}
