// red-proof: with the frame count ignored (the old time-only hold) "a slow first frame still sees the key" fails; with the edge rules ignored "a held Select acts once" fails. Both go through RedProof() in `tools\cli test --no-build --red LF_Test_DialogueHeldKeys`.
//! #4720, #4725, #4683: the harness's `input MenuSelect` was held 300 ms of wall time, so one slow
//! client frame (a starved two-client session) ended the hold before the dialogue's per-frame poll
//! saw the key and the reply was never picked. The hold now lasts at least three frames, and the
//! dialogue acts on a key going down, not on each tick it stays down, so the longer hold picks once.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_DialogueHeldKeys : LF_Test
{
	override string GetName()
	{
		return "dialogue/held-keys";
	}

	//------------------------------------------------------------------------------------------------
	//! How many ticks of a key held for `ticks` ticks act, reading it the way LF_DialogueScene.Tick does.
	protected int Acts(int ticks)
	{
		bool held = false;
		int acted = 0;
		for (int i = 0; i < ticks; i++)
		{
			bool edge = true;
			if (!RedProof())
				edge = LF_DialogueScene.PressEdge(true, held);

			if (edge)
				acted++;

			held = true;
		}

		return acted;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// The first frame after the press arrives 400 ms later (the hold's 300 ms is gone): the key
		// must still be asserted for that frame.
		bool stillHeld = true;
		if (!RedProof())
			stillHeld = LF_AgentHarness.HoldActive(1400, 1300, 0);
		else
			stillHeld = 1400 < 1300;

		ctx.True(stillHeld, "a slow first frame still sees the key");
		ctx.True(LF_AgentHarness.HoldActive(1000, 1300, 3), "time left keeps the hold");
		ctx.False(LF_AgentHarness.HoldActive(1400, 1300, LF_AgentHarness.HOLD_FRAMES), "time and frames done end the hold");
		ctx.True(LF_AgentHarness.HoldActive(1400, 1300, LF_AgentHarness.HOLD_FRAMES - 1), "one frame short keeps the hold");

		ctx.Equal(1, Acts(6), "a held Select acts once");
		ctx.True(LF_DialogueScene.PressEdge(true, false), "a new press acts");
		ctx.False(LF_DialogueScene.PressEdge(false, true), "a release does not act");

		ctx.Equal(1, LF_DialogueScene.DigitEdge(1, -1), "digit 1 going down acts");
		ctx.Equal(-1, LF_DialogueScene.DigitEdge(1, 1), "digit 1 held does not act again");
		ctx.Equal(2, LF_DialogueScene.DigitEdge(2, 1), "another digit acts");
		ctx.Equal(-1, LF_DialogueScene.DigitEdge(-1, 1), "no digit down, none pressed");
		ctx.Equal(0, LF_DialogueScene.DigitEdge(0, -1), "digit 0 (Goodbye) going down acts");

		ctx.True(LF_AgentCommand.IsMenuAction("LF_Keypad1"), "the harness may press a reply digit");
		ctx.True(LF_AgentCommand.IsMenuAction("LF_Keypad0"), "the harness may press the Goodbye digit");
		ctx.False(LF_AgentCommand.IsMenuAction("LF_Keypad"), "a bare LF_Keypad is no action");
		ctx.False(LF_AgentCommand.IsMenuAction("LF_Keypad12"), "two digits are no action");
		ctx.False(LF_AgentCommand.IsMenuAction("LF_KeypadBack"), "only the digits, not the other keypad keys");
	}
}
