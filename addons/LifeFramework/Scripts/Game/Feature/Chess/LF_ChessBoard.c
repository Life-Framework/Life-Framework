//! A pure chess position (#1931, chapter #1011 Evenings in the village). No entity, no world, no
//! replication: 64 squares, the side to move, castling rights, the en passant square and the
//! clocks. Rules live in LF_ChessMoves; a later slice owns the table, the pieces and the sheet.
//!
//! Squares are 0..63, square = rank * 8 + file, a1 = 0, h1 = 7, a8 = 56, h8 = 63. A piece is a
//! signed int: the type 1..6 is positive for white and negative for black, 0 is an empty square.
class LF_ChessBoard
{
	static const int EMPTY = 0;
	static const int PAWN = 1;
	static const int KNIGHT = 2;
	static const int BISHOP = 3;
	static const int ROOK = 4;
	static const int QUEEN = 5;
	static const int KING = 6;

	static const int CASTLE_WK = 1;
	static const int CASTLE_WQ = 2;
	static const int CASTLE_BK = 4;
	static const int CASTLE_BQ = 8;

	static const string START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

	ref array<int> m_aSquares = new array<int>();
	bool m_bWhiteToMove = true;
	int m_iCastling;
	int m_iEnPassant = -1;
	int m_iHalfmove;
	int m_iFullmove = 1;

	//------------------------------------------------------------------------------------------------
	void LF_ChessBoard()
	{
		for (int i = 0; i < 64; i++)
			m_aSquares.Insert(EMPTY);
	}

	//------------------------------------------------------------------------------------------------
	static LF_ChessBoard Start()
	{
		LF_ChessBoard board = new LF_ChessBoard();
		board.SetFen(START_FEN);
		return board;
	}

	//------------------------------------------------------------------------------------------------
	static int Square(int file, int rank)
	{
		return rank * 8 + file;
	}

	//------------------------------------------------------------------------------------------------
	static int FileOf(int sq)
	{
		return sq & 7;
	}

	//------------------------------------------------------------------------------------------------
	static int RankOf(int sq)
	{
		return sq >> 3;
	}

	//------------------------------------------------------------------------------------------------
	//! "e4" to 28, or -1 for anything that is not a square.
	static int SquareFromName(string name)
	{
		if (name.Length() != 2)
			return -1;
		int file = "abcdefgh".IndexOf(name.Substring(0, 1));
		int rank = "12345678".IndexOf(name.Substring(1, 1));
		if (file < 0 || rank < 0)
			return -1;
		return Square(file, rank);
	}

	//------------------------------------------------------------------------------------------------
	static string SquareName(int sq)
	{
		if (sq < 0 || sq > 63)
			return "-";
		return "abcdefgh".Substring(FileOf(sq), 1) + "12345678".Substring(RankOf(sq), 1);
	}

