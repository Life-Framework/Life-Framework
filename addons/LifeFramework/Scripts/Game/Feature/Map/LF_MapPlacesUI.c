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
//! Which icon quad a POI draws. The pack's imageset (m_IconSet of its POI overlay) is tried
//! first, the core placeholders second.
class LF_MapPlaceIcons
{
	static const ResourceName CORE_IMAGESET = "{E8ABC6949B537023}UI/Imagesets/LF_MapIcons.imageset";

	//------------------------------------------------------------------------------------------------
	//! The pack's icon set (the POI table's m_IconSet, set by a pack overlay), or empty: the neutral category
	//! glyphs only. Without it no 1989 pictogram ever drew and every place was a generic glyph.
	static ResourceName PackImageSet()
	{
		LF_MapPoiConfig config = LF_MapPoiConfig.Get();
		if (!config)
			return "";

		return config.m_IconSet;
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
		LF_MapIconPick pick = Resolve(widget, candidates);
		return pick && !pick.m_sQuad.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! Loads the first quad that exists and says which one it was (the imageset and the quad name), so a
	//! caller can remember the answer and not search the sets again (#2397). An empty quad means none loaded.
	static LF_MapIconPick Resolve(ImageWidget widget, notnull array<string> candidates)
	{
		LF_MapIconPick pick = new LF_MapIconPick();
		if (!widget)
			return pick;

		ResourceName pack = PackImageSet();
		foreach (string quad : candidates)
		{
			if (!pack.IsEmpty() && widget.LoadImageFromSet(0, pack, quad))
			{
				pick.m_Set = pack;
				pick.m_sQuad = quad;
				return pick;
			}

			if (widget.LoadImageFromSet(0, CORE_IMAGESET, quad))
			{
				pick.m_Set = CORE_IMAGESET;
				pick.m_sQuad = quad;
				return pick;
			}
		}

		return pick;
	}

	//------------------------------------------------------------------------------------------------
	//! Loads a quad `Resolve` found before. \return true when it loaded (nothing is loaded for an empty pick).
	static bool Load(ImageWidget widget, notnull LF_MapIconPick pick)
	{
		if (!widget || pick.m_sQuad.IsEmpty())
			return false;

		return widget.LoadImageFromSet(0, pick.m_Set, pick.m_sQuad);
	}
}

//------------------------------------------------------------------------------------------------
//! Which quad of which imageset a list of candidates came to (#2397). One object per distinct candidate list,
//! kept by the layer while the map is open; a widget that already shows it is not touched again.
class LF_MapIconPick : Managed
{
	ResourceName m_Set;
	string m_sQuad;
}

//------------------------------------------------------------------------------------------------
//! What a drawn icon shows, remembered so a rebuild writes only what changed (#2397): the engine calls a
//! restyle makes (image load, opacity, count text) cost about 15 us a widget, 400 widgets a zoom end, and the
//! solo zooms rebuild groups that are the same as before.
class LF_MapIconState : Managed
{
	//! The pick the icon shows (null before the first style).
	LF_MapIconPick m_Pick;
	bool m_bFaded;
	//! 0 for a single place (count hidden), the member count for a stamp, -1 before the first style.
	int m_iCount = -1;
	//! The place a single icon was styled for, and the state it was drawn in (null for a stamp). A place is
	//! built once and never edited, so the same object in the same state needs no restyle at all.
	LF_MapPoi m_Poi;
	LF_EMapPoiState m_ePoiState;
	//! The name written beside the icon (empty: none), and whether one was ever written.
	string m_sLabel;
	bool m_bLabelSet;

	//------------------------------------------------------------------------------------------------
	//! True when the icon already shows `poi` in `state`.
	bool SamePlace(LF_MapPoi poi, LF_EMapPoiState state)
	{
		return poi && m_Poi == poi && m_ePoiState == state;
	}

	//------------------------------------------------------------------------------------------------
	//! True when the icon must load `pick`.
	bool PickChanged(LF_MapIconPick pick)
	{
		return pick != m_Pick;
	}

	//------------------------------------------------------------------------------------------------
	bool FadedChanged(bool faded)
	{
		return faded != m_bFaded;
	}

	//------------------------------------------------------------------------------------------------
	bool CountChanged(int count)
	{
		return count != m_iCount;
	}

