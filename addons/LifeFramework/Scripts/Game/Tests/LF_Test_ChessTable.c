// red-proof: new test, no failing state to restore. `tools\cli test --no-build --red LF_Test_ChessTable`
// skips white's e2e4 behind the RedProof() hook (2026-11-16). Observed:
//   RED-OK chess/table (fails with the hook on)
//   red: the last move is replicated (expected 'e2e4', got '')

// tier: WORLD
//! #1934: a chess table holds one server-side game. Two players sit (white first), a third only
//! watches; a move from a watcher, from the wrong seat or out of turn is refused on the server;
//! standing up mid-game holds the seat for the grace period, then resigns for the absent side.
class LF_Test_ChessTable : LF_Test
{
	static const ResourceName TABLE_PREFAB = "{33881F3E4E64549D}Prefabs/Props/Chess/LF_ChessTable.et";
	static const int WHITE = 101;
	static const int BLACK = 102;
	static const int WATCHER = 103;

	override string GetName()
	{
		return "chess/table";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity entity = LF_Utils.SpawnEntityPrefab(TABLE_PREFAB, "0 0 0");
		ctx.True(entity != null, "the chess table prefab spawns");
		LF_ChessTableComponent table = LF_ChessTableComponent.Of(entity);
		ctx.True(table != null, "the table carries LF_ChessTableComponent");
		if (!table || !table.GetTable())
		{
			ctx.True(false, "the table has a server-side game");
			SCR_EntityHelper.DeleteEntityAndChildren(entity);
			return;
		}

		CheckSeats(ctx, table);
		CheckMoves(ctx, table);
		CheckStandUp(ctx, table);
		SCR_EntityHelper.DeleteEntityAndChildren(entity);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckSeats(LF_TestContext ctx, LF_ChessTableComponent table)
	{
		ctx.Equal(LF_ChessTable.SEAT_WHITE, table.ServerSit(WHITE), "the first player sits as white");
		ctx.Equal(LF_ChessTable.SEAT_BLACK, table.ServerSit(BLACK), "the second player sits as black");
		ctx.Equal(LF_ChessTable.SEAT_NONE, table.ServerSit(WATCHER), "a third player only watches");
		ctx.Equal(LF_ChessTable.SEAT_WHITE, table.ServerSit(WHITE), "sitting again keeps the seat");
		ctx.Equal(WHITE, table.GetWhiteId(), "the replicated white seat names the first player");
		ctx.Equal(BLACK, table.GetBlackId(), "the replicated black seat names the second player");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckMoves(LF_TestContext ctx, LF_ChessTableComponent table)
	{
		string start = table.GetFen();
		ctx.False(table.ServerMove(BLACK, "e7e5"), "black may not move first");
		ctx.False(table.ServerMove(WATCHER, "e2e4"), "a watcher may not move");
		ctx.False(table.ServerMove(BLACK, "e2e4"), "black may not move a white piece");
		ctx.False(table.ServerMove(WHITE, "e2e5"), "an illegal move is refused");
		ctx.EqualStr(start, table.GetFen(), "refused moves leave the board alone");

		bool played = false;
		if (!RedProof())
			played = table.ServerMove(WHITE, "e2e4");
		ctx.True(played, "white plays e2e4");
		ctx.EqualStr("e2e4", table.GetLastMove(), "the last move is replicated");
		ctx.True(table.GetFen().StartsWith("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b"), "the replicated FEN shows the move and black to move");
		ctx.False(table.ServerMove(WHITE, "d2d4"), "white may not move twice");
		ctx.True(table.ServerMove(BLACK, "e7e5"), "black answers in turn");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckStandUp(LF_TestContext ctx, LF_ChessTableComponent table)
	{
		LF_ChessTable seats = table.GetTable();
		int t0 = 1000;
		ctx.True(seats.StandUp(BLACK, t0), "black stands up mid-game");
		table.ServerTickAt(t0 + LF_ChessTable.GRACE_MS - 1);
		ctx.Equal(LF_ChessGame.RESULT_ONGOING, table.GetResult(), "the game runs on inside the grace period");
		ctx.Equal(BLACK, table.GetBlackId(), "the seat is held inside the grace period");

		ctx.Equal(LF_ChessTable.SEAT_BLACK, table.ServerSit(BLACK), "black sits back down in time");
		ctx.True(seats.StandUp(BLACK, t0), "black stands up again");
		table.ServerTickAt(t0 + LF_ChessTable.GRACE_MS);
		ctx.Equal(LF_ChessGame.RESULT_WHITE_WINS, table.GetResult(), "after the grace period black has resigned");
		ctx.Equal(LF_ChessGame.END_RESIGNATION, table.GetEnd(), "the game ended by resignation");
		ctx.Equal(0, table.GetBlackId(), "the black seat is free again");

		ctx.Equal(LF_ChessTable.SEAT_BLACK, table.ServerSit(WATCHER), "the watcher takes the free seat");
		ctx.Equal(LF_ChessGame.RESULT_ONGOING, table.GetResult(), "a new game starts when someone sits at a finished one");
		ctx.EqualStr(LF_ChessBoard.START_FEN, table.GetFen(), "the new game is at the start position");
	}
}
