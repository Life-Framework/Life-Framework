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
  (`MoveListText` as `1. e2e4 e7e5 2. ...`), reports `IsCheck`, and ends on
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

### Chess board layout — `LF_ChessScreen` (#2099, #2056, chapter #1011)
- 🧪 Every one of the 64 keys shows its own label centred, the a-file and the h-file included. The kit
  key is the pause-menu button (40 px of left padding and a 64 px arrow glyph before the label), which
  pushed each label one key to the right in a key one eighth of the sheet wide. `LF_ChessScreen.FitKey`
  drops the padding and the glyph, centres the label and sets a 40 px height; the board draws on its own
  640 x 760 sheet (`BoardSkin`) so the last-move line and Stand up show without a scroll at 1280x720.
  The kit itself is untouched (the row-width fix for other screens is #2080).
  Tested by `LF_Test_ChessScreenLayout` (red with the fit skipped: 50 px padding, glyph present).
  Not yet seen in a session picture: the grid-world session did not start on the pool (see the PR).

### Chess griefing fixes (#2405, life review 2026-11-17)
- ✅ Chess asks (move, stand up) have their own rate bucket, `LF_ERequestKind.CHESS` (6 per 5 s, `LF_ChessTableComponent.AllowAsk`), so a game no longer eats the phone's COMMS bucket and a phone call no longer blocks a move. Test `chess/rate-bucket`.
- ✅ A bystander cannot wipe a finished game. With both seats held and the game over, `LF_ChessTable.Sit` refuses a third player (they watch) until `RESULT_HOLD_MS` (60 s) after the last move; the two players keep their seats, and a free seat still starts a new game for a newcomer. A refused sit no longer re-publishes the table's RplProps. Test `chess/result-hold`.

### Play for a stake — `LF_ChessStake` (#3196, chapter #1011)
- 🧪 The pub chess table has a reason to sit down. The table offers "Play chess" (free) plus "Play for $20" and "Play for $50"
  (`LF_ChessSitAction.m_iStake`, set on the prefab; the tiers are `LF_ChessStake.STAKE_LOW` and `STAKE_HIGH`, from config in a
  follow-up). The first player sits at a tier; the second sits only by paying the same stake and carrying the cash. Each stake
  leaves carried cash into server-side escrow (`LF_ChessStake`, ledger reason `chess_stake`); the client sends nothing but the
  action it used. The winner of a mate is paid the pot (2x the stake). A player who leaves or stands up mid-game and does not
  come back inside the grace period forfeits: the opponent wins the pot. A draw, both players away, a table deleted at world end,
  and a stand-up before the second seat paid all refund (reason `chess_refund`). A crash with stakes in escrow refunds them into
  the payers' bank accounts once on the next boot (save section `chess_escrow`, `LF_ChessStakeSavePayload`). A staked sit
  counts against the chess rate limit.
  A refund for a player with no body goes to their bank account by uid. Every ending drains the escrow, so a second ending or a
  refund after a payout pays nothing. A seat the cash cannot cover, or whose stake differs from the other seat's, is refused and
  nothing moves; a free sit at a staked table only watches.
  Tested by `LF_Test_ChessStake` (rules, charge, pot, refunds, conservation of cash). Not yet: stakes from a config file, the stake
  shown on the board sheet, the pack's own stakes.
