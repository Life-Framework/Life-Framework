// red-proof: observed 2026-11-17 with `tools\cli test --no-build --only LF_Test_IdRegistryUnreadable` on the unfixed serializer (no NoteRegistryUnreadable call), FAIL id-card/registry-unreadable:
// `the new card does not reuse the number an old card carries`, `numbering resumes past the floor`, `the old card's number matches no row: it reads as unregistered`, `the old card is not valid for anyone`. Also RED-OK under --red. Restored; green.
// tier: WORLD
//! id-card/registry-unreadable (#2413): when the saved "idcards" payload cannot be read the rows are
//! gone, and numbering used to restart at 1. A new holder then got the number an old card still
//! carries, and that card printed as the new holder. Numbering now resumes past
//! LF_PhotoIdCard.UNREADABLE_FLOOR, and the old number reads as unregistered.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_IdRegistryUnreadable : LF_Test
{
	protected static const string OLD_HOLDER = "lf-test-unreadable-old";
	protected static const string NEW_HOLDER = "lf-test-unreadable-new";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "id-card/registry-unreadable";
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

		// A card printed before the restart: the card item keeps its number in the world.
		LF_PhotoIdRow old = LF_PhotoIdCard.Issue(OLD_HOLDER, "Ada", "Novak", 31, "01/06/1989", "");
		ctx.NotNull(old, "a card is printed");
		if (!old)
		{
			kept.Restore();
			return;
		}

		string oldNumber = old.m_sNumber;
		ctx.EqualStr(LF_PhotoIdCard.FormatNumber(1), oldNumber, "the first card is number 1");

		// The restart: the registry is empty and the "idcards" payload will not read.
		LF_PhotoIdCard.ResetStatics();
		if (!RedProof())
		{
			JsonSaveContext broken = new JsonSaveContext(false);
			broken.WriteValue("version", LF_TestSaveVersion.CURRENT);
			JsonLoadContext loadContext = new JsonLoadContext(false);
			ctx.True(loadContext.LoadFromString(broken.SaveToString()), "the broken save parses as JSON");
			LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, loadContext, "idcards");
			ctx.True(LF_PersistenceComponentSerializer.GetReadFailures() > 0, "the unreadable registry is counted as a read failure");
			LF_PersistenceComponentSerializer.ClearLoadDamaged();
		}

		LF_PhotoIdRow fresh = LF_PhotoIdCard.Issue(NEW_HOLDER, "Bob", "Kral", 45, "02/06/1989", "");
		ctx.NotNull(fresh, "a new card is printed after the failed read");
		if (fresh)
		{
			ctx.True(fresh.m_sNumber != oldNumber, "the new card does not reuse the number an old card carries");
			ctx.True(LF_PhotoIdCard.SequenceOf(fresh.m_sNumber) > LF_PhotoIdCard.UNREADABLE_FLOOR, "numbering resumes past the floor");
		}

		ctx.True(LF_PhotoIdCard.FindNumber(oldNumber) == null, "the old card's number matches no row: it reads as unregistered");
		ctx.False(LF_PhotoIdCard.IsValid(oldNumber), "the old card is not valid for anyone");

		kept.Restore();
	}
}
