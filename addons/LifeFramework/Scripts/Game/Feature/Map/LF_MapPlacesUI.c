// LF_MapPlacesUI.c: the points of interest drawn on the open map (#2232, epic #2223, card M4;
// docs/architecture/everon-map.md section 1, "Rendering").
//
// One small widget per drawn POI or cluster stamp in a frame over the map, placed with
// SCR_MapEntity.WorldToScreen. The clusters are rebuilt when a zoom ends (they depend on the zoom only: the
// buckets are in world space), the positions on pan and zoom events only; nothing runs while the map is
// closed. LF_MapDeclutter sizes the icons by zoom and folds icons that would overlap into one mark. No name is
// written on the map: the card under the map cursor (mouse or gamepad) names the place. A click on a place
// sets the waypoint (again: clears it), a click on a stamp or a crowded mark zooms in. The rules are LF_MapClusterRules, one case table with the node twin
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
	//! The place whose icon and name the group draws (an index into the POI list), or -1 for a cluster
	//! stamp. A group of several places that only crowd one spot on the screen (LF_MapDeclutter) keeps
	//! its most important place as the lead.
	int m_iLead = -1;

	//------------------------------------------------------------------------------------------------
	bool IsSingle()
	{
		return m_aMembers.Count() == 1;
	}

	//------------------------------------------------------------------------------------------------
	bool HasLead()
	{
		return m_iLead >= 0;
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
			if (group.IsSingle())
				group.m_iLead = group.m_aMembers[0];

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
	//! A crowded mark's card names this many of the other places.
	static const int MAX_NAMED = 3;	// lf-fixed: one line on the card

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

	//------------------------------------------------------------------------------------------------
	//! True when the waypoint, whoever set it, stands on this place.
	static bool IsWaypoint(vector position)
	{
		return LF_Waypoint.IsActive() && vector.DistanceXZ(LF_Waypoint.GetTarget(), position) < LF_MapWaypoint.SAME_PLACE_M;
	}

	//------------------------------------------------------------------------------------------------
	//! A click on a place: the waypoint goes there, or, when it already stands there, it is cleared (click it
	//! again to clear). \return "set", "cleared" or "refused" (no name).
	static string ClickPlace(string title, vector position)
	{
		if (IsWaypoint(position))
		{
			LF_Waypoint.Clear("clicked again on the map");
			return "cleared";
		}

		if (SetWaypoint(title, position))
			return "set";

		return "refused";
	}

	//------------------------------------------------------------------------------------------------
	//! The card's line for a place: what a click does there, on a mouse or a gamepad.
	static string NoteKey(bool isWaypoint, bool pad)
	{
		if (isWaypoint)
		{
			if (pad)
				return "#LF-MapPlaces_CardClearPad";

			return "#LF-MapPlaces_CardClear";
		}

		if (pad)
			return "#LF-MapPlaces_CardSetPad";

		return "#LF-MapPlaces_CardSet";
	}

	//------------------------------------------------------------------------------------------------
	//! "ATM, Cafe and 2 more": the other places of a crowded mark, at most MAX_NAMED by name.
	static string JoinNames(notnull array<string> names)
	{
		string text;
		int named = LF_Utils.MinInt(names.Count(), MAX_NAMED);
		for (int i = 0; i < named; i++)
		{
			if (i > 0)
				text += ", ";

			text += names[i];
		}

		if (names.Count() > named)
			text += " +" + (names.Count() - named).ToString();

		return text;
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
	//! The size the icon was given (0 before the first style).
	float m_fSize;

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
	bool SizeChanged(float size)
	{
		return Math.AbsFloat(size - m_fSize) > 0.01;
	}
}

//------------------------------------------------------------------------------------------------
//! The POI layer. Listed in Configs/Map/LF_MapFullscreen.conf like vanilla's map UI components, so it
//! exists only while the map is open.
//! The icons ignore the pointer (a drag that starts over one still pans the map); the layer finds the
//! mark under the map cursor itself (LF_MapDeclutter.Hit), which a gamepad cursor drives as well as a mouse.
class LF_MapPlacesUI : SCR_MapUIBaseComponent
{
	static const string FEATURE = "MapPlaces";
	static const ResourceName POI_LAYOUT = "{CB59D49AE5564BCD}UI/Layouts/Map/LF_MapPoi.layout";
	static const ResourceName CARD_LAYOUT = "{CC650B4F4E7A5A77}UI/Layouts/Map/LF_MapPoiCard.layout";
	//! Paper-outlined map type: the waypoint's tag and the legend's type names.
	static const ResourceName TYPE_LAYOUT = "{4D2B9E6A1C73F808}UI/Layouts/Map/LF_MapPoiLabel.layout";
	protected static const string SELECT_ACTION = "MapSelect";	// vanilla: left click, gamepad A
	protected static const float CULL_MARGIN = 40;	// lf-fixed: reference units off screen before an icon is hidden
	protected static const float CARD_GAP = 16;	// lf-fixed: the card sits this far from its icon
	protected static const float FADED_ALPHA = 0.45;	// lf-fixed: heard of, not visited
	//! A click on a crowded mark zooms in this many levels, up to MERGED_MAX_ZOOM; at that zoom a click sets
	//! the waypoint on the mark's lead place instead.
	protected static const float MERGED_ZOOM_STEP = 1.5;	// lf-fixed
	protected static const float MERGED_MAX_ZOOM = 8;	// lf-fixed: a view 50 m wide, closer than any two places on the island
	protected static const string PROBE_PARAM = "lfMapPlacesProbe";

	protected Widget m_wLayer;
	protected Widget m_wCard;
	protected ref array<ref LF_MapPoi> m_aPois = {};
	protected ref array<ref LF_MapClusterGroup> m_aGroups = {};
	protected ref array<Widget> m_aWidgets = {};
	protected ref array<bool> m_aShown = {};
	protected ref array<ImageWidget> m_aIcons = {};	// the "Icon" and "Count" widgets of each group widget, found once (#2397)
	protected ref array<TextWidget> m_aCounts = {};
	protected ref array<ref LF_MapIconState> m_aStates = {};	// what each icon shows now
	protected ref map<string, ref LF_MapIconPick> m_mPicks = new map<string, ref LF_MapIconPick>();	// candidate list to the quad it resolved to
	// Per group: its icon size, set at a zoom end (LF_MapDeclutter works in screen units, which a pan only
	// shifts), and where it is on the screen now, set on every reposition.
	protected ref array<float> m_aSizes = {};
	protected ref array<float> m_aScreenX = {};
	protected ref array<float> m_aScreenY = {};
	protected int m_iHovered = -1;
	protected int m_iPinned = -1;
	protected int m_iCardGroup = -1;
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
			m_wCard.SetFlags(WidgetFlags.IGNORE_CURSOR);
			m_wCard.SetVisible(false);
		}

		LF_MapPoiAudience audience = LF_MapPoiAudience.Local();
		LF_MapPois.CollectLive(audience, m_aPois);

		m_MapEntity.GetOnMapPan().Insert(OnMapPan);
		m_MapEntity.GetOnMapZoom().Insert(OnMapZoom);
		m_MapEntity.GetOnMapZoomEnd().Insert(OnMapZoomEnd);
		GetGame().GetInputManager().AddActionListener(SELECT_ACTION, EActionTrigger.DOWN, OnSelect);
		LF_Debug.Info(FEATURE, string.Format("map opened: %1 places for this player", m_aPois.Count()));

		m_fFrameSeconds = 0;
		m_iFrames = 0;
		Rebuild();
		if (System.IsCLIParam(PROBE_PARAM))
			RunProbe();
	}

	//------------------------------------------------------------------------------------------------
	//! The card follows the map cursor: the mark under it, else a pinned one (a search pick).
	override void Update(float timeSlice)
	{
		m_fFrameSeconds += timeSlice;
		m_iFrames++;
		if (!m_wLayer)
			return;

		int hovered = MarkAt(SCR_MapCursorInfo.x, SCR_MapCursorInfo.y);
		if (hovered == m_iHovered)
			return;

		m_iHovered = hovered;
		if (hovered >= 0)
			ShowCard(hovered);
		else if (m_iPinned >= 0)
			ShowCard(m_iPinned);
		else
			HideCard();
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
		GetGame().GetInputManager().RemoveActionListener(SELECT_ACTION, EActionTrigger.DOWN, OnSelect);
		if (m_MapEntity)
		{
			m_MapEntity.GetOnMapPan().Remove(OnMapPan);
			m_MapEntity.GetOnMapZoom().Remove(OnMapZoom);
			m_MapEntity.GetOnMapZoomEnd().Remove(OnMapZoomEnd);
		}

		if (m_wLayer)
			m_wLayer.RemoveFromHierarchy();

		m_wLayer = null;
		m_wCard = null;
		m_aPois.Clear();
		m_aGroups.Clear();
		m_aWidgets.Clear();
		m_aShown.Clear();
		m_aIcons.Clear();
		m_aCounts.Clear();
		m_aStates.Clear();
		m_mPicks.Clear();
		m_aSizes.Clear();
		m_aScreenX.Clear();
		m_aScreenY.Clear();
		m_iHovered = -1;
		m_iPinned = -1;
		m_iCardGroup = -1;
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
		array<ref LF_MapClusterGroup> raw = {};
		LF_MapClusterRules.BuildFromPois(m_aPois, m_fZoom, raw);
		Declutter(raw);
		HideCard();
		m_iHovered = -1;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		while (m_aWidgets.Count() < m_aGroups.Count())
		{
			Widget created = workspace.CreateWidgets(POI_LAYOUT, m_wLayer);
			if (!created)
			{
				LF_Debug.Error(FEATURE, "a place icon was not created; the layer stops here");
				break;
			}

			created.SetFlags(WidgetFlags.IGNORE_CURSOR);
			FrameSlot.SetAlignment(created, 0.5, 0.5);
			m_aWidgets.Insert(created);
			m_aShown.Insert(true);
			m_aIcons.Insert(ImageWidget.Cast(created.FindAnyWidget("Icon")));
			m_aCounts.Insert(TextWidget.Cast(created.FindAnyWidget("Count")));
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
	//! Turns the zoom's buckets into what is drawn (LF_MapDeclutter): marks whose icons would overlap on the
	//! screen join the most important one. The result is m_aGroups, most important first, with each size.
	protected void Declutter(notnull array<ref LF_MapClusterGroup> raw)
	{
		m_aGroups.Clear();
		m_aSizes.Clear();

		array<float> xs = {};
		array<float> ys = {};
		array<float> sizes = {};
		array<int> priorities = {};
		foreach (LF_MapClusterGroup group : raw)
		{
			float x;
			float y;
			ScreenOf(group, x, y);
			xs.Insert(x);
			ys.Insert(y);
			sizes.Insert(LF_MapDeclutter.IconSize(m_fZoom, !group.HasLead()));
			if (group.HasLead())
				priorities.Insert(LF_MapDeclutter.Priority(m_aPois[group.m_iLead].m_sCategory, group.m_eState, false));
			else
				priorities.Insert(LF_MapDeclutter.Priority("", group.m_eState, true));
		}

		array<int> order = {};
		LF_MapDeclutter.Order(priorities, order);
		array<int> leadOf = {};
		LF_MapDeclutter.Merge(xs, ys, sizes, order, leadOf);

		map<int, int> drawnAt = new map<int, int>();	// raw index of a mark to its index in m_aGroups
		foreach (int i : order)
		{
			LF_MapClusterGroup source = raw[i];
			if (leadOf[i] == i)
			{
				drawnAt.Set(i, m_aGroups.Count());
				m_aGroups.Insert(source);
				m_aSizes.Insert(sizes[i]);
				continue;
			}

			LF_MapClusterGroup mark = m_aGroups[drawnAt.Get(leadOf[i])];
			foreach (int member : source.m_aMembers)
			{
				mark.m_aMembers.Insert(member);
			}

			if (!source.HasLead())
			{
				// A stamp swallowed a place: the mark is a stamp now.
				mark.m_iLead = -1;
				m_aSizes[drawnAt.Get(leadOf[i])] = LF_MapDeclutter.IconSize(m_fZoom, true);
			}

			if (source.m_eState == LF_EMapPoiState.FULL || (source.m_eState == LF_EMapPoiState.FADED && mark.m_eState == LF_EMapPoiState.PENCIL))
				mark.m_eState = source.m_eState;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Where a group is on the screen now, in reference units.
	protected void ScreenOf(notnull LF_MapClusterGroup group, out float x, out float y)
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		int screenX;
		int screenY;
		m_MapEntity.WorldToScreen(group.m_fX, group.m_fZ, screenX, screenY, true);
		x = workspace.DPIUnscale(screenX);
		y = workspace.DPIUnscale(screenY);
	}

	//------------------------------------------------------------------------------------------------
	protected string PlaceName(int poiIndex)
	{
		return LF_Terms.Expand(WidgetManager.Translate(m_aPois[poiIndex].m_sLabel));
	}

	//------------------------------------------------------------------------------------------------
	//! Icon, size, count and tint for group i.
	//! Only what differs from what the widget shows is written (#2397): the quad lookup is remembered per
	//! candidate list, and the image, size, opacity and count text are touched only when they change.
	protected void Style(int i)
	{
		LF_MapClusterGroup group = m_aGroups[i];
		LF_MapIconState state = m_aStates[i];
		LF_MapPoi place;
		if (group.HasLead())
			place = m_aPois[group.m_iLead];

		// The size depends on the zoom as well as the place, so it is settled before the same-place early out.
		if (state.SizeChanged(m_aSizes[i]))
		{
			FrameSlot.SetSize(m_aWidgets[i], m_aSizes[i], m_aSizes[i]);
			if (m_aIcons[i])
				m_aIcons[i].SetSize(m_aSizes[i], m_aSizes[i]);

			state.m_fSize = m_aSizes[i];
		}

		int members = 0;
		if (!group.IsSingle())
			members = group.m_aMembers.Count();

		// The solo zooms rebuild the same single places over and over: nothing to write.
		if (state.SamePlace(place, group.m_eState) && !state.CountChanged(members))
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

		if (count && state.CountChanged(members))
		{
			count.SetVisible(members > 0);
			if (members > 0)
				count.SetText(members.ToString());

			state.m_iCount = members;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The quads group i tries, in order: the kind's own, the row's icon, the category's; a pencil cross for a
	//! place only suspected; a stamp for a cluster. A crowded mark draws its lead place's icon. A modded
	//! layer (the heard-of "?") overrides this, not Style.
	protected void IconCandidates(notnull LF_MapClusterGroup group, notnull array<string> candidates)
	{
		if (!group.HasLead())
		{
			candidates.Insert("stamp_town");
			candidates.Insert("cat_civic");
			return;
		}

		LF_MapPoi poi = m_aPois[group.m_iLead];
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
	}

	//------------------------------------------------------------------------------------------------
	//! Moves every icon to its place on the screen; an icon off the screen is hidden. The pan and zoom path.
	//! It also keeps the screen positions the pointer is tested against.
	protected void Reposition()
	{
		if (!m_wLayer)
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		float width = workspace.DPIUnscale(workspace.GetWidth());
		float height = workspace.DPIUnscale(workspace.GetHeight());
		int groups = m_aGroups.Count();
		m_aScreenX.Clear();
		m_aScreenY.Clear();
		for (int i = 0; i < groups; i++)
		{
			float x;
			float y;
			ScreenOf(m_aGroups[i], x, y);
			m_aScreenX.Insert(x);
			m_aScreenY.Insert(y);
			if (i >= m_aWidgets.Count())
				continue;

			bool onScreen = x > -CULL_MARGIN && y > -CULL_MARGIN && x < width + CULL_MARGIN && y < height + CULL_MARGIN;
			SetShown(i, onScreen);
			if (onScreen)
				FrameSlot.SetPos(m_aWidgets[i], x, y);
		}

		if (m_iCardGroup >= 0)
			PlaceCard();
	}

	//------------------------------------------------------------------------------------------------
	//! The drawn mark at (x, y) in reference units, or -1.
	int MarkAt(float x, float y)
	{
		int groups = m_aGroups.Count();
		if (m_aScreenX.Count() != groups || m_aShown.Count() < groups)
			return -1;

		return LF_MapDeclutter.Hit(x, y, m_aScreenX, m_aScreenY, m_aSizes, m_aShown);
	}

	//------------------------------------------------------------------------------------------------
	//! The place under the map cursor that a waypoint can go to: a single place, or a crowded mark's lead.
	//! \return false over a stamp or nothing.
	bool PlaceAtCursor(out string title, out vector position)
	{
		int i = MarkAt(SCR_MapCursorInfo.x, SCR_MapCursorInfo.y);
		if (i < 0 || !m_aGroups[i].HasLead())
			return false;

		title = PlaceTitle(m_aGroups[i].m_iLead);
		position = m_aPois[m_aGroups[i].m_iLead].m_vPos;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! HARNESS ONLY (`map hover <x> <z>`): pins the card of the mark nearest world (x, z), the card a pointer
	//! over that mark shows. \return the card's title, or empty with no mark.
	string HarnessHover(float x, float z)
	{
		int best = -1;
		float bestM = 0;
		foreach (int i, LF_MapClusterGroup group : m_aGroups)
		{
			float d = vector.DistanceXZ(Vector(group.m_fX, 0, group.m_fZ), Vector(x, 0, z));
			if (best == -1 || d < bestM)
			{
				best = i;
				bestM = d;
			}
		}

		if (best == -1)
			return "";

		m_iPinned = best;
		ShowCard(best);
		return GroupTitle(best);
	}

	//------------------------------------------------------------------------------------------------
	//! A plain click (left click, gamepad A): on a place it sets the waypoint there, or clears it when the
	//! waypoint is already on it; on a stamp, or a crowded mark not yet at the closest zoom, it zooms in.
	//! With the waypoint modifier held (Shift, gamepad Y) LF_MapWaypointUI takes the click instead.
	protected void OnSelect(float value, EActionTrigger reason)
	{
		if (!m_wLayer || LF_MapWaypoint.ModifierHeld() || !LF_MapWaypoint.MapTakesClicks(m_MapEntity))
			return;

		int index = MarkAt(SCR_MapCursorInfo.x, SCR_MapCursorInfo.y);
		if (index < 0)
		{
			if (m_iPinned >= 0)
			{
				m_iPinned = -1;
				HideCard();
			}

			return;
		}

		LF_MapClusterGroup group = m_aGroups[index];
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!group.HasLead())
		{
			float soloWidth = LF_MapClusterRules.WORLD_M / Math.Pow(2, LF_MapClusterRules.SOLO_ZOOM);
			m_MapEntity.ZoomPanSmooth(workspace.GetWidth() / soloWidth, group.m_fX, group.m_fZ);
			HideCard();
			return;
		}

		if (!group.IsSingle() && m_fZoom < MERGED_MAX_ZOOM - 0.01)
		{
			float closer = m_fZoom + MERGED_ZOOM_STEP;
			if (closer > MERGED_MAX_ZOOM)
				closer = MERGED_MAX_ZOOM;

			float viewWidth = LF_MapClusterRules.WORLD_M / Math.Pow(2, closer);
			m_MapEntity.ZoomPanSmooth(workspace.GetWidth() / viewWidth, group.m_fX, group.m_fZ);
			HideCard();
			return;
		}

		LF_MapPoi lead = m_aPois[group.m_iLead];
		string outcome = LF_MapPlaceCard.ClickPlace(PlaceTitle(group.m_iLead), lead.m_vPos);
		LF_Debug.Info(FEATURE, string.Format("click on %1: waypoint %2", lead.m_sId, outcome));
		ShowCard(index);
	}

	//------------------------------------------------------------------------------------------------
	//! Moves the visible card beside its icon, kept inside the screen.
	protected void PlaceCard()
	{
		if (!m_wCard || m_iCardGroup < 0 || m_iCardGroup >= m_aScreenX.Count())
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		float x = m_aScreenX[m_iCardGroup] + CARD_GAP;
		float y = m_aScreenY[m_iCardGroup] - CARD_GAP;
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
	//! "Bank, Saint-Philippe": a place's name and its town.
	protected string PlaceTitle(int poiIndex)
	{
		return LF_MapPlaceCard.Title(PlaceName(poiIndex), m_aPois[poiIndex].m_sTown);
	}

	//------------------------------------------------------------------------------------------------
	//! The title of group i: its lead place's name with its town, or the count of a cluster.
	protected string GroupTitle(int i)
	{
		LF_MapClusterGroup group = m_aGroups[i];
		if (!group.HasLead())
			return WidgetManager.Translate("#LF-MapPlaces_ClusterTitle", group.m_aMembers.Count().ToString());

		return PlaceTitle(group.m_iLead);
	}

	//------------------------------------------------------------------------------------------------
	//! The card's second line: what a click does here, for the device in use.
	protected string GroupNote(int i)
	{
		LF_MapClusterGroup group = m_aGroups[i];
		if (!group.HasLead())
			return WidgetManager.Translate("#LF-MapPlaces_ClusterNote");

		bool pad = !GetGame().GetInputManager().IsUsingMouseAndKeyboard();
		if (!group.IsSingle() && m_fZoom < MERGED_MAX_ZOOM - 0.01)
		{
			array<string> others = {};
			foreach (int member : group.m_aMembers)
			{
				if (member != group.m_iLead)
					others.Insert(PlaceName(member));
			}

			return WidgetManager.Translate("#LF-MapPlaces_CardCrowded", LF_MapPlaceCard.JoinNames(others));
		}

		LF_MapPoi lead = m_aPois[group.m_iLead];
		string note = WidgetManager.Translate(LF_MapPlaceCard.NoteKey(LF_MapPlaceCard.IsWaypoint(lead.m_vPos), pad));
		if (group.m_eState == LF_EMapPoiState.FADED)
			note = WidgetManager.Translate("#LF-MapPlaces_HeardOf") + " " + note;

		return note;
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowCard(int i)
	{
		if (!m_wCard || i < 0 || i >= m_aGroups.Count())
			return;

		m_iCardGroup = i;
		TextWidget title = TextWidget.Cast(m_wCard.FindAnyWidget("Title"));
		TextWidget note = TextWidget.Cast(m_wCard.FindAnyWidget("Note"));
		if (title)
			title.SetText(GroupTitle(i));

		if (note)
			note.SetText(GroupNote(i));

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
