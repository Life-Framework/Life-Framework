// LF_MapWaypointUI.c: the personal waypoint on the map (#2235, epic #2223, card M7;
// docs/architecture/everon-map.md section 3).
//
// One waypoint, as the town board has it: LF_Waypoint stays the single personal target. Right-click on the
// map (gamepad: the contextual-menu button) sets it with the id `map`, a second click moves it, and a
// waypoint the board set is drawn here too. It is drawn as a pencil circle with a cross, in graphite, the
// same hand as the personal marks. The HUD marker over the target (LF_WaypointHud) is the only GPS-like aid
// (D-M9): no route line and no bearing line is drawn.

//------------------------------------------------------------------------------------------------
//! The waypoint rules, with no map and no widgets, so a LOGIC test covers them (LF_Test_MapWaypoint).
class LF_MapWaypoint
{
	//! The LF_Waypoint id of a mark set by a map click.
	static const string ID = "map";
	//! The mark's name: shown under the HUD marker. A string key the widget translates.
	static const string LABEL = "#LF-MapWaypoint_Label";
	//! The pencil circle is this many straight segments.
	static const int CIRCLE_SEGMENTS = 24;	// lf-fixed: a round ring at a few dozen pixels
	//! The cross strokes run this far toward the ring (a fraction of its radius).
	static const float CROSS_REACH = 0.6;	// lf-fixed: a cross inside the ring, not touching it

	//------------------------------------------------------------------------------------------------
	//! A map click at the world position `world`. Sets the waypoint with the id `map`; when the map waypoint
	//! is already set it moves (LF_Waypoint.Mark on the same id would take it away). A board waypoint is
	//! replaced: there is one waypoint.
	//! \return true when the waypoint is set now.
	static bool SetFromMap(vector world)
	{
		if (LF_Waypoint.IsMarked(ID))
			LF_Waypoint.Clear("moved on the map");

		LF_Debug.Info(LF_Waypoint.FEATURE, string.Format("map click at %1", world));
		return LF_Waypoint.Mark(ID, LABEL, world);
	}

	//------------------------------------------------------------------------------------------------
	//! What the map draws: the waypoint, whoever set it (the board or the map).
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
//! The map UI component: listens for the click, keeps one canvas over the map and draws the mark.
//! Listed in `Configs/Map/LF_MapFullscreen.conf`. Client only (the map is).
class LF_MapWaypointUI : SCR_MapUIBaseComponent
{
	protected static const string CLICK_ACTION = "MapContextualMenu";	// right mouse button; gamepad x
	protected static const float MARK_RADIUS = 14;	// lf-fixed: reference units, the same at every zoom like a pencil mark
	protected static const float MARK_WIDTH = 2.5;	// lf-fixed: a pencil stroke
	protected static const int MARK_COLOR = 0xFF2B2B2B;	// lf-fixed: graphite, the personal-mark ink

	protected CanvasWidget m_wCanvas;
	protected ref array<ref CanvasWidgetCommand> m_aCommands = {};
	protected bool m_bDrawn;
	protected vector m_vDrawn;

	//------------------------------------------------------------------------------------------------
	//! The input listener: a right click (or the gamepad button) on the map sets or moves the waypoint.
	protected void OnMapRightClick(float value, EActionTrigger reason)
	{
		if (!m_MapEntity || IsToolMenuFocused())
			return;

		float worldX;
		float worldZ;
		m_MapEntity.ScreenToWorldNoFlip(SCR_MapCursorInfo.Scale(SCR_MapCursorInfo.x), SCR_MapCursorInfo.Scale(SCR_MapCursorInfo.y), worldX, worldZ);
		vector world = Vector(worldX, 0, worldZ);
		BaseWorld baseWorld = GetGame().GetWorld();
		if (baseWorld)
			world[1] = baseWorld.GetSurfaceY(worldX, worldZ);

		LF_MapWaypoint.SetFromMap(world);
		Redraw();
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
	//! Draws the mark at the waypoint's place on the screen, or nothing with no waypoint or off the map.
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
			LF_MapWaypoint.BuildMark(workspace.DPIUnscale(screenX), workspace.DPIUnscale(screenY), MARK_RADIUS, segments);
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
		}

		m_wCanvas.SetDrawCommands(m_aCommands);
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

		m_MapEntity.GetOnMapPan().Insert(OnMapPan);
		m_MapEntity.GetOnMapZoom().Insert(OnMapZoom);
		GetGame().GetInputManager().AddActionListener(CLICK_ACTION, EActionTrigger.DOWN, OnMapRightClick);
		Redraw();
	}

	//------------------------------------------------------------------------------------------------
	override void OnMapClose(MapConfiguration config)
	{
		GetGame().GetInputManager().RemoveActionListener(CLICK_ACTION, EActionTrigger.DOWN, OnMapRightClick);
		if (m_MapEntity)
		{
			m_MapEntity.GetOnMapPan().Remove(OnMapPan);
			m_MapEntity.GetOnMapZoom().Remove(OnMapZoom);
		}

		if (m_wCanvas)
			m_wCanvas.RemoveFromHierarchy();

		m_wCanvas = null;
		m_aCommands.Clear();
		m_bDrawn = false;
		super.OnMapClose(config);
	}

	//------------------------------------------------------------------------------------------------
	//! A waypoint set or cleared elsewhere while the map is open (the board, an arrival) shows or goes.
	override void Update(float timeSlice)
	{
		if (!m_wCanvas)
			return;

		vector world;
		string label;
		bool drawn = LF_MapWaypoint.Drawn(world, label);
		if (drawn != m_bDrawn || (drawn && world != m_vDrawn))
			Redraw();
	}
}
