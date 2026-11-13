//------------------------------------------------------------------------------------------------
//! The case board's face on an officer's client (#148). The wanted board's render target
//! (LF_WantedPosterFace: the local face quad, the cork, the live window and the freeze) with three
//! case columns instead of a row of posters: the suspect's sheet on top (the portrait once
//! identified, else an empty photo frame and UNKNOWN PERSON), the pinned evidence cards under it,
//! and a red string from each card's pin to the suspect's. A string is one LF_CaseBoardString.layout:
//! two pin heads and a chain of beads between them.
class LF_CaseBoardFace : LF_WantedPosterFace
{
	static const ResourceName STRING_LAYOUT = "{22DB59D5A86C7CD0}UI/Layouts/Investigation/LF_CaseBoardString.layout";

	//! A case column and the sheets in it, in face layout units (1920 x 1080).
	static const float COLUMN_WIDTH = 640;	// lf-fixed: three columns on the face
	static const float CASE_WIDTH = 300;	// lf-fixed: the layout size
	static const float CASE_HEIGHT = 424;	// lf-fixed: the poster sheet's 512 x 724 at CASE_WIDTH
	static const float CASE_TOP = 36;	// lf-fixed: the layout size
	static const float CARD_WIDTH = 280;	// lf-fixed: an index card
	static const float CARD_HEIGHT = 196;	// lf-fixed: an index card
	static const float CARD_GAP = 24;	// lf-fixed: the layout size
	static const float CARD_TOP = 520;	// lf-fixed: below the suspect's sheet, room for the strings
	//! The pin on a card, from its top edge; the pin on the suspect's sheet, from its bottom edge.
	static const float CARD_PIN = 16;	// lf-fixed: the layout size
	static const float CASE_PIN = 22;	// lf-fixed: the layout size
	static const float PIN = 16;	// lf-fixed: the pin head
	//! The beads of a string (LF_CaseBoardString.layout has BEADS of them), BEAD units square, one
	//! every BEAD_STEP units at most; a longer string spaces them out.
	static const int BEADS = 64;	// lf-fixed: the layout has 64 beads
	static const float BEAD = 6;	// lf-fixed: the layout size
	static const float BEAD_STEP = 5;	// lf-fixed: close enough to read as one thread

	protected int m_iCases;
	protected int m_iCards;

	//------------------------------------------------------------------------------------------------
	//! Where the column of case `index` of `count` starts: the columns are centred on the face.
	static float ColumnLeft(int index, int count)
	{
		int shown = Math.ClampInt(count, 1, LF_CaseBoard.MAX_CASES);
		return (FACE_WIDTH - shown * COLUMN_WIDTH) * 0.5 + index * COLUMN_WIDTH;
	}

