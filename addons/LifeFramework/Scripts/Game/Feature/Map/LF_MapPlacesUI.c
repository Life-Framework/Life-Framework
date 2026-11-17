// LF_MapPlacesUI.c: the points of interest drawn on the open map (#2232, epic #2223, card M4;
// docs/architecture/everon-map.md section 1, "Rendering").
//
// One small widget per drawn POI or cluster stamp in a frame over the map, placed with
// SCR_MapEntity.WorldToScreen. The clusters are rebuilt when a zoom ends (they depend on the zoom only: the
// buckets are in world space), the positions on pan and zoom events only; nothing runs while the map is
// closed. A hover or a click shows a paper card with the place's name and Set waypoint. A cluster stamp
// zooms in on a click. The rules are LF_MapClusterRules, one case table with the node twin
// tools/map/rules/cluster.mjs.

//------------------------------------------------------------------------------------------------
//! What the layer draws for one bucket: one place (a single icon) or several (a cluster stamp).
class LF_MapClusterGroup
{
	//! Indexes into the list the groups were built from.
	ref array<int> m_aMembers = {};
	float m_fX;
	float m_fZ;
	LF_EMapPoiState m_eState = LF_EMapPoiState.FULL;

	//------------------------------------------------------------------------------------------------
	bool IsSingle()
	{
		return m_aMembers.Count() == 1;
	}
}

//------------------------------------------------------------------------------------------------
//! The POI layer's min-zoom and cluster rules as pure functions. Node twin: tools/map/rules/cluster.mjs;
//! both run Configs/Map/Rules/cluster.cases.json (LF_Test_MapPlacesLayout), so they cannot drift.
//! Zoom: the view is 12800 m / 2^zoom wide (zoom 0 is the island, the emulator's and map-shots' --zoom).
class LF_MapClusterRules
{
	static const float WORLD_M = 12800;	// lf-fixed: Everon's map width, the unit of zoom
	static const float SOLO_ZOOM = 4;	// lf-fixed: from here on every point is its own icon
	static const float CELLS = 8;	// lf-fixed: a bucket is this fraction of the view width

	//------------------------------------------------------------------------------------------------
	//! The zoom of a view `widthM` metres wide. A degenerate width reads as the closest zoom.
	static float ZoomOfWidth(float widthM)
	{
		if (widthM <= 0.001)
			return 20;

		return Math.Log2(WORLD_M / widthM);
	}

