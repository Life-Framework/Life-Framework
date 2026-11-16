// LF_ChessScreen.c — the board a player at a chess table sees (#1934). Client only, built from UI
// kit parts: eight KEYS rows of eight squares, the seat and turn lines, and Stand up. Click a piece
// of your colour, then the square to move it to; the move goes to the server as an ask and the
// board redraws from the table's replicated FEN, so nothing here decides what is legal. A pawn that
// reaches the last rank becomes a queen. Black sees the board from its own side.
// Closing the board (Stand up, MenuBack, death, walking away) tells the server the player got up.

class LF_ChessScreen
{
	static const string PART_SEAT = "ChessSeat";
	static const string PART_TURN = "ChessTurn";
	static const string PART_NOTE = "ChessNote";
	static const float CLOSE_DISTANCE = 4; // lf-fixed: the board closes when you walk off, like the other sheets
	static const float SHEET_WIDTH = 640; // lf-fixed: eight keys of about 70 reference pixels each
	static const float SHEET_HEIGHT = 760; // lf-fixed: title, two lines, eight rows, last move and Stand up fit a 720p screen
	static const float KEY_HEIGHT = 40; // lf-fixed: the kit button is 48 plus padding; eight rows would not fit the sheet
	static const int KEY_FONT = 22; // lf-fixed: "[Q]" and "e4" fit one key

	protected static ref LF_ChessScreen s_Screen;

	protected ref LF_UIScreen m_Screen;
	protected EntityID m_TableId;
	protected RplId m_TableRpl;
	protected int m_iSelected = -1;
	protected bool m_bFlipped;

	//------------------------------------------------------------------------------------------------
	//! The key name of a square ("Chess_e4Button": the harness can click names ending in Button).
	static string KeyName(int sq)
	{
		return "Chess_" + LF_ChessBoard.SquareName(sq) + "Button";
	}

	//------------------------------------------------------------------------------------------------
	//! The square of a key name, -1 for any other part.
	static int SquareOfKey(string name)
	{
		if (!name.StartsWith("Chess_") || name.Length() != 14)
			return -1;
		return LF_ChessBoard.SquareFromName(name.Substring(6, 2));
	}

	//------------------------------------------------------------------------------------------------
	//! What a square shows: the FEN letter of the piece (upper case white), blank when empty.
	static string PieceText(int piece)
	{
		if (piece == LF_ChessBoard.EMPTY)
			return " ";
		string white = "PNBRQK";
		string black = "pnbrqk";
		if (piece > 0)
			return white.Substring(piece - 1, 1);
		return black.Substring(-piece - 1, 1);
	}

	//------------------------------------------------------------------------------------------------
	//! The sheet the board is drawn on: the paper form, wide enough for eight keys and short enough
	//! that the last-move line and Stand up show without a scroll.
	static LF_UISkin BoardSkin()
	{
		LF_UISkin skin = LF_UISkin.Paper();
		skin.m_fWidth = SHEET_WIDTH;
		skin.m_fHeight = SHEET_HEIGHT;
		return skin;
	}

