// red-proof: RED-OK 2026-11-22 `node tools/cli.mjs test --no-build --red LF_Test_DialogueNumbers`: with the hook on `0 picks the Goodbye row` fails (expected 3, got -1) and so do `the row after the replies is Goodbye` and `a reply that leads to the bye node is the NPC's farewell`; the green run passes every assertion.
//! The numbers of the NPC conversation menu: replies 1..n, then Goodbye as 0, always last. A digit,
//! the printed number and the focus step all come from LF_DialogueKeys, and closed scenes have
//! nothing to leave.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_DialogueNumbers : LF_Test
{
	override string GetName()
	{
		return "dialogue/numbers";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// A node with three replies: keys 1, 2, 3 pick rows 0, 1, 2; 0 picks the Goodbye row (3).
		ctx.Equal(0, LF_DialogueKeys.RowOfDigit(1, 3), "1 picks the first reply");
		ctx.Equal(2, LF_DialogueKeys.RowOfDigit(3, 3), "3 picks the third reply");
		int bye = LF_DialogueKeys.RowOfDigit(0, 3);
		if (RedProof())
			bye = -1;

		ctx.Equal(3, bye, "0 picks the Goodbye row");
		ctx.True(LF_DialogueKeys.IsGoodbye(bye, 3), "the row after the replies is Goodbye");
		ctx.False(LF_DialogueKeys.IsGoodbye(2, 3), "the last reply is not Goodbye");

		// A digit with no row on screen picks nothing.
		ctx.Equal(-1, LF_DialogueKeys.RowOfDigit(4, 3), "4 picks nothing with three replies");
		ctx.Equal(-1, LF_DialogueKeys.RowOfDigit(9, 3), "9 picks nothing with three replies");
		ctx.Equal(-1, LF_DialogueKeys.RowOfDigit(-1, 3), "a negative digit picks nothing");

		// A full node (four replies, the most the screen shows) still ends in "0. Goodbye".
		ctx.Equal(4, LF_DialogueFlow.MAX_OPTIONS, "the menu shows four replies");
		ctx.Equal(5, LF_DialogueKeys.RowCount(LF_DialogueFlow.MAX_OPTIONS), "four replies and Goodbye make five rows");
		ctx.Equal(3, LF_DialogueKeys.RowOfDigit(4, 4), "4 picks the fourth reply");
		ctx.Equal(4, LF_DialogueKeys.RowOfDigit(0, 4), "0 picks Goodbye after four replies");

		// What is printed in front of each row is the key that picks it.
		for (int replies = 1; replies <= LF_DialogueFlow.MAX_OPTIONS; replies++)
		{
			for (int row = 0; row < LF_DialogueKeys.RowCount(replies); row++)
			{
				int digit = LF_DialogueKeys.DigitOfRow(row, replies);
				ctx.Equal(row, LF_DialogueKeys.RowOfDigit(digit, replies), string.Format("the number printed on row %1 of %2 picks that row", row, replies));
			}
		}

		ctx.Equal(0, LF_DialogueKeys.DigitOfRow(3, 3), "Goodbye is printed as 0");

		// A pad walks the rows with up and down and wraps through Goodbye.
		ctx.Equal(1, LF_DialogueKeys.Step(0, 1, 3), "down from the first reply");
		ctx.Equal(3, LF_DialogueKeys.Step(2, 1, 3), "down from the last reply lands on Goodbye");
		ctx.Equal(0, LF_DialogueKeys.Step(3, 1, 3), "down from Goodbye wraps to the first reply");
		ctx.Equal(3, LF_DialogueKeys.Step(0, -1, 3), "up from the first reply wraps to Goodbye");
		ctx.Equal(0, LF_DialogueKeys.Step(0, 0, 3), "no step keeps the focus");

		// A reply that only says goodbye gets no row of its own: Goodbye (0) picks it, so no menu shows two.
		LF_DialogueOption farewell = new LF_DialogueOption();
		farewell.m_sNext = "bye";
		bool dropped = LF_DialogueKeys.IsFarewell(farewell);
		if (RedProof())
			dropped = false;

		ctx.True(dropped, "a reply that leads to the bye node is the NPC's farewell");
		LF_DialogueOption work = new LF_DialogueOption();
		work.m_sNext = "work";
		ctx.False(LF_DialogueKeys.IsFarewell(work), "a reply to another node is a normal row");
		LF_DialogueOption sellAndGo = new LF_DialogueOption();
		sellAndGo.m_sNext = "bye";
		sellAndGo.m_sAction = "OpenShop";
		ctx.False(LF_DialogueKeys.IsFarewell(sellAndGo), "a reply that runs an action is never swallowed");
		ctx.False(LF_DialogueKeys.IsFarewell(null), "no reply is no farewell");

		// Close path: a scene that is not open has nothing to leave, and does not break.
		ctx.False(LF_DialogueScene.IsOpen(), "no conversation is open in the suite");
		ctx.False(LF_DialogueScene.Goodbye(), "Goodbye with no conversation open leaves nothing");
		ctx.False(LF_DialogueScene.Back(), "Back with no conversation open leaves nothing");
		ctx.False(LF_DialogueScene.PickRow(LF_DialogueKeys.RowOfDigit(1, 3)), "a pick with no conversation open does nothing");
	}
}
