// LF_MapWaypointUI.c: the personal waypoint on the map (#2235, epic #2223, card M7;
// docs/architecture/everon-map.md section 3). Reworked after the 0.2.0 island playtest ("setting a waypoint
// is not intuitive"): a right click both opened vanilla's radial menu (which pans the map to the cursor) and
// dropped a waypoint, and nothing on the map said how to set one or clear it.
//
// One waypoint, as the town board has it: LF_Waypoint stays the single personal target.
// - Shift + left click on the map (gamepad: hold Y and press A) sets it there, or on the place under the
//   cursor with that place's name; the same on the waypoint clears it. The click is vanilla's MapSelect with
//   the LF_MapWaypointModifier action held, so one click never runs two paths.
// - A plain click on a place sets it too (LF_MapPlacesUI); right click is vanilla's menu only.
// - Backspace (gamepad: double-tap Y) clears it.
// It is drawn as a bold stamp-red ring with a cross on a light halo and a pale disc (the one red mark on the map), with its name
// and distance beside it, and the "How to use the map" card (bottom left, LF_MapHelp) says how to set and clear
// it. The HUD marker
// over the target (LF_WaypointHud) is the only GPS-like aid (D-M9): no route line and no bearing line is drawn.

//------------------------------------------------------------------------------------------------
//! The waypoint rules, with no map and no widgets, so a LOGIC test covers them (LF_Test_MapWaypoint,
//! LF_Test_MapWaypointClick).
class LF_MapWaypoint
{
	//! The LF_Waypoint id of a mark set on the map.
	static const string ID = "map";
	//! The mark's name when it is not on a place: shown under the HUD marker. A string key the widget translates.
	static const string LABEL = "#LF-MapWaypoint_Label";
	//! Held with a map click (MapSelect), it sets the waypoint: Shift, gamepad Y.
	static const string MODIFIER_ACTION = "LF_MapWaypointModifier";
	//! Clears the waypoint while the map is open: Backspace, gamepad Y twice.
	static const string CLEAR_ACTION = "LF_MapWaypointClear";
	//! A waypoint closer than this to a place is on that place.
	static const float SAME_PLACE_M = 1;	// lf-fixed: places are metres apart; a float rounding is not
	//! The pencil circle is this many straight segments.
	static const int CIRCLE_SEGMENTS = 24;	// lf-fixed: a round ring at a few dozen pixels
	//! The cross strokes run this far toward the ring (a fraction of its radius).
	static const float CROSS_REACH = 0.6;	// lf-fixed: a cross inside the ring, not touching it

	//------------------------------------------------------------------------------------------------
	//! A waypoint at the world position `world`, named "Waypoint". Sets the waypoint with the id `map`; when
	//! the map waypoint is already set it moves (LF_Waypoint.Mark on the same id would take it away). A board
	//! waypoint is replaced: there is one waypoint.
	//! \return true when the waypoint is set now.
	static bool SetFromMap(vector world)
	{
		if (LF_Waypoint.IsMarked(ID))
			LF_Waypoint.Clear("moved on the map");

		LF_Debug.Info(LF_Waypoint.FEATURE, string.Format("map click at %1", world));
		return LF_Waypoint.Mark(ID, LABEL, world);
	}

	//------------------------------------------------------------------------------------------------
	//! Shift + left click (gamepad Y + A) at `world`. Within `nearM` of the waypoint it clears it; over a place
	//! (`hasPlace`) it sets it on that place with the place's name; anywhere else it sets it at the click.
	//! \return "cleared", "place", "point" or "refused".
	static string ModifiedClick(vector world, float nearM, bool hasPlace, string title, vector placePos)
	{
		if (LF_Waypoint.IsActive() && vector.DistanceXZ(LF_Waypoint.GetTarget(), world) <= nearM)
		{
			LF_Waypoint.Clear("shift-clicked again on the map");
			return "cleared";
		}

		if (hasPlace)
		{
			if (LF_MapPlaceCard.SetWaypoint(title, placePos))
				return "place";

			return "refused";
		}

		if (SetFromMap(world))
			return "point";

		return "refused";
	}

