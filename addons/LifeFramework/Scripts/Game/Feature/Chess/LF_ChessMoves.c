//! Chess rules over an LF_ChessBoard (#1931): every legal move for every piece, captures, castling,
//! en passant and promotion. Pure and static: no state, no entity, no replication, so the server can
//! validate a client's move with it and a test can walk the whole tree (perft).
//!
//! A move is one int: from (bits 0-5), to (6-11), promotion piece type (12-14, 0 for none) and the
//! LF_ChessMoves.FLAG_* bits (15 up). Not a Managed object, so a move list is a plain array<int>.
class LF_ChessMoves
{
	static const int FLAG_CAPTURE = 1;
	static const int FLAG_EN_PASSANT = 2;
	static const int FLAG_CASTLE_KING = 4;
	static const int FLAG_CASTLE_QUEEN = 8;
	static const int FLAG_DOUBLE_PUSH = 16;

	//------------------------------------------------------------------------------------------------
	static int Pack(int fromSq, int toSq, int promo, int flags)
	{
		return fromSq | (toSq << 6) | (promo << 12) | (flags << 15);
	}

	static int From(int move)
	{
		return move & 63;
	}

	static int To(int move)
	{
		return (move >> 6) & 63;
	}

	static int Promotion(int move)
	{
		return (move >> 12) & 7;
	}

	static int Flags(int move)
	{
		return move >> 15;
	}

	//------------------------------------------------------------------------------------------------
	//! "e2e4", "e7e8q": the long algebraic form a client sends and a sheet shows.
	static string MoveName(int move)
	{
		string name = LF_ChessBoard.SquareName(From(move)) + LF_ChessBoard.SquareName(To(move));
		int promo = Promotion(move);
		if (promo != 0)
			name += "pnbrqk".Substring(promo - 1, 1);
		return name;
	}

	//------------------------------------------------------------------------------------------------
	//! The legal move that matches "e2e4" / "e7e8q" (a bare "e7e8" is a queen promotion), or -1.
	static int FindMove(LF_ChessBoard board, string name)
	{
		if (!board || name.Length() < 4)
			return -1;
		int fromSq = LF_ChessBoard.SquareFromName(name.Substring(0, 2));
		int toSq = LF_ChessBoard.SquareFromName(name.Substring(2, 2));
		if (fromSq < 0 || toSq < 0)
			return -1;
		int promo = 0;
		if (name.Length() == 5)
		{
			promo = "pnbrqk".IndexOf(name.Substring(4, 1)) + 1;
			if (promo < LF_ChessBoard.KNIGHT || promo > LF_ChessBoard.QUEEN)
				return -1;
		}
		return FindMoveByParts(board, fromSq, toSq, promo);
	}

