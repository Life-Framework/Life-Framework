// red-proof: new test, no failing state to restore. `tools\cli test --no-build --red LF_Test_ChessGame`
// skips the moves behind the RedProof() hook (2026-11-16). Observed:
//   RED-OK chess/game (fails with the hook on)
//   red: the move list reads in order (expected '1. f2f3 e7e5 2. g2g4 d8h4', got '')

// tier: LOGIC
//! #1933: a chess game wraps the board. Fool's mate ends in mate for black, a queen move into a
//! known stalemate ends drawn, a move out of turn is refused, resignation hands the other side the
//! game, and nothing is playable once a game is over.
class LF_Test_ChessGame : LF_Test
{
	override string GetName()
	{
		return "chess/game";
	}

	override void Run(LF_TestContext ctx)
	{
		CheckFoolsMate(ctx);
		CheckStalemate(ctx);
		CheckRefusals(ctx);
		CheckResign(ctx);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckFoolsMate(LF_TestContext ctx)
	{
		LF_ChessGame game = new LF_ChessGame();
		ctx.Equal(LF_ChessGame.RESULT_ONGOING, game.GetResult(), "a new game is ongoing");
		ctx.True(game.IsWhiteToMove(), "white moves first");

		bool played = true;
		if (!RedProof())
		{
			played = game.TryMove(true, "f2f3") && played;
			played = game.TryMove(false, "e7e5") && played;
			played = game.TryMove(true, "g2g4") && played;
			ctx.False(game.IsCheck(), "no check before the mating move");
			played = game.TryMove(false, "d8h4") && played;
		}
		ctx.True(played, "the four fool's mate moves are all accepted");
		ctx.Equal(LF_ChessGame.RESULT_BLACK_WINS, game.GetResult(), "fool's mate is a win for black");
		ctx.Equal(LF_ChessGame.END_CHECKMATE, game.GetEnd(), "fool's mate ends in checkmate");
		ctx.True(game.IsCheckmate(), "IsCheckmate is true");
		ctx.True(game.IsCheck(), "white is in check");
		ctx.True(game.IsOver(), "the game is over");
		ctx.Equal(4, game.MoveCount(), "four moves were played");
		ctx.EqualStr("1. f2f3 e7e5 2. g2g4 d8h4", game.MoveListText(), "the move list reads in order");
		ctx.False(game.TryMove(true, "e2e4"), "no move after mate");
		ctx.Equal(4, game.MoveCount(), "a refused move is not recorded");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckStalemate(LF_TestContext ctx)
	{
		// White Kg6, Qf2, black Kh8: Qf7 leaves black no move and no check.
		LF_ChessGame game = LF_ChessGame.FromFen("7k/8/6K1/8/8/8/5Q2/8 w - - 0 1");
		ctx.Equal(LF_ChessGame.RESULT_ONGOING, game.GetResult(), "the setup position is ongoing");
		ctx.True(game.TryMove(true, "f2f7"), "Qf7 is legal");
		ctx.Equal(LF_ChessGame.RESULT_DRAW, game.GetResult(), "stalemate is a draw");
		ctx.Equal(LF_ChessGame.END_STALEMATE, game.GetEnd(), "the game ends in stalemate");
		ctx.True(game.IsStalemate(), "IsStalemate is true");
		ctx.False(game.IsCheck(), "stalemate is not check");
		ctx.False(game.IsCheckmate(), "stalemate is not mate");
		ctx.False(game.TryMove(false, "h8g8"), "no move after stalemate");

		// A game loaded already mated or stalemated is over at once.
		LF_ChessGame mated = LF_ChessGame.FromFen("rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 1 3");
		ctx.Equal(LF_ChessGame.RESULT_BLACK_WINS, mated.GetResult(), "a mate loaded from FEN is over");
		LF_ChessGame still = LF_ChessGame.FromFen("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1");
		ctx.Equal(LF_ChessGame.RESULT_DRAW, still.GetResult(), "a stalemate loaded from FEN is a draw");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckRefusals(LF_TestContext ctx)
	{
		LF_ChessGame game = new LF_ChessGame();
		ctx.False(game.TryMove(false, "e7e5"), "black cannot move first");
		ctx.Equal(0, game.MoveCount(), "an out-of-turn move is not recorded");
		ctx.True(game.IsWhiteToMove(), "an out-of-turn move leaves white on turn");
		ctx.False(game.TryMove(true, "e2e5"), "an illegal move is refused");
		ctx.False(game.TryMove(true, "zz"), "a malformed move is refused");
		ctx.True(game.TryMove(true, "e2e4"), "e4 is accepted");
		ctx.False(game.TryMove(true, "d2d4"), "white cannot move twice");
		ctx.Equal(1, game.MoveCount(), "only the legal move was recorded");
		ctx.False(game.IsWhiteToMove(), "black is now on turn");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckResign(LF_TestContext ctx)
	{
		LF_ChessGame game = new LF_ChessGame();
		ctx.True(game.TryMove(true, "e2e4"), "e4 is accepted");
		ctx.True(game.Resign(true), "white resigns while black is on turn");
		ctx.Equal(LF_ChessGame.RESULT_BLACK_WINS, game.GetResult(), "white resigning gives black the game");
		ctx.Equal(LF_ChessGame.END_RESIGNATION, game.GetEnd(), "the end is a resignation");
		ctx.False(game.TryMove(false, "e7e5"), "no move after resignation");
		ctx.False(game.Resign(false), "a finished game cannot be resigned again");
		ctx.Equal(LF_ChessGame.RESULT_BLACK_WINS, game.GetResult(), "the result stands");

		LF_ChessGame other = new LF_ChessGame();
		ctx.True(other.Resign(false), "black resigns on white's turn");
		ctx.Equal(LF_ChessGame.RESULT_WHITE_WINS, other.GetResult(), "black resigning gives white the game");
	}
}
