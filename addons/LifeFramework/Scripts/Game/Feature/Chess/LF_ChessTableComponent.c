// LF_ChessTableComponent.c — a chess table with two seats that holds one server-side game (#1934,
// chapter #1011 Evenings in the village). Built on LF_ChessGame (#1933).
//
// The server is the only machine with a game. Sitting takes the free seat (white first), a move is
// a client ask through the player controller (RpcAsk_LF_ChessMove) that the server checks against
// the asker's seat, the turn and the rules, and the position goes to every machine as a FEN string
// in an RplProp (so late joiners get it from the snapshot). Standing up, walking away or leaving the
// server starts a grace period; a player who does not sit again within it resigns.
//
//   LF_ChessTable            the seats and the game, pure script (the test drives it with a clock)
//   LF_ChessTableComponent   the table entity: replicated state, the server tick, the asks
//   LF_ChessSitAction        "Play chess" on the table: sit (or watch when both seats are taken)
//   SCR_PlayerController     the asks to the server and the board opening on the owner

//------------------------------------------------------------------------------------------------
//! The seats and the game of one table. No entity, no replication: the owner passes the clock.
class LF_ChessTable
{
	static const string FEATURE = "Chess";
	static const int SEAT_NONE = -1;
	static const int SEAT_WHITE = 0;
	static const int SEAT_BLACK = 1;
	//! A seated player who stood up keeps the seat this long; then the game is resigned for them.
	static const int GRACE_MS = 30000; // lf-fixed: table etiquette, long enough to fetch a drink; a server setting is a follow-up
	//! A finished game stays on the board for its two players this long; then a third player may clear it.
	static const int RESULT_HOLD_MS = 60000; // lf-fixed: long enough to read the result and ask for a rematch; a server setting is a follow-up

	protected ref LF_ChessGame m_Game = new LF_ChessGame();
	protected int m_iWhiteId;
	protected int m_iBlackId;
	protected int m_iWhiteLeftMs = -1;
	protected int m_iBlackLeftMs = -1;
	protected int m_iOverMs = -1;

	//------------------------------------------------------------------------------------------------
	LF_ChessGame GetGame()
	{
		return m_Game;
	}

	int GetWhiteId()
	{
		return m_iWhiteId;
	}

	int GetBlackId()
	{
		return m_iBlackId;
	}

