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

### `LF_ChessGame` — `Feature/Chess/` (#1933, chapter #1011)
- ✅ One game from the first move to the end, still pure script. `LF_ChessGame` wraps an
  `LF_ChessBoard`: `TryMove(white, "e2e4")` refuses (logs, changes nothing) a move out of turn, an
  illegal or malformed move, and any move once the game is over. It keeps the move list
  (`GetMoveNames`, `MoveListText` as `1. e2e4 e7e5 2. ...`), reports `IsCheck`, and ends on
  checkmate (the side that mated wins), stalemate (a draw) or `Resign(white)` (the other side wins,
  allowed off turn). `FromFen` loads a position and a game loaded already mated or stalemated is
  over at once. Result is `GetResult()` (`RESULT_*`) and the reason `GetEnd()` (`END_*`).
  Tested by `LF_Test_ChessGame`: fool's mate is mate for black, `Qf7` into the corner is a stalemate
  draw, an out-of-turn move is refused, resignation gives the other side the game. Not yet: a
  draw by agreement, repetition or the fifty-move rule, a clock, and the table that seats players.

### `LF_ChessTableComponent` and `LF_ChessScreen` — `Feature/Chess/` (#1934, chapter #1011)
- 🧪 A chess table two players sit at and play. `Prefabs/Props/Chess/LF_ChessTable.et` (a base-game
  small table) carries `LF_ChessTableComponent` and the "Play chess" action. The server holds the
  only game (`LF_ChessTable`: seats plus an `LF_ChessGame`): the first player to sit plays white, the
  second black, anyone after that watches. The board is a UI-kit sheet of eight KEYS rows; click a
  piece of your colour, then where it goes (a pawn on the last rank becomes a queen). The move goes to
  the server as `RpcAsk_LF_ChessMove` on the player controller, which takes the player from the
  controller and refuses a watcher, the wrong seat, out of turn, illegal moves and askers farther than
  6 m. The position reaches every machine, late joiners included, as a FEN string in an `RplProp`
  with the seats, result and last move. Closing the board, walking more than 4 m away or leaving the
  server holds the seat for 30 s; after that the absent side resigns. Sitting at a finished game sets
  up a new one. One table stands in GridWorld cell `chess-table`.
  Tested by `LF_Test_ChessTable`. Not yet: a 3D board and pieces on the table, a sit pose on stools,
  a draw offer, clocks, and the grace period as a server setting.
