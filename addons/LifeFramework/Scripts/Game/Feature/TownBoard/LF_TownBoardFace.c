//------------------------------------------------------------------------------------------------
//! The town board's face on a client: the wanted board's render target over the cork
//! (LF_WantedPosterFace: the local face quad, the cork, the live window and the freeze) with a paper
//! header and one pinned index card per thing to do, four across and two down. A card prints its
//! category in red, the title, one line, and where the place is from the board. Static content from
//! config: everyone near the board draws the same face, nothing comes over the network.
class LF_TownBoardFace : LF_WantedPosterFace
{
	//! The case board's pin (two pin heads and beads; only one head is used here).
	static const ResourceName PIN_LAYOUT = "{22DB59D5A86C7CD0}UI/Layouts/Investigation/LF_CaseBoardString.layout";

	//! The cards, in face layout units (1920 x 1080).
	static const int COLUMNS = 4;	// lf-fixed: the prefab has a context over each slot
	static const float CARD_WIDTH = 400;	// lf-fixed: the layout size
	static const float CARD_HEIGHT = 400;	// lf-fixed: the layout size
	static const float CARD_GAP_X = 40;	// lf-fixed: the layout size
	static const float CARD_GAP_Y = 36;	// lf-fixed: the layout size
	static const float CARDS_TOP = 172;	// lf-fixed: below the header sheet
	//! The header sheet across the top.
	static const float HEADER_WIDTH = 760;	// lf-fixed: the layout size
	static const float HEADER_HEIGHT = 120;	// lf-fixed: the layout size
	static const float HEADER_TOP = 22;	// lf-fixed: the layout size
	static const float PIN = 18;	// lf-fixed: the pin head
	static const float PIN_DOWN = 16;	// lf-fixed: the pin sits this far below a sheet's top edge
	static const int BEADS = 64;	// lf-fixed: LF_CaseBoardString.layout has 64 beads

	protected int m_iCards;

