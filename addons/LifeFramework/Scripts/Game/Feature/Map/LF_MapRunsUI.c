//------------------------------------------------------------------------------------------------
//! Job runs as chains on the map (LF_MapRuns): while the pointer (mouse or gamepad cursor) is on a badge
//! that belongs to a run, thin dashed lines join that run's stops in order with a small numbered disc on
//! each, and the card says "Mining run: step 3 of 5, next: furnace". Nothing is drawn when the hover ends.
modded class LF_MapPlacesUI
{
	protected static const int RUN_LINE_COLOR = 0xE01C1A16; // lf-fixed: graphite ink, slightly see-through
	protected static const int RUN_DISC_COLOR = 0xFF1C1A16; // lf-fixed: graphite disc under a step number
	protected static const float RUN_DISC_RADIUS = 9; // lf-fixed: reference units
	protected static const float RUN_DISC_OFFSET = 13; // lf-fixed: reference units up and left of the badge centre
	protected static const int RUN_MAX_LABELS = 12; // lf-fixed: step numbers drawn at once

	protected CanvasWidget m_wLF_RunCanvas;
	protected ref array<ref CanvasWidgetCommand> m_aLF_RunCmds = {};
	protected ref array<ref LF_MapRunPoint> m_aLF_RunPoints = {};
	protected ref array<ref LF_MapRunMatch> m_aLF_RunMatches = {};
	protected ref array<TextWidget> m_aLF_RunLabels = {};
	protected int m_iLF_RunGroup = -1;
	protected int m_iLF_RunPoisSeen = -1;

	//------------------------------------------------------------------------------------------------
	override void OnMapOpen(MapConfiguration config)
	{
		super.OnMapOpen(config);
		m_iLF_RunGroup = -1;
		m_iLF_RunPoisSeen = -1;
		LF_MapRunConfig runs = LF_MapRunConfig.Get();
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!runs || !workspace || !m_RootWidget)
			return;

		int found = LF_MapRuns.Resolve(runs, m_aLF_RunPoints);
		if (found < m_aLF_RunPoints.Count())
			LF_Debug.Warn(LF_MapRunConfig.FEATURE, string.Format("%1 of %2 run stop(s) have no entity in this world", m_aLF_RunPoints.Count() - found, m_aLF_RunPoints.Count()));

		m_wLF_RunCanvas = CanvasWidget.Cast(workspace.CreateWidget(WidgetType.CanvasWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.NOFOCUS, Color.FromInt(Color.WHITE), 0, m_RootWidget));
		if (!m_wLF_RunCanvas)
		{
			LF_Debug.Error(LF_MapRunConfig.FEATURE, "run canvas not created; job runs are not drawn");
			return;
		}

		FrameSlot.SetAnchorMin(m_wLF_RunCanvas, 0, 0);
		FrameSlot.SetAnchorMax(m_wLF_RunCanvas, 1, 1);
		FrameSlot.SetOffsets(m_wLF_RunCanvas, 0, 0, 0, 0);
		for (int i = 0; i < RUN_MAX_LABELS; i++)
		{
			TextWidget label = TextWidget.Cast(workspace.CreateWidgets(TYPE_LAYOUT, m_RootWidget));
			if (!label)
				break;

			label.SetFlags(WidgetFlags.IGNORE_CURSOR);
			label.SetExactFontSize(13);
			label.SetColor(Color.FromInt(Color.WHITE));
			FrameSlot.SetSize(label, RUN_DISC_RADIUS * 2, RUN_DISC_RADIUS * 2);
			FrameSlot.SetAlignment(label, 0.5, 0.5);
			label.SetVisible(false);
			m_aLF_RunLabels.Insert(label);
		}
	}

	//------------------------------------------------------------------------------------------------
	override void OnMapClose(MapConfiguration config)
	{
		if (m_wLF_RunCanvas)
			m_wLF_RunCanvas.RemoveFromHierarchy();

		foreach (TextWidget label : m_aLF_RunLabels)
		{
			if (label)
				label.RemoveFromHierarchy();
		}

		m_wLF_RunCanvas = null;
		m_aLF_RunLabels.Clear();
		m_aLF_RunCmds.Clear();
		m_aLF_RunPoints.Clear();
		m_aLF_RunMatches.Clear();
		m_iLF_RunGroup = -1;
		super.OnMapClose(config);
	}

	//------------------------------------------------------------------------------------------------
	override void Update(float timeSlice)
	{
		super.Update(timeSlice);
		if (!m_wLF_RunCanvas)
			return;

		int group = m_iHovered;
		if (group < 0)
			group = m_iPinned;

		if (group != m_iLF_RunGroup)
		{
			m_iLF_RunGroup = group;
			LF_FindRuns(group);
			if (group >= 0 && !m_aLF_RunMatches.IsEmpty())
				ShowCard(group);
		}

		LF_DrawRuns();
	}

	//------------------------------------------------------------------------------------------------
	override protected string GroupNote(int i)
	{
		string line = LF_RunLine(i);
		if (line.IsEmpty())
			return super.GroupNote(i);

		return line;
	}

	//------------------------------------------------------------------------------------------------
	//! The runs badge `group` belongs to (its lead place's position against the shown stops).
	protected void LF_FindRuns(int group)
	{
		m_aLF_RunMatches.Clear();
		if (group < 0 || group >= m_aGroups.Count() || m_aLF_RunPoints.IsEmpty())
			return;

		if (m_iLF_RunPoisSeen != m_aPois.Count())
		{
			array<vector> known = {};
			foreach (LF_MapPoi poi : m_aPois)
			{
				known.Insert(poi.m_vPos);
			}

			LF_MapRunRules.MarkShown(m_aLF_RunPoints, known);
			m_iLF_RunPoisSeen = m_aPois.Count();
		}

		LF_MapClusterGroup g = m_aGroups[group];
		if (!g.HasLead())
			return;

		int matched = LF_MapRunRules.Match(m_aPois[g.m_iLead].m_vPos, m_aLF_RunPoints, m_aLF_RunMatches);
		if (matched > 0)
			LF_Debug.Info(LF_MapRunConfig.FEATURE, string.Format("hover shows %1 run(s) at %2", matched, m_aPois[g.m_iLead].m_sId));
	}

	//------------------------------------------------------------------------------------------------
	//! The card line for badge i: "Mining run: step 3 of 5, next: furnace" for its first run, or empty.
	protected string LF_RunLine(int i)
	{
		if (i != m_iLF_RunGroup || m_aLF_RunMatches.IsEmpty())
			return "";

		LF_MapRunConfig config = LF_MapRunConfig.Get();
		if (!config)
			return "";

		string text;
		foreach (LF_MapRunMatch match : m_aLF_RunMatches)
		{
			LF_MapRun run = config.m_aRuns[match.m_iRun];
			string stepText;
			string countText;
			string key = LF_MapRunRules.CardKey(match.m_iStep, run.StopCount(), stepText, countText);
			string next;
			if (match.m_iStep + 1 < run.StopCount())
				next = WidgetManager.Translate(run.m_aStops[match.m_iStep + 1].m_sLabel);

			if (!text.IsEmpty())
				text += "\n";

			text += WidgetManager.Translate(key, WidgetManager.Translate(run.m_sLabel), stepText, countText, next);
		}

		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! The dashed lines and the numbered discs of the matched runs, in screen pixels at the current view.
	protected void LF_DrawRuns()
	{
		m_aLF_RunCmds.Clear();
		int used = 0;
		if (!m_aLF_RunMatches.IsEmpty() && m_MapEntity)
		{
			WorkspaceWidget workspace = GetGame().GetWorkspace();
			foreach (LF_MapRunMatch match : m_aLF_RunMatches)
			{
				float lastX;
				float lastY;
				bool hasLast;
				foreach (LF_MapRunPoint point : m_aLF_RunPoints)
				{
					if (point.m_iRun != match.m_iRun || !point.m_bFound || !point.m_bShown)
						continue;

					int sx;
					int sy;
					m_MapEntity.WorldToScreen(point.m_vPos[0], point.m_vPos[2], sx, sy, true);
					if (hasLast)
						LF_AddDashes(lastX, lastY, sx, sy);

					lastX = sx;
					lastY = sy;
					hasLast = true;
					// The number sits on the badge's top-left corner, so the badge stays readable.
					float nx = sx - workspace.DPIScale(RUN_DISC_OFFSET);
					float ny = sy - workspace.DPIScale(RUN_DISC_OFFSET);
					LF_AddDisc(nx, ny, workspace.DPIScale(RUN_DISC_RADIUS));
					if (used < m_aLF_RunLabels.Count())
					{
						TextWidget label = m_aLF_RunLabels[used];
						label.SetText((point.m_iStep + 1).ToString());
						FrameSlot.SetPos(label, workspace.DPIUnscale(nx), workspace.DPIUnscale(ny));
						label.SetVisible(true);
						used++;
					}
				}
			}
		}

		for (int i = used; i < m_aLF_RunLabels.Count(); i++)
		{
			m_aLF_RunLabels[i].SetVisible(false);
		}

		m_wLF_RunCanvas.SetDrawCommands(m_aLF_RunCmds);
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_AddDashes(float x0, float y0, float x1, float y1)
	{
		array<float> dashes = {};
		LF_MapRunRules.Dashes(x0, y0, x1, y1, dashes);
		for (int s = 0; s + 3 < dashes.Count(); s += 4)
		{
			LineDrawCommand line = new LineDrawCommand();
			line.m_Vertices = {dashes[s], dashes[s + 1], dashes[s + 2], dashes[s + 3]};
			line.m_fWidth = 2;
			line.m_iColor = RUN_LINE_COLOR;
			m_aLF_RunCmds.Insert(line);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_AddDisc(float x, float y, float r)
	{
		PolygonDrawCommand disc = new PolygonDrawCommand();
		disc.m_iColor = RUN_DISC_COLOR;
		disc.m_Vertices = {};
		for (int i = 0; i < 14; i++)
		{
			float a = Math.PI2 * i / 14;
			disc.m_Vertices.Insert(x + r * Math.Cos(a));
			disc.m_Vertices.Insert(y + r * Math.Sin(a));
		}

		m_aLF_RunCmds.Insert(disc);
	}
}