	//------------------------------------------------------------------------------------------------
	//! True when the label text must be written. The first style always writes (the layout starts visible and empty).
	bool LabelChanged(string text)
	{
		return text != m_sLabel || !m_bLabelSet;
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
	static const ResourceName LABEL_LAYOUT = "{4D2B9E6A1C73F808}UI/Layouts/Map/LF_MapPoiLabel.layout";
	static const ResourceName CARD_LAYOUT = "{CC650B4F4E7A5A77}UI/Layouts/Map/LF_MapPoiCard.layout";
	protected static const float CULL_MARGIN = 40;	// lf-fixed: reference units off screen before an icon is hidden
	protected static const float CARD_GAP = 16;	// lf-fixed: the card sits this far from its icon
	protected static const int HIDE_DELAY_MS = 200;	// lf-fixed: the pointer may cross from the icon to the card
	protected static const float FADED_ALPHA = 0.45;	// lf-fixed: heard of, not visited
	//! A place's name is written beside its icon from this zoom on (a view about 400 m wide): closer in, the names
	//! fit between the icons; further out, the stamps and bare icons keep the map readable.
	protected static const float LABEL_ZOOM = 5;	// lf-fixed: map readability, not a server tunable
	protected static const float LABEL_GAP = 46;	// lf-fixed: reference units from the icon centre to the start of its name (the icon is drawn 64 wide)
	protected static const float LABEL_DROP = 4;	// lf-fixed: reference units the name sits below the icon centre, level with the glyph
	protected static const string PROBE_PARAM = "lfMapPlacesProbe";

	protected Widget m_wLayer;
	protected Widget m_wCard;
	protected ref array<ref LF_MapPoi> m_aPois = {};
	protected ref array<ref LF_MapClusterGroup> m_aGroups = {};
	protected ref array<Widget> m_aWidgets = {};
	protected ref array<ref LF_MapPoiHandler> m_aHandlers = {};
	protected ref array<bool> m_aShown = {};
	protected ref array<ImageWidget> m_aIcons = {};	// the "Icon" and "Count" widgets of each group widget, found once (#2397)
	protected ref array<TextWidget> m_aCounts = {};
	protected ref array<TextWidget> m_aLabels = {};	// one name widget per group widget, beside its icon (a sibling in the layer, not a child: a child took the icon's width)
	protected ref array<bool> m_aLabelOn = {};	// whether the name has text to show (it also follows the icon off screen)
	protected ref array<ref LF_MapIconState> m_aStates = {};	// what each icon shows now
	protected ref map<string, ref LF_MapIconPick> m_mPicks = new map<string, ref LF_MapIconPick>();	// candidate list to the quad it resolved to
	protected ref LF_MapPoiHandler m_CardHandler;
	protected ref LF_MapPoiHandler m_ButtonHandler;
	protected int m_iHovered = -1;
	protected int m_iPinned = -1;
	protected int m_iCardGroup = -1;
	protected bool m_bOverCard;
	protected float m_fZoom;
	protected float m_fFrameSeconds;	// frame time summed since the map opened (the harness `map probe` reads it)
	protected int m_iFrames;

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

		LF_MapPoiAudience audience = LF_MapPoiAudience.Local();
		LF_MapPois.CollectLive(audience, m_aPois);

		m_MapEntity.GetOnMapPan().Insert(OnMapPan);
		m_MapEntity.GetOnMapZoom().Insert(OnMapZoom);
		m_MapEntity.GetOnMapZoomEnd().Insert(OnMapZoomEnd);
		LF_Debug.Info(FEATURE, string.Format("map opened: %1 places for this player", m_aPois.Count()));

		m_fFrameSeconds = 0;
		m_iFrames = 0;
		Rebuild();
		if (System.IsCLIParam(PROBE_PARAM))
			RunProbe();
	}

	//------------------------------------------------------------------------------------------------
	override void Update(float timeSlice)
	{
		m_fFrameSeconds += timeSlice;
		m_iFrames++;
	}

	//------------------------------------------------------------------------------------------------
	//! The mean frame time in ms since the map opened, and the frames it covers.
	float FrameMs(out int frames)
	{
		frames = m_iFrames;
		if (m_iFrames <= 0)
			return 0;

		return m_fFrameSeconds * 1000 / m_iFrames;
	}

	//------------------------------------------------------------------------------------------------
	//! The `-lfMapPlacesProbe` measurement on demand (the harness `map probe`). \return its log line.
	string Probe()
	{
		return RunProbe();
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
		m_aIcons.Clear();
		m_aCounts.Clear();
		m_aLabels.Clear();
		m_aLabelOn.Clear();
		m_aStates.Clear();
		m_mPicks.Clear();
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
		RebuildAt(CurrentZoom());
	}

