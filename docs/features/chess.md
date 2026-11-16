# Chess

### `LF_ChessBoard` and `LF_ChessMoves` — `Feature/Chess/` (#1931, chapter #1011)
- ✅ A pure chess position and its rules, no entity and no replication yet. `LF_ChessBoard` holds
  64 squares (a1 = 0, h8 = 63, signed piece ints), the side to move, castling rights, the en passant
  square and the clocks, and reads and writes FEN. `LF_ChessMoves` generates every legal move for
  every piece: captures, castling (refused out of, through and into check, and after a king or rook
  has moved or the rook was taken), en passant (refused when it would expose the king) and promotion
  to queen, rook, bishop or knight. `FindMove(board, "e2e4")` is the entry a server validates a
  client move with; `Apply` plays a move on a copy; `IsCheckmate` and `IsStalemate` end a game.
  A move is one int, so a move list is a plain `array<int>`.
  Tested by `LF_Test_ChessMoves`: the start position has 20 moves, perft 400 and 8902, and four
  published perft positions agree to depth 3. Not yet: a table, pieces in the world, a sheet, clocks,
  draw by repetition or the fifty-move rule, and a Systems switch (the catalog entry is `NOT_YET`).
