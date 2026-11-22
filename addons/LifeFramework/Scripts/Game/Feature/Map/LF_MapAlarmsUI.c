//------------------------------------------------------------------------------------------------
//! The ring's geometry and pulse, pure so a LOGIC test can check it with no map open.
class LF_MapAlarmRing
{
	static const int SEGMENTS = 20; // lf-fixed: ring smoothness
	static const float PULSE_SECONDS = 1.2; // lf-fixed: one beat of the pulse
	//! The ring's radius against the badge size, at rest and at the top of a beat.
	static const float RADIUS_MIN = 0.62; // lf-fixed: drawing
	static const float RADIUS_MAX = 0.95; // lf-fixed: drawing
	static const int RING_RGB = 0xD8231E; // lf-fixed: alarm red
	static const int RECENT_COLOR = 0xFF8C8A86; // lf-fixed: faded grey for "robbed recently"

	//------------------------------------------------------------------------------------------------
	//! 0..1 through the current beat.
	static float Phase(float clock)
	{
		if (clock < 0)
			clock = 0;

		float beats = clock / PULSE_SECONDS;
		return beats - Math.Floor(beats);
	}

	//------------------------------------------------------------------------------------------------
	//! The ring radius for a badge of `size` at `phase`: it grows through the beat.
	static float Radius(float size, float phase)
	{
		return size * (RADIUS_MIN + (RADIUS_MAX - RADIUS_MIN) * phase);
	}

	//------------------------------------------------------------------------------------------------
	//! ARGB of the ring at `phase`: solid at rest, fading out as it grows.
	static int RingColor(float phase)
	{
		int alpha = (int)(255 * (1 - 0.75 * phase));
		return (alpha << 24) | RING_RGB;
	}