	//------------------------------------------------------------------------------------------------
	//! Top-left of card slot `index` (0 .. MAX_CARDS - 1) on the face: rows of COLUMNS, centred.
	static vector SlotPosition(int index)
	{
		float rowWidth = COLUMNS * CARD_WIDTH + (COLUMNS - 1) * CARD_GAP_X;
		int col = index % COLUMNS;
		int row = index / COLUMNS;
		float x = (FACE_WIDTH - rowWidth) * 0.5 + col * (CARD_WIDTH + CARD_GAP_X);
		float y = CARDS_TOP + row * (CARD_HEIGHT + CARD_GAP_Y);
		return Vector(x, y, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! The centre of card slot `index` in the face entity's frame, metres: x to the reader's right,
	//! y up, from the face's centre, for a face of `faceScale` (1 is 1.78 x 1 m). The prefab puts a
	//! card's action context here (LF_TownBoard.et; townboard/card-slots checks them).
	static vector SlotCentreMeters(int index, float faceScale)
	{
		vector slot = SlotPosition(index);
		float metersPerUnit = 1.0 * faceScale / FACE_HEIGHT;
		float u = slot[0] + CARD_WIDTH * 0.5 - FACE_WIDTH * 0.5;
		float v = slot[1] + CARD_HEIGHT * 0.5 - FACE_HEIGHT * 0.5;
		return Vector(u * metersPerUnit, -v * metersPerUnit, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Draws the header and the cards, `origin` being where the board stands (for the distances),
	//! then renders live and freezes. \return Cards drawn.
	int ShowBoard(vector origin)
	{
		if (!IsBuilt())
			return 0;

		ClearPosters();
		m_iCards = 0;
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return 0;

		if (!DrawHeader(workspace))
			return 0;

		for (int i = 0; i < LF_TownBoard.CardCount(); i++)
		{
			LF_TownBoardCard card = LF_TownBoard.CardAt(i);
			if (!card)
				continue;

			if (!DrawCard(workspace, card, i, origin))
				break;

			m_iCards++;
		}

		Thaw(LIVE_MS);
		ScriptCallQueue queue = GetGame().GetCallqueue();
		if (queue)
		{
			queue.Remove(Refresh);
			queue.CallLater(Refresh, SETTLE_REFRESH_MS, false);
			queue.CallLater(Refresh, LATE_REFRESH_MS, false);
		}

		return m_iCards;
	}

	//------------------------------------------------------------------------------------------------
	int GetCardCount()
	{
		return m_iCards;
	}

	//------------------------------------------------------------------------------------------------
	//! The paper header pinned across the top: the board's title.
	protected bool DrawHeader(notnull WorkspaceWidget workspace)
	{
		Widget sheet = NewSheet(workspace);
		if (!sheet)
			return false;

		float left = (FACE_WIDTH - HEADER_WIDTH) * 0.5;
		Place(sheet, left, HEADER_TOP, HEADER_WIDTH, HEADER_HEIGHT, 1);
		Place(sheet.FindAnyWidget("Paper"), 0, 0, HEADER_WIDTH, HEADER_HEIGHT, 1);
		CardLine(sheet, "Header", string.Empty, 0, 1, 8, CARD_WIDTH);
		CardLine(sheet, "Name", LF_TownBoard.Title(), 30, 70, 56, HEADER_WIDTH);
		CardLine(sheet, "Offence", string.Empty, 0, 1, 8, CARD_WIDTH);
		CardLine(sheet, "Detail", string.Empty, 0, 1, 8, CARD_WIDTH);
		CardLine(sheet, "Reward", string.Empty, 0, 1, 8, CARD_WIDTH);
		m_aPosters.Insert(sheet);
		return DrawPin(workspace, left + HEADER_WIDTH * 0.5, HEADER_TOP + PIN_DOWN);
	}

	//------------------------------------------------------------------------------------------------
	//! One index card: the category in red, the title, the line, where it is.
	protected bool DrawCard(notnull WorkspaceWidget workspace, notnull LF_TownBoardCard card, int slot, vector origin)
	{
		Widget sheet = NewSheet(workspace);
		if (!sheet)
			return false;

		vector at = SlotPosition(slot);
		Place(sheet, at[0], at[1], CARD_WIDTH, CARD_HEIGHT, 1);
		Place(sheet.FindAnyWidget("Paper"), 0, 0, CARD_WIDTH, CARD_HEIGHT, 1);
		string category = LF_TownBoard.Text(card.m_sCategory);
		category.ToUpper();
		CardLine(sheet, "Header", category, 34, 40, 30, CARD_WIDTH);
		CardLine(sheet, "Name", LF_TownBoard.Text(card.m_sTitle), 76, 100, 42, CARD_WIDTH);
		CardLine(sheet, "Offence", LF_TownBoard.Text(card.m_sLine), 180, 128, 27, CARD_WIDTH);
		CardLine(sheet, "Detail", LF_TownBoard.WhereText(card, origin), 318, 66, 25, CARD_WIDTH);
		CardLine(sheet, "Reward", string.Empty, 0, 1, 8, CARD_WIDTH);
		m_aPosters.Insert(sheet);
		return DrawPin(workspace, at[0] + CARD_WIDTH * 0.5, at[1] + PIN_DOWN);
	}

	//------------------------------------------------------------------------------------------------
	//! A poster sheet with its photo parts hidden: plain paper for text.
	protected Widget NewSheet(notnull WorkspaceWidget workspace)
	{
		Widget sheet = workspace.CreateWidgets(POSTER_LAYOUT, m_wPosters);
		if (!sheet)
		{
			LF_Debug.Error(LF_TownBoard.FEATURE, "card layout does not load: " + POSTER_LAYOUT);
			return null;
		}

		array<string> photoParts = {"PhotoBack", "PhotoClip", "PlateBack"};
		foreach (string part : photoParts)
		{
			Widget w = sheet.FindAnyWidget(part);
			if (w)
				w.SetVisible(false);
		}

		return sheet;
	}

	//------------------------------------------------------------------------------------------------
	//! A text line in the sheet's units, 14 in from each side of a sheet `width` wide; an
	//! empty text hides the line.
	protected static void CardLine(Widget sheet, string widgetName, string text, float y, float height, float font, float width)
	{
		TextWidget w = TextWidget.Cast(sheet.FindAnyWidget(widgetName));
		if (!w)
			return;

		Place(w, 14, y, width - 28, height, 1);
		w.SetExactFontSize(Math.Round(font));
		w.SetTextWrapping(true);
		w.SetText(text);
		w.SetVisible(!text.IsEmpty());
	}

	//------------------------------------------------------------------------------------------------
	//! One red pin head at x, y: the case board's pin, without its string.
	protected bool DrawPin(notnull WorkspaceWidget workspace, float x, float y)
	{
		Widget root = workspace.CreateWidgets(PIN_LAYOUT, m_wPosters);
		if (!root)
		{
			LF_Debug.Error(LF_TownBoard.FEATURE, "pin layout does not load: " + PIN_LAYOUT);
			return false;
		}

		Place(root, 0, 0, FACE_WIDTH, FACE_HEIGHT, 1);
		m_aPosters.Insert(root);
		for (int i = 0; i < BEADS; i++)
		{
			Widget bead = root.FindAnyWidget("Bead" + i.ToString());
			if (bead)
				bead.SetVisible(false);
		}

		Widget other = root.FindAnyWidget("PinB");
		if (other)
			other.SetVisible(false);

		Place(root.FindAnyWidget("PinA"), x - PIN * 0.5, y - PIN * 0.5, PIN, PIN, 1);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/TownBoard", description: "Draws the town board's cards on the cork for every player near it")]
class LF_TownBoardComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! The town board prop (Prefabs/Props/Signs/LF_TownBoard.et). On a rendering client it draws its face
//! within DRAW_RADIUS of the camera and releases it beyond, like the wanted board; one timer for every
//! board, once a second. A dedicated server does nothing here.
class LF_TownBoardComponent : ScriptComponent
{
	static const float DRAW_RADIUS = 30;	// lf-fixed: the square is about this wide
	static const int TICK_MS = 1000;	// lf-fixed: a distance check, not tuning

	[Attribute("0 1.555 -0.006", UIWidgets.EditBox, "Face position relative to the board root: the cork's centre, just in front of it")]
	protected vector m_vFaceOffset;

	[Attribute("-90 0 0", UIWidgets.EditBox, "Face rotation relative to the board root (yaw pitch roll, degrees); LF_BoardFace.xob faces +X, the cork -Z")]
	protected vector m_vFaceAngles;

	[Attribute("0.965", UIWidgets.EditBox, "Face scale. 1 is 1.78 x 1 m; the cork board's cork is about 1.71 x 0.96 m")]
	protected float m_fFaceScale;

	protected static ref array<LF_TownBoardComponent> s_aBoards;
	protected static bool s_bTicking;

	protected ref LF_TownBoardFace m_Face;
	protected bool m_bBuildFailed;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		SetEventMask(owner, EntityEvent.INIT);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		if (!GetGame() || !GetGame().InPlayMode())
			return;

		if (RplSession.Mode() == RplMode.Dedicated || !GetGame().GetWorkspace())
			return;

		if (!s_aBoards)
			s_aBoards = {};

		s_aBoards.Insert(this);
		if (!s_bTicking && GetGame().GetCallqueue())
		{
			s_bTicking = true;
			GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);
		}
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		if (s_bTicking && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Tick);

		s_bTicking = false;
		s_aBoards = null;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tick()
	{
		vector viewer;
		if (!s_aBoards || !LF_WantedPosterFaces.ViewerPosition(viewer))
			return;

		foreach (LF_TownBoardComponent board : s_aBoards)
		{
			if (board)
				board.UpdateView(viewer);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The nearest town board on this machine as "cards/face" ("8/frozen"); "-" with none.
	//! The harness's `seen` reads it.
	static string DescribeNearest(vector origin)
	{
		LF_TownBoardComponent nearest;
		float best = float.MAX;
		if (s_aBoards)
		{
			foreach (LF_TownBoardComponent board : s_aBoards)
			{
				if (!board || !board.GetOwner())
					continue;

				float distance = vector.Distance(origin, board.GetOwner().GetOrigin());
				if (distance < best)
				{
					best = distance;
					nearest = board;
				}
			}
		}

		if (!nearest)
			return "-";

		return nearest.Describe();
	}

	//------------------------------------------------------------------------------------------------
	string Describe()
	{
		if (!m_Face || !m_Face.IsBuilt())
			return "0/released";

		string state = "live";
		if (m_Face.IsFrozen())
			state = "frozen";

		return string.Format("%1/%2", m_Face.GetCardCount(), state);
	}

	//------------------------------------------------------------------------------------------------
	float GetFaceScale()
	{
		return m_fFaceScale;
	}

	//------------------------------------------------------------------------------------------------
	vector GetFaceOffset()
	{
		return m_vFaceOffset;
	}

	//------------------------------------------------------------------------------------------------
	void UpdateView(vector viewer)
	{
		IEntity owner = GetOwner();
		if (!owner || owner.IsDeleted() || m_bBuildFailed)
			return;

		bool built = m_Face && m_Face.IsBuilt();
		int decision = LF_WantedPosterFaces.Decide(vector.Distance(viewer, owner.GetOrigin()), DRAW_RADIUS, built);
		if (decision > 0)
			BuildFace();
		else if (decision < 0)
			ReleaseFace("out of range");
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildFace()
	{
		int started = System.GetTickCount();
		m_Face = new LF_TownBoardFace();
		if (!m_Face.Build(GetOwner(), m_vFaceOffset, m_vFaceAngles, m_fFaceScale))
		{
			m_Face = null;
			m_bBuildFailed = true;
			return;
		}

		int drawn = m_Face.ShowBoard(GetOwner().GetOrigin());
		LF_Debug.Info(LF_TownBoard.FEATURE, string.Format("face built at %1 with %2 card(s) in %3 ms", GetOwner().GetOrigin(), drawn, System.GetTickCount() - started));
	}

	//------------------------------------------------------------------------------------------------
	void ReleaseFace(string why)
	{
		if (!m_Face)
			return;

		m_Face.Release();
		m_Face = null;
		LF_Debug.Info(LF_TownBoard.FEATURE, string.Format("face released at %1 (%2)", GetOwner().GetOrigin(), why));
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (s_aBoards)
			s_aBoards.RemoveItem(this);

		ReleaseFace("deleted");
		super.OnDelete(owner);
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_TownBoardComponent's statics (registered at script load; Core/LF_StaticReset.c).
class LF_TownBoardComponent_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_TownBoardComponent_StaticResetHook());

	override string Owner()
	{
		return "LF_TownBoardComponent";
	}

	override void Apply()
	{
		LF_TownBoardComponent.ResetStatics();
	}
}
