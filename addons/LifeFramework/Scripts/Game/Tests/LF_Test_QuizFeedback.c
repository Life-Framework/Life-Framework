// red-proof: observed 2026-11-22 with `tools\cli test --no-build --red LF_Test_QuizFeedback`: `RED-OK rules/quiz-feedback (fails with the hook on, 16 assertions)`; the hook skips CheckOne and the Arrival skin.
//! Rules quiz feedback after each question (first-2h review A1, A2): the server says right or wrong with
//! the rule line for one pick without scoring it, the feedback page has a Next or See-result button, the
//! shipped quiz no longer kicks, and the arrival sheets sit in the middle of the screen. No world.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_QuizFeedback : LF_Test
{
	override string GetName()
	{
		return "rules/quiz-feedback";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_Test_Quiz.Use(3);
		int attemptsBefore = LF_Quiz.GetAttempts();

		string note;
		bool right = false;
		bool wrong = true;
		bool outside = true;
		if (!RedProof())
		{
			right = LF_Quiz.CheckOne(1, 1, note);
			wrong = LF_Quiz.CheckOne(1, 0, note);
			outside = LF_Quiz.CheckOne(7, 0, note);
		}

		ctx.True(right, "the right option of question 1 is judged right");
		ctx.False(wrong, "a wrong option is judged wrong");
		ctx.False(outside, "a question that does not exist is judged wrong, not a crash");
		ctx.Equal(attemptsBefore, LF_Quiz.GetAttempts(), "judging one question scores nothing");

		string ruleNote;
		LF_Quiz.CheckOne(2, 0, ruleNote);
		ctx.EqualStr("Rule 2", ruleNote, "the rule line comes with the verdict");

		array<ref LF_UIPart> rightPage = LF_QuizSheet.BuildFeedbackParts(0, 3, true, "Rule 0", false);
		array<ref LF_UIPart> lastPage = LF_QuizSheet.BuildFeedbackParts(2, 3, false, "Rule 2", true);
		ctx.Equal(5, rightPage.Count(), "title, number, verdict, rule line and the Next button");
		ctx.EqualStr(LF_QuizSheet.PART_NEXT, rightPage[rightPage.Count() - 1].GetName(), "the page ends in the Next button");
		ctx.Equal(5, lastPage.Count(), "the last page has the same parts");

		LF_Quiz.ResetStatics();
		ctx.Equal(-1, LF_Quiz.AttemptsLeft("quiz-feedback-account"), "the shipped quiz has no attempt limit, so no one is kicked");

		float width = 0;
		bool centred = false;
		if (!RedProof())
		{
			LF_UISkin skin = LF_UISkin.Arrival(500);
			centred = skin.m_bCentered;
			width = skin.m_fWidth;
		}

		ctx.True(centred, "the arrival skin is centred");
		ctx.True(width > 0, "the arrival skin has a width");
	}
}
