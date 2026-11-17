// red-proof: observed 2026-11-17 with `tools\cli test --no-build --only LF_Test_IdRequestReask` on the unfixed Open() (old sheet closed before the answered flag was reset), FAIL id-check/reask:
// `closing the replaced sheet sent no answer for the new request (expected 0, got 1)`, `dismissing the sheet sent no answer (expected 0, got 1)`. Restored; green.
// tier: WORLD
//! id-check/reask (#2413): an officer asking again while the first ID sheet is still open replaces
//! the sheet. The old sheet closes without an answer: LF_IdRequestSheet.OnClosed used to send a
//! refusal for it (the answered flag was reset only after the close), and the server read that as
//! the answer to the new request, so the officer heard "refused". Only the citizen's own button, or
//! closing the sheet they are looking at, answers.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_IdRequestReask : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "id-check/reask";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_IdRequestSheet.ResetStatics();
		int before = LF_IdRequestSheet.AnswersSent();

		LF_UIScreen first = LF_IdRequestSheet.Open();
		ctx.NotNull(first, "the first request opens the sheet");
		if (!first)
			return;

		LF_UIScreen second = LF_IdRequestSheet.Open();
		ctx.NotNull(second, "a second request opens a fresh sheet");
		ctx.True(second != first, "the second sheet replaces the first");
		ctx.True(LF_IdRequestSheet.Current() == second, "the open sheet is the new one");
		ctx.Equal(before, LF_IdRequestSheet.AnswersSent(), "closing the replaced sheet sent no answer for the new request");

		// The new sheet answers on its own: dismissing it unanswered is not an answer either, so
		// the world-end reset below leaves nothing behind.
		LF_IdRequestSheet.Dismiss();
		ctx.Equal(before, LF_IdRequestSheet.AnswersSent(), "dismissing the sheet sent no answer");
		LF_IdRequestSheet.ResetStatics();
	}
}