	//------------------------------------------------------------------------------------------------
	//! The legal move from `from` to `to`; promo 0 means a queen when the move promotes. -1 if none.
	static int FindMoveByParts(LF_ChessBoard board, int fromSq, int toSq, int promo)
	{
		if (!board)
			return -1;
		array<int> moves = {};
		GenerateLegal(board, moves);
		foreach (int move : moves)
		{
			if (From(move) != fromSq || To(move) != toSq)
				continue;
			int want = promo;
			if (Promotion(move) != 0 && want == 0)
				want = LF_ChessBoard.QUEEN;
			if (Promotion(move) == want)
				return move;
		}
		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! The side to move is in check.
	static bool InCheck(LF_ChessBoard board)
	{
		return SideInCheck(board, board.m_bWhiteToMove);
	}

	protected static bool SideInCheck(LF_ChessBoard board, bool white)
	{
		int king = board.FindKing(white);
		if (king < 0)
			return false;
		return IsAttacked(board, king, !white);
	}

	//------------------------------------------------------------------------------------------------
	static bool IsCheckmate(LF_ChessBoard board)
	{
		return InCheck(board) && CountLegal(board) == 0;
	}

	static bool IsStalemate(LF_ChessBoard board)
	{
		return !InCheck(board) && CountLegal(board) == 0;
	}

	static int CountLegal(LF_ChessBoard board)
	{
		array<int> moves = {};
		GenerateLegal(board, moves);
		return moves.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Nodes at `depth` plies: the standard move-generator check (docs: chessprogramming.org/Perft).
	static int Perft(LF_ChessBoard board, int depth)
	{
		if (depth <= 0)
			return 1;
		array<int> moves = {};
		GenerateLegal(board, moves);
		if (depth == 1)
			return moves.Count();
		int nodes = 0;
		foreach (int move : moves)
		{
			// A local keeps the child alive for the whole recursive call; a temporary argument does not.
			LF_ChessBoard child = Apply(board, move);
			nodes += Perft(child, depth - 1);
		}
		return nodes;
	}

	//------------------------------------------------------------------------------------------------
	//! Plays `move` on a copy and returns it; `board` is untouched. The move is not checked: pass one
	//! from GenerateLegal / FindMove.
	static LF_ChessBoard Apply(LF_ChessBoard board, int move)
	{
		LF_ChessBoard next = new LF_ChessBoard();
		ApplyOnto(board, move, next);
		return next;
	}

	//------------------------------------------------------------------------------------------------
	//! Apply into an existing board: `next` becomes `board` with `move` played (it must not be `board`).
	protected static void ApplyOnto(LF_ChessBoard board, int move, LF_ChessBoard next)
	{
		next.CopyFrom(board);
		int fromSq = From(move);
		int toSq = To(move);
		int flags = Flags(move);
		int piece = next.Get(fromSq);
		int type = LF_ChessBoard.TypeOf(piece);
		bool white = board.m_bWhiteToMove;
		int captured = next.Get(toSq);

		next.m_iEnPassant = -1;
		next.m_iHalfmove++;
		if (type == LF_ChessBoard.PAWN || captured != LF_ChessBoard.EMPTY)
			next.m_iHalfmove = 0;

		next.Set(fromSq, LF_ChessBoard.EMPTY);
		if (flags & FLAG_EN_PASSANT)
			next.Set(LF_ChessBoard.Square(LF_ChessBoard.FileOf(toSq), LF_ChessBoard.RankOf(fromSq)), LF_ChessBoard.EMPTY);

		int placed = piece;
		int promo = Promotion(move);
		if (promo != 0)
		{
			placed = promo;
			if (!white)
				placed = -promo;
		}
		next.Set(toSq, placed);

		if (flags & FLAG_CASTLE_KING)
		{
			next.Set(toSq + 1, LF_ChessBoard.EMPTY);
			next.Set(toSq - 1, LF_ChessBoard.ROOK);
			if (!white)
				next.Set(toSq - 1, -LF_ChessBoard.ROOK);
		}
		else if (flags & FLAG_CASTLE_QUEEN)
		{
			next.Set(toSq - 2, LF_ChessBoard.EMPTY);
			next.Set(toSq + 1, LF_ChessBoard.ROOK);
			if (!white)
				next.Set(toSq + 1, -LF_ChessBoard.ROOK);
		}
		if (flags & FLAG_DOUBLE_PUSH)
			next.m_iEnPassant = (fromSq + toSq) / 2;

		// A king or rook leaving its corner, or a rook taken on it, ends that right for good.
		next.m_iCastling = next.m_iCastling & (15 - (RightsLostOn(fromSq) | RightsLostOn(toSq)));

		if (!white)
			next.m_iFullmove++;
		next.m_bWhiteToMove = !white;
	}

	protected static int RightsLostOn(int sq)
	{
		switch (sq)
		{
			case 0: return LF_ChessBoard.CASTLE_WQ;
			case 4: return LF_ChessBoard.CASTLE_WK | LF_ChessBoard.CASTLE_WQ;
			case 7: return LF_ChessBoard.CASTLE_WK;
			case 56: return LF_ChessBoard.CASTLE_BQ;
			case 60: return LF_ChessBoard.CASTLE_BK | LF_ChessBoard.CASTLE_BQ;
			case 63: return LF_ChessBoard.CASTLE_BK;
		}
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Is `sq` attacked by a piece of the side `byWhite`? Works on an empty or occupied square (castling
	//! asks about empty ones; a king in check asks about its own).
	static bool IsAttacked(LF_ChessBoard board, int sq, bool byWhite)
	{
		int file = LF_ChessBoard.FileOf(sq);
		int rank = LF_ChessBoard.RankOf(sq);
		int sign = 1;
		if (!byWhite)
			sign = -1;

		// Pawns: a white pawn attacks up the board, so it sits one rank below the square.
		// Direct reads of the square array (the rank * 8 + file index Square() gives, bounds already checked here):
		// this runs for every trial move of every perft node, and a Get(Square()) pair per read cost a third of it.
		array<int> squares = board.m_aSquares;
		int pawnRank = rank - sign;
		if (pawnRank >= 0 && pawnRank <= 7)
		{
			if (file > 0 && squares[pawnRank * 8 + file - 1] == sign * LF_ChessBoard.PAWN)
				return true;
			if (file < 7 && squares[pawnRank * 8 + file + 1] == sign * LF_ChessBoard.PAWN)
				return true;
		}

		// Knights: (+-1, +-2) and (+-2, +-1).
		for (int i = 0; i < 8; i++)
		{
			int df = 1 - 2 * (i & 1);
			int dr = 2 - 4 * ((i >> 1) & 1);
			if ((i >> 2) & 1)
			{
				int swap = df;
				df = dr;
				dr = swap;
			}
			int nf = file + df;
			int nr = rank + dr;
			if (nf < 0 || nf > 7 || nr < 0 || nr > 7)
				continue;
			if (squares[nr * 8 + nf] == sign * LF_ChessBoard.KNIGHT)
				return true;
		}

		// The eight neighbours: a king next to it, or a slider along the line.
		for (int d = 0; d < 9; d++)
		{
			if (d == 4)
				continue;
			int sf = d % 3 - 1;
			int sr = d / 3 - 1;
			bool straight = sf == 0 || sr == 0;
			int cf = file + sf;
			int cr = rank + sr;
			bool first = true;
			while (cf >= 0 && cf <= 7 && cr >= 0 && cr <= 7)
			{
				int piece = squares[cr * 8 + cf];
				if (piece != LF_ChessBoard.EMPTY)
				{
					if (piece * sign > 0)
					{
						int type = piece;
						if (type < 0)
							type = -type;
						if (type == LF_ChessBoard.QUEEN)
							return true;
						if (first && type == LF_ChessBoard.KING)
							return true;
						if (straight && type == LF_ChessBoard.ROOK)
							return true;
						if (!straight && type == LF_ChessBoard.BISHOP)
							return true;
					}
					break;
				}
				first = false;
				cf += sf;
				cr += sr;
			}
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Every legal move for the side to move (own king never left attacked). Clears `moves` first.
	static void GenerateLegal(LF_ChessBoard board, array<int> moves)
	{
		moves.Clear();
		if (!board)
			return;
		array<int> pseudo = {};
		GeneratePseudo(board, pseudo);
		bool white = board.m_bWhiteToMove;
		// One scratch board for every trial move: only the check test reads it, nothing keeps it.
		LF_ChessBoard next = new LF_ChessBoard();
		foreach (int move : pseudo)
		{
			ApplyOnto(board, move, next);
			if (!SideInCheck(next, white))
				moves.Insert(move);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void GeneratePseudo(LF_ChessBoard board, array<int> moves)
	{
		bool white = board.m_bWhiteToMove;
		int sign = 1;
		if (!white)
			sign = -1;

		for (int sq = 0; sq < 64; sq++)
		{
			int piece = board.Get(sq);
			if (piece * sign <= 0)
				continue;
			int type = LF_ChessBoard.TypeOf(piece);
			if (type == LF_ChessBoard.PAWN)
				AddPawnMoves(board, sq, sign, moves);
			else if (type == LF_ChessBoard.KNIGHT)
				AddKnightMoves(board, sq, sign, moves);
			else if (type == LF_ChessBoard.KING)
				AddKingMoves(board, sq, sign, moves);
			else
				AddSliderMoves(board, sq, type, sign, moves);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void AddPawnMoves(LF_ChessBoard board, int sq, int sign, array<int> moves)
	{
		int file = LF_ChessBoard.FileOf(sq);
		int rank = LF_ChessBoard.RankOf(sq);
		int startRank = 1;
		int lastRank = 7;
		if (sign < 0)
		{
			startRank = 6;
			lastRank = 0;
		}
		int nr = rank + sign;
		if (nr < 0 || nr > 7)
			return;

		int ahead = LF_ChessBoard.Square(file, nr);
		if (board.Get(ahead) == LF_ChessBoard.EMPTY)
		{
			AddPawnMove(sq, ahead, nr == lastRank, 0, moves);
			if (rank == startRank)
			{
				int twoAhead = LF_ChessBoard.Square(file, nr + sign);
				if (board.Get(twoAhead) == LF_ChessBoard.EMPTY)
					moves.Insert(Pack(sq, twoAhead, 0, FLAG_DOUBLE_PUSH));
			}
		}

		for (int side = -1; side <= 1; side += 2)
		{
			int cf = file + side;
			if (cf < 0 || cf > 7)
				continue;
			int target = LF_ChessBoard.Square(cf, nr);
			int occupant = board.Get(target);
			if (occupant * sign < 0)
				AddPawnMove(sq, target, nr == lastRank, FLAG_CAPTURE, moves);
			else if (occupant == LF_ChessBoard.EMPTY && target == board.m_iEnPassant)
			{
				// The pawn to take stands beside this one, on this rank, in the target's file.
				int victim = board.Get(LF_ChessBoard.Square(cf, rank));
				if (victim == -sign * LF_ChessBoard.PAWN)
					moves.Insert(Pack(sq, target, 0, FLAG_CAPTURE | FLAG_EN_PASSANT));
			}
		}
	}

	protected static void AddPawnMove(int fromSq, int toSq, bool promotes, int flags, array<int> moves)
	{
		if (!promotes)
		{
			moves.Insert(Pack(fromSq, toSq, 0, flags));
			return;
		}
		moves.Insert(Pack(fromSq, toSq, LF_ChessBoard.QUEEN, flags));
		moves.Insert(Pack(fromSq, toSq, LF_ChessBoard.ROOK, flags));
		moves.Insert(Pack(fromSq, toSq, LF_ChessBoard.BISHOP, flags));
		moves.Insert(Pack(fromSq, toSq, LF_ChessBoard.KNIGHT, flags));
	}

	//------------------------------------------------------------------------------------------------
	protected static void AddStep(LF_ChessBoard board, int sq, int nf, int nr, int sign, array<int> moves)
	{
		if (nf < 0 || nf > 7 || nr < 0 || nr > 7)
			return;
		int target = LF_ChessBoard.Square(nf, nr);
		int occupant = board.Get(target);
		if (occupant == LF_ChessBoard.EMPTY)
			moves.Insert(Pack(sq, target, 0, 0));
		else if (occupant * sign < 0)
			moves.Insert(Pack(sq, target, 0, FLAG_CAPTURE));
	}

	protected static void AddKnightMoves(LF_ChessBoard board, int sq, int sign, array<int> moves)
	{
		int file = LF_ChessBoard.FileOf(sq);
		int rank = LF_ChessBoard.RankOf(sq);
		for (int i = 0; i < 8; i++)
		{
			int df = 1 - 2 * (i & 1);
			int dr = 2 - 4 * ((i >> 1) & 1);
			if ((i >> 2) & 1)
			{
				int swap = df;
				df = dr;
				dr = swap;
			}
			AddStep(board, sq, file + df, rank + dr, sign, moves);
		}
	}

	protected static void AddKingMoves(LF_ChessBoard board, int sq, int sign, array<int> moves)
	{
		int file = LF_ChessBoard.FileOf(sq);
		int rank = LF_ChessBoard.RankOf(sq);
		for (int d = 0; d < 9; d++)
		{
			if (d == 4)
				continue;
			AddStep(board, sq, file + d % 3 - 1, rank + d / 3 - 1, sign, moves);
		}
		AddCastling(board, sq, sign, moves);
	}

	//------------------------------------------------------------------------------------------------
	//! Castling: the right is still held, king and rook are home, the squares between are empty, and the
	//! king is not in check, does not cross an attacked square and does not land on one (the landing
	//! square is also caught by the legality filter).
	protected static void AddCastling(LF_ChessBoard board, int sq, int sign, array<int> moves)
	{
		bool white = sign > 0;
		int home = 4;
		int kingRight = LF_ChessBoard.CASTLE_WK;
		int queenRight = LF_ChessBoard.CASTLE_WQ;
		if (!white)
		{
			home = 60;
			kingRight = LF_ChessBoard.CASTLE_BK;
			queenRight = LF_ChessBoard.CASTLE_BQ;
		}
		if (sq != home || board.Get(sq) != sign * LF_ChessBoard.KING)
			return;
		if (!(board.m_iCastling & (kingRight | queenRight)))
			return;
		if (IsAttacked(board, sq, !white))
			return;

		if ((board.m_iCastling & kingRight) && board.Get(sq + 3) == sign * LF_ChessBoard.ROOK
			&& board.Get(sq + 1) == LF_ChessBoard.EMPTY && board.Get(sq + 2) == LF_ChessBoard.EMPTY
			&& !IsAttacked(board, sq + 1, !white) && !IsAttacked(board, sq + 2, !white))
		{
			moves.Insert(Pack(sq, sq + 2, 0, FLAG_CASTLE_KING));
		}
		if ((board.m_iCastling & queenRight) && board.Get(sq - 4) == sign * LF_ChessBoard.ROOK
			&& board.Get(sq - 1) == LF_ChessBoard.EMPTY && board.Get(sq - 2) == LF_ChessBoard.EMPTY
			&& board.Get(sq - 3) == LF_ChessBoard.EMPTY
			&& !IsAttacked(board, sq - 1, !white) && !IsAttacked(board, sq - 2, !white))
		{
			moves.Insert(Pack(sq, sq - 2, 0, FLAG_CASTLE_QUEEN));
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void AddSliderMoves(LF_ChessBoard board, int sq, int type, int sign, array<int> moves)
	{
		int file = LF_ChessBoard.FileOf(sq);
		int rank = LF_ChessBoard.RankOf(sq);
		for (int d = 0; d < 9; d++)
		{
			if (d == 4)
				continue;
			int sf = d % 3 - 1;
			int sr = d / 3 - 1;
			bool straight = sf == 0 || sr == 0;
			if (type == LF_ChessBoard.ROOK && !straight)
				continue;
			if (type == LF_ChessBoard.BISHOP && straight)
				continue;
			int cf = file + sf;
			int cr = rank + sr;
			while (cf >= 0 && cf <= 7 && cr >= 0 && cr <= 7)
			{
				int target = LF_ChessBoard.Square(cf, cr);
				int occupant = board.Get(target);
				if (occupant == LF_ChessBoard.EMPTY)
					moves.Insert(Pack(sq, target, 0, 0));
				else
				{
					if (occupant * sign < 0)
						moves.Insert(Pack(sq, target, 0, FLAG_CAPTURE));
					break;
				}
				cf += sf;
				cr += sr;
			}
		}
	}
}