	//------------------------------------------------------------------------------------------------
	static int TypeOf(int piece)
	{
		if (piece < 0)
			return -piece;
		return piece;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsWhite(int piece)
	{
		return piece > 0;
	}

	//------------------------------------------------------------------------------------------------
	int Get(int sq)
	{
		if (sq < 0 || sq > 63)
			return EMPTY;
		return m_aSquares[sq];
	}

	//------------------------------------------------------------------------------------------------
	void Set(int sq, int piece)
	{
		if (sq < 0 || sq > 63)
			return;
		m_aSquares[sq] = piece;
	}

	//------------------------------------------------------------------------------------------------
	//! The square of a side's king, or -1 when the position has none (a malformed FEN).
	int FindKing(bool white)
	{
		int king = KING;
		if (!white)
			king = -KING;
		for (int i = 0; i < 64; i++)
		{
			if (m_aSquares[i] == king)
				return i;
		}
		return -1;
	}

	//------------------------------------------------------------------------------------------------
	LF_ChessBoard Copy()
	{
		LF_ChessBoard copy = new LF_ChessBoard();
		for (int i = 0; i < 64; i++)
			copy.m_aSquares[i] = m_aSquares[i];
		copy.m_bWhiteToMove = m_bWhiteToMove;
		copy.m_iCastling = m_iCastling;
		copy.m_iEnPassant = m_iEnPassant;
		copy.m_iHalfmove = m_iHalfmove;
		copy.m_iFullmove = m_iFullmove;
		return copy;
	}

	//------------------------------------------------------------------------------------------------
	//! Loads a FEN string. False (and an empty board) when it is malformed: the caller decides what
	//! a bad position means; nothing here throws.
	bool SetFen(string fen)
	{
		array<string> parts = {};
		fen.Split(" ", parts, true);
		if (parts.Count() < 2)
			return false;

		for (int i = 0; i < 64; i++)
			m_aSquares[i] = EMPTY;
		m_bWhiteToMove = true;
		m_iCastling = 0;
		m_iEnPassant = -1;
		m_iHalfmove = 0;
		m_iFullmove = 1;

		array<string> ranks = {};
		parts[0].Split("/", ranks, true);
		if (ranks.Count() != 8)
			return Reject();

		string whiteLetters = "PNBRQK";
		string blackLetters = "pnbrqk";
		for (int r = 0; r < 8; r++)
		{
			string row = ranks[r];
			int rank = 7 - r;
			int file = 0;
			for (int c = 0; c < row.Length(); c++)
			{
				string ch = row.Substring(c, 1);
				int skip = "12345678".IndexOf(ch);
				if (skip >= 0)
				{
					file += skip + 1;
					continue;
				}
				int w = whiteLetters.IndexOf(ch);
				int b = blackLetters.IndexOf(ch);
				if (file > 7 || (w < 0 && b < 0))
					return Reject();
				if (w >= 0)
					m_aSquares[Square(file, rank)] = w + 1;
				else
					m_aSquares[Square(file, rank)] = -(b + 1);
				file++;
			}
			if (file != 8)
				return Reject();
		}

		if (parts[1] == "b")
			m_bWhiteToMove = false;
		else if (parts[1] != "w")
			return Reject();

		if (parts.Count() > 2)
		{
			string rights = parts[2];
			if (rights.IndexOf("K") >= 0)
				m_iCastling = m_iCastling | CASTLE_WK;
			if (rights.IndexOf("Q") >= 0)
				m_iCastling = m_iCastling | CASTLE_WQ;
			if (rights.IndexOf("k") >= 0)
				m_iCastling = m_iCastling | CASTLE_BK;
			if (rights.IndexOf("q") >= 0)
				m_iCastling = m_iCastling | CASTLE_BQ;
		}
		if (parts.Count() > 3 && parts[3] != "-")
		{
			m_iEnPassant = SquareFromName(parts[3]);
			if (m_iEnPassant < 0)
				return Reject();
		}
		if (parts.Count() > 4)
			m_iHalfmove = parts[4].ToInt();
		if (parts.Count() > 5)
			m_iFullmove = parts[5].ToInt();
		if (m_iFullmove < 1)
			m_iFullmove = 1;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool Reject()
	{
		for (int i = 0; i < 64; i++)
			m_aSquares[i] = EMPTY;
		m_iCastling = 0;
		m_iEnPassant = -1;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	string ToFen()
	{
		string whiteLetters = "PNBRQK";
		string blackLetters = "pnbrqk";
		string fen = "";
		for (int rank = 7; rank >= 0; rank--)
		{
			int empty = 0;
			for (int file = 0; file < 8; file++)
			{
				int piece = m_aSquares[Square(file, rank)];
				if (piece == EMPTY)
				{
					empty++;
					continue;
				}
				if (empty > 0)
				{
					fen += empty.ToString();
					empty = 0;
				}
				if (piece > 0)
					fen += whiteLetters.Substring(piece - 1, 1);
				else
					fen += blackLetters.Substring(-piece - 1, 1);
			}
			if (empty > 0)
				fen += empty.ToString();
			if (rank > 0)
				fen += "/";
		}

		if (m_bWhiteToMove)
			fen += " w ";
		else
			fen += " b ";

		string rights = "";
		if (m_iCastling & CASTLE_WK)
			rights += "K";
		if (m_iCastling & CASTLE_WQ)
			rights += "Q";
		if (m_iCastling & CASTLE_BK)
			rights += "k";
		if (m_iCastling & CASTLE_BQ)
			rights += "q";
		if (rights == "")
			rights = "-";
		fen += rights + " " + SquareName(m_iEnPassant) + " " + m_iHalfmove.ToString() + " " + m_iFullmove.ToString();
		return fen;
	}
}
