// red-proof: observed 2026-11-15 with `tools\cli test --no-build --only LF_Test_QuizHarness` while LF_QuizHarness.Clear was a stub that
// forgot nothing:
//   SUMMARY tier=only passed=0 failed=1 total=1 skipped=0
//   [LFTEST] FAIL harness/quiz-clear
//   [LFTEST]   - after the clear the quiz check says the player has not passed
// Implemented; green.
// tier: LOGIC
//! Harness verb `quizclear` (#1567): forgets one account's rules-quiz pass in memory and in the progression
//! store, so a proof can make a rejoin see the quiz again. Harness-only: a build without LF_HARNESS refuses it.
class LF_Test_QuizHarness : LF_Test
{
	protected static const string ACCOUNT = "quiz-harness-account";
	protected static const string OTHER = "quiz-harness-other";

	override string GetName()
	{
		return "harness/quiz-clear";
	}

	//------------------------------------------------------------------------------------------------
	protected void Clean()
	{
		LF_Progression.Forget(ACCOUNT);
		LF_Progression.Forget(OTHER);
		LF_Quiz.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		Clean();
		LF_Test_Quiz.Use(5);

		// The stored pass (the flag LF_Progression.NoteFlag keeps) is a pass until it is cleared.
		LF_Progression.NoteFlag(ACCOUNT, LF_Quiz.PASS_FLAG);
		LF_Progression.NoteFlag(OTHER, LF_Quiz.PASS_FLAG);
		ctx.True(LF_Quiz.HasPassed(ACCOUNT), "the noted flag is a pass");
		ctx.False(LF_Quiz.Required(ACCOUNT), "a passed account is not asked");

		ctx.True(LF_QuizHarness.Clear(ACCOUNT), "the clear finds a pass to forget");
		ctx.False(LF_Quiz.HasPassed(ACCOUNT), "after the clear the quiz check says the player has not passed");
		ctx.True(LF_Quiz.Required(ACCOUNT), "and the server asks the account again");
		ctx.False(LF_Progression.HasFlag(ACCOUNT, LF_Quiz.PASS_FLAG), "the progression store no longer holds the flag");
		ctx.True(LF_Quiz.HasPassed(OTHER), "another account keeps its pass");
		ctx.False(LF_QuizHarness.Clear(ACCOUNT), "a second clear finds nothing to forget");

		// A pass earned in this session lives in memory as well as in the store; both go.
		LF_Quiz.ResetStatics();
		LF_Test_Quiz.Use(5);
		ctx.True(LF_Quiz.Submit(ACCOUNT, LF_Test_Quiz.Answers(5, 5)).m_bPassed, "the account passes in the session");
		ctx.True(LF_Quiz.HasPassed(ACCOUNT), "the session pass counts");
		ctx.True(LF_QuizHarness.Clear(ACCOUNT), "the clear forgets a session pass");
		ctx.False(LF_Quiz.HasPassed(ACCOUNT), "the in-memory pass is gone");
		ctx.False(LF_Progression.HasFlag(ACCOUNT, LF_Quiz.PASS_FLAG), "and the stored one");
		ctx.False(LF_QuizHarness.Clear(""), "no account, nothing to clear");

		// The verb: recognised, and refused in any build without LF_HARNESS (the suite is not one).
		ctx.True(LF_QuizHarness.IsVerb("quizclear"), "quizclear is this harness verb");
		ctx.False(LF_QuizHarness.IsVerb("quizclearall"), "a longer word is not");
		ctx.False(LF_QuizHarness.IsVerb("itemgive {A}x.et"), "another verb is not");
		ctx.False(LF_DevGate.HarnessBuild(), "the test suite is not a harness build");
		LF_Progression.NoteFlag(ACCOUNT, LF_Quiz.PASS_FLAG);
		ctx.EqualStr("error harness_only", LF_QuizHarness.Run(null, "quizclear"), "a build without LF_HARNESS refuses the verb");
		ctx.True(LF_Quiz.HasPassed(ACCOUNT), "and the refused verb left the pass alone");

		Clean();
	}
}
