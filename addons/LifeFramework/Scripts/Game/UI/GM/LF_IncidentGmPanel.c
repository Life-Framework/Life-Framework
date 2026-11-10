//------------------------------------------------------------------------------------------------
//! Game Master incident panel (#240): the client half. Lives in vanilla Game Master EDIT mode, no
//! parallel admin screen. LF_IncidentGmView draws a marker at each recent incident near the camera;
//! the marker nearest the screen centre is the selected one and its rows are listed here.
//! Selection follows the Game Master camera, so it needs no extra binding on keyboard or gamepad.
//!
//! The data comes only by RPC from LF_IncidentGm, which answers administrators only. This class
//! holds no incident source of its own and sends nothing but the camera position.
//------------------------------------------------------------------------------------------------
class LF_IncidentGmPanelUI : SCR_ScriptedWidgetComponent
{
	protected TextWidget m_wTitle;
	protected TextWidget m_wBody;

	//------------------------------------------------------------------------------------------------
	override void HandlerAttached(Widget w)
	{
		super.HandlerAttached(w);
		if (!m_wRoot)
			return;

		m_wTitle = TextWidget.Cast(m_wRoot.FindAnyWidget("Title"));
		m_wBody = TextWidget.Cast(m_wRoot.FindAnyWidget("Body"));
		if (m_wTitle)
			m_wTitle.SetText("#LF-IncidentGm_Title");
	}

	//------------------------------------------------------------------------------------------------
	void SetBody(string text)
	{
		if (m_wBody)
			m_wBody.SetText(text);
	}
}

//------------------------------------------------------------------------------------------------
//! One marker: the incidents within one grid cell, drawn where the newest happened.
class LF_IncidentGmCluster
{
	static const float CELL = 25;

	vector m_vPosition;
	ref array<string> m_aRows = new array<string>();
	Widget m_wMarker;
	bool m_bFlagged;
}

//------------------------------------------------------------------------------------------------
//! Per-frame marker projection and panel text while EDIT mode is up. Owned by the modded
//! SCR_EditModeEditorUIComponent, which attaches and detaches it with the mode.
class LF_IncidentGmView
{
	protected const ResourceName MARKER_LAYOUT = "{AC9B7462ED38E313}UI/Layouts/GM/LF_IncidentGmMarker.layout";
	protected const int ASK_INTERVAL_MS = 4000;

	protected MenuRootBase m_Menu;
	protected Widget m_wParent;
	protected LF_IncidentGmPanelUI m_Panel;
	protected ref array<ref LF_IncidentGmCluster> m_aClusters = new array<ref LF_IncidentGmCluster>();
	protected int m_iSeenAnswers;
	protected string m_sLastBody;
	//! Rows a player reported (#241), from every cluster; they head the list.
	protected ref array<string> m_aFlaggedRows = new array<string>();

	//------------------------------------------------------------------------------------------------
	void Attach(MenuRootBase menu, Widget parent, LF_IncidentGmPanelUI panel)
	{
		m_Menu = menu;
		m_wParent = parent;
		m_Panel = panel;
		m_iSeenAnswers = SCR_PlayerController.LF_GetIncidentAnswers();
		if (m_Menu)
			m_Menu.GetOnMenuUpdate().Insert(OnMenuUpdate);

		GetGame().GetCallqueue().CallLater(Ask, ASK_INTERVAL_MS, true);
		Ask();
		if (m_Panel)
			m_Panel.SetBody("#LF-IncidentGm_Waiting");
	}

	//------------------------------------------------------------------------------------------------
	void Detach()
	{
		GetGame().GetCallqueue().Remove(Ask);
		if (m_Menu)
			m_Menu.GetOnMenuUpdate().Remove(OnMenuUpdate);

		ClearMarkers();
		m_Menu = null;
		m_wParent = null;
		m_Panel = null;
	}