	//------------------------------------------------------------------------------------------------
	//! Rebuild for a given zoom (the probe alternates two, so every pass changes the groups as a real zoom end does).
	protected void RebuildAt(float zoom)
	{
		if (!m_wLayer)
			return;

		m_fZoom = zoom;
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
			m_aIcons.Insert(ImageWidget.Cast(created.FindAnyWidget("Icon")));
			m_aCounts.Insert(TextWidget.Cast(created.FindAnyWidget("Count")));
			TextWidget label = TextWidget.Cast(workspace.CreateWidgets(LABEL_LAYOUT, m_wLayer));
			if (label)
			{
				FrameSlot.SetAlignment(label, 0, 0.5);
				label.SetVisible(false);
			}

			m_aLabels.Insert(label);
			m_aLabelOn.Insert(false);
			m_aStates.Insert(new LF_MapIconState());
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
	//! Only what differs from what the widget shows is written (#2397): the quad lookup is remembered per
	//! candidate list, and the image, opacity and count text are touched only when they change.
	protected void Style(int i)
	{
		LF_MapClusterGroup group = m_aGroups[i];
		LF_MapIconState state = m_aStates[i];
		m_aHandlers[i].SetIndex(i);
		LF_MapPoi place;
		if (group.IsSingle())
			place = m_aPois[group.m_aMembers[0]];

		// The name depends on the zoom as well as the place, so it is settled before the same-place early out.
		StyleLabel(i, group, state);

		// The solo zooms rebuild the same single places over and over: nothing to write.
		if (state.SamePlace(place, group.m_eState))
			return;

		state.m_Poi = place;
		state.m_ePoiState = group.m_eState;
		ImageWidget icon = m_aIcons[i];
		TextWidget count = m_aCounts[i];
		array<string> candidates = {};
		IconCandidates(group, candidates);
		if (icon)
		{
			LF_MapIconPick pick = PickFor(icon, candidates);
			if (state.PickChanged(pick))
			{
				LF_MapPlaceIcons.Load(icon, pick);
				state.m_Pick = pick;
			}

			bool faded = group.m_eState == LF_EMapPoiState.FADED;
			if (state.FadedChanged(faded))
			{
				if (faded)
					icon.SetOpacity(FADED_ALPHA);
				else
					icon.SetOpacity(1);

				state.m_bFaded = faded;
			}
		}

		if (count)
		{
			int members = 0;
			if (!group.IsSingle())
				members = group.m_aMembers.Count();

			if (state.CountChanged(members))
			{
				count.SetVisible(members > 0);
				if (members > 0)
					count.SetText(members.ToString());

				state.m_iCount = members;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The name of a single place beside its icon, from LABEL_ZOOM on. A stamp, a pencil mark (a hidden place the
	//! player only suspects) and a far view carry none. Written only when it changes.
	protected void StyleLabel(int i, notnull LF_MapClusterGroup group, notnull LF_MapIconState state)
	{
		TextWidget label = m_aLabels[i];
		if (!label)
			return;

		string text;
		if (group.IsSingle() && group.m_eState != LF_EMapPoiState.PENCIL && m_fZoom >= LABEL_ZOOM)
			text = LF_Terms.Expand(WidgetManager.Translate(m_aPois[group.m_aMembers[0]].m_sLabel));

		if (!state.LabelChanged(text))
			return;

		label.SetText(text);
		m_aLabelOn[i] = !text.IsEmpty();
		label.SetVisible(m_aLabelOn[i] && m_aShown[i]);
		state.m_sLabel = text;
		state.m_bLabelSet = true;
	}

	//------------------------------------------------------------------------------------------------
	//! The quads group i tries, in order: the kind's own, the row's icon, the category's; a pencil cross for a
	//! place only suspected; a stamp for a cluster. A modded layer (the heard-of "?") overrides this, not Style.
	protected void IconCandidates(notnull LF_MapClusterGroup group, notnull array<string> candidates)
	{
		if (!group.IsSingle())
		{
			candidates.Insert("stamp_town");
			candidates.Insert("cat_civic");
			return;
		}

		LF_MapPoi poi = m_aPois[group.m_aMembers[0]];
		if (group.m_eState == LF_EMapPoiState.PENCIL)
			candidates.Insert("pencil_cross");
		else
			LF_MapPlaceIcons.Candidates(poi.m_sKind, poi.m_sIcon, poi.m_sCategory, candidates);
	}

	//------------------------------------------------------------------------------------------------
	//! The quad a candidate list resolves to, searched in the imagesets once per list while the map is open.
	//! The first sight of a list loads it into `icon`.
	protected LF_MapIconPick PickFor(ImageWidget icon, notnull array<string> candidates)
	{
		string key;
		foreach (string quad : candidates)
		{
			key += quad + "|";
		}

		LF_MapIconPick pick;
		if (m_mPicks.Find(key, pick))
			return pick;

		pick = LF_MapPlaceIcons.Resolve(icon, candidates);
		m_mPicks.Set(key, pick);
		return pick;
	}

	//------------------------------------------------------------------------------------------------
	protected void SetShown(int i, bool shown)
	{
		if (m_aShown[i] == shown)
			return;

		m_aShown[i] = shown;
		m_aWidgets[i].SetVisible(shown);
		if (m_aLabels[i])
			m_aLabels[i].SetVisible(shown && m_aLabelOn[i]);
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
			{
				FrameSlot.SetPos(m_aWidgets[i], x, y);
				if (m_aLabels[i])
					FrameSlot.SetPos(m_aLabels[i], x + LABEL_GAP, y + LABEL_DROP);
			}
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
		return LF_MapPlaceCard.Title(LF_Terms.Expand(WidgetManager.Translate(poi.m_sLabel)), poi.m_sTown);
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
	protected string RunProbe()
	{
		array<ref LF_MapPoi> real = {};
		foreach (LF_MapPoi saved : m_aPois)
			real.Insert(saved);

		m_aPois.Clear();
		array<string> kinds = {"Bank", "Shop", "Clinic", "PoliceStation", "BusStop", "Garage"};
		array<string> categories = {"civic", "shop", "health", "law", "travel", "work"};
		// Spread over the ground the map shows now, so every icon is on screen (the dearest case).
		float centreX;
		float centreZ;
		m_MapEntity.GetMapCenterWorldPosition(centreX, centreZ);
		float viewM = 800;
		float ppu = m_MapEntity.GetCurrentZoom();
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (workspace && ppu > 0)
			viewM = workspace.GetWidth() / ppu;

		for (int i = 0; i < 400; i++)
		{
			LF_MapPoi probe = new LF_MapPoi();
			probe.m_sId = "probe" + i.ToString();
			probe.m_sKind = kinds[i % kinds.Count()];
			probe.m_sCategory = categories[i % categories.Count()];
			float fx = Math.Mod(i * 0.6180339, 1.0) - 0.5;
			float fz = Math.Mod(i * 0.7548776, 1.0) - 0.5;
			probe.m_vPos = Vector(centreX + fx * viewM * 0.9, 0, centreZ + fz * viewM * 0.5);
			probe.m_eState = LF_EMapPoiState.FULL;
			m_aPois.Insert(probe);
		}

		// A real zoom end changes the view, so the passes alternate two zooms: clustered (2 and 3) and solo (4 and 5).
		int passes = 50;
		float zoomLow = 2;
		float zoomHigh = 3;
		string line = "probe widgets: 400 POIs";
		for (int band = 0; band < 2; band++)
		{
			string name = "clustered 2/3";
			if (band == 1)
			{
				zoomLow = 4;
				zoomHigh = 5;
				name = "solo 4/5";
			}

			array<ref LF_MapClusterGroup> scratch = {};
			int start = System.GetTickCount();
			for (int c = 0; c < passes; c++)
			{
				scratch.Clear();
				float zoomC = zoomLow;
				if ((c % 2) == 1)
					zoomC = zoomHigh;

				LF_MapClusterRules.BuildFromPois(m_aPois, zoomC, scratch);
			}

			int clusterMs = System.GetTickCount() - start;
			start = System.GetTickCount();
			for (int p = 0; p < passes; p++)
			{
				float zoomP = zoomLow;
				if ((p % 2) == 1)
					zoomP = zoomHigh;

				RebuildAt(zoomP);
			}

			int rebuildMs = System.GetTickCount() - start;
			line += string.Format(" | zoom end %1, %2 groups: rebuild %3 us (cluster %4 us)", name, m_aGroups.Count(), rebuildMs * 1000 / passes, clusterMs * 1000 / passes);
		}

		int start2 = System.GetTickCount();
		for (int r = 0; r < passes * 10; r++)
			Reposition();

		int repositionMs = System.GetTickCount() - start2;
		line += string.Format(" | reposition %1 us per pass", repositionMs * 1000 / (passes * 10));
		LF_Debug.Info(FEATURE, line);

		m_aPois.Clear();
		foreach (LF_MapPoi keep : real)
			m_aPois.Insert(keep);

		Rebuild();
		return line;
	}
}
