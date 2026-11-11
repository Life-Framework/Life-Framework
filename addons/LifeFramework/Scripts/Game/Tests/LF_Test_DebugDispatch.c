// red-proof: Observed 2026-11-10 with `tools\cli test --no-build --only LF_Test_DebugDispatch`: expecting 2 board entries after the on-duty test call failed it with `- the test call reached dispatch once (expected 2, got 1)`. Restored; green.
//------------------------------------------------------------------------------------------------
//! comms/debug-dispatch: the dispatch console's test call is refused when the game mode has it
//! off, refused with the no-answer line when nobody is on duty, and otherwise reaches dispatch
//! once through the same path a payphone call takes.
// tier: WORLD
class LF_Test_DebugDispatch : LF_Test
{
	override string GetName()
	{
		return "comms/debug-dispatch";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_EventBus.ResetStatics();
		LF_Dispatch.ResetStatics();
		LF_ServiceCallDispatch.ResetStatics();
		LF_ServiceCallDispatch.Install();

		ctx.EqualStr("#LF-Dispatch_TestOff", LF_DebugDispatch.RefusalKey(false, true, true), "off in this game mode: the off line");
		ctx.EqualStr(LF_ServiceCalls.NO_ANSWER_KEY, LF_DebugDispatch.RefusalKey(true, true, false), "nobody on duty: the no-answer line");
		ctx.EqualStr(LF_ServiceCalls.NO_ANSWER_KEY, LF_DebugDispatch.RefusalKey(true, false, false), "unknown service: the no-answer line");
		ctx.EqualStr("", LF_DebugDispatch.RefusalKey(true, true, true), "answered: no refusal");

		array<ref LF_ServiceCandidate> officer = {};
		officer.Insert(LF_ServiceCandidate.Make(1, true, LF_EJobType.UNEMPLOYED));
		array<ref LF_ServiceCandidate> nobody = {};
		array<string> board = {};
		vector spot = "284.3 0 240.2";
		string summary = "#LF-Payphone_Summary_Crime";

		ctx.EqualStr(LF_DebugDispatch.RefusalKey(false, true, true), LF_DebugDispatch.Fire(false, "tester", "police", summary, spot, officer), "the gate off refuses");
		LF_Dispatch.BuildBoardEntries(10, board);
		ctx.Equal(0, board.Count(), "a refused test call reaches no dispatch");

		ctx.EqualStr(LF_ServiceCalls.NO_ANSWER_KEY, LF_DebugDispatch.Fire(true, "tester", "police", summary, spot, nobody), "nobody on duty gives no answer");
		LF_Dispatch.BuildBoardEntries(10, board);
		ctx.Equal(0, board.Count(), "an unanswered test call reaches no dispatch");

		ctx.EqualStr("", LF_DebugDispatch.Fire(true, "tester", "police", summary, spot, officer), "an on-duty officer answers");
		LF_Dispatch.BuildBoardEntries(10, board);
		ctx.Equal(1, board.Count(), "the test call reached dispatch once");

		LF_EventBus.ResetStatics();
		LF_Dispatch.ResetStatics();
		LF_ServiceCallDispatch.ResetStatics();
	}
}
