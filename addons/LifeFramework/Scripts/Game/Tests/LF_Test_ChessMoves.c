// red-proof: new test, no failing state to restore. `tools\cli test --no-build --red LF_Test_ChessMoves`
// (2026-11-16) skips the move generation behind the RedProof() hook. Observed:
//   RED-OK chess/moves (fails with the hook on, 106 assertions)
//   red: the start position has 20 legal moves (expected 20, got 0)

// tier: LOGIC
//! #1931: chess rules for every piece. The start position has 20 legal moves and perft 400 / 8902;
//! four well-known positions (chessprogramming.org "Perft Results") cover castling, en passant,
//! promotion, pins and discovered checks; castling through, out of and into check is refused.
class LF_Test_ChessMoves : LF_Test
{
	override string GetName()
	{
		return "chess/moves";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_ChessBoard start = LF_ChessBoard.Start();
		ctx.EqualStr(LF_ChessBoard.START_FEN, start.ToFen(), "the start position round-trips through FEN");
		int legal = 0;
		if (!RedProof())
			legal = LF_ChessMoves.CountLegal(start);
		ctx.Equal(20, legal, "the start position has 20 legal moves");
		ctx.Equal(400, LF_ChessMoves.Perft(start, 2), "perft 2 from the start is 400");
		ctx.Equal(8902, LF_ChessMoves.Perft(start, 3), "perft 3 from the start is 8902");

		// Kiwipete: both sides can castle either way, en passant, promotions, pins.
		LF_ChessBoard kiwi = Fen(ctx, "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
		ctx.Equal(48, LF_ChessMoves.Perft(kiwi, 1), "kiwipete perft 1");
		ctx.Equal(2039, LF_ChessMoves.Perft(kiwi, 2), "kiwipete perft 2");
		ctx.Equal(97862, LF_ChessMoves.Perft(kiwi, 3), "kiwipete perft 3");

		// Rook endgame: en passant that exposes the king along a rank, discovered checks.
		LF_ChessBoard endgame = Fen(ctx, "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1");
		ctx.Equal(14, LF_ChessMoves.Perft(endgame, 1), "rook endgame perft 1");
		ctx.Equal(191, LF_ChessMoves.Perft(endgame, 2), "rook endgame perft 2");
		ctx.Equal(2812, LF_ChessMoves.Perft(endgame, 3), "rook endgame perft 3");

		// Promotion with capture, castling rights lost to a captured rook.
		LF_ChessBoard promo = Fen(ctx, "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1");
		ctx.Equal(6, LF_ChessMoves.Perft(promo, 1), "promotion position perft 1");
		ctx.Equal(264, LF_ChessMoves.Perft(promo, 2), "promotion position perft 2");
		ctx.Equal(9467, LF_ChessMoves.Perft(promo, 3), "promotion position perft 3");

		LF_ChessBoard mixed = Fen(ctx, "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8");
		ctx.Equal(44, LF_ChessMoves.Perft(mixed, 1), "mixed position perft 1");
		ctx.Equal(1486, LF_ChessMoves.Perft(mixed, 2), "mixed position perft 2");
		ctx.Equal(62379, LF_ChessMoves.Perft(mixed, 3), "mixed position perft 3");

		CheckCastling(ctx);
		CheckSpecialMoves(ctx);
		CheckEnd(ctx);
		CheckPlay(ctx);
	}

	//------------------------------------------------------------------------------------------------
	protected LF_ChessBoard Fen(LF_TestContext ctx, string fen)
	{
		LF_ChessBoard board = new LF_ChessBoard();
		ctx.True(board.SetFen(fen), "FEN parses: " + fen);
		return board;
	}

	protected bool Has(LF_ChessBoard board, string name)
	{
		return LF_ChessMoves.FindMove(board, name) != -1;
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckCastling(LF_TestContext ctx)
	{
		// A rook on f2 guards f1: the king may not cross it, but castling long is open.
		LF_ChessBoard through = Fen(ctx, "r3k2r/8/8/8/8/8/5r2/R3K2R w KQkq - 0 1");
		ctx.False(Has(through, "e1g1"), "castling through an attacked square is refused");
		ctx.True(Has(through, "e1c1"), "castling the other way is still allowed");

		// Out of check: neither side.
		LF_ChessBoard outOfCheck = Fen(ctx, "4r2k/8/8/8/8/8/8/R3K2R w KQ - 0 1");
		ctx.False(Has(outOfCheck, "e1g1"), "castling out of check is refused (short)");
		ctx.False(Has(outOfCheck, "e1c1"), "castling out of check is refused (long)");

		// Into check: g1 is covered by a rook on g8.
		LF_ChessBoard into = Fen(ctx, "6rk/8/8/8/8/8/8/R3K2R w KQ - 0 1");
		ctx.False(Has(into, "e1g1"), "castling into check is refused");
		ctx.True(Has(into, "e1c1"), "castling long beside a guarded g file is allowed");

		// The rook on a1 may be attacked (b1 guarded): the king only crosses d1 and c1.
		LF_ChessBoard rookAttacked = Fen(ctx, "1r5k/8/8/8/8/8/8/R3K3 w Q - 0 1");
		ctx.True(Has(rookAttacked, "e1c1"), "an attacked rook does not stop castling long");

		// Blocked and right-less castles.
		LF_ChessBoard blocked = Fen(ctx, "4k3/8/8/8/8/8/8/R2QK1NR w KQ - 0 1");
		ctx.False(Has(blocked, "e1g1"), "a piece between king and rook blocks castling");
		ctx.False(Has(blocked, "e1c1"), "a queen between king and rook blocks castling long");
		LF_ChessBoard noRights = Fen(ctx, "4k3/8/8/8/8/8/8/R3K2R w - - 0 1");
		ctx.False(Has(noRights, "e1g1"), "no castling right, no castle");

		// The castle moves the rook and spends both rights; a rook move spends one.
		LF_ChessBoard shortCastled = Fen(ctx, "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
		shortCastled = LF_ChessMoves.Apply(shortCastled, LF_ChessMoves.FindMove(shortCastled, "e1g1"));
		ctx.EqualStr("r3k2r/8/8/8/8/8/8/R4RK1 b kq - 1 1", shortCastled.ToFen(), "short castling puts the rook on f1 and drops white's rights");
		LF_ChessBoard longCastled = Fen(ctx, "r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1");
		longCastled = LF_ChessMoves.Apply(longCastled, LF_ChessMoves.FindMove(longCastled, "e8c8"));
		ctx.EqualStr("2kr3r/8/8/8/8/8/8/R3K2R w KQ - 1 2", longCastled.ToFen(), "long castling puts the rook on d8 and drops black's rights");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckSpecialMoves(LF_TestContext ctx)
	{
		LF_ChessBoard ep = Fen(ctx, "4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1");
		int take = LF_ChessMoves.FindMove(ep, "e5d6");
		ctx.True(take != -1, "en passant is offered right after the double push");
		if (take != -1)
			ctx.EqualStr("4k3/8/3P4/8/8/8/8/4K3 b - - 0 1", LF_ChessMoves.Apply(ep, take).ToFen(), "en passant removes the pawn that passed");
		LF_ChessBoard epGone = Fen(ctx, "4k3/8/8/3pP3/8/8/8/4K3 w - - 0 1");
		ctx.False(Has(epGone, "e5d6"), "en passant lapses when the right is not on the board");

		// The double push opens en passant only for the next move.
		LF_ChessBoard pushed = Fen(ctx, "4k3/3p4/8/4P3/8/8/8/4K3 b - - 0 1");
		pushed = LF_ChessMoves.Apply(pushed, LF_ChessMoves.FindMove(pushed, "d7d5"));
		ctx.EqualStr("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 2", pushed.ToFen(), "a double push records the en passant square");

		// Pinned en passant: taking would expose the king along the fifth rank.
		LF_ChessBoard pinnedEp = Fen(ctx, "8/8/8/K2pP2r/8/8/8/4k3 w - d6 0 1");
		ctx.False(Has(pinnedEp, "e5d6"), "en passant that opens the king to a rook is refused");

		// Promotion: four choices, a bare move queens, black promotes too.
		LF_ChessBoard promo = Fen(ctx, "4k3/P7/8/8/8/8/p7/4K3 w - - 0 1");
		ctx.True(Has(promo, "a7a8q"), "promote to a queen");
		ctx.True(Has(promo, "a7a8r"), "promote to a rook");
		ctx.True(Has(promo, "a7a8b"), "promote to a bishop");
		ctx.True(Has(promo, "a7a8n"), "promote to a knight");
		ctx.False(Has(promo, "a7a8k"), "no promotion to a king");
		ctx.False(Has(promo, "a7a8p"), "no promotion to a pawn");
		int bare = LF_ChessMoves.FindMove(promo, "a7a8");
		ctx.EqualStr("a7a8q", LF_ChessMoves.MoveName(bare), "a promotion without a letter is a queen");
		ctx.EqualStr("Q3k3/8/8/8/8/8/p7/4K3 b - - 0 1", LF_ChessMoves.Apply(promo, bare).ToFen(), "the promoted queen is on the board");
		LF_ChessBoard blackPromo = Fen(ctx, "4k3/8/8/8/8/8/p7/4K3 b - - 0 1");
		int under = LF_ChessMoves.FindMove(blackPromo, "a2a1n");
		ctx.True(under != -1, "black promotes too");
		if (under != -1)
			ctx.EqualStr("4k3/8/8/8/8/8/8/n3K3 w - - 0 2", LF_ChessMoves.Apply(blackPromo, under).ToFen(), "black's knight is black");

		// A pawn is blocked by a piece in front and takes only diagonally.
		LF_ChessBoard pawn = Fen(ctx, "4k3/8/8/8/3p4/3P4/4P3/4K3 w - - 0 1");
		ctx.False(Has(pawn, "d3d4"), "a pawn cannot push into a piece");
		ctx.False(Has(pawn, "d3d5"), "a pawn cannot jump over a piece");
		ctx.True(Has(pawn, "e2e4"), "an unmoved pawn can double push");

		// A pinned piece cannot leave the line.
		LF_ChessBoard pin = Fen(ctx, "4r1k1/8/8/8/8/8/4N3/4K3 w - - 0 1");
		ctx.Equal(4, LF_ChessMoves.CountLegal(pin), "a pinned knight has no move, the king has four");
		ctx.False(Has(pin, "e2c3"), "the pinned knight stays");
		ctx.False(Has(pin, "e1e2"), "the king cannot step onto its own piece");

		// In check, only an answer to the check is legal.
		LF_ChessBoard check = Fen(ctx, "4k3/8/8/8/8/8/4r3/R3K3 w - - 0 1");
		ctx.True(LF_ChessMoves.InCheck(check), "the king on e1 is in check from e2");
		ctx.True(Has(check, "e1e2"), "the king can take the checking rook");
		ctx.False(Has(check, "a1a2"), "a move that leaves the check on is refused");

		// Each piece's own pattern on an open board (the white king sits in the h1 corner: 3 moves).
		ctx.Equal(8 + 3, LF_ChessMoves.CountLegal(Fen(ctx, "k7/8/8/8/3N4/8/8/7K w - - 0 1")), "a knight in the open has 8 moves");
		ctx.Equal(14 + 3, LF_ChessMoves.CountLegal(Fen(ctx, "k7/8/8/8/3R4/8/8/7K w - - 0 1")), "a rook in the open has 14 moves");
		ctx.Equal(13 + 3, LF_ChessMoves.CountLegal(Fen(ctx, "k7/8/8/8/3B4/8/8/7K w - - 0 1")), "a bishop in the open has 13 moves");
		ctx.Equal(27 + 3, LF_ChessMoves.CountLegal(Fen(ctx, "k7/8/8/8/3Q4/8/8/7K w - - 0 1")), "a queen in the open has 27 moves");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckEnd(LF_TestContext ctx)
	{
		LF_ChessBoard mate = Fen(ctx, "rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 1 3");
		ctx.True(LF_ChessMoves.IsCheckmate(mate), "the fool's mate is checkmate");
		ctx.False(LF_ChessMoves.IsStalemate(mate), "checkmate is not stalemate");
		ctx.Equal(0, LF_ChessMoves.CountLegal(mate), "no legal move in checkmate");

		LF_ChessBoard stale = Fen(ctx, "7k/5Q2/6K1/8/8/8/8/8 b - - 0 1");
		ctx.True(LF_ChessMoves.IsStalemate(stale), "king boxed in and not attacked is stalemate");
		ctx.False(LF_ChessMoves.IsCheckmate(stale), "stalemate is not checkmate");
		ctx.False(LF_ChessMoves.IsCheckmate(LF_ChessBoard.Start()), "the start is neither");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckPlay(LF_TestContext ctx)
	{
		// Scholar's mate, move by move through the same entry a client move would take.
		LF_ChessBoard board = LF_ChessBoard.Start();
		array<string> line = {"e2e4", "e7e5", "d1h5", "b8c6", "f1c4", "g8f6", "h5f7"};
		foreach (string name : line)
		{
			int move = LF_ChessMoves.FindMove(board, name);
			ctx.True(move != -1, "legal in the game: " + name);
			if (move == -1)
				return;
			board = LF_ChessMoves.Apply(board, move);
		}
		ctx.True(LF_ChessMoves.IsCheckmate(board), "scholar's mate ends the game");
		ctx.EqualStr("r1bqkb1r/pppp1Qpp/2n2n2/4p3/2B1P3/8/PPPP1PPP/RNB1K1NR b KQkq - 0 4", board.ToFen(), "the board after scholar's mate");

		LF_ChessBoard fresh = LF_ChessBoard.Start();
		ctx.Equal(-1, LF_ChessMoves.FindMove(fresh, "e2e5"), "a pawn cannot jump three");
		ctx.Equal(-1, LF_ChessMoves.FindMove(fresh, "e7e5"), "the wrong side cannot move");
		ctx.Equal(-1, LF_ChessMoves.FindMove(fresh, "zz"), "garbage is not a move");
		ctx.Equal(-1, LF_ChessMoves.FindMove(null, "e2e4"), "no board, no move");
		ctx.EqualStr(LF_ChessBoard.START_FEN, fresh.ToFen(), "Apply works on a copy");
		LF_ChessBoard bad = new LF_ChessBoard();
		ctx.False(bad.SetFen("not a fen"), "a malformed FEN is refused");
		ctx.False(bad.SetFen("8/8/8/8/8/8/8 w - - 0 1"), "seven ranks is refused");
	}
}