	//------------------------------------------------------------------------------------------------
	//! Line segments (x0, y0, x1, y1, ...) of a circle.
	static void Circle(float cx, float cy, float radius, notnull array<float> into)
	{
		for (int i = 0; i < SEGMENTS; i++)
		{
			float a0 = Math.PI2 * i / SEGMENTS;
			float a1 = Math.PI2 * (i + 1) / SEGMENTS;
			into.Insert(cx + radius * Math.Cos(a0));
			into.Insert(cy + radius * Math.Sin(a0));
			into.Insert(cx + radius * Math.Cos(a1));
			into.Insert(cy + radius * Math.Sin(a1));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The alarm state a badge shows for its places: in progress wins over recent; the newest alarm time.
	static LF_EMapAlarmState Strongest(notnull array<int> states, notnull array<int> raised, out int when)
	{
		LF_EMapAlarmState best = LF_EMapAlarmState.NONE;
		when = 0;
		foreach (int i, int state : states)
		{
			if (state == LF_EMapAlarmState.NONE)
				continue;

			bool stronger = best == LF_EMapAlarmState.NONE || (state == LF_EMapAlarmState.IN_PROGRESS && best == LF_EMapAlarmState.RECENT);
			if (stronger || (state == best && raised[i] > when))
			{
				best = state;
				when = raised[i];
			}
		}

		return best;
	}
}

//------------------------------------------------------------------------------------------------
//! The robbery alarm on the place badges (LF_MapAlarms): a pulsing red ring with a small "!" on a place
//! being robbed, a grey badge on a place robbed recently, and the hover card's line. One canvas over the
//! map, drawn each frame only while a ringing badge is on screen; the badges restyle when the replicated
//! rows change.
modded class LF_MapPlacesUI
{
	protected CanvasWidget m_wLF_AlarmCanvas;
	protected ref array<ref CanvasWidgetCommand> m_aLF_AlarmCmds = {};
	protected float m_fLF_AlarmClock;
	protected bool m_bLF_AlarmDrawn;

	//------------------------------------------------------------------------------------------------
	override void OnMapOpen(MapConfiguration config)
	{
		super.OnMapOpen(config);

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace || !m_RootWidget)
			return;

		m_wLF_AlarmCanvas = CanvasWidget.Cast(workspace.CreateWidget(WidgetType.CanvasWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.NOFOCUS, Color.FromInt(Color.WHITE), 0, m_RootWidget));
		if (!m_wLF_AlarmCanvas)
		{
			LF_Debug.Error(LF_MapAlarms.FEATURE, "alarm canvas not created; robbery rings are not drawn");
			return;
		}

		FrameSlot.SetAnchorMin(m_wLF_AlarmCanvas, 0, 0);
		FrameSlot.SetAnchorMax(m_wLF_AlarmCanvas, 1, 1);
		FrameSlot.SetOffsets(m_wLF_AlarmCanvas, 0, 0, 0, 0);
		LF_MapAlarms.OnChanged().Insert(LF_OnAlarmsChanged);
		m_fLF_AlarmClock = 0;
	}

	//------------------------------------------------------------------------------------------------
	override void OnMapClose(MapConfiguration config)
	{
		LF_MapAlarms.OnChanged().Remove(LF_OnAlarmsChanged);
		if (m_wLF_AlarmCanvas)
			m_wLF_AlarmCanvas.RemoveFromHierarchy();

		m_wLF_AlarmCanvas = null;
		m_aLF_AlarmCmds.Clear();
		m_bLF_AlarmDrawn = false;
		super.OnMapClose(config);
	}

	//------------------------------------------------------------------------------------------------
	override void Update(float timeSlice)
	{
		super.Update(timeSlice);
		m_fLF_AlarmClock += timeSlice;
		if (LF_MapAlarms.ClientCount() > 0 || m_bLF_AlarmDrawn)
			LF_DrawAlarms();
	}

	//------------------------------------------------------------------------------------------------
	override protected void Style(int i)
	{
		super.Style(i);
		if (i < 0 || i >= m_aIcons.Count() || !m_aIcons[i])
			return;

		int raised;
		if (LF_GroupAlarm(i, raised) == LF_EMapAlarmState.RECENT)
			m_aIcons[i].SetColor(Color.FromInt(LF_MapAlarmRing.RECENT_COLOR));
		else
			m_aIcons[i].SetColor(Color.FromInt(Color.WHITE));
	}

	//------------------------------------------------------------------------------------------------
	override protected string GroupNote(int i)
	{
		int raised;
		LF_EMapAlarmState state = LF_GroupAlarm(i, raised);
		if (state == LF_EMapAlarmState.NONE)
			return super.GroupNote(i);

		string arg;
		string key = LF_MapAlarmRules.CardKey(state, LF_MapAlarmRules.MinutesAgo(raised, System.GetUnixTime()), arg);
		return WidgetManager.Translate(key, arg);
	}

	//------------------------------------------------------------------------------------------------
	//! The alarm a badge shows: the strongest of its places' alarms.
	protected LF_EMapAlarmState LF_GroupAlarm(int i, out int raised)
	{
		raised = 0;
		if (i < 0 || i >= m_aGroups.Count() || LF_MapAlarms.ClientCount() == 0)
			return LF_EMapAlarmState.NONE;

		array<int> states = {};
		array<int> times = {};
		foreach (int member : m_aGroups[i].m_aMembers)
		{
			if (member < 0 || member >= m_aPois.Count())
				continue;

			int when;
			LF_MapPoi poi = m_aPois[member];
			states.Insert(LF_MapAlarms.StateOf(poi.m_sKind, poi.m_sId, when));
			times.Insert(when);
		}

		return LF_MapAlarmRing.Strongest(states, times, raised);
	}

	//------------------------------------------------------------------------------------------------
	//! The replicated rows changed: restyle the badges and refresh an open card.
	protected void LF_OnAlarmsChanged()
	{
		for (int i = 0; i < m_aGroups.Count() && i < m_aIcons.Count(); i++)
		{
			Style(i);
		}

		if (m_iCardGroup >= 0)
			ShowCard(m_iCardGroup);

		LF_DrawAlarms();
	}

	//------------------------------------------------------------------------------------------------
	//! The pulsing ring and "!" on every shown badge whose place is being robbed.
	protected void LF_DrawAlarms()
	{
		if (!m_wLF_AlarmCanvas)
			return;

		m_aLF_AlarmCmds.Clear();
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		float phase = LF_MapAlarmRing.Phase(m_fLF_AlarmClock);
		int ringColor = LF_MapAlarmRing.RingColor(phase);
		int groups = m_aGroups.Count();
		for (int i = 0; i < groups; i++)
		{
			if (i >= m_aScreenX.Count() || i >= m_aShown.Count() || !m_aShown[i])
				continue;

			int raised;
			if (LF_GroupAlarm(i, raised) != LF_EMapAlarmState.IN_PROGRESS)
				continue;

			float size = workspace.DPIScale(m_aSizes[i]);
			float cx = workspace.DPIScale(m_aScreenX[i]);
			float cy = workspace.DPIScale(m_aScreenY[i]);
			LF_AddLines(LF_RingSegments(cx, cy, LF_MapAlarmRing.Radius(size, phase)), ringColor, 3);
			LF_AddLines(LF_RingSegments(cx, cy, size * LF_MapAlarmRing.RADIUS_MIN), (255 << 24) | LF_MapAlarmRing.RING_RGB, 2);
			LF_AddBang(cx + size * 0.5, cy - size * 0.5, size * 0.42);
		}

		m_bLF_AlarmDrawn = !m_aLF_AlarmCmds.IsEmpty();
		m_wLF_AlarmCanvas.SetDrawCommands(m_aLF_AlarmCmds);
	}

	//------------------------------------------------------------------------------------------------
	protected array<float> LF_RingSegments(float cx, float cy, float radius)
	{
		array<float> segments = {};
		LF_MapAlarmRing.Circle(cx, cy, radius, segments);
		return segments;
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_AddLines(notnull array<float> segments, int color, float width)
	{
		for (int s = 0; s + 3 < segments.Count(); s += 4)
		{
			LineDrawCommand line = new LineDrawCommand();
			line.m_Vertices = {segments[s], segments[s + 1], segments[s + 2], segments[s + 3]};
			line.m_fWidth = width;
			line.m_iColor = color;
			m_aLF_AlarmCmds.Insert(line);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! A small red disc with a white "!" at the badge's top-right corner.
	protected void LF_AddBang(float x, float y, float height)
	{
		PolygonDrawCommand disc = new PolygonDrawCommand();
		disc.m_iColor = (255 << 24) | LF_MapAlarmRing.RING_RGB;
		disc.m_Vertices = {};
		float r = height * 0.62;
		for (int i = 0; i < 12; i++)
		{
			float a = Math.PI2 * i / 12;
			disc.m_Vertices.Insert(x + r * Math.Cos(a));
			disc.m_Vertices.Insert(y + r * Math.Sin(a));
		}

		m_aLF_AlarmCmds.Insert(disc);
		LineDrawCommand stem = new LineDrawCommand();
		stem.m_Vertices = {x, y - height * 0.38, x, y + height * 0.08};
		stem.m_fWidth = 2;
		stem.m_iColor = Color.WHITE;
		m_aLF_AlarmCmds.Insert(stem);
		LineDrawCommand dot = new LineDrawCommand();
		dot.m_Vertices = {x, y + height * 0.22, x, y + height * 0.34};
		dot.m_fWidth = 2;
		dot.m_iColor = Color.WHITE;
		m_aLF_AlarmCmds.Insert(dot);
	}
}
