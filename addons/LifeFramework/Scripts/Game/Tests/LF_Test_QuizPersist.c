// red-proof: observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_Quiz,LF_Test_QuizPersist`, the stored-pass read in
// LF_Quiz.HasPassed replaced by `return false;`:
//   SUMMARY tier=only passed=0 failed=2 total=2 skipped=0
//   [LFTEST] FAIL persist/quiz-pass
//   [LFTEST]   - a restart does not ask a passed account again
// Restored; green.
// tier: PERSISTENCE
//! Rules orientation quiz (#915): a pass is saved with the account's progression and read back, so a
//! restart does not ask again. A quest log built from the same record does not list the pass as a quest.
class LF_Test_QuizPersist : LF_Test
{
	protected static const string PASSED = "quiz-persist-passed";
	protected static const string FRESH = "quiz-persist-fresh";

	override string GetName()
	{
		return "persist/quiz-pass";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_PersistenceComponent persistence = LF_TestProgressionKit.Persistence(ctx);
		if (!persistence)
			return;

		// Reloading "progression" rebuilds job XP from the legacy single-level fields, which would drop the
		// farmer and miner rows E2E_SaveState wrote before this test; keep and put back what was there.
		LF_TestLegacySnapshot kept = LF_TestLegacySnapshot.Take();
		RunQuiz(ctx, persistence);
		kept.Restore();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunQuiz(LF_TestContext ctx, LF_PersistenceComponent persistence)
	{
		LF_Progression.Forget(PASSED);
		LF_Progression.Forget(FRESH);
		LF_Quiz.ResetStatics();
		LF_Test_Quiz.Use(5);

		ctx.True(LF_Quiz.Submit(PASSED, LF_Test_Quiz.Answers(5, 5)).m_bPassed, "the account passes in the session");

		// Save through the real write path.
		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(PASSED), "the save carries the passed account");

		// Restart: the session holds none of it.
		LF_Progression.Forget(PASSED);
		LF_Quiz.ResetStatics();
		LF_Test_Quiz.Use(5);
		ctx.True(LF_Quiz.Required(PASSED), "after the wipe the account would be asked again");

		// Load through the real read path.
		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the save parses");
		LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, loadContext, "progression");
		ctx.True(LF_Progression.HasFlag(PASSED, LF_Quiz.PASS_FLAG), "the pass is back after the load");
		ctx.False(LF_Quiz.Required(PASSED), "a restart does not ask a passed account again");
		ctx.True(LF_Quiz.Required(FRESH), "an account that never passed is still asked");

		// A character's quest log is built from the same list: the pass is not a quest in it.
		LF_QuestTracker tracker = new LF_QuestTracker();
		ctx.False(tracker.RestoreTurnedIn(LF_Progression.FLAG_PREFIX + LF_Quiz.PASS_FLAG), "the pass is not restored as a turned-in quest");

		// A reset is saved too: the next save has no pass, so a retake is forced after a restart.
		LF_Quiz.Reset(PASSED);
		JsonSaveContext afterReset = new JsonSaveContext(false);
		LF_PersistenceComponentSerializer.SerializeStore(persistence, afterReset);
		LF_Progression.Forget(PASSED);
		JsonLoadContext reload = new JsonLoadContext(false);
		reload.LoadFromString(afterReset.SaveToString());
		LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, reload, "progression");
		ctx.True(LF_Quiz.Required(PASSED), "a reset account is asked again after a restart");

		LF_Progression.Forget(PASSED);
		LF_Progression.Forget(FRESH);
		LF_Quiz.ResetStatics();
	}
}