	//------------------------------------------------------------------------------------------------
	protected void Ask()
	{
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		CameraManager cameras = GetGame().GetCameraManager();
		if (!controller || !cameras)
			return;

		CameraBase camera = cameras.CurrentCamera();
		if (!camera)
			return;

		controller.LF_AskIncidents(camera.GetOrigin());
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearMarkers()
	{
		foreach (LF_IncidentGmCluster cluster : m_aClusters)
		{
			if (cluster.m_wMarker)
				cluster.m_wMarker.RemoveFromHierarchy();
		}

		m_aClusters.Clear();
		m_aFlaggedRows.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! A new answer: rebuild the clusters and their marker widgets.
	protected void Rebuild(array<string> rows)
	{
		ClearMarkers();
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace || !m_wParent)
			return;

		foreach (string row : rows)
		{
			array<string> parts = {};
			row.Split("|", parts, false);
			if (parts.Count() < 12)
				continue;

			vector position = Vector(parts[3].ToFloat(), 0, parts[4].ToFloat());
			position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]);
			LF_IncidentGmCluster cluster = ClusterAt(position);
			if (!cluster)
			{
				cluster = new LF_IncidentGmCluster();
				cluster.m_wMarker = workspace.CreateWidgets(MARKER_LAYOUT, m_wParent);
				m_aClusters.Insert(cluster);
			}

			cluster.m_vPosition = position;
			cluster.m_aRows.Insert(row);
			if (parts[11] == "1")
			{
				m_aFlaggedRows.Insert(row);
				cluster.m_bFlagged = true;
				TextWidget mark = TextWidget.Cast(cluster.m_wMarker);
				if (mark)
				{
					mark.SetText("!!");
					mark.SetColor(Color.FromInt(0xFFFFD21F));
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected LF_IncidentGmCluster ClusterAt(vector position)
	{
		foreach (LF_IncidentGmCluster cluster : m_aClusters)
		{
			if (vector.DistanceXZ(cluster.m_vPosition, position) <= LF_IncidentGmCluster.CELL)
				return cluster;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsFlaggedRow(string row)
	{
		array<string> p = {};
		row.Split("|", p, false);
		return p.Count() > 11 && p[11] == "1";
	}

	//------------------------------------------------------------------------------------------------
	protected string FormatRow(string row)
	{
		array<string> p = {};
		row.Split("|", p, false);
		string kind = p[1];
		if (kind == "death" && p[10] == "1")
			kind = "murder";

		string kindKey = "#LF-IncidentGm_Kind_death";
		if (kind == "murder")
			kindKey = "#LF-IncidentGm_Kind_murder";
		else if (kind == "damage")
			kindKey = "#LF-IncidentGm_Kind_damage";
		else if (kind == "cash")
			kindKey = "#LF-IncidentGm_Kind_cash";
		else if (kind == "downed")
			kindKey = "#LF-IncidentGm_Kind_downed";
		else if (kind == "holdup")
			kindKey = "#LF-IncidentGm_Kind_holdup";
		else if (kind == "admin")
			kindKey = "#LF-IncidentGm_Kind_admin";
		else if (kind == "aim")
			kindKey = "#LF-IncidentGm_Kind_aim";
		else if (kind == "nlr")
			kindKey = "#LF-IncidentGm_Kind_nlr";

		string kindText = WidgetManager.Translate(kindKey);
		string text;
		if (p[11] == "1")
			text = WidgetManager.Translate("#LF-IncidentGm_RowFlagged", p[0], kindText, (p[2].ToInt() / 60).ToString(), p[5], p[6]);
		else
			text = WidgetManager.Translate("#LF-IncidentGm_Row", kindText, (p[2].ToInt() / 60).ToString(), p[5], p[6]);

		if (kind == "damage" || kind == "cash")
			text = text + " " + p[7];
		else if (kind == "admin")
			text = text + " " + WidgetManager.Translate(p[7]);

		return text;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnMenuUpdate(float tDelta)
	{
		int answers = SCR_PlayerController.LF_GetIncidentAnswers();
		if (answers != m_iSeenAnswers)
		{
			m_iSeenAnswers = answers;
			Rebuild(SCR_PlayerController.LF_GetIncidentRows());
		}

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		float centerX = workspace.DPIUnscale(workspace.GetWidth()) * 0.5;
		float centerY = workspace.DPIUnscale(workspace.GetHeight()) * 0.5;
		LF_IncidentGmCluster selected;
		float best = 1000000000;
		foreach (LF_IncidentGmCluster cluster : m_aClusters)
		{
			if (!cluster.m_wMarker)
				continue;

			vector screen = workspace.ProjWorldToScreen(cluster.m_vPosition + "0 2 0", GetGame().GetWorld());
			bool visible = screen[2] > 0;
			cluster.m_wMarker.SetVisible(visible);
			if (!visible)
				continue;

			FrameSlot.SetPos(cluster.m_wMarker, screen[0], screen[1]);
			float dx = screen[0] - centerX;
			float dy = screen[1] - centerY;
			float distSq = dx * dx + dy * dy;
			if (distSq < best)
			{
				best = distSq;
				selected = cluster;
			}
		}

		if (!m_Panel)
			return;

		string body;
		if (m_iSeenAnswers == 0)
			body = "#LF-IncidentGm_Waiting";
		else if (m_aClusters.IsEmpty())
			body = "#LF-IncidentGm_None";
		else
		{
			// Reported incidents first (at most four), then the selected marker newest first;
			// eight lines at most so the panel stays its size.
			int lines;
			for (int f = m_aFlaggedRows.Count() - 1; f >= 0 && lines < 4; f--)
			{
				body = body + FormatRow(m_aFlaggedRows[f]) + "\n";
				lines++;
			}

			if (selected)
			{
				for (int i = selected.m_aRows.Count() - 1; i >= 0 && lines < 8; i--)
				{
					if (!IsFlaggedRow(selected.m_aRows[i]))
					{
						body = body + FormatRow(selected.m_aRows[i]) + "\n";
						lines++;
					}
				}
			}
		}

		if (body != m_sLastBody)
		{
			m_sLastBody = body;
			m_Panel.SetBody(body);
		}
	}
}

//------------------------------------------------------------------------------------------------
//! Adds the incident panel to Game Master EDIT mode (the one mode with full Game Master powers).
//! The panel is created with the mode's widget tree and goes with it.
modded class SCR_EditModeEditorUIComponent
{
	protected const ResourceName LF_INCIDENT_PANEL_LAYOUT = "{73887C8BF4278E4E}UI/Layouts/GM/LF_IncidentGmPanel.layout";

	protected ref LF_IncidentGmView m_LF_IncidentView;

	//------------------------------------------------------------------------------------------------
	override protected void HandlerAttachedScripted(Widget w)
	{
		super.HandlerAttachedScripted(w);
		if (!w)
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		Widget panel = workspace.CreateWidgets(LF_INCIDENT_PANEL_LAYOUT, w);
		if (!panel)
		{
			LF_Debug.Error(LF_IncidentGm.FEATURE, "incident panel not created: " + LF_INCIDENT_PANEL_LAYOUT);
			return;
		}

		LF_IncidentGmPanelUI ui = LF_IncidentGmPanelUI.Cast(panel.FindHandler(LF_IncidentGmPanelUI));
		m_LF_IncidentView = new LF_IncidentGmView();
		m_LF_IncidentView.Attach(GetMenu(), w, ui);
	}

	//------------------------------------------------------------------------------------------------
	override void HandlerDeattached(Widget w)
	{
		if (m_LF_IncidentView)
		{
			m_LF_IncidentView.Detach();
			m_LF_IncidentView = null;
		}

		super.HandlerDeattached(w);
	}
}
