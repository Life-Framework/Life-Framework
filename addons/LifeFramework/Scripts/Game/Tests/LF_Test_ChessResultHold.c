// red-proof: 2026-11-17, bug fix. On the unfixed LF_ChessTable.Sit (a finished game cleared for any newcomer) `--only LF_Test_ChessResultHold` fails "a bystander does not wipe a finished game". With the hold, green.

// tier: LOGIC
//! #2405 (review 2026-11-17): with both seats held and the game over, a third player pressing Play
//! used to call NewGame() and wipe the result. Now only the two players keep the board, until
//! RESULT_HOLD_MS after the last move; a free seat still starts a new game for a newcomer.
class LF_Test_ChessResultHold : LF_Test
{
	protected static const int WHITE = 201;
	protected static const int BLACK = 202;
	protected static const int BYSTANDER = 203;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "chess/result-hold";
	}

	//------------------------------------------------------------------------------------------------
	//! A table where white has been mated by fool's mate at t = 2000.
	protected LF_ChessTable Finished(LF_TestContext ctx)
	{
		LF_ChessTable table = new LF_ChessTable();
		table.Sit(WHITE, 1000);
		table.Sit(BLACK, 1000);
		table.Move(WHITE, "f2f3", 1000);
		table.Move(BLACK, "e7e5", 1000);
		table.Move(WHITE, "g2g4", 1000);
		table.Move(BLACK, "d8h4", 2000);
		ctx.True(table.GetGame().IsOver(), "fool's mate ends the game");
		return table;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ChessTable table = Finished(ctx);
		ctx.Equal(LF_ChessTable.SEAT_NONE, table.Sit(BYSTANDER, 2001), "a third player gets no seat at a finished game");
		bool wiped = false;
		if (!table.GetGame().IsOver())
			wiped = true;
		ctx.False(wiped, "a bystander does not wipe a finished game");
		ctx.Equal(LF_ChessGame.RESULT_BLACK_WINS, table.GetGame().GetResult(), "the result stands");
		ctx.Equal(WHITE, table.GetWhiteId(), "white keeps the seat");
		ctx.Equal(BLACK, table.GetBlackId(), "black keeps the seat");
		ctx.Equal(LF_ChessTable.SEAT_WHITE, table.Sit(WHITE, 2002), "a player pressing Play at their own finished game keeps the seat");
		ctx.True(table.GetGame().IsOver(), "and does not clear it");
		ctx.False(table.ResultHoldOver(2000 + LF_ChessTable.RESULT_HOLD_MS - 1), "held until the timeout");

		// After the timeout anyone may clear the board.
		ctx.True(table.ResultHoldOver(2000 + LF_ChessTable.RESULT_HOLD_MS), "released after the timeout");
		table.Sit(BYSTANDER, 2000 + LF_ChessTable.RESULT_HOLD_MS);
		ctx.False(table.GetGame().IsOver(), "after the timeout a newcomer clears the finished game");

		// A free seat is unchanged: the loser walks away, a newcomer sits and a new game starts.
		LF_ChessTable open = Finished(ctx);
		ctx.True(open.StandUp(BLACK, 2100), "black stands up from the finished game");
		ctx.Equal(LF_ChessTable.SEAT_BLACK, open.Sit(BYSTANDER, 2101), "a newcomer takes the freed seat");
		ctx.False(open.GetGame().IsOver(), "and gets a fresh game");
		ctx.Equal(WHITE, open.GetWhiteId(), "white stays seated");
	}
}
