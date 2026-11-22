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
// It is drawn as a pencil circle with a cross, in graphite, the same hand as the personal marks, with its name
// and distance beside it, and a strip at the bottom of the map says how to set and clear it. The HUD marker
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
	//! The hint strip's first line, for the device in use.
	static string HintKey(bool pad)
	{
		if (pad)
			return "#LF-MapWaypoint_HintSetPad";

		return "#LF-MapWaypoint_HintSet";
	}

	//------------------------------------------------------------------------------------------------
	//! The hint strip's second line: how to set one another way with no waypoint, how to clear it with one.
	static string StatusKey(bool active, bool pad)
	{
		if (!active)
			return "#LF-MapWaypoint_HintNone";

		if (pad)
			return "#LF-MapWaypoint_HintActivePad";

		return "#LF-MapWaypoint_HintActive";
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
//! The map UI component: takes the waypoint clicks and the clear key, keeps one canvas over the map with the
//! mark, its name and distance beside it, and the hint strip. Listed in `Configs/Map/LF_MapFullscreen.conf`.
//! Client only (the map is).
class LF_MapWaypointUI : SCR_MapUIBaseComponent
{
	protected static const string SELECT_ACTION = "MapSelect";	// vanilla: left click, gamepad A
	protected static const ResourceName TAG_LAYOUT = "{4D2B9E6A1C73F808}UI/Layouts/Map/LF_MapPoiLabel.layout";	// the place names' paper-outlined type
	protected static const ResourceName HINT_LAYOUT = "{CC650B4F4E7A5A77}UI/Layouts/Map/LF_MapPoiCard.layout";	// the place card's paper slip
	protected static const float MARK_RADIUS = 14;	// lf-fixed: reference units, the same at every zoom like a pencil mark
	protected static const float MARK_WIDTH = 2.5;	// lf-fixed: a pencil stroke
	protected static const int MARK_COLOR = 0xFF2B2B2B;	// lf-fixed: graphite, the personal-mark ink
	//! A Shift + click this close to the ring's centre (a share of its radius) clears the waypoint.
	protected static const float CLEAR_REACH = 1.6;	// lf-fixed: on the ring or just outside it
	protected static const float TAG_GAP = 6;	// lf-fixed: reference units between the ring and its name
	protected static const float TAG_WIDTH = 320;	// lf-fixed: room for a long place name and its distance
	protected static const float HINT_WIDTH = 440;	// lf-fixed
	protected static const float HINT_HEIGHT = 58;	// lf-fixed: two lines
	protected static const float HINT_BOTTOM = 100;	// lf-fixed: above the vanilla scale bar and the server's link line
	protected static const int REFRESH_MS = 250;	// lf-fixed: the distance and the device's words follow at this pace

	protected CanvasWidget m_wCanvas;
	protected TextWidget m_wTag;
	protected Widget m_wHint;
	protected TextWidget m_wHintTitle;
	protected TextWidget m_wHintNote;
	protected ref array<ref CanvasWidgetCommand> m_aCommands = {};
	protected bool m_bDrawn;
	protected vector m_vDrawn;
	protected float m_fSinceRefresh;

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
			LF_MapWaypoint.BuildMark(screenX, screenY, workspace.DPIScale(MARK_RADIUS), segments);
			for (int i = 0; i + 3 < segments.Count(); i += 4)
			{
				LineDrawCommand line = new LineDrawCommand();
				line.m_Vertices = new array<float>();
				line.m_Vertices.Insert(segments[i]);
				line.m_Vertices.Insert(segments[i + 1]);
				line.m_Vertices.Insert(segments[i + 2]);
				line.m_Vertices.Insert(segments[i + 3]);
				line.m_fWidth = MARK_WIDTH;
				line.m_iColor = MARK_COLOR;
				m_aCommands.Insert(line);
			}

			if (m_wTag)
				FrameSlot.SetPos(m_wTag, workspace.DPIUnscale(screenX) + MARK_RADIUS + TAG_GAP, workspace.DPIUnscale(screenY) - LF_MapDeclutter.LABEL_HEIGHT * 0.5);
		}

		m_wCanvas.SetDrawCommands(m_aCommands);
		RefreshText();
	}

	//------------------------------------------------------------------------------------------------
	//! The tag's name and distance, and the hint strip's two lines.
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
		if (m_wTag)
		{
			m_wTag.SetVisible(m_bDrawn);
			if (m_bDrawn)
			{
				string tag = name;
				if (!distance.IsEmpty())
					tag = name + ", " + distance;

				m_wTag.SetText(tag);
				FrameSlot.SetSize(m_wTag, LF_MapDeclutter.LabelWidth(tag), LF_MapDeclutter.LABEL_HEIGHT);
			}
		}

		if (m_wHintTitle)
			m_wHintTitle.SetText(WidgetManager.Translate(LF_MapWaypoint.HintKey(pad)));

		if (m_wHintNote)
			m_wHintNote.SetText(WidgetManager.Translate(LF_MapWaypoint.StatusKey(m_bDrawn, pad), name, distance));
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
		BuildHint(workspace);

		m_MapEntity.GetOnMapPan().Insert(OnMapPan);
		m_MapEntity.GetOnMapZoom().Insert(OnMapZoom);
		InputManager input = GetGame().GetInputManager();
		input.AddActionListener(SELECT_ACTION, EActionTrigger.DOWN, OnSelect);
		input.AddActionListener(LF_MapWaypoint.CLEAR_ACTION, EActionTrigger.DOWN, OnClear);
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
	//! The paper slip at the bottom of the map: how to set a waypoint, and the one set now with how to clear it.
	protected void BuildHint(notnull WorkspaceWidget workspace)
	{
		m_wHint = workspace.CreateWidgets(HINT_LAYOUT, m_RootWidget);
		if (!m_wHint)
		{
			LF_Debug.Error(LF_Waypoint.FEATURE, "the waypoint hint was not created; the map shows no waypoint keys");
			return;
		}

		m_wHint.SetFlags(WidgetFlags.IGNORE_CURSOR);
		FrameSlot.SetAnchorMin(m_wHint, 0.5, 1);
		FrameSlot.SetAnchorMax(m_wHint, 0.5, 1);
		FrameSlot.SetAlignment(m_wHint, 0.5, 1);
		FrameSlot.SetSize(m_wHint, HINT_WIDTH, HINT_HEIGHT);
		FrameSlot.SetPos(m_wHint, 0, -HINT_BOTTOM);
		m_wHintTitle = TextWidget.Cast(m_wHint.FindAnyWidget("Title"));
		m_wHintNote = TextWidget.Cast(m_wHint.FindAnyWidget("Note"));
	}

	//------------------------------------------------------------------------------------------------
	override void OnMapClose(MapConfiguration config)
	{
		InputManager input = GetGame().GetInputManager();
		input.RemoveActionListener(SELECT_ACTION, EActionTrigger.DOWN, OnSelect);
		input.RemoveActionListener(LF_MapWaypoint.CLEAR_ACTION, EActionTrigger.DOWN, OnClear);
		if (m_MapEntity)
		{
			m_MapEntity.GetOnMapPan().Remove(OnMapPan);
			m_MapEntity.GetOnMapZoom().Remove(OnMapZoom);
		}

		if (m_wCanvas)
			m_wCanvas.RemoveFromHierarchy();

		if (m_wTag)
			m_wTag.RemoveFromHierarchy();

		if (m_wHint)
			m_wHint.RemoveFromHierarchy();

		m_wCanvas = null;
		m_wTag = null;
		m_wHint = null;
		m_wHintTitle = null;
		m_wHintNote = null;
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
