// LF_TownBoardSheet.c — the card a player picked off the town board, as one UI kit sheet: the
// title, the category, WHERE (the place, how far, which way), the detail, Mark on map, Close.
// Client only. Closes on Close, MenuBack, death, losing the character, or walking away from the board.

class LF_TownBoardSheet
{
	static const float CLOSE_DISTANCE = 4;	// lf-fixed: the sign card's reach
	static const string KEY_WHERE_LABEL = "#LF-TownBoard_WhereLabel";
	static const string KEY_MARK = "#LF-TownBoard_Mark";
	static const string KEY_UNMARK = "#LF-TownBoard_Unmark";
	static const string BUTTON_MARK = "MarkButton";

	protected static ref LF_TownBoardSheet s_Current;

	protected ref LF_UIScreen m_Screen;
	protected string m_sCardId;
	protected vector m_vBoard;

	//------------------------------------------------------------------------------------------------
	//! The sheet's parts for `card` read at a board standing at `boardOrigin`. A card with no place on
	//! this map has no Mark button. `marked`: the card is the current waypoint (the button takes it off).
	static array<ref LF_UIPart> BuildParts(LF_TownBoardCard card, vector boardOrigin, LF_UISkin skin, bool marked)
	{
		ref array<ref LF_UIPart> parts = {};
		if (!card)
			return parts;

		string accent;
		if (skin)
			accent = skin.m_sAccentRgba;

		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, "Title", LF_SignCatalog.ResolveTitle(card.m_sTitle)));
		string category = LF_TownBoard.Text(card.m_sCategory);
		category.ToUpper();
		if (!category.IsEmpty())
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.HEADING, "Category", category));

		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.RULE, "TitleRule", ""));
		string whereLabel = LF_TownBoard.Text(KEY_WHERE_LABEL);
		whereLabel.ToUpper();
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.HEADING, "WhereLabel", whereLabel));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "Where", LF_TownBoard.WhereText(card, boardOrigin)));
		string body = LF_SignCatalog.RichBody(LF_TownBoard.Text(card.m_sDetail), accent);
		body.TrimInPlace();
		if (!body.IsEmpty())
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.RICH, "Detail", body));

		vector place;
		if (LF_TownBoard.FindPlace(card, place))
		{
			string mark = KEY_MARK;
			if (marked)
				mark = KEY_UNMARK;

			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.BUTTON, BUTTON_MARK, mark));
		}

		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, "Close", LF_SignCatalog.KEY_CLOSE));
		return parts;
	}

	//------------------------------------------------------------------------------------------------
	//! Opens the sheet for `card` read at `board`. \return The screen, or null (logged).
	static LF_UIScreen Open(LF_TownBoardCard card, IEntity board)
	{
		Hide();
		if (!card || !board)
		{
			LF_Debug.Error(LF_TownBoard.FEATURE, "card sheet refused: no card or no board");
			return null;
		}

		LF_UISkin skin = LF_SignCatalog.CardSkin(false, vector.Zero, vector.Zero, vector.Zero);
		array<ref LF_UIPart> parts = BuildParts(card, board.GetOrigin(), skin, LF_Waypoint.IsMarked(card.m_sId));
		skin.m_fTypeScale = LF_SignCatalog.FitScale(parts, skin.m_fWidth);
		LF_UIScreen screen = LF_UIScreen.Open(parts, skin);
		if (!screen)
		{
			LF_Debug.Error(LF_TownBoard.FEATURE, "card sheet failed to open for " + card.m_sId);
			return null;
		}

		LF_TownBoardSheet sheet = new LF_TownBoardSheet();
		sheet.m_Screen = screen;
		sheet.m_sCardId = card.m_sId;
		sheet.m_vBoard = board.GetOrigin();
		screen.GetOnButton().Insert(sheet.OnButton);
		screen.CloseOnDeath();
		screen.CloseWhenUncontrolled();
		screen.CloseBeyond(CLOSE_DISTANCE, board.GetID());
		s_Current = sheet;
		LF_Debug.Info(LF_TownBoard.FEATURE, "card sheet open: " + card.m_sId);
		return screen;
	}

	//------------------------------------------------------------------------------------------------
	//! Mark on map: the place becomes the waypoint (or stops being it), and the sheet closes so the
	//! player sees the marker.
	protected void OnButton(string name)
	{
		if (name != BUTTON_MARK)
			return;

		LF_TownBoardCard card = LF_TownBoard.FindCard(m_sCardId);
		vector place;
		if (!card || !LF_TownBoard.FindPlace(card, place))
		{
			LF_Debug.Warn(LF_TownBoard.FEATURE, string.Format("mark refused: card '%1' has no place on this map", m_sCardId));
			return;
		}

		LF_Waypoint.Mark(card.m_sId, LF_TownBoard.PlaceName(card), place);
		// Closed after the button's own handler has returned, not from inside it.
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().CallLater(Hide, 0, false);
		else
			Hide();
	}

	//------------------------------------------------------------------------------------------------
	static LF_UIScreen Current()
	{
		if (!s_Current || !s_Current.m_Screen || !s_Current.m_Screen.IsOpen())
			return null;

		return s_Current.m_Screen;
	}

	//------------------------------------------------------------------------------------------------
	static void Hide()
	{
		if (!s_Current)
			return;

		LF_TownBoardSheet sheet = s_Current;
		s_Current = null;
		if (sheet.m_Screen)
			sheet.m_Screen.Close();
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		Hide();
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_TownBoardSheet's statics (registered at script load; Core/LF_StaticReset.c).
class LF_TownBoardSheet_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_TownBoardSheet_StaticResetHook());

	override string Owner()
	{
		return "LF_TownBoardSheet";
	}

	override void Apply()
	{
		LF_TownBoardSheet.ResetStatics();
	}
}