	//------------------------------------------------------------------------------------------------
	//! The clear key on the map. \return true when there was a waypoint to clear.
	static bool ClearFromMap()
	{
		return LF_Waypoint.Clear("cleared on the map");
	}

	//------------------------------------------------------------------------------------------------
	//! What the map draws: the waypoint, whoever set it (the board, a place or a click).
	//! \return false when there is no waypoint.
	static bool Drawn(out vector position, out string label)
	{
		if (!LF_Waypoint.IsActive())
			return false;

		position = LF_Waypoint.GetTarget();
		label = LF_Waypoint.GetLabel();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Flat metres from `from` to `to`, as the HUD marker prints them.
	static int DistanceMeters(vector from, vector to)
	{
		return Math.Round(vector.DistanceXZ(from, to));
	}

	//------------------------------------------------------------------------------------------------
	//! True when the waypoint carries no name of its own: its name is the generic "Waypoint".
	static bool IsUnnamed(string name, string genericName)
	{
		return name.IsEmpty() || name == genericName;
	}

	//------------------------------------------------------------------------------------------------
	//! What the strip says after "Waypoint: ": the distance alone for an unnamed waypoint ("95 m"), the place
	//! and the distance for a named one ("Cafe, 95 m"), so the word Waypoint is never written twice.
	static string Detail(string name, string genericName, string distance)
	{
		if (IsUnnamed(name, genericName))
		{
			if (distance.IsEmpty())
				return genericName;

			return distance;
		}

		if (distance.IsEmpty())
			return name;

		return name + ", " + distance;
	}

	//------------------------------------------------------------------------------------------------
	//! The tag beside the ring: "Waypoint: 95 m" for an unnamed waypoint, "Cafe, 95 m" for a named one.
	static string Tag(string name, string genericName, string distance)
	{
		if (!IsUnnamed(name, genericName))
			return Detail(name, genericName, distance);

		if (distance.IsEmpty())
			return genericName;

		return genericName + ": " + distance;
	}

	//------------------------------------------------------------------------------------------------
	//! True when the modifier (Shift, gamepad Y) is held now.
	static bool ModifierHeld()
	{
		InputManager input = GetGame().GetInputManager();
		return input && input.GetActionValue(MODIFIER_ACTION) > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! False while a click on the map belongs to something else: a vanilla pan, drawing or tool, the radial
	//! menu, or a focused widget (a dialog, the map key sheet).
	static bool MapTakesClicks(SCR_MapEntity mapEntity)
	{
		if (!mapEntity)
			return false;

		SCR_MapCursorModule cursor = SCR_MapCursorModule.Cast(mapEntity.GetMapModule(SCR_MapCursorModule));
		if (cursor && (cursor.GetCursorState() & (SCR_MapCursorModule.STATE_POPUP_RESTRICTED | EMapCursorState.CS_CONTEXTUAL_MENU)) != 0)
			return false;

		// A click on the key column (right edge) is the key's, not the map's under it.
		if (LF_MapLegendUI.CursorOverKey(SCR_MapCursorInfo.x, SCR_MapCursorInfo.y))
			return false;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		return !workspace || !workspace.GetFocusedWidget();
	}

	//------------------------------------------------------------------------------------------------
	//! The pencil circle with a cross as segments, x0 y0 x1 y1 each, centred at (cx, cy) in screen units:
	//! CIRCLE_SEGMENTS for the ring, then two crossing strokes. Every point is within `radius`.
	static void BuildMark(float cx, float cy, float radius, notnull array<float> into)
	{
		for (int i = 0; i < CIRCLE_SEGMENTS; i++)
		{
			float a0 = Math.PI2 * i / CIRCLE_SEGMENTS;
			float a1 = Math.PI2 * (i + 1) / CIRCLE_SEGMENTS;
			into.Insert(cx + radius * Math.Cos(a0));
			into.Insert(cy + radius * Math.Sin(a0));
			into.Insert(cx + radius * Math.Cos(a1));
			into.Insert(cy + radius * Math.Sin(a1));
		}

		float reach = radius * CROSS_REACH;
		into.Insert(cx - reach);
		into.Insert(cy - reach);
		into.Insert(cx + reach);
		into.Insert(cy + reach);
		into.Insert(cx - reach);
		into.Insert(cy + reach);
		into.Insert(cx + reach);
		into.Insert(cy - reach);
	}
}

//------------------------------------------------------------------------------------------------
//! The "How to use the map" card in the bottom-left corner (map clean-up, 2026-11-22: "instructions on how to set
//! waypoints on it"): one line per thing the map does, in the words of the device in use, and the waypoint set
//! now. F1 (gamepad: click the left stick) folds it to one line; the choice is kept in the player's own profile
//! (`$profile:LifeFramework/map-help.txt`), so it stays folded on the next open and the next session.
//! The rules have no widgets, so a LOGIC test covers them (LF_Test_MapHelp).
class LF_MapHelp
{
	//! Folds or unfolds the card while the map is open: F1, gamepad left stick click.
	static const string TOGGLE_ACTION = "LF_MapHelpToggle";
	static const string PREF_DIR = "$profile:LifeFramework";
	static const string PREF_PATH = "$profile:LifeFramework/map-help.txt";
	static const string PREF_OPEN = "open";
	static const string PREF_FOLDED = "folded";
	//! Card layout in reference units: the title row, one body row per line, the waypoint row.
	static const float CARD_WIDTH = 570;	// lf-fixed: the longest line at 18 px
	static const float TITLE_ROW = 42;	// lf-fixed: the layout's title slot
	static const float LINE_ROW = 25;	// lf-fixed: 18 px type and its leading
	static const float STATUS_ROW = 46;	// lf-fixed: the layout's status slot and the bottom margin

	//------------------------------------------------------------------------------------------------
	//! The title: what the card is, and how to fold or unfold it with the device in use.
	static string TitleKey(bool pad, bool folded)
	{
		if (folded)
		{
			if (pad)
				return "#LF-MapHelp_FoldedPad";

			return "#LF-MapHelp_Folded";
		}

		if (pad)
			return "#LF-MapHelp_TitlePad";

		return "#LF-MapHelp_Title";
	}

	//------------------------------------------------------------------------------------------------
	//! The body lines, in order: set, clear, click a place, hover, legend. None when folded.
	static int LineKeys(bool pad, bool folded, notnull array<string> into)
	{
		into.Clear();
		if (folded)
			return 0;

		if (pad)
		{
			into.Insert("#LF-MapHelp_SetPad");
			into.Insert("#LF-MapHelp_ClearPad");
			into.Insert("#LF-MapHelp_PlacePad");
			into.Insert("#LF-MapHelp_HoverPad");
		}
		else
		{
			into.Insert("#LF-MapHelp_Set");
			into.Insert("#LF-MapHelp_Clear");
			into.Insert("#LF-MapHelp_Place");
			into.Insert("#LF-MapHelp_Hover");
		}

		into.Insert("#LF-MapHelp_Legend");
		return into.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! The waypoint row: "Waypoint: Cafe, 95 m" with one set, "No waypoint set" without.
	static string WaypointKey(bool active)
	{
		if (active)
			return "#LF-MapHelp_Waypoint";

		return "#LF-MapHelp_NoWaypoint";
	}

	//------------------------------------------------------------------------------------------------
	//! The card's height for `lines` body lines.
	static float CardHeight(int lines)
	{
		return TITLE_ROW + lines * LINE_ROW + STATUS_ROW;
	}

	//------------------------------------------------------------------------------------------------
	//! The saved choice: folded only when the file says so; a missing or unreadable file shows the card open,
	//! so a new player sees the instructions.
	static bool ParseFolded(string text)
	{
		string word = text.Trim();
		word.ToLower();
		return word == PREF_FOLDED;
	}

	//------------------------------------------------------------------------------------------------
	static string PrefText(bool folded)
	{
		if (folded)
			return PREF_FOLDED;

		return PREF_OPEN;
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the player's choice from their profile. \return true when the card should open folded.
	static bool LoadFolded(string path = PREF_PATH)
	{
		if (!FileIO.FileExists(path))
			return false;

		FileHandle handle = FileIO.OpenFile(path, FileMode.READ);
		if (!handle)
		{
			LF_Debug.Warn(LF_Waypoint.FEATURE, "map help: cannot read " + path + "; it opens unfolded");
			return false;
		}

		string line;
		handle.ReadLine(line);
		handle.Close();
		return ParseFolded(line);
	}

	//------------------------------------------------------------------------------------------------
	//! Keeps the player's choice in their profile. \return false when the file cannot be written.
	static bool SaveFolded(bool folded, string path = PREF_PATH)
	{
		FileIO.MakeDirectory(PREF_DIR);
		FileHandle handle = FileIO.OpenFile(path, FileMode.WRITE);
		if (!handle)
		{
			LF_Debug.Warn(LF_Waypoint.FEATURE, "map help: cannot write " + path + "; the choice lasts until the game closes");
			return false;
		}

		handle.WriteLine(PrefText(folded));
		handle.Close();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The map UI component: takes the waypoint clicks and the clear key, keeps one canvas over the map with the
//! mark, its name and distance beside it, and the "How to use the map" card. Listed in
//! `Configs/Map/LF_MapFullscreen.conf`. Client only (the map is).
class LF_MapWaypointUI : SCR_MapUIBaseComponent
{
	protected static const string SELECT_ACTION = "MapSelect";	// vanilla: left click, gamepad A
	protected static const ResourceName TAG_LAYOUT = "{4D2B9E6A1C73F808}UI/Layouts/Map/LF_MapPoiLabel.layout";	// the place names' paper-outlined type
	protected static const ResourceName HELP_LAYOUT = "{8E41C7A23B9D5F06}UI/Layouts/Map/LF_MapHelpCard.layout";	// the paper slip with a title, the lines and the waypoint row
	protected static const float MARK_RADIUS = 20;	// lf-fixed: reference units, the same at every zoom; larger than a badge
	protected static const float MARK_WIDTH = 4;	// lf-fixed: a bold stroke that reads over badges and roads
	protected static const int MARK_COLOR = 0xFFC62828;	// lf-fixed: stamp red, the one red mark on the map (2026-11-22: "more obvious")
	protected static const float HALO_WIDTH = 9;	// lf-fixed: a light rim round every stroke so the red reads on grey and yellow
	protected static const int HALO_COLOR = 0xF0FFFAF0;	// lf-fixed: the badges' halo paper
	protected static const int DISC_COLOR = 0x66FFFAF0;	// lf-fixed: a pale disc inside the ring lifts it off the map
	//! A Shift + click this close to the ring's centre (a share of its radius) clears the waypoint.
	protected static const float CLEAR_REACH = 1.6;	// lf-fixed: on the ring or just outside it
	protected static const float TAG_GAP = 6;	// lf-fixed: reference units between the ring and its name
	protected static const float TAG_WIDTH = 320;	// lf-fixed: room for a long place name and its distance
	protected static const float HELP_LEFT = 16;	// lf-fixed: the map's left margin
	protected static const float HELP_BOTTOM = 140;	// lf-fixed: above the vanilla zoom keys and the scale bar
	protected static const int REFRESH_MS = 250;	// lf-fixed: the distance and the device's words follow at this pace

	protected CanvasWidget m_wCanvas;
	protected TextWidget m_wTag;
	protected Widget m_wHelp;
	protected TextWidget m_wHelpTitle;
	protected TextWidget m_wHelpBody;
	protected TextWidget m_wHelpStatus;
	protected ref array<ref CanvasWidgetCommand> m_aCommands = {};
	protected ref array<string> m_aHelpKeys = {};
	protected bool m_bDrawn;
	protected vector m_vDrawn;
	protected float m_fSinceRefresh;
	protected bool m_bFolded;
	protected bool m_bHelpPad;
	protected int m_iHelpLines = -1;

	//------------------------------------------------------------------------------------------------
	//! Shift + left click (gamepad: Y held + A): set, move or clear the waypoint at the cursor.
	protected void OnSelect(float value, EActionTrigger reason)
	{
		if (!m_MapEntity || !m_wCanvas || IsToolMenuFocused() || !LF_MapWaypoint.ModifierHeld() || !LF_MapWaypoint.MapTakesClicks(m_MapEntity))
			return;

		float worldX;
		float worldZ;
		m_MapEntity.ScreenToWorldNoFlip(SCR_MapCursorInfo.Scale(SCR_MapCursorInfo.x), SCR_MapCursorInfo.Scale(SCR_MapCursorInfo.y), worldX, worldZ);
		vector world = Vector(worldX, 0, worldZ);
		BaseWorld baseWorld = GetGame().GetWorld();
		if (baseWorld)
			world[1] = baseWorld.GetSurfaceY(worldX, worldZ);

		string title;
		vector placePos;
		bool hasPlace;
		LF_MapPlacesUI places = LF_MapPlacesUI.Cast(m_MapEntity.GetMapUIComponent(LF_MapPlacesUI));
		if (places)
			hasPlace = places.PlaceAtCursor(title, placePos);

		string outcome = LF_MapWaypoint.ModifiedClick(world, ClearReachMeters(), hasPlace, title, placePos);
		LF_Debug.Info(LF_Waypoint.FEATURE, string.Format("shift-click on the map at %1: %2", world, outcome));
		Redraw();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnClear(float value, EActionTrigger reason)
	{
		if (!m_wCanvas || IsToolMenuFocused())
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (workspace && workspace.GetFocusedWidget())
			return;	// Backspace in a text field (the marker dialog, the map key search)

		if (LF_MapWaypoint.ClearFromMap())
			Redraw();
	}

	//------------------------------------------------------------------------------------------------
	//! F1 (gamepad: left stick click): fold or unfold the help card, and keep the choice.
	protected void OnHelpToggle(float value, EActionTrigger reason)
	{
		if (!m_wHelp || IsToolMenuFocused())
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (workspace && workspace.GetFocusedWidget())
			return;	// typing in the marker dialog or the map key search

		SetFolded(!m_bFolded);
	}

	//------------------------------------------------------------------------------------------------
	protected void SetFolded(bool folded)
	{
		m_bFolded = folded;
		LF_MapHelp.SaveFolded(folded);
		LF_Debug.Info(LF_Waypoint.FEATURE, string.Format("map help card folded=%1", folded));
		m_iHelpLines = -1;
		RefreshText();
	}

	//------------------------------------------------------------------------------------------------
	//! For the harness: fold ("closed") or unfold ("open") the help card on the open map, or read its state.
	static bool HarnessHelp(string word, out string detail)
	{
		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (!mapEntity || !mapEntity.IsOpen())
		{
			detail = "map_not_open";
			return false;
		}

		LF_MapWaypointUI ui = LF_MapWaypointUI.Cast(mapEntity.GetMapUIComponent(LF_MapWaypointUI));
		if (!ui || !ui.m_wHelp)
		{
			detail = "no_help_card";
			return false;
		}

		if (word == "open")
			ui.SetFolded(false);
		else if (word == "closed")
			ui.SetFolded(true);

		detail = string.Format("help folded=%1 lines=%2", ui.m_bFolded, ui.m_iHelpLines);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The ring's clear reach in metres at the current zoom.
	protected float ClearReachMeters()
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		float ppu = m_MapEntity.GetCurrentZoom();
		if (!workspace || ppu <= 0)
			return 0;

		return workspace.DPIScale(MARK_RADIUS * CLEAR_REACH) / ppu;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnMapPan(float x, float y, bool adjustedPan)
	{
		Redraw();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnMapZoom(float pixelPerUnit)
	{
		Redraw();
	}

	//------------------------------------------------------------------------------------------------
	//! Draws the mark and its tag at the waypoint's place on the screen, or nothing with no waypoint.
	protected void Redraw()
	{
		if (!m_wCanvas)
			return;

		m_aCommands.Clear();
		vector world;
		string label;
		m_bDrawn = LF_MapWaypoint.Drawn(world, label);
		if (m_bDrawn)
		{
			m_vDrawn = world;
			float screenX;
			float screenY;
			m_MapEntity.WorldToScreen(world[0], world[2], screenX, screenY, true);
			WorkspaceWidget workspace = GetGame().GetWorkspace();
			array<float> segments = {};
			// Canvas commands take screen pixels as WorldToScreen gives them (unscaling put the mark 1.5 times
			// too far from the corner at 720p, the map proof pass); the radius is reference units, so scale it.
			float radius = workspace.DPIScale(MARK_RADIUS);
			LF_MapWaypoint.BuildMark(screenX, screenY, radius, segments);
			// The pale disc: the ring's own points as one polygon.
			PolygonDrawCommand disc = new PolygonDrawCommand();
			disc.m_iColor = DISC_COLOR;
			disc.m_Vertices = new array<float>();
			for (int d = 0; d + 3 < LF_MapWaypoint.CIRCLE_SEGMENTS * 4; d += 4)
			{
				disc.m_Vertices.Insert(segments[d]);
				disc.m_Vertices.Insert(segments[d + 1]);
			}

			m_aCommands.Insert(disc);
			// Every stroke twice: the light halo under it, then the red.
			AddStrokes(segments, workspace.DPIScale(HALO_WIDTH), HALO_COLOR);
			AddStrokes(segments, workspace.DPIScale(MARK_WIDTH), MARK_COLOR);

			if (m_wTag)
				FrameSlot.SetPos(m_wTag, workspace.DPIUnscale(screenX) + MARK_RADIUS + TAG_GAP, workspace.DPIUnscale(screenY) - LF_MapDeclutter.LABEL_HEIGHT * 0.5);
		}

		m_wCanvas.SetDrawCommands(m_aCommands);
		RefreshText();
	}

	//------------------------------------------------------------------------------------------------
	//! One line command per segment (x0 y0 x1 y1 each) at the given width and colour.
	protected void AddStrokes(notnull array<float> segments, float width, int color)
	{
		for (int i = 0; i + 3 < segments.Count(); i += 4)
		{
			LineDrawCommand line = new LineDrawCommand();
			line.m_Vertices = new array<float>();
			line.m_Vertices.Insert(segments[i]);
			line.m_Vertices.Insert(segments[i + 1]);
			line.m_Vertices.Insert(segments[i + 2]);
			line.m_Vertices.Insert(segments[i + 3]);
			line.m_fWidth = width;
			line.m_iColor = color;
			m_aCommands.Insert(line);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The tag's name and distance, and the help card's waypoint row.
	protected void RefreshText()
	{
		bool pad = !GetGame().GetInputManager().IsUsingMouseAndKeyboard();
		string distance;
		if (m_bDrawn)
		{
			IEntity me = SCR_PlayerController.GetLocalControlledEntity();
			if (me)
				distance = WidgetManager.Translate("#LF-Waypoint_Distance", LF_MapWaypoint.DistanceMeters(me.GetOrigin(), m_vDrawn));
		}

		string name = LF_Terms.Expand(WidgetManager.Translate(LF_Waypoint.GetLabel()));
		string genericName = WidgetManager.Translate(LF_MapWaypoint.LABEL);
		if (m_wTag)
		{
			m_wTag.SetVisible(m_bDrawn);
			if (m_bDrawn)
			{
				string tag = LF_MapWaypoint.Tag(name, genericName, distance);
				m_wTag.SetText(tag);
				FrameSlot.SetSize(m_wTag, LF_MapDeclutter.LabelWidth(tag), LF_MapDeclutter.LABEL_HEIGHT);
			}
		}

		RefreshHelp(pad, LF_MapWaypoint.Detail(name, genericName, distance));
	}

	//------------------------------------------------------------------------------------------------
	//! The help card's title, lines and waypoint row. The lines are rebuilt only when the device or the fold
	//! changes; the waypoint row follows every refresh.
	protected void RefreshHelp(bool pad, string waypointDetail)
	{
		if (!m_wHelp)
			return;

		if (m_iHelpLines < 0 || pad != m_bHelpPad)
		{
			m_bHelpPad = pad;
			m_iHelpLines = LF_MapHelp.LineKeys(pad, m_bFolded, m_aHelpKeys);
			string body;
			foreach (int i, string key : m_aHelpKeys)
			{
				if (i > 0)
					body += "\n";

				body += WidgetManager.Translate(key);
			}

			if (m_wHelpTitle)
				m_wHelpTitle.SetText(WidgetManager.Translate(LF_MapHelp.TitleKey(pad, m_bFolded)));

			if (m_wHelpBody)
			{
				m_wHelpBody.SetText(body);
				m_wHelpBody.SetVisible(m_iHelpLines > 0);
			}

			FrameSlot.SetSize(m_wHelp, LF_MapHelp.CARD_WIDTH, LF_MapHelp.CardHeight(m_iHelpLines));
		}

		if (m_wHelpStatus)
			m_wHelpStatus.SetText(WidgetManager.Translate(LF_MapHelp.WaypointKey(m_bDrawn), waypointDetail));
	}

	//------------------------------------------------------------------------------------------------
	override void OnMapOpen(MapConfiguration config)
	{
		super.OnMapOpen(config);

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace || !m_RootWidget)
		{
			LF_Debug.Error(LF_Waypoint.FEATURE, "the map has no root widget; the waypoint is not drawn on the map");
			return;
		}

		m_wCanvas = CanvasWidget.Cast(workspace.CreateWidget(WidgetType.CanvasWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.NOFOCUS, Color.FromInt(Color.WHITE), 0, m_RootWidget));
		if (!m_wCanvas)
		{
			LF_Debug.Error(LF_Waypoint.FEATURE, "the waypoint canvas was not created; the waypoint is not drawn on the map");
			return;
		}

		FrameSlot.SetAnchorMin(m_wCanvas, 0, 0);
		FrameSlot.SetAnchorMax(m_wCanvas, 1, 1);
		FrameSlot.SetOffsets(m_wCanvas, 0, 0, 0, 0);
		BuildTag(workspace);
		BuildHelp(workspace);

		m_MapEntity.GetOnMapPan().Insert(OnMapPan);
		m_MapEntity.GetOnMapZoom().Insert(OnMapZoom);
		InputManager input = GetGame().GetInputManager();
		input.AddActionListener(SELECT_ACTION, EActionTrigger.DOWN, OnSelect);
		input.AddActionListener(LF_MapWaypoint.CLEAR_ACTION, EActionTrigger.DOWN, OnClear);
		input.AddActionListener(LF_MapHelp.TOGGLE_ACTION, EActionTrigger.DOWN, OnHelpToggle);
		m_fSinceRefresh = 0;
		Redraw();
	}

	//------------------------------------------------------------------------------------------------
	//! The waypoint's name beside its ring, in the place names' type.
	protected void BuildTag(notnull WorkspaceWidget workspace)
	{
		m_wTag = TextWidget.Cast(workspace.CreateWidgets(TAG_LAYOUT, m_RootWidget));
		if (!m_wTag)
		{
			LF_Debug.Error(LF_Waypoint.FEATURE, "the waypoint tag was not created; the ring is drawn without its name");
			return;
		}

		m_wTag.SetFlags(WidgetFlags.IGNORE_CURSOR);
		FrameSlot.SetSize(m_wTag, TAG_WIDTH, LF_MapDeclutter.LABEL_HEIGHT);
		m_wTag.SetVisible(false);
	}

	//------------------------------------------------------------------------------------------------
	//! The "How to use the map" card in the bottom-left corner, folded or not as the player left it.
	protected void BuildHelp(notnull WorkspaceWidget workspace)
	{
		m_wHelp = workspace.CreateWidgets(HELP_LAYOUT, m_RootWidget);
		if (!m_wHelp)
		{
			LF_Debug.Error(LF_Waypoint.FEATURE, "the map help card was not created; the map shows no instructions");
			return;
		}

		m_wHelp.SetFlags(WidgetFlags.IGNORE_CURSOR);
		FrameSlot.SetAnchorMin(m_wHelp, 0, 1);
		FrameSlot.SetAnchorMax(m_wHelp, 0, 1);
		FrameSlot.SetAlignment(m_wHelp, 0, 1);
		FrameSlot.SetPos(m_wHelp, HELP_LEFT, -HELP_BOTTOM);
		m_wHelpTitle = TextWidget.Cast(m_wHelp.FindAnyWidget("Title"));
		m_wHelpBody = TextWidget.Cast(m_wHelp.FindAnyWidget("Body"));
		m_wHelpStatus = TextWidget.Cast(m_wHelp.FindAnyWidget("Status"));
		m_bFolded = LF_MapHelp.LoadFolded();
		m_iHelpLines = -1;
	}

	//------------------------------------------------------------------------------------------------
	override void OnMapClose(MapConfiguration config)
	{
		InputManager input = GetGame().GetInputManager();
		input.RemoveActionListener(SELECT_ACTION, EActionTrigger.DOWN, OnSelect);
		input.RemoveActionListener(LF_MapWaypoint.CLEAR_ACTION, EActionTrigger.DOWN, OnClear);
		input.RemoveActionListener(LF_MapHelp.TOGGLE_ACTION, EActionTrigger.DOWN, OnHelpToggle);
		if (m_MapEntity)
		{
			m_MapEntity.GetOnMapPan().Remove(OnMapPan);
			m_MapEntity.GetOnMapZoom().Remove(OnMapZoom);
		}

		if (m_wCanvas)
			m_wCanvas.RemoveFromHierarchy();

		if (m_wTag)
			m_wTag.RemoveFromHierarchy();

		if (m_wHelp)
			m_wHelp.RemoveFromHierarchy();

		m_wCanvas = null;
		m_wTag = null;
		m_wHelp = null;
		m_wHelpTitle = null;
		m_wHelpBody = null;
		m_wHelpStatus = null;
		m_iHelpLines = -1;
		m_aCommands.Clear();
		m_bDrawn = false;
		super.OnMapClose(config);
	}

	//------------------------------------------------------------------------------------------------
	//! A waypoint set or cleared elsewhere while the map is open (a place click, the board, an arrival) shows
	//! or goes; the distance and the device's words follow a few times a second.
	override void Update(float timeSlice)
	{
		if (!m_wCanvas)
			return;

		vector world;
		string label;
		bool drawn = LF_MapWaypoint.Drawn(world, label);
		if (drawn != m_bDrawn || (drawn && world != m_vDrawn))
		{
			Redraw();
			m_fSinceRefresh = 0;
			return;
		}

		m_fSinceRefresh += timeSlice * 1000;
		if (m_fSinceRefresh < REFRESH_MS)
			return;

		m_fSinceRefresh = 0;
		RefreshText();
	}
}