	//------------------------------------------------------------------------------------------------
	//! Top-left of card `index` in a column starting at `left`: two rows of two.
	static vector CardPosition(float left, int index)
	{
		float rowWidth = 2 * CARD_WIDTH + CARD_GAP;
		int col = index % 2;
		int row = index / 2;
		float x = left + (COLUMN_WIDTH - rowWidth) * 0.5 + col * (CARD_WIDTH + CARD_GAP);
		float y = CARD_TOP + row * (CARD_HEIGHT + CARD_GAP);
		return Vector(x, y, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! The string from a card's pin to the suspect sheet's pin: centre, length and angle (degrees,
	//! clockwise on the face, y down). The line starts on the card's pin and turns about it.
	static void StringBetween(vector from, vector to, out vector centre, out float length, out float angle)
	{
		centre = (from + to) * 0.5;
		float dx = to[0] - from[0];
		float dy = to[1] - from[1];
		length = Math.Sqrt(dx * dx + dy * dy);
		angle = Math.Atan2(dy, dx) * Math.RAD2DEG;
	}

	//------------------------------------------------------------------------------------------------
	//! Replace the cases and render them live, then freeze (the wanted board's windows).
	//! \return Cases drawn.
	override int Show(notnull array<string> entries)
	{
		if (!IsBuilt())
			return 0;

		ClearPosters();
		m_iCases = 0;
		m_iCards = 0;
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return 0;

		array<ref LF_CaseBoardCase> cases = {};
		LF_CaseBoard.Decode(entries, cases);
		foreach (int index, LF_CaseBoardCase entry : cases)
		{
			if (!DrawCase(workspace, entry, ColumnLeft(index, cases.Count())))
				break;

			m_iCases++;
		}

		Thaw(LIVE_MS);
		ScriptCallQueue queue = GetGame().GetCallqueue();
		if (queue)
		{
			queue.Remove(Refresh);
			queue.Remove(ReleaseDummies);
			queue.CallLater(Refresh, SETTLE_REFRESH_MS, false);
			queue.CallLater(Refresh, LATE_REFRESH_MS, false);
			queue.CallLater(ReleaseDummies, DUMMY_RELEASE_MS, false);
		}

		return m_iCases;
	}

	//------------------------------------------------------------------------------------------------
	int GetCaseCount()
	{
		return m_iCases;
	}

	//------------------------------------------------------------------------------------------------
	int GetCardCount()
	{
		return m_iCards;
	}

	//------------------------------------------------------------------------------------------------
	//! One column: the suspect's sheet, the cards, the strings. False (logged) when a layout is missing.
	protected bool DrawCase(notnull WorkspaceWidget workspace, notnull LF_CaseBoardCase entry, float left)
	{
		Widget sheet = workspace.CreateWidgets(POSTER_LAYOUT, m_wPosters);
		if (!sheet)
		{
			LF_Debug.Error(LF_CaseBoard.FEATURE, "case sheet layout does not load: " + POSTER_LAYOUT);
			return false;
		}

		float sheetLeft = left + (COLUMN_WIDTH - CASE_WIDTH) * 0.5;
		Place(sheet, sheetLeft, CASE_TOP, CASE_WIDTH, CASE_HEIGHT, 1);
		FillCase(sheet, entry);
		m_aPosters.Insert(sheet);
		vector suspectPin = Vector(sheetLeft + CASE_WIDTH * 0.5, CASE_TOP + CASE_HEIGHT - CASE_PIN, 0);

		array<vector> pins = {};
		foreach (int i, string kind : entry.m_aKinds)
		{
			Widget card = workspace.CreateWidgets(POSTER_LAYOUT, m_wPosters);
			if (!card)
				return false;

			vector at = CardPosition(left, i);
			Place(card, at[0], at[1], CARD_WIDTH, CARD_HEIGHT, 1);
			FillCard(card, kind, entry.m_aNotes[i]);
			m_aPosters.Insert(card);
			pins.Insert(Vector(at[0] + CARD_WIDTH * 0.5, at[1] + CARD_PIN, 0));
			m_iCards++;
		}

		// The strings go over the paper: pinned on top of the sheets they join.
		foreach (vector cardPin : pins)
		{
			if (!DrawString(workspace, cardPin, suspectPin))
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! One red thread between two pins.
	protected bool DrawString(notnull WorkspaceWidget workspace, vector from, vector to)
	{
		Widget root = workspace.CreateWidgets(STRING_LAYOUT, m_wPosters);
		if (!root)
		{
			LF_Debug.Error(LF_CaseBoard.FEATURE, "case string layout does not load: " + STRING_LAYOUT);
			return false;
		}

		Place(root, 0, 0, FACE_WIDTH, FACE_HEIGHT, 1);
		m_aPosters.Insert(root);
		vector centre;
		float length;
		float angle;
		StringBetween(from, to, centre, length, angle);
		// The thread is a chain of beads: small squares along the line, close enough to read as one
		// string. A single image turned in the render target is shrunk to fit its own unturned box (a
		// 1 cm dash between the pins), whatever size it is given (docs/architecture/visual-boards.md).
		int steps = Math.Ceil(length / BEAD_STEP);
		int beads = Math.ClampInt(steps + 1, 2, BEADS);
		for (int i = 0; i < BEADS; i++)
		{
			Widget bead = root.FindAnyWidget("Bead" + i.ToString());
			if (!bead)
				continue;

			if (i >= beads)
			{
				bead.SetVisible(false);
				continue;
			}

			float along = i / (beads - 1.0);
			vector at = from + (to - from) * along;
			Place(bead, at[0] - BEAD * 0.5, at[1] - BEAD * 0.5, BEAD, BEAD, 1);
		}

		Place(root.FindAnyWidget("PinA"), from[0] - PIN * 0.5, from[1] - PIN * 0.5, PIN, PIN, 1);
		Place(root.FindAnyWidget("PinB"), to[0] - PIN * 0.5, to[1] - PIN * 0.5, PIN, PIN, 1);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A text line in a sheet's own units; an empty text hides the line.
	protected static void SheetLine(Widget sheet, string widgetName, string text, float x, float y, float width, float height, float font)
	{
		TextWidget w = TextWidget.Cast(sheet.FindAnyWidget(widgetName));
		if (!w)
			return;

		Place(w, x, y, width, height, 1);
		w.SetExactFontSize(Math.Round(font));
		w.SetTextWrapping(true);
		w.SetText(text);
		w.SetVisible(!text.IsEmpty());
	}

	//------------------------------------------------------------------------------------------------
	//! CASE n, the photo frame (the portrait once identified), the name or UNKNOWN PERSON, the offence,
	//! the place and the time.
	protected void FillCase(notnull Widget sheet, notnull LF_CaseBoardCase entry)
	{
		float scale = CASE_WIDTH / DESIGN_WIDTH;
		Place(sheet.FindAnyWidget("Paper"), 0, 0, DESIGN_WIDTH, 724, scale);
		Place(sheet.FindAnyWidget("PhotoBack"), 96, 112, 320, 340, scale);
		Widget clip = sheet.FindAnyWidget("PhotoClip");
		Place(clip, 96, 112, 320, 340, scale);
		if (clip)
			clip.SetFlags(WidgetFlags.CLIPCHILDREN);

		ItemPreviewWidget photo = ItemPreviewWidget.Cast(sheet.FindAnyWidget("Photo"));
		Place(photo, MUGSHOT_X, MUGSHOT_Y, MUGSHOT_WIDTH, MUGSHOT_HEIGHT, scale);
		LF_WantedPortrait portrait = new LF_WantedPortrait();
		int dressed = portrait.Show(photo, entry.m_sAppearance);
		m_aPortraits.Insert(portrait);
		if (dressed >= 0)
			LF_Debug.Info(LF_CaseBoard.FEATURE, string.Format("case #%1: portrait of the identified suspect, %2 item(s)", entry.m_iReport, dressed));

		Widget plate = sheet.FindAnyWidget("PlateBack");
		if (plate)
			plate.SetVisible(false);

		string header = WidgetManager.Translate("#LF-CaseBoard_CaseHeader", entry.m_iReport);
		header.ToUpper();
		string name = entry.m_sName;
		if (name.IsEmpty())
			name = WidgetManager.Translate("#LF-Wanted_Unknown");

		name.ToUpper();
		string where = LF_WantedBoard.ResolvePlace(entry.m_sPlace);
		if (!entry.m_sTime.IsEmpty())
			where = WidgetManager.Translate("#LF-CaseBoard_PlaceTime", where, entry.m_sTime);

		float x = 16 * scale;
		float width = (DESIGN_WIDTH - 32) * scale;
		SheetLine(sheet, "Header", header, x, 10 * scale, width, 96 * scale, 80 * scale);
		SheetLine(sheet, "Name", name, x, 458 * scale, width, 70 * scale, 46 * scale);
		SheetLine(sheet, "Offence", LF_WantedBoard.ResolveKey(entry.m_sOffence), x, 528 * scale, width, 56 * scale, 36 * scale);
		SheetLine(sheet, "Detail", where, x, 590 * scale, width, 110 * scale, 30 * scale);
		SheetLine(sheet, "Reward", string.Empty, x, 664 * scale, width, 50 * scale, 34 * scale);
	}

	//------------------------------------------------------------------------------------------------
	//! An index card: the kind of evidence in red, what it says under it.
	protected void FillCard(notnull Widget card, string kind, string note)
	{
		Place(card.FindAnyWidget("Paper"), 0, 0, CARD_WIDTH, CARD_HEIGHT, 1);
		array<string> photoParts = {"PhotoBack", "PhotoClip", "PlateBack"};
		foreach (string part : photoParts)
		{
			Widget w = card.FindAnyWidget(part);
			if (w)
				w.SetVisible(false);
		}

		string title = KindTitle(kind);
		title.ToUpper();
		string text = LF_ScenePieces.NoteText(kind, note);
		if (text.IsEmpty())
			text = WidgetManager.Translate("#LF-CaseBoard_NoNote");

		SheetLine(card, "Header", title, 12, CARD_PIN + 12, CARD_WIDTH - 24, 40, 24);
		SheetLine(card, "Name", string.Empty, 12, 0, CARD_WIDTH - 24, 1, 8);
		SheetLine(card, "Offence", string.Empty, 12, 0, CARD_WIDTH - 24, 1, 8);
		SheetLine(card, "Detail", text, 12, CARD_PIN + 58, CARD_WIDTH - 24, CARD_HEIGHT - CARD_PIN - 66, 24);
		SheetLine(card, "Reward", string.Empty, 12, 0, CARD_WIDTH - 24, 1, 8);
	}

	//------------------------------------------------------------------------------------------------
	//! The card title of an evidence kind; an unknown kind reads as itself.
	static string KindTitle(string kind)
	{
		if (kind == LF_Evidence.KIND_CASING)
			return WidgetManager.Translate("#LF-CaseBoard_Kind_casing");

		if (kind == LF_Evidence.KIND_DROPPED)
			return WidgetManager.Translate("#LF-CaseBoard_Kind_dropped");

		if (kind == LF_Evidence.KIND_FOOTPRINT)
			return WidgetManager.Translate("#LF-CaseBoard_Kind_footprint");

		if (kind == LF_Evidence.KIND_STATEMENT)
			return WidgetManager.Translate("#LF-CaseBoard_Kind_statement");

		return kind;
	}
}

//------------------------------------------------------------------------------------------------
//! An officer's client: the case list the server sent, which only an on-duty officer gets, and the
//! one timer for every case board here (once a second: a duty check and a distance check, not a data
//! poll). Coming on duty asks the server for the list (a late joiner on duty asks at its first
//! tick); going off duty drops it and releases the faces.
class LF_CaseBoardView
{
	static const int TICK_MS = 1000;	// lf-fixed: a distance and duty check, not tuning

	protected static ref array<string> s_aEntries;
	protected static bool s_bOnDuty;
	protected static bool s_bTicking;
	protected static int s_iReceived;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		if (s_bTicking && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Tick);

		s_aEntries = null;
		s_bOnDuty = false;
		s_bTicking = false;
		s_iReceived = 0;
	}

	//------------------------------------------------------------------------------------------------
	static void EnsureTicking()
	{
		if (s_bTicking || !GetGame() || !GetGame().GetCallqueue())
			return;

		s_bTicking = true;
		GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	//! The list this client holds; empty off duty.
	static array<string> Entries()
	{
		if (!s_aEntries)
			s_aEntries = {};

		return s_aEntries;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsOnDuty()
	{
		return s_bOnDuty;
	}

	//------------------------------------------------------------------------------------------------
	//! Lists received since the client started. Proofs read it.
	static int ReceivedCount()
	{
		return s_iReceived;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the server sent the list (only to an on-duty officer). Every built face redraws.
	static void Receive(array<string> entries, string source)
	{
		s_aEntries = {};
		if (entries)
		{
			foreach (string entry : entries)
				s_aEntries.Insert(entry);
		}

		s_iReceived++;
		LF_Debug.Info(LF_CaseBoard.FEATURE, string.Format("client: %1 entr(ies) from %2", s_aEntries.Count(), source));
		LF_CaseBoardComponent.ChangedAll();
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the duty state changed. On duty asks the server for the list; off duty drops it.
	static void DutyChanged(bool onDuty)
	{
		s_bOnDuty = onDuty;
		if (!onDuty)
		{
			s_aEntries = {};
			LF_Debug.Info(LF_CaseBoard.FEATURE, "client: off duty, the case board shows plain cork");
			LF_CaseBoardComponent.ReleaseAll("off duty");
			return;
		}

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (controller)
			controller.LF_AskCaseBoard();
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tick()
	{
		IEntity me = LF_Utils.GetLocalCharacter();
		bool onDuty = me && LF_AccountStateBits.IsOnDutyPolice(LF_AccountReplication.GetState(me));
		if (onDuty != s_bOnDuty)
			DutyChanged(onDuty);

		if (!s_bOnDuty)
			return;

		vector viewer;
		if (LF_WantedPosterFaces.ViewerPosition(viewer))
			LF_CaseBoardComponent.ViewAll(viewer);
	}
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Investigation", description: "Draws the open cases on the police case board for on-duty officers near it")]
class LF_CaseBoardComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! The case board prop (Prefabs/Investigation/LF_CaseBoard.et). It holds no case data: the list
//! reaches an officer's client by its player controller (LF_CaseBoard.SendTo), never by the prop's
//! replication, so a client off duty never has it. On an on-duty officer's client, each board draws
//! its face within DRAW_RADIUS and releases it beyond, like the wanted board.
class LF_CaseBoardComponent : ScriptComponent
{
	//! Indoors, by the evidence locker: drawn for a viewer in the station, not from the street.
	static const float DRAW_RADIUS = 15;	// lf-fixed: the board hangs indoors

	[Attribute("0 0.555 -0.006", UIWidgets.EditBox, "Face position relative to the board: the cork's centre, just in front of it")]
	protected vector m_vFaceOffset;

	[Attribute("-90 0 0", UIWidgets.EditBox, "Face rotation relative to the board (yaw pitch roll, degrees); LF_BoardFace.xob faces +X, the board -Z")]
	protected vector m_vFaceAngles;

	[Attribute("0.965", UIWidgets.EditBox, "Face scale. 1 is 1.78 x 1 m; the cork board's cork is about 1.71 x 0.96 m")]
	protected float m_fFaceScale;

	protected static ref array<LF_CaseBoardComponent> s_aBoards;

	protected ref LF_CaseBoardFace m_Face;
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
		LF_CaseBoardView.EnsureTicking();
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aBoards = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the distance gate for every board.
	static void ViewAll(vector viewer)
	{
		if (!s_aBoards)
			return;

		foreach (LF_CaseBoardComponent board : s_aBoards)
		{
			if (board)
				board.UpdateView(viewer);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Client: a new list. A built face is built again with it: a fresh render target draws its first
	//! frame every time, where redrawing a frozen one sometimes kept the old frame on a busy client
	//! (session, 2026-11-13: the pinned cards never reached the frozen face). A change is a pin, a
	//! crime or an identification, so this is rare; a build takes about 2 ms.
	static void ChangedAll()
	{
		if (!s_aBoards)
			return;

		foreach (LF_CaseBoardComponent board : s_aBoards)
		{
			if (board && board.m_Face && board.m_Face.IsBuilt())
			{
				board.ReleaseFace("new list");
				board.BuildFace();
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	static void ReleaseAll(string why)
	{
		if (!s_aBoards)
			return;

		foreach (LF_CaseBoardComponent board : s_aBoards)
		{
			if (board)
				board.ReleaseFace(why);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The nearest case board on this machine as "cases/cards/face" ("1/2/frozen"); "-" with none.
	//! The harness's `seen` reads it.
	static string DescribeNearest(vector origin)
	{
		LF_CaseBoardComponent nearest;
		float best = float.MAX;
		if (s_aBoards)
		{
			foreach (LF_CaseBoardComponent board : s_aBoards)
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
			return "0/0/released";

		string state = "live";
		if (m_Face.IsFrozen())
			state = "frozen";

		return string.Format("%1/%2/%3", m_Face.GetCaseCount(), m_Face.GetCardCount(), state);
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
		m_Face = new LF_CaseBoardFace();
		if (!m_Face.Build(GetOwner(), m_vFaceOffset, m_vFaceAngles, m_fFaceScale))
		{
			m_Face = null;
			m_bBuildFailed = true;
			return;
		}

		int drawn = m_Face.Show(LF_CaseBoardView.Entries());
		LF_Debug.Info(LF_CaseBoard.FEATURE, string.Format("client: face built at %1 with %2 case(s), %3 card(s) in %4 ms", GetOwner().GetOrigin(), drawn, m_Face.GetCardCount(), System.GetTickCount() - started));
	}

	//------------------------------------------------------------------------------------------------
	void ReleaseFace(string why)
	{
		if (!m_Face)
			return;

		m_Face.Release();
		m_Face = null;
		LF_Debug.Info(LF_CaseBoard.FEATURE, string.Format("client: face released at %1 (%2)", GetOwner().GetOrigin(), why));
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
//! The case board's wire: the client asks on its own controller (the only entity it may RPC on),
//! the server answers that owner only. The client sends nothing but the ask.
modded class SCR_PlayerController
{
	//! The server's last answer to this controller's ask, so a client cannot make it rebuild in a loop.
	protected int m_iLF_CaseBoardAskedAt = -100000;

	//------------------------------------------------------------------------------------------------
	//! Client: ask for the case list (coming on duty).
	void LF_AskCaseBoard()
	{
		if (Replication.IsServer())
		{
			LF_CaseBoard.SendTo(this);
			return;
		}

		Rpc(RpcAsk_LF_CaseBoard);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_CaseBoard()
	{
		int now = System.GetTickCount();
		if (now - m_iLF_CaseBoardAskedAt < 1000)
		{
			LF_Debug.Warn(LF_CaseBoard.FEATURE, string.Format("ask from player %1 ignored: asked again within a second", GetPlayerId()));
			return;
		}

		m_iLF_CaseBoardAskedAt = now;
		LF_CaseBoard.SendTo(this);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the case list to this player only.
	void LF_SendCaseBoard(array<string> entries)
	{
		if (GetGame().GetPlayerController() == this)
			RpcDo_LF_CaseBoard(entries);
		else
			Rpc(RpcDo_LF_CaseBoard, entries);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_CaseBoard(array<string> entries)
	{
		LF_CaseBoardView.Receive(entries, "rpc");
	}
}