	//------------------------------------------------------------------------------------------------
	//! Makes one kit button a chess square (#2099). The kit button is the pause-menu button: its label
	//! row has 40 reference pixels of left padding and a 64 pixel arrow glyph in front of the text, so
	//! in a key one eighth of the sheet wide the label was pushed a whole key to the right (the a-file
	//! blank, the h-file clipped). The square drops the padding and the glyph, centres the label in
	//! its own button and takes a shorter height.
	//! eturn false when the key is missing the parts of the kit button.
	static bool FitKey(Widget key)
	{
		if (!key)
			return false;

		Widget layout = key.FindAnyWidget("HorizontalLayout0");
		Widget text = key.FindAnyWidget("Text");
		if (!layout || !text)
		{
			LF_Debug.Error(LF_ChessTable.FEATURE, "key " + key.GetName() + " is missing its label row");
			return false;
		}

		AlignableSlot.SetPadding(layout, 0, 0, 0, 0);
		AlignableSlot.SetHorizontalAlign(layout, LayoutHorizontalAlign.Center);
		AlignableSlot.SetVerticalAlign(layout, LayoutVerticalAlign.Center);

		Widget glyph = key.FindAnyWidget("Scale0");
		if (glyph)
		{
			LayoutSlot.SetPadding(glyph, 0, 0, 0, 0);
			glyph.SetVisible(false);
		}

		LayoutSlot.SetPadding(text, 0, 0, 0, 0);
		TextWidget label = TextWidget.Cast(text);
		if (label)
		{
			label.SetTextWrapping(false);
			label.SetExactFontSize(KEY_FONT);
		}

		SizeLayoutWidget size = SizeLayoutWidget.Cast(key.FindAnyWidget("SizeLayout"));
		if (size)
		{
			size.SetHeightOverride(KEY_HEIGHT);
			AlignableSlot.SetPadding(size, 0, 1, 0, 1);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Fits all 64 keys and the eight rows of the board that is open on the workspace.
	//! eturn How many keys were fitted.
	static int FitBoard()
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return 0;

		int fitted = 0;
		for (int sq = 0; sq < 64; sq++)
		{
			if (FitKey(workspace.FindAnyWidget(KeyName(sq))))
				fitted++;
		}

		for (int rank = 1; rank <= 8; rank++)
		{
			Widget row = workspace.FindAnyWidget("ChessRank" + rank.ToString());
			if (row)
				AlignableSlot.SetPadding(row, LF_UITheme.PAD_LEFT, 1, LF_UITheme.PAD_RIGHT, 1);
		}

		if (fitted != 64)
			LF_Debug.Warn(LF_ChessTable.FEATURE, "board fitted " + fitted.ToString() + " of 64 keys");
		return fitted;
	}

	//------------------------------------------------------------------------------------------------
	static array<ref LF_UIPart> BuildParts(bool flipped)
	{
		array<ref LF_UIPart> parts = {};
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, "ChessTitle", "#LF-Chess_Title"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, PART_SEAT, ""));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, PART_TURN, ""));
		for (int row = 0; row < 8; row++)
		{
			int rank = 7 - row;
			if (flipped)
				rank = row;
			array<string> names = {};
			array<string> texts = {};
			for (int col = 0; col < 8; col++)
			{
				int file = col;
				if (flipped)
					file = 7 - col;
				names.Insert(KeyName(LF_ChessBoard.Square(file, rank)));
				texts.Insert(" ");
			}
			parts.Insert(LF_UIPart.CreateKeys("ChessRank" + (rank + 1).ToString(), names, texts));
		}
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, PART_NOTE, ""));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, "ChessStand", "#LF-Chess_StandUp"));
		return parts;
	}

	//------------------------------------------------------------------------------------------------
	static LF_UIScreen Open(IEntity table)
	{
		Hide();
		LF_ChessTableComponent component = LF_ChessTableComponent.Of(table);
		if (!component)
		{
			LF_Debug.Error(LF_ChessTable.FEATURE, "board not opened: no chess table");
			return null;
		}

		LF_ChessScreen screen = new LF_ChessScreen();
		screen.m_TableId = table.GetID();
		screen.m_TableRpl = LF_NetworkUtils.GetRplId(table);
		// Black's seat is decided on the server; the board flips once the state says so.
		screen.m_bFlipped = screen.LocalSeat(component) == LF_ChessTable.SEAT_BLACK;
		screen.m_Screen = LF_UIScreen.Open(BuildParts(screen.m_bFlipped), BoardSkin());
		if (!screen.m_Screen)
		{
			LF_Debug.Error(LF_ChessTable.FEATURE, "board screen failed to open");
			return null;
		}
		FitBoard();

		screen.m_Screen.GetOnButton().Insert(screen.OnButton);
		screen.m_Screen.GetOnClosed().Insert(screen.OnClosed);
		screen.m_Screen.CloseOnDeath();
		screen.m_Screen.CloseWhenUncontrolled();
		screen.m_Screen.CloseBeyond(CLOSE_DISTANCE, table.GetID());
		component.GetOnChanged().Insert(screen.OnTableChanged);

		s_Screen = screen;
		screen.Redraw();
		LF_Debug.Info(LF_ChessTable.FEATURE, "board open");
		return screen.m_Screen;
	}

	//------------------------------------------------------------------------------------------------
	static LF_UIScreen Current()
	{
		if (!s_Screen)
			return null;
		return s_Screen.m_Screen;
	}

	//------------------------------------------------------------------------------------------------
	static void Hide()
	{
		if (!s_Screen)
			return;
		LF_ChessScreen screen = s_Screen;
		if (screen.m_Screen)
			screen.m_Screen.Close();
		s_Screen = null;
	}

	//------------------------------------------------------------------------------------------------
	//! World end: forget the open board (its widgets went with the world).
	static void ResetStatics()
	{
		s_Screen = null;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_ChessTableComponent Table()
	{
		return LF_ChessTableComponent.Of(GetGame().GetWorld().FindEntityByID(m_TableId));
	}

	//------------------------------------------------------------------------------------------------
	protected int LocalSeat(LF_ChessTableComponent component)
	{
		PlayerController controller = GetGame().GetPlayerController();
		if (!controller || !component)
			return LF_ChessTable.SEAT_NONE;
		int playerId = controller.GetPlayerId();
		if (playerId > 0 && playerId == component.GetWhiteId())
			return LF_ChessTable.SEAT_WHITE;
		if (playerId > 0 && playerId == component.GetBlackId())
			return LF_ChessTable.SEAT_BLACK;
		return LF_ChessTable.SEAT_NONE;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnTableChanged(LF_ChessTableComponent component)
	{
		if (!m_Screen || !m_Screen.IsOpen())
			return;
		// Seated as black after the board opened: open it again from black's side.
		bool flipped = LocalSeat(component) == LF_ChessTable.SEAT_BLACK;
		if (flipped != m_bFlipped)
		{
			IEntity table = GetGame().GetWorld().FindEntityByID(m_TableId);
			component.GetOnChanged().Remove(OnTableChanged);
			m_Screen.GetOnClosed().Remove(OnClosed);
			if (table)
				Open(table);
			return;
		}
		Redraw();
	}

	//------------------------------------------------------------------------------------------------
	protected void Redraw()
	{
		LF_ChessTableComponent component = Table();
		if (!component || !m_Screen)
			return;

		LF_ChessBoard board = new LF_ChessBoard();
		if (!board.SetFen(component.GetFen()))
		{
			LF_Debug.Error(LF_ChessTable.FEATURE, "board not drawn: bad FEN from the table");
			return;
		}

		for (int sq = 0; sq < 64; sq++)
		{
			string text = PieceText(board.Get(sq));
			if (sq == m_iSelected)
				text = "[" + text + "]";
			m_Screen.SetButtonText(KeyName(sq), text);
		}

		int seat = LocalSeat(component);
		if (seat == LF_ChessTable.SEAT_WHITE)
			m_Screen.SetLine(PART_SEAT, "#LF-Chess_YouWhite");
		else if (seat == LF_ChessTable.SEAT_BLACK)
			m_Screen.SetLine(PART_SEAT, "#LF-Chess_YouBlack");
		else
			m_Screen.SetLine(PART_SEAT, "#LF-Chess_Watching");

		m_Screen.SetLine(PART_TURN, TurnText(component, board));

		string last = component.GetLastMove();
		if (m_iSelected >= 0)
			m_Screen.SetLine(PART_NOTE, LF_Terms.Text("#LF-Chess_Selected", LF_ChessBoard.SquareName(m_iSelected)));
		else if (!last.IsEmpty())
			m_Screen.SetLine(PART_NOTE, LF_Terms.Text("#LF-Chess_LastMove", last));
		else
			m_Screen.SetLine(PART_NOTE, "");
	}

	//------------------------------------------------------------------------------------------------
	protected string TurnText(LF_ChessTableComponent component, LF_ChessBoard board)
	{
		int result = component.GetResult();
		if (result == LF_ChessGame.RESULT_WHITE_WINS)
			return "#LF-Chess_WhiteWins";
		if (result == LF_ChessGame.RESULT_BLACK_WINS)
			return "#LF-Chess_BlackWins";
		if (result == LF_ChessGame.RESULT_DRAW)
			return "#LF-Chess_Draw";
		if (component.GetWhiteId() == 0 || component.GetBlackId() == 0)
			return "#LF-Chess_Waiting";
		if (component.IsCheck())
		{
			if (board.m_bWhiteToMove)
				return "#LF-Chess_WhiteInCheck";
			return "#LF-Chess_BlackInCheck";
		}
		if (board.m_bWhiteToMove)
			return "#LF-Chess_WhiteToMove";
		return "#LF-Chess_BlackToMove";
	}

	//------------------------------------------------------------------------------------------------
	protected void OnButton(string name)
	{
		int sq = SquareOfKey(name);
		if (sq < 0)
			return;

		LF_ChessTableComponent component = Table();
		if (!component)
			return;
		int seat = LocalSeat(component);
		if (seat == LF_ChessTable.SEAT_NONE)
			return;

		LF_ChessBoard board = new LF_ChessBoard();
		if (!board.SetFen(component.GetFen()))
			return;

		int piece = board.Get(sq);
		bool mine = piece != LF_ChessBoard.EMPTY && LF_ChessBoard.IsWhite(piece) == (seat == LF_ChessTable.SEAT_WHITE);
		if (m_iSelected < 0 || mine)
		{
			// First click, or a different piece of your own: select it.
			if (mine)
				m_iSelected = sq;
			else
				m_iSelected = -1;
			Redraw();
			return;
		}

		string move = LF_ChessBoard.SquareName(m_iSelected) + LF_ChessBoard.SquareName(sq);
		int moving = board.Get(m_iSelected);
		int rank = LF_ChessBoard.RankOf(sq);
		if (LF_ChessBoard.TypeOf(moving) == LF_ChessBoard.PAWN && (rank == 0 || rank == 7))
			move += "q";
		m_iSelected = -1;
		Redraw();

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
		{
			LF_Debug.Error(LF_ChessTable.FEATURE, "move not sent: no player controller");
			return;
		}
		LF_Debug.Info(LF_ChessTable.FEATURE, "asking move " + move);
		controller.LF_AskChessMove(m_TableRpl, move);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnClosed()
	{
		LF_ChessTableComponent component = Table();
		if (component)
			component.GetOnChanged().Remove(OnTableChanged);
		if (s_Screen == this)
			s_Screen = null;

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (controller && LocalSeat(component) != LF_ChessTable.SEAT_NONE)
			controller.LF_AskChessStand(m_TableRpl);
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_ChessScreen's statics (registered at script load; Core/LF_StaticReset.c).
class LF_ChessScreen_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_ChessScreen_StaticResetHook());

	override string Owner()
	{
		return "LF_ChessScreen";
	}

	override void Apply()
	{
		LF_ChessScreen.ResetStatics();
	}
}