	//------------------------------------------------------------------------------------------------
	//! A kind row's min zoom against the view zoom: shown at exactly its min zoom and closer.
	static bool ShowAtZoom(float zoom, float minZoom)
	{
		return minZoom <= zoom + 0.000001;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsSolo(float zoom)
	{
		return zoom >= SOLO_ZOOM - 0.000001;
	}

	//------------------------------------------------------------------------------------------------
	//! The bucket size in metres at a zoom.
	static float CellSize(float zoom)
	{
		return WORLD_M / Math.Pow(2, zoom) / CELLS;
	}

	//------------------------------------------------------------------------------------------------
	//! Buckets the points whose min zoom the view reaches. Groups come out in first-member order, each
	//! with the member indexes in input order and the mean position. The state stays FULL here.
	static void Build(notnull array<float> xs, notnull array<float> zs, notnull array<float> minZooms, float zoom, notnull array<ref LF_MapClusterGroup> into)
	{
		bool solo = IsSolo(zoom);
		float cell = CellSize(zoom);
		map<string, int> byCell = new map<string, int>();
		int first = into.Count();
		foreach (int i, float x : xs)
		{
			if (!ShowAtZoom(zoom, minZooms[i]))
				continue;

			LF_MapClusterGroup group = null;
			if (!solo)
			{
				int cx = Math.Floor(x / cell);
				int cz = Math.Floor(zs[i] / cell);
				string key = cx.ToString() + "," + cz.ToString();
				int at = 0;
				if (byCell.Find(key, at))
					group = into[at];
				else
					byCell.Set(key, into.Count());
			}

			if (!group)
			{
				group = new LF_MapClusterGroup();
				into.Insert(group);
			}

			group.m_aMembers.Insert(i);
			group.m_fX += x;
			group.m_fZ += zs[i];
		}

		for (int g = first; g < into.Count(); g++)
		{
			float count = into[g].m_aMembers.Count();
			into[g].m_fX = into[g].m_fX / count;
			into[g].m_fZ = into[g].m_fZ / count;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The groups for the POIs the map draws now. A group's state is its strongest member: full over
	//! faded over a pencil mark.
	static void BuildFromPois(notnull array<ref LF_MapPoi> pois, float zoom, notnull array<ref LF_MapClusterGroup> into)
	{
		array<float> xs = {};
		array<float> zs = {};
		array<float> minZooms = {};
		foreach (LF_MapPoi poi : pois)
		{
			xs.Insert(poi.m_vPos[0]);
			zs.Insert(poi.m_vPos[2]);
			minZooms.Insert(poi.m_fMinZoom);
		}

		int first = into.Count();
		Build(xs, zs, minZooms, zoom, into);
		for (int g = first; g < into.Count(); g++)
		{
			LF_MapClusterGroup group = into[g];
			group.m_eState = pois[group.m_aMembers[0]].m_eState;
			foreach (int member : group.m_aMembers)
			{
				LF_EMapPoiState state = pois[member].m_eState;
				if (state == LF_EMapPoiState.FULL || (state == LF_EMapPoiState.FADED && group.m_eState == LF_EMapPoiState.PENCIL))
					group.m_eState = state;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The case table's entry point (op minzoom | mode | cluster). Points are "id:x:z:minZoom" comma separated.
	static string Evaluate(string op, float zoom, float minZoom, string points)
	{
		if (op == "minzoom")
		{
			if (ShowAtZoom(zoom, minZoom))
				return "show";

			return "hide";
		}

		if (op == "mode")
		{
			if (IsSolo(zoom))
				return "solo";

			return "clustered";
		}

		if (op != "cluster")
			return "unknown op " + op;

		array<string> ids = {};
		array<float> xs = {};
		array<float> zs = {};
		array<float> minZooms = {};
		array<string> items = {};
		points.Split(",", items, true);
		foreach (string item : items)
		{
			array<string> fields = {};
			item.Split(":", fields, false);
			if (fields.Count() < 3)
				continue;

			ids.Insert(fields[0]);
			xs.Insert(fields[1].ToFloat());
			zs.Insert(fields[2].ToFloat());
			float itemMin = 0;
			if (fields.Count() > 3)
				itemMin = fields[3].ToFloat();

			minZooms.Insert(itemMin);
		}

		array<ref LF_MapClusterGroup> groups = {};
		Build(xs, zs, minZooms, zoom, groups);
		string result;
		foreach (int g, LF_MapClusterGroup group : groups)
		{
			if (g > 0)
				result += ",";

			foreach (int m, int member : group.m_aMembers)
			{
				if (m > 0)
					result += "+";

				result += ids[member];
			}
		}

		return result;
	}
}

//------------------------------------------------------------------------------------------------
//! The hover card's words and its one action. Pure, so a LOGIC test covers them.
class LF_MapPlaceCard
{
	//------------------------------------------------------------------------------------------------
	//! "Bank, Saint-Philippe": the kind's name and its town. No kind name: no card.
	static string Title(string label, string town)
	{
		if (label.IsEmpty())
			return "";

		if (town.IsEmpty())
			return label;

		return label + ", " + town;
	}

	//------------------------------------------------------------------------------------------------
	//! Set waypoint: the single personal waypoint (id `map`, as a map click and the board set it). A second
	//! place moves it rather than clearing it. An empty title is refused.
	static bool SetWaypoint(string title, vector position)
	{
		if (title.IsEmpty())
		{
			LF_Debug.Warn("MapPlaces", "Set waypoint refused: the place has no name");
			return false;
		}

		if (LF_Waypoint.IsMarked(LF_MapWaypoint.ID))
			LF_Waypoint.Clear("moved from a map card");

		return LF_Waypoint.Mark(LF_MapWaypoint.ID, title, position);
	}
}

//------------------------------------------------------------------------------------------------
//! Which icon quad a POI draws. The pack's imageset (override PackImageSet in a modded copy) is tried
//! first, the core placeholders second.
class LF_MapPlaceIcons
{
	static const ResourceName CORE_IMAGESET = "{E8ABC6949B537023}UI/Imagesets/LF_MapIcons.imageset";

	//------------------------------------------------------------------------------------------------
	//! The pack's icon set, or empty. A pack mods this to return its own imageset.
	static ResourceName PackImageSet()
	{
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Quad names in the order tried: the kind's own, the row's icon, the category's fallback.
	static void Candidates(string kind, string icon, string category, notnull array<string> into)
	{
		if (!kind.IsEmpty())
			into.Insert("poi_" + kind);

		if (!icon.IsEmpty())
			into.Insert(icon);

		if (!category.IsEmpty())
			into.Insert("cat_" + category);

		into.Insert("cat_civic");
	}

	//------------------------------------------------------------------------------------------------
	//! Loads the first quad that exists. \return true when one loaded.
	static bool Apply(ImageWidget widget, notnull array<string> candidates)
	{
		if (!widget)
			return false;

		ResourceName pack = PackImageSet();
		foreach (string quad : candidates)
		{
			if (!pack.IsEmpty() && widget.LoadImageFromSet(0, pack, quad))
				return true;

			if (widget.LoadImageFromSet(0, CORE_IMAGESET, quad))
				return true;
		}

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! The hover, click and card events of one drawn icon, forwarded to the layer by index.
class LF_MapPoiHandler : ScriptedWidgetComponent
{
	protected LF_MapPlacesUI m_Layer;
	protected int m_iIndex;
	protected bool m_bCard;

	//------------------------------------------------------------------------------------------------
	void Init(LF_MapPlacesUI layer, int index, bool card)
	{
		m_Layer = layer;
		m_iIndex = index;
		m_bCard = card;
	}

	//------------------------------------------------------------------------------------------------
	void SetIndex(int index)
	{
		m_iIndex = index;
	}

	//------------------------------------------------------------------------------------------------
	override bool OnMouseEnter(Widget w, int x, int y)
	{
		if (m_Layer)
			m_Layer.OnPointerEnter(m_iIndex, m_bCard);

		return false;
	}

	//------------------------------------------------------------------------------------------------
	override bool OnMouseLeave(Widget w, Widget enterW, int x, int y)
	{
		if (m_Layer)
			m_Layer.OnPointerLeave(m_iIndex, m_bCard);

		return false;
	}

	//------------------------------------------------------------------------------------------------
	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (button != 0 || !m_Layer)
			return false;

		m_Layer.OnPointerClick(m_iIndex, m_bCard);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The POI layer. Listed in Configs/Map/LF_MapFullscreen.conf like vanilla's map UI components, so it
//! exists only while the map is open.
class LF_MapPlacesUI : SCR_MapUIBaseComponent
{
	static const string FEATURE = "MapPlaces";
	static const ResourceName POI_LAYOUT = "{CB59D49AE5564BCD}UI/Layouts/Map/LF_MapPoi.layout";
	static const ResourceName CARD_LAYOUT = "{CC650B4F4E7A5A77}UI/Layouts/Map/LF_MapPoiCard.layout";
	protected static const float CULL_MARGIN = 40;	// lf-fixed: reference units off screen before an icon is hidden
	protected static const float CARD_GAP = 16;	// lf-fixed: the card sits this far from its icon
	protected static const int HIDE_DELAY_MS = 200;	// lf-fixed: the pointer may cross from the icon to the card
	protected static const float FADED_ALPHA = 0.45;	// lf-fixed: heard of, not visited
	protected static const string PROBE_PARAM = "lfMapPlacesProbe";

	protected Widget m_wLayer;
	protected Widget m_wCard;
	protected ref array<ref LF_MapPoi> m_aPois = {};
	protected ref array<ref LF_MapClusterGroup> m_aGroups = {};
	protected ref array<Widget> m_aWidgets = {};
	protected ref array<ref LF_MapPoiHandler> m_aHandlers = {};
	protected ref array<bool> m_aShown = {};
	protected ref LF_MapPoiHandler m_CardHandler;
	protected ref LF_MapPoiHandler m_ButtonHandler;
	protected int m_iHovered = -1;
	protected int m_iPinned = -1;
	protected int m_iCardGroup = -1;
	protected bool m_bOverCard;
	protected float m_fZoom;

	//------------------------------------------------------------------------------------------------
	//! The zoom of the view the map shows now.
	protected float CurrentZoom()
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		float ppu = m_MapEntity.GetCurrentZoom();
		if (!workspace || ppu <= 0)
			return 0;

		return LF_MapClusterRules.ZoomOfWidth(workspace.GetWidth() / ppu);
	}

	//------------------------------------------------------------------------------------------------
	override void OnMapOpen(MapConfiguration config)
	{
		super.OnMapOpen(config);

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace || !m_RootWidget)
		{
			LF_Debug.Error(FEATURE, "the map has no root widget; places are not drawn on the map");
			return;
		}

		m_wLayer = workspace.CreateWidget(WidgetType.FrameWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.NOFOCUS | WidgetFlags.IGNORE_CURSOR, Color.FromInt(Color.WHITE), 0, m_RootWidget);
		if (!m_wLayer)
		{
			LF_Debug.Error(FEATURE, "the places layer was not created; places are not drawn on the map");
			return;
		}

		FrameSlot.SetAnchorMin(m_wLayer, 0, 0);
		FrameSlot.SetAnchorMax(m_wLayer, 1, 1);
		FrameSlot.SetOffsets(m_wLayer, 0, 0, 0, 0);

		m_wCard = workspace.CreateWidgets(CARD_LAYOUT, m_wLayer);
		if (m_wCard)
		{
			m_wCard.SetVisible(false);
			m_CardHandler = new LF_MapPoiHandler();
			m_CardHandler.Init(this, -1, true);
			m_wCard.AddHandler(m_CardHandler);
			Widget button = m_wCard.FindAnyWidget("SetWaypoint");
			if (button)
			{
				m_ButtonHandler = new LF_MapPoiHandler();
				m_ButtonHandler.Init(this, -1, true);
				button.AddHandler(m_ButtonHandler);
			}
		}

		LF_MapPoiAudience audience = new LF_MapPoiAudience();
		LF_MapKnown.FillAudience(audience);
		LF_MapPois.CollectLive(audience, m_aPois);

		m_MapEntity.GetOnMapPan().Insert(OnMapPan);
		m_MapEntity.GetOnMapZoom().Insert(OnMapZoom);
		m_MapEntity.GetOnMapZoomEnd().Insert(OnMapZoomEnd);
		LF_Debug.Info(FEATURE, string.Format("map opened: %1 places for this player", m_aPois.Count()));

		Rebuild();
		if (System.IsCLIParam(PROBE_PARAM))
			RunProbe();
	}

	//------------------------------------------------------------------------------------------------
	override void OnMapClose(MapConfiguration config)
	{
		if (m_MapEntity)
		{
			m_MapEntity.GetOnMapPan().Remove(OnMapPan);
			m_MapEntity.GetOnMapZoom().Remove(OnMapZoom);
			m_MapEntity.GetOnMapZoomEnd().Remove(OnMapZoomEnd);
		}

		GetGame().GetCallqueue().Remove(HideIfIdle);
		if (m_wLayer)
			m_wLayer.RemoveFromHierarchy();

		m_wLayer = null;
		m_wCard = null;
		m_aPois.Clear();
		m_aGroups.Clear();
		m_aWidgets.Clear();
		m_aHandlers.Clear();
		m_aShown.Clear();
		m_CardHandler = null;
		m_ButtonHandler = null;
		m_iHovered = -1;
		m_iPinned = -1;
		m_iCardGroup = -1;
		m_bOverCard = false;
		super.OnMapClose(config);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnMapPan(float x, float y, bool adjustedPan)
	{
		Reposition();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnMapZoom(float pixelPerUnit)
	{
		Reposition();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnMapZoomEnd(float pixelPerUnit)
	{
		Rebuild();
	}

	//------------------------------------------------------------------------------------------------
	//! Buckets the POIs for the current zoom and gives every group a widget (reused, never freed while
	//! the map is open). Runs on open and when a zoom ends, not per frame.
	protected void Rebuild()
	{
		if (!m_wLayer)
			return;

		m_fZoom = CurrentZoom();
		m_aGroups.Clear();
		LF_MapClusterRules.BuildFromPois(m_aPois, m_fZoom, m_aGroups);
		HideCard();

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		while (m_aWidgets.Count() < m_aGroups.Count())
		{
			Widget created = workspace.CreateWidgets(POI_LAYOUT, m_wLayer);
			if (!created)
			{
				LF_Debug.Error(FEATURE, "a place icon was not created; the layer stops here");
				break;
			}

			FrameSlot.SetAlignment(created, 0.5, 0.5);
			LF_MapPoiHandler handler = new LF_MapPoiHandler();
			handler.Init(this, m_aWidgets.Count(), false);
			created.AddHandler(handler);
			m_aHandlers.Insert(handler);
			m_aWidgets.Insert(created);
			m_aShown.Insert(true);
		}

		for (int i = 0; i < m_aWidgets.Count(); i++)
		{
			if (i >= m_aGroups.Count())
			{
				SetShown(i, false);
				continue;
			}

			Style(i);
		}

		Reposition();
	}

	//------------------------------------------------------------------------------------------------
	//! Icon, count and tint for group i.
	protected void Style(int i)
	{
		LF_MapClusterGroup group = m_aGroups[i];
		Widget root = m_aWidgets[i];
		ImageWidget icon = ImageWidget.Cast(root.FindAnyWidget("Icon"));
		TextWidget count = TextWidget.Cast(root.FindAnyWidget("Count"));
		array<string> candidates = {};
		if (group.IsSingle())
		{
			LF_MapPoi poi = m_aPois[group.m_aMembers[0]];
			if (group.m_eState == LF_EMapPoiState.PENCIL)
				candidates.Insert("pencil_cross");
			else
				LF_MapPlaceIcons.Candidates(poi.m_sKind, poi.m_sIcon, poi.m_sCategory, candidates);
		}
		else
		{
			candidates.Insert("stamp_town");
			candidates.Insert("cat_civic");
		}

		LF_MapPlaceIcons.Apply(icon, candidates);
		if (icon)
		{
			if (group.m_eState == LF_EMapPoiState.FADED)
				icon.SetOpacity(FADED_ALPHA);
			else
				icon.SetOpacity(1);
		}

		if (count)
		{
			count.SetVisible(!group.IsSingle());
			if (!group.IsSingle())
				count.SetText(group.m_aMembers.Count().ToString());
		}

		m_aHandlers[i].SetIndex(i);
	}

	//------------------------------------------------------------------------------------------------
	protected void SetShown(int i, bool shown)
	{
		if (m_aShown[i] == shown)
			return;

		m_aShown[i] = shown;
		m_aWidgets[i].SetVisible(shown);
	}

	//------------------------------------------------------------------------------------------------
	//! Moves every icon to its place on the screen; an icon off the screen is hidden. The pan and zoom path.
	protected void Reposition()
	{
		if (!m_wLayer)
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		float width = workspace.DPIUnscale(workspace.GetWidth());
		float height = workspace.DPIUnscale(workspace.GetHeight());
		int groups = m_aGroups.Count();
		for (int i = 0; i < groups; i++)
		{
			LF_MapClusterGroup group = m_aGroups[i];
			int screenX;
			int screenY;
			m_MapEntity.WorldToScreen(group.m_fX, group.m_fZ, screenX, screenY, true);
			float x = workspace.DPIUnscale(screenX);
			float y = workspace.DPIUnscale(screenY);
			bool onScreen = x > -CULL_MARGIN && y > -CULL_MARGIN && x < width + CULL_MARGIN && y < height + CULL_MARGIN;
			SetShown(i, onScreen);
			if (onScreen)
				FrameSlot.SetPos(m_aWidgets[i], x, y);
		}

		if (m_iCardGroup >= 0)
			PlaceCard();
	}

	//------------------------------------------------------------------------------------------------
	//! Moves the visible card beside its icon, kept inside the screen.
	protected void PlaceCard()
	{
		if (!m_wCard || m_iCardGroup < 0 || m_iCardGroup >= m_aGroups.Count())
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		LF_MapClusterGroup group = m_aGroups[m_iCardGroup];
		int screenX;
		int screenY;
		m_MapEntity.WorldToScreen(group.m_fX, group.m_fZ, screenX, screenY, true);
		float x = workspace.DPIUnscale(screenX) + CARD_GAP;
		float y = workspace.DPIUnscale(screenY) - CARD_GAP;
		float cardW;
		float cardH;
		m_wCard.GetScreenSize(cardW, cardH);
		cardW = workspace.DPIUnscale(cardW);
		cardH = workspace.DPIUnscale(cardH);
		float width = workspace.DPIUnscale(workspace.GetWidth());
		float height = workspace.DPIUnscale(workspace.GetHeight());
		if (x + cardW > width)
			x = x - cardW - CARD_GAP * 2;

		if (y + cardH > height)
			y = height - cardH;

		if (y < 0)
			y = 0;

		FrameSlot.SetPos(m_wCard, x, y);
	}

	//------------------------------------------------------------------------------------------------
	//! The title of group i: a place's name with its town, or the count of a cluster.
	protected string GroupTitle(int i)
	{
		LF_MapClusterGroup group = m_aGroups[i];
		if (!group.IsSingle())
			return WidgetManager.Translate("#LF-MapPlaces_ClusterTitle", group.m_aMembers.Count().ToString());

		LF_MapPoi poi = m_aPois[group.m_aMembers[0]];
		return LF_MapPlaceCard.Title(WidgetManager.Translate(poi.m_sLabel), poi.m_sTown);
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowCard(int i)
	{
		if (!m_wCard || i < 0 || i >= m_aGroups.Count())
			return;

		LF_MapClusterGroup group = m_aGroups[i];
		m_iCardGroup = i;
		TextWidget title = TextWidget.Cast(m_wCard.FindAnyWidget("Title"));
		TextWidget note = TextWidget.Cast(m_wCard.FindAnyWidget("Note"));
		Widget button = m_wCard.FindAnyWidget("SetWaypoint");
		if (title)
			title.SetText(GroupTitle(i));

		string noteText;
		if (!group.IsSingle())
			noteText = WidgetManager.Translate("#LF-MapPlaces_ClusterNote");
		else if (group.m_eState == LF_EMapPoiState.FADED)
			noteText = WidgetManager.Translate("#LF-MapPlaces_HeardOf");

		if (note)
			note.SetText(noteText);

		if (button)
			button.SetVisible(group.IsSingle());

		m_wCard.SetVisible(true);
		PlaceCard();
	}

	//------------------------------------------------------------------------------------------------
	protected void HideCard()
	{
		m_iCardGroup = -1;
		m_iPinned = -1;
		if (m_wCard)
			m_wCard.SetVisible(false);
	}

	//------------------------------------------------------------------------------------------------
	//! The pointer left an icon or the card a moment ago: hide the card when it is over neither.
	protected void HideIfIdle()
	{
		if (m_iHovered == -1 && !m_bOverCard && m_iPinned == -1)
			HideCard();
	}

	//------------------------------------------------------------------------------------------------
	void OnPointerEnter(int index, bool card)
	{
		if (card)
		{
			m_bOverCard = true;
			return;
		}

		m_iHovered = index;
		ShowCard(index);
	}

	//------------------------------------------------------------------------------------------------
	void OnPointerLeave(int index, bool card)
	{
		if (card)
			m_bOverCard = false;
		else if (m_iHovered == index)
			m_iHovered = -1;

		GetGame().GetCallqueue().Remove(HideIfIdle);
		GetGame().GetCallqueue().CallLater(HideIfIdle, HIDE_DELAY_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	//! A click: on a cluster stamp it zooms in to the solo zoom there; on a single icon it pins the card;
	//! on the card's button (index -1) it sets the waypoint.
	void OnPointerClick(int index, bool card)
	{
		if (card)
		{
			if (m_iCardGroup < 0 || m_iCardGroup >= m_aGroups.Count())
				return;

			LF_MapClusterGroup shown = m_aGroups[m_iCardGroup];
			if (shown.IsSingle())
			{
				LF_MapPoi poi = m_aPois[shown.m_aMembers[0]];
				LF_MapPlaceCard.SetWaypoint(GroupTitle(m_iCardGroup), poi.m_vPos);
			}

			HideCard();
			return;
		}

		if (index < 0 || index >= m_aGroups.Count())
			return;

		LF_MapClusterGroup group = m_aGroups[index];
		if (group.IsSingle())
		{
			m_iPinned = index;
			ShowCard(index);
			return;
		}

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		float soloWidth = LF_MapClusterRules.WORLD_M / Math.Pow(2, LF_MapClusterRules.SOLO_ZOOM);
		m_MapEntity.ZoomPanSmooth(workspace.GetWidth() / soloWidth, group.m_fX, group.m_fZ);
		HideCard();
	}

	//------------------------------------------------------------------------------------------------
	//! Measures the widget route in the engine: a rebuild (cluster plus restyle) and a reposition over 400
	//! synthetic POIs, many passes. Runs only with `-lfMapPlacesProbe`, logs the mean cost in microseconds
	//! (#2232 notes; the numbers are in docs/architecture/everon-map.md).
	protected void RunProbe()
	{
		array<ref LF_MapPoi> real = {};
		foreach (LF_MapPoi saved : m_aPois)
			real.Insert(saved);

		m_aPois.Clear();
		for (int i = 0; i < 400; i++)
		{
			LF_MapPoi probe = new LF_MapPoi();
			probe.m_sId = "probe" + i.ToString();
			probe.m_sKind = "Bank";
			probe.m_sCategory = "civic";
			probe.m_vPos = Vector(Math.Mod(i * 7919.0, 12800), 0, Math.Mod(i * 104729.0, 12800));
			probe.m_eState = LF_EMapPoiState.FULL;
			m_aPois.Insert(probe);
		}

		int passes = 50;
		int start = System.GetTickCount();
		for (int p = 0; p < passes; p++)
			Rebuild();

		int rebuildMs = System.GetTickCount() - start;
		start = System.GetTickCount();
		for (int r = 0; r < passes * 10; r++)
			Reposition();

		int repositionMs = System.GetTickCount() - start;
		LF_Debug.Info(FEATURE, string.Format("probe widgets: 400 POIs, zoom %1, %2 groups: rebuild %3 us, reposition %4 us per pass", m_fZoom, m_aGroups.Count(), rebuildMs * 1000 / passes, repositionMs * 1000 / (passes * 10)));

		m_aPois.Clear();
		foreach (LF_MapPoi keep : real)
			m_aPois.Insert(keep);

		Rebuild();
	}
}
