//! One chess game from the first move to the end (#1933, chapter #1011 Evenings in the village).
//! Wraps an LF_ChessBoard: whose turn it is, a move list, check, and the four ways a game ends:
//! checkmate, stalemate, resignation (a draw by agreement and the repetition and fifty-move draws
//! are not here yet). Pure script, no entity, no replication: the table slice owns who may call it.
//!
//! A move is refused, never applied, when the game is over, the side is not on turn, or the move
//! name is not legal; every refusal logs through LF_Debug and returns false.
class LF_ChessGame
{
	static const int RESULT_ONGOING = 0;
	static const int RESULT_WHITE_WINS = 1;
	static const int RESULT_BLACK_WINS = 2;
	static const int RESULT_DRAW = 3;

	static const int END_NONE = 0;
	static const int END_CHECKMATE = 1;
	static const int END_STALEMATE = 2;
	static const int END_RESIGNATION = 3;

	protected ref LF_ChessBoard m_Board;
	protected ref array<string> m_aMoveNames = {};
	protected int m_iResult = RESULT_ONGOING;
	protected int m_iEnd = END_NONE;

	//------------------------------------------------------------------------------------------------
	void LF_ChessGame()
	{
		m_Board = LF_ChessBoard.Start();
	}

	//------------------------------------------------------------------------------------------------
	static LF_ChessGame FromFen(string fen)
	{
		LF_ChessGame game = new LF_ChessGame();
		if (!game.m_Board.SetFen(fen))
		{
			LF_Debug.Error("Chess", "bad FEN, game stays at the start position: " + fen);
			game.m_Board = LF_ChessBoard.Start();
			return game;
		}
		game.CheckEnd();
		return game;
	}

	//------------------------------------------------------------------------------------------------
	LF_ChessBoard GetBoard()
	{
		return m_Board;
	}

	bool IsWhiteToMove()
	{
		return m_Board.m_bWhiteToMove;
	}

	int GetResult()
	{
		return m_iResult;
	}

	int GetEnd()
	{
		return m_iEnd;
	}

	bool IsOver()
	{
		return m_iResult != RESULT_ONGOING;
	}

	//! The side to move is in check (false once the game is over by mate: that is IsCheckmate).
	bool IsCheck()
	{
		return LF_ChessMoves.InCheck(m_Board);
	}

	bool IsCheckmate()
	{
		return m_iEnd == END_CHECKMATE;
	}

	bool IsStalemate()
	{
		return m_iEnd == END_STALEMATE;
	}

	int MoveCount()
	{
		return m_aMoveNames.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! The moves played, long algebraic ("e2e4"), in order.
	void GetMoveNames(array<string> names)
	{
		names.Clear();
		foreach (string name : m_aMoveNames)
			names.Insert(name);
	}

	//------------------------------------------------------------------------------------------------
	//! "1. e2e4 e7e5 2. g1f3": the move list a sheet shows.
	string MoveListText()
	{
		string text = "";
		for (int i = 0; i < m_aMoveNames.Count(); i++)
		{
			if (text != "")
				text += " ";
			if (i % 2 == 0)
				text += ((i / 2) + 1).ToString() + ". ";
			text += m_aMoveNames[i];
		}
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Plays `name` ("e2e4", "e7e8q") for a side. False, and nothing changes, when the game is over,
	//! it is the other side's turn, or the move is not legal.
	bool TryMove(bool white, string name)
	{
		if (IsOver())
		{
			LF_Debug.Warn("Chess", "move refused, the game is over: " + name);
			return false;
		}
		if (white != m_Board.m_bWhiteToMove)
		{
			LF_Debug.Warn("Chess", "move refused, not this side's turn: " + name);
			return false;
		}
		int move = LF_ChessMoves.FindMove(m_Board, name);
		if (move == -1)
		{
			LF_Debug.Warn("Chess", "move refused, not legal: " + name);
			return false;
		}
		m_aMoveNames.Insert(LF_ChessMoves.MoveName(move));
		m_Board = LF_ChessMoves.Apply(m_Board, move);
		CheckEnd();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A side gives up and the other side wins. A side may resign off its turn. False when the game
	//! is already over.
	bool Resign(bool white)
	{
		if (IsOver())
		{
			LF_Debug.Warn("Chess", "resign refused, the game is over");
			return false;
		}
		m_iEnd = END_RESIGNATION;
		if (white)
			m_iResult = RESULT_BLACK_WINS;
		else
			m_iResult = RESULT_WHITE_WINS;
		LF_Debug.Info("Chess", "resignation, result " + m_iResult.ToString());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckEnd()
	{
		if (LF_ChessMoves.CountLegal(m_Board) > 0)
			return;
		if (LF_ChessMoves.InCheck(m_Board))
		{
			m_iEnd = END_CHECKMATE;
			// The side to move is mated, so the other side won.
			if (m_Board.m_bWhiteToMove)
				m_iResult = RESULT_BLACK_WINS;
			else
				m_iResult = RESULT_WHITE_WINS;
		}
		else
		{
			m_iEnd = END_STALEMATE;
			m_iResult = RESULT_DRAW;
		}
		LF_Debug.Info("Chess", "game over, end " + m_iEnd.ToString() + " result " + m_iResult.ToString());
	}
}