	//------------------------------------------------------------------------------------------------
	int SeatOf(int playerId)
	{
		if (playerId <= 0)
			return SEAT_NONE;
		if (playerId == m_iWhiteId)
			return SEAT_WHITE;
		if (playerId == m_iBlackId)
			return SEAT_BLACK;
		return SEAT_NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! True while the player has stood up and the grace period runs.
	bool IsAway(int playerId)
	{
		int seat = SeatOf(playerId);
		if (seat == SEAT_WHITE)
			return m_iWhiteLeftMs >= 0;
		if (seat == SEAT_BLACK)
			return m_iBlackLeftMs >= 0;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Takes a seat: the player's own seat again (ends a grace period), else white, else black.
	//! SEAT_NONE when both seats are held by others. A finished game is cleared for a new one first,
	//! but not while both seats are still held by its players: a bystander must not wipe their result
	//! (#2405); once RESULT_HOLD_MS has passed since the last move, anyone may.
	int Sit(int playerId, int nowMs)
	{
		if (playerId <= 0)
		{
			LF_Debug.Warn(FEATURE, "sit refused: no player");
			return SEAT_NONE;
		}

		int seat = SeatOf(playerId);
		if (seat == SEAT_WHITE)
		{
			m_iWhiteLeftMs = -1;
			LF_Debug.Info(FEATURE, string.Format("player %1 back in the white seat", playerId));
			return seat;
		}
		if (seat == SEAT_BLACK)
		{
			m_iBlackLeftMs = -1;
			LF_Debug.Info(FEATURE, string.Format("player %1 back in the black seat", playerId));
			return seat;
		}

		if (m_Game.IsOver())
		{
			if (m_iWhiteId != 0 && m_iBlackId != 0 && !ResultHoldOver(nowMs))
			{
				LF_Debug.Info(FEATURE, string.Format("player %1 found a finished game held by its players; watching", playerId));
				return SEAT_NONE;
			}

			NewGame();
		}

		if (m_iWhiteId == 0)
		{
			m_iWhiteId = playerId;
			m_iWhiteLeftMs = -1;
			LF_Debug.Info(FEATURE, string.Format("player %1 sits as white", playerId));
			return SEAT_WHITE;
		}
		if (m_iBlackId == 0)
		{
			m_iBlackId = playerId;
			m_iBlackLeftMs = -1;
			LF_Debug.Info(FEATURE, string.Format("player %1 sits as black", playerId));
			return SEAT_BLACK;
		}

		LF_Debug.Info(FEATURE, string.Format("player %1 found both seats taken; watching", playerId));
		return SEAT_NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! A seated player gets up. Before both seats are filled, or after the game ended, the seat is
	//! free at once; during a game it is held for GRACE_MS. False when the player was not seated.
	bool StandUp(int playerId, int nowMs)
	{
		int seat = SeatOf(playerId);
		if (seat == SEAT_NONE)
			return false;

		if (!HasGameRunning())
		{
			FreeSeat(seat);
			LF_Debug.Info(FEATURE, string.Format("player %1 stood up; no game running, seat free", playerId));
			return true;
		}

		if (seat == SEAT_WHITE && m_iWhiteLeftMs < 0)
			m_iWhiteLeftMs = nowMs;
		else if (seat == SEAT_BLACK && m_iBlackLeftMs < 0)
			m_iBlackLeftMs = nowMs;
		else
			return false;

		LF_Debug.Info(FEATURE, string.Format("player %1 stood up mid-game; %2 s to come back", playerId, GRACE_MS / 1000));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Ends grace periods that ran out: the absent side resigns and the seat frees. True on a change.
	bool Tick(int nowMs)
	{
		bool changed = false;
		if (m_iWhiteLeftMs >= 0 && nowMs - m_iWhiteLeftMs >= GRACE_MS)
		{
			Expire(SEAT_WHITE);
			changed = true;
		}
		if (m_iBlackLeftMs >= 0 && nowMs - m_iBlackLeftMs >= GRACE_MS)
		{
			Expire(SEAT_BLACK);
			changed = true;
		}
		return changed;
	}

	//------------------------------------------------------------------------------------------------
	//! Plays a move for the player. Refused (logged, nothing changes) for a player not seated, while
	//! the other seat is empty, out of turn, illegal, or once the game is over.
	bool Move(int playerId, string name, int nowMs)
	{
		int seat = SeatOf(playerId);
		if (seat == SEAT_NONE)
		{
			LF_Debug.Warn(FEATURE, string.Format("move %1 refused: player %2 is not seated", name, playerId));
			return false;
		}
		if (m_iWhiteId == 0 || m_iBlackId == 0)
		{
			LF_Debug.Warn(FEATURE, string.Format("move %1 refused: waiting for an opponent", name));
			return false;
		}
		if (!m_Game.TryMove(seat == SEAT_WHITE, name))
			return false;

		if (m_Game.IsOver())
			m_iOverMs = nowMs;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! True once the finished game's players have had RESULT_HOLD_MS. A game over with no stamp
	//! (never reached through Move) is not held.
	bool ResultHoldOver(int nowMs)
	{
		if (m_iOverMs < 0)
			return true;

		return nowMs < m_iOverMs || nowMs - m_iOverMs >= RESULT_HOLD_MS;
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasGameRunning()
	{
		return m_iWhiteId != 0 && m_iBlackId != 0 && !m_Game.IsOver();
	}

	//------------------------------------------------------------------------------------------------
	protected void Expire(int seat)
	{
		if (HasGameRunning())
		{
			LF_Debug.Info(FEATURE, "grace period over; the absent side resigns");
			m_Game.Resign(seat == SEAT_WHITE);
		}
		FreeSeat(seat);
	}

	//------------------------------------------------------------------------------------------------
	protected void FreeSeat(int seat)
	{
		if (seat == SEAT_WHITE)
		{
			m_iWhiteId = 0;
			m_iWhiteLeftMs = -1;
		}
		else if (seat == SEAT_BLACK)
		{
			m_iBlackId = 0;
			m_iBlackLeftMs = -1;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! A fresh board. A player still seated (not away) keeps the seat; an away player loses it.
	protected void NewGame()
	{
		m_Game = new LF_ChessGame();
		m_iOverMs = -1;
		if (m_iWhiteLeftMs >= 0)
			FreeSeat(SEAT_WHITE);
		if (m_iBlackLeftMs >= 0)
			FreeSeat(SEAT_BLACK);
		LF_Debug.Info(FEATURE, "new game set up");
	}
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Chess", description: "A chess table: two seats and one server-side game")]
class LF_ChessTableComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_ChessTableComponent : ScriptComponent
{
	//! A seated player farther than this from the table has stood up.
	static const float SEAT_RANGE = 4.0; // lf-fixed: geometry of one table, not a server choice
	//! An ask from farther than this is refused.
	static const float ASK_RANGE = 6.0; // lf-fixed: anti-cheat reach, a little past SEAT_RANGE
	static const int TICK_MS = 1000; // lf-fixed: how often seats are checked, not a gameplay value

	protected ref LF_ChessTable m_Table;

	// What every machine knows. Simple types only; the FEN is under 100 characters.
	[RplProp(onRplName: "OnStateRpl")]
	protected string m_sFen = LF_ChessBoard.START_FEN;
	[RplProp(onRplName: "OnStateRpl")]
	protected int m_iWhiteId;
	[RplProp(onRplName: "OnStateRpl")]
	protected int m_iBlackId;
	[RplProp(onRplName: "OnStateRpl")]
	protected int m_iResult;
	[RplProp(onRplName: "OnStateRpl")]
	protected int m_iEnd;
	[RplProp(onRplName: "OnStateRpl")]
	protected string m_sLastMove;
	[RplProp(onRplName: "OnStateRpl")]
	protected bool m_bCheck;

	protected ref ScriptInvoker m_OnChanged = new ScriptInvoker();

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		// No game in the Workbench editor world, only in play.
		if (!GetGame().InPlayMode() || !Replication.IsServer())
			return;

		m_Table = new LF_ChessTable();
		GetGame().GetCallqueue().CallLater(ServerTick, TICK_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(ServerTick);
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	static LF_ChessTableComponent Of(IEntity table)
	{
		if (!table)
			return null;
		return LF_ChessTableComponent.Cast(table.FindComponent(LF_ChessTableComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! Fires on every machine when the replicated state changed. Screens listen here.
	ScriptInvoker GetOnChanged()
	{
		return m_OnChanged;
	}

	string GetFen()
	{
		return m_sFen;
	}

	int GetWhiteId()
	{
		return m_iWhiteId;
	}

	int GetBlackId()
	{
		return m_iBlackId;
	}

	int GetResult()
	{
		return m_iResult;
	}

	int GetEnd()
	{
		return m_iEnd;
	}

	string GetLastMove()
	{
		return m_sLastMove;
	}

	bool IsCheck()
	{
		return m_bCheck;
	}

	//------------------------------------------------------------------------------------------------
	//! Server only: the seats and the game (null on a client).
	LF_ChessTable GetTable()
	{
		return m_Table;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the player sits (or watches when both seats are taken). Returns the seat.
	int ServerSit(int playerId)
	{
		if (!m_Table)
		{
			LF_Debug.Error(LF_ChessTable.FEATURE, "sit asked off the server");
			return LF_ChessTable.SEAT_NONE;
		}
		int seat = m_Table.Sit(playerId, NowMs());
		// A refused sit changed nothing: do not rebuild the FEN and bump seven props for a watcher.
		if (seat != LF_ChessTable.SEAT_NONE)
			Publish();
		return seat;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the player gets up (a grace period during a game).
	void ServerStandUp(int playerId)
	{
		if (!m_Table)
			return;
		if (m_Table.StandUp(playerId, NowMs()))
			Publish();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a move for the player, checked against seat, turn and rules.
	bool ServerMove(int playerId, string name)
	{
		if (!m_Table)
		{
			LF_Debug.Error(LF_ChessTable.FEATURE, "move asked off the server");
			return false;
		}
		if (!m_Table.Move(playerId, name, NowMs()))
			return false;

		m_sLastMove = name;
		LF_Debug.Info(LF_ChessTable.FEATURE, string.Format("player %1 played %2", playerId, name));
		Publish();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: runs the grace clock at a given time (the tick passes the world time; tests pass theirs).
	void ServerTickAt(int nowMs)
	{
		if (m_Table && m_Table.Tick(nowMs))
			Publish();
	}

	//------------------------------------------------------------------------------------------------
	protected void ServerTick()
	{
		if (!m_Table)
			return;

		CheckSeated(m_Table.GetWhiteId());
		CheckSeated(m_Table.GetBlackId());
		ServerTickAt(NowMs());
	}

	//------------------------------------------------------------------------------------------------
	//! A seated player who left the server, lost the character or walked away has stood up.
	protected void CheckSeated(int playerId)
	{
		if (playerId <= 0 || m_Table.IsAway(playerId))
			return;

		IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (character && vector.Distance(character.GetOrigin(), GetOwner().GetOrigin()) <= SEAT_RANGE)
			return;

		ServerStandUp(playerId);
	}

	//------------------------------------------------------------------------------------------------
	protected int NowMs()
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return 0;
		return world.GetWorldTime();
	}

	//------------------------------------------------------------------------------------------------
	protected void Publish()
	{
		LF_ChessGame game = m_Table.GetGame();
		m_sFen = game.GetBoard().ToFen();
		m_iWhiteId = m_Table.GetWhiteId();
		m_iBlackId = m_Table.GetBlackId();
		m_iResult = game.GetResult();
		m_iEnd = game.GetEnd();
		m_bCheck = !game.IsOver() && game.IsCheck();
		if (game.MoveCount() == 0)
			m_sLastMove = string.Empty;
		Replication.BumpMe();
		OnStateRpl();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnStateRpl()
	{
		m_OnChanged.Invoke(this);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: counts one chess ask against the player's own chess bucket, apart from the phone's
	//! COMMS bucket (#2405). The asks go through here so a test can drive the real kind.
	static bool AllowAsk(int playerId, string request, bool notify = true)
	{
		return LF_RateLimit.Allow(playerId, LF_ERequestKind.CHESS, request, notify);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: is this character close enough to ask anything of the table.
	bool InReach(IEntity character)
	{
		if (!character)
			return false;
		return vector.Distance(character.GetOrigin(), GetOwner().GetOrigin()) <= ASK_RANGE;
	}
}

//------------------------------------------------------------------------------------------------
//! "Play chess" on the table. The server takes a seat for the player and opens the board on the
//! player's machine. Both seats taken: the board opens to watch.
class LF_ChessSitAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBroadcastScript()
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-Chess_Play";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		LF_ChessTableComponent table = LF_ChessTableComponent.Of(pOwnerEntity);
		if (!table || !pUserEntity)
		{
			LF_Debug.Error(LF_ChessTable.FEATURE, "play chess with no table component or no player");
			return;
		}

		// The server seats the player and opens the board on the player's own machine.
		if (!Replication.IsServer())
			return;

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(pUserEntity);
		table.ServerSit(playerId);
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		if (!controller)
		{
			LF_Debug.Error(LF_ChessTable.FEATURE, string.Format("board not opened: no controller for player %1", playerId));
			return;
		}
		controller.LF_OpenChessBoard(LF_NetworkUtils.GetRplId(pOwnerEntity));
	}
}

//------------------------------------------------------------------------------------------------
//! Owner -> server asks for the chess table. The server takes the player from this controller,
//! never from the payload, and the table re-checks seat, turn, rules and reach.
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	//! Server: open the board of the table with RplId `table` on this player's machine.
	void LF_OpenChessBoard(RplId table)
	{
		if (GetGame().GetPlayerController() == this)
			RpcDo_LF_OpenChessBoard(table);
		else
			Rpc(RpcDo_LF_OpenChessBoard, table);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_OpenChessBoard(RplId table)
	{
		IEntity entity = LF_NetworkUtils.FindEntityByRplId(table);
		if (!entity)
		{
			LF_Debug.Error(LF_ChessTable.FEATURE, "board not opened: the table is not in this client's world");
			return;
		}
		LF_ChessScreen.Open(entity);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: play `move` ("e2e4", "e7e8q") at the table with RplId `table`.
	void LF_AskChessMove(RplId table, string move)
	{
		if (Replication.IsServer())
			RpcAsk_LF_ChessMove(table, move);
		else
			Rpc(RpcAsk_LF_ChessMove, table, move);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_ChessMove(RplId table, string move)
	{
		if (move.Length() < 4 || move.Length() > 5)
		{
			LF_Debug.Warn(LF_ChessTable.FEATURE, "move ask refused: malformed move");
			return;
		}
		if (!LF_ChessTableComponent.AllowAsk(GetPlayerId(), "chess move"))
			return;

		LF_ChessTableComponent component = LF_ChessFindTable(table);
		if (!component)
			return;
		if (!component.InReach(GetControlledEntity()))
		{
			LF_Debug.Warn(LF_ChessTable.FEATURE, string.Format("move ask from player %1 refused: not at the table", GetPlayerId()));
			return;
		}

		component.ServerMove(GetPlayerId(), move);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: this player got up from the table (closed the board).
	void LF_AskChessStand(RplId table)
	{
		if (Replication.IsServer())
			RpcAsk_LF_ChessStand(table);
		else
			Rpc(RpcAsk_LF_ChessStand, table);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_ChessStand(RplId table)
	{
		if (!LF_ChessTableComponent.AllowAsk(GetPlayerId(), "chess stand up"))
			return;

		LF_ChessTableComponent component = LF_ChessFindTable(table);
		if (component)
			component.ServerStandUp(GetPlayerId());
	}

	//------------------------------------------------------------------------------------------------
	protected LF_ChessTableComponent LF_ChessFindTable(RplId table)
	{
		LF_ChessTableComponent component = LF_ChessTableComponent.Of(LF_NetworkUtils.FindEntityByRplId(table));
		if (!component)
			LF_Debug.Warn(LF_ChessTable.FEATURE, "chess ask refused: no table with that id");
		return component;
	}
}
