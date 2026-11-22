// LF_MapLegendUI.c: the map key, the category filters and the place search on the open map (#2244, epic
// #2223, card M16; docs/architecture/everon-map.md section 5).
//
// A small "Map key" tab on the left edge of the map opens one UI kit sheet (LF_UIScreen parts, the paper
// theme): a search field with up to 8 matches, then one line per category the player's map shows, each
// with a Hide / Show button. Hiding a category takes its POIs off the map at once (LF_MapPlacesUI reads
// the same hidden list). Search matches only the places this player knows (the list the map already
// draws for them, so an unknown place is never found); a pick pans and zooms the map there and pins the
// place's card. The rules are LF_MapLegend (pure, so LF_Test_MapSearch covers them).

//------------------------------------------------------------------------------------------------
//! The legend, filter and search rules with no map and no widgets.
class LF_MapLegend
{
	static const string FEATURE = "MapLegend";
	//! Search lists at most this many places.
	static const int MAX_RESULTS = 8;	// lf-fixed: a short list the sheet shows without scrolling

	//! The legend order of the categories (the POI table's `m_sCategory` words).
	static ref array<string> s_aOrder = {"civic", "shop", "work", "law", "health", "travel", "story", "underworld"};
	//! Categories this viewer hid. A per-viewer preference, held by this client only and never sent.
	protected static ref array<string> s_aHidden = {};

	//------------------------------------------------------------------------------------------------
	static array<string> Hidden()
	{
		return s_aHidden;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aHidden.Clear();
		s_sSolo = "";
	}

	//! The one place type the key column shows alone (a click on its row), "" for all. Per viewer, never sent.
	protected static string s_sSolo;

	//------------------------------------------------------------------------------------------------
	static string Solo()
	{
		return s_sSolo;
	}

	//------------------------------------------------------------------------------------------------
	//! A click on a key row: show only that type; the same row again shows every type. \return the solo type now.
	static string ToggleSolo(string typeKey)
	{
		if (s_sSolo == typeKey)
			s_sSolo = "";
		else
			s_sSolo = typeKey;

		return s_sSolo;
	}

	//------------------------------------------------------------------------------------------------
	//! One key row per icon and type name: "poi_Bakery|shop|Bakery". Two kinds that draw the same icon under
	//! the same name share a row (and a highlight, and a solo filter).
	static string TypeKey(notnull LF_MapPoi poi)
	{
		string icon = poi.m_sIcon;
		if (icon.IsEmpty())
			icon = "poi_" + poi.m_sKind;

		return icon + "|" + poi.m_sCategory + "|" + TypeName(poi);
	}

	//------------------------------------------------------------------------------------------------
	//! The key column's lines: a header for each category in legend order, then one row per type in it.
	//! `pois` are the places the map knows (before any filter). \return how many lines.
	static int KeyLines(notnull array<ref LF_MapPoi> pois, notnull array<ref LF_MapKeyLine> into)
	{
		into.Clear();
		array<int> rows = {};
		IconRows(pois, rows);
		string category = "\n";
		foreach (int index : rows)
		{
			LF_MapPoi poi = pois[index];
			if (poi.m_sCategory != category)
			{
				category = poi.m_sCategory;
				into.Insert(LF_MapKeyLine.Header(category));
			}

			into.Insert(LF_MapKeyLine.Row(category, TypeKey(poi), index));
		}

		return into.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! A category header's swatch: the badge disc colour of that group (tools/map/icons/build-icons.mjs
	//! GROUP_FILL), so the header matches the badges under it.
	static int CategoryColor(string category)
	{
		if (category == "civic")
			return 0xFF1F9D9A;

		if (category == "shop")
			return 0xFF2FA04A;

		if (category == "work")
			return 0xFFB87A34;

		if (category == "law")
			return 0xFF2D4FD0;

		if (category == "health")
			return 0xFFE0352F;

		if (category == "travel")
			return 0xFF3A9BD9;

		if (category == "story")
			return 0xFFD0388C;

		if (category == "underworld")
			return 0xFF6B5B8A;

		return 0xFF4F5D6B;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsHidden(notnull array<string> hidden, string category)
	{
		return hidden.Find(category) != -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Flips a category in the hidden list. \return true when it is hidden after the flip.
	static bool Toggle(notnull array<string> hidden, string category)
	{
		int at = hidden.Find(category);
		if (at != -1)
		{
			hidden.RemoveOrdered(at);
			return false;
		}

		hidden.Insert(category);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The categories the POIs hold: the legend order first, any other word after, each once. A category
	//! with no place the player can see has no line (the legend never names what the player has not met).
	static void Categories(notnull array<ref LF_MapPoi> pois, notnull array<string> into)
	{
		foreach (string known : s_aOrder)
		{
			foreach (LF_MapPoi poi : pois)
			{
				if (poi.m_sCategory == known)
				{
					into.Insert(known);
					break;
				}
			}
		}

		foreach (LF_MapPoi other : pois)
		{
			if (!other.m_sCategory.IsEmpty() && into.Find(other.m_sCategory) == -1)
				into.Insert(other.m_sCategory);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The POIs whose category is not hidden, in input order.
	//! \return How many were added to into.
	static int Filter(notnull array<ref LF_MapPoi> pois, notnull array<string> hidden, notnull array<ref LF_MapPoi> into)
	{
		int added = 0;
		foreach (LF_MapPoi poi : pois)
		{
			if (IsHidden(hidden, poi.m_sCategory))
				continue;

			if (!s_sSolo.IsEmpty() && TypeKey(poi) != s_sSolo)
				continue;

			into.Insert(poi);
			added++;
		}

		return added;
	}

	//------------------------------------------------------------------------------------------------
	//! "Bank, Saint-Philippe": what the hover card and the search list call a place.
	static string NameOf(notnull LF_MapPoi poi)
	{
		return LF_MapPlaceCard.Title(WidgetManager.Translate(poi.m_sLabel), poi.m_sTown);
	}

	//------------------------------------------------------------------------------------------------
	//! A place's type as the legend prints it: its kind's name, no town ("Bakery").
	static string TypeName(notnull LF_MapPoi poi)
	{
		return LF_Terms.Expand(WidgetManager.Translate(poi.m_sLabel));
	}

	//------------------------------------------------------------------------------------------------
	//! The icon legend's lines: one POI per icon and type name the list holds (the many payphones are one
	//! line, and so are two kinds that draw the same icon under the same name), in the legend's category order, then first seen. A pencil mark (a place only suspected) has no
	//! line: its type is not known. \return how many lines.
	static int IconRows(notnull array<ref LF_MapPoi> pois, notnull array<int> into)
	{
		array<string> seen = {};
		array<int> priorities = {};
		array<int> found = {};
		foreach (int i, LF_MapPoi poi : pois)
		{
			if (poi.m_eState == LF_EMapPoiState.PENCIL)
				continue;

			string key = TypeKey(poi);
			if (seen.Find(key) != -1)
				continue;

			seen.Insert(key);
			found.Insert(i);
			priorities.Insert(LF_MapDeclutter.Priority(poi.m_sCategory, LF_EMapPoiState.FULL, false));
		}

		array<int> order = {};
		LF_MapDeclutter.Order(priorities, order);
		foreach (int at : order)
		{
			into.Insert(found[at]);
		}

		return order.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! A lower-case copy, so search ignores case.
	static string Fold(string text)
	{
		string folded = text;
		folded.ToLower();
		return folded;
	}

	//------------------------------------------------------------------------------------------------
	//! The places a typed word finds: only places the player knows (a hidden state never matches), by a
	//! case-insensitive part of the name, names that begin with the word before names that only contain it.
	//! An empty word finds nothing.
	//! \param[out] into Indexes into pois, at most limit.
	//! \return How many matched.
	static int Search(string query, notnull array<ref LF_MapPoi> pois, int limit, notnull array<int> into)
	{
		string word = Fold(query).Trim();
		if (word.IsEmpty() || limit <= 0)
			return 0;

		array<int> prefix = {};
		array<int> inside = {};
		foreach (int i, LF_MapPoi poi : pois)
		{
			if (poi.m_eState == LF_EMapPoiState.HIDDEN)
				continue;

			int at = Fold(NameOf(poi)).IndexOf(word);
			if (at == 0)
				prefix.Insert(i);
			else if (at > 0)
				inside.Insert(i);
		}

		int added = 0;
		foreach (int p : prefix)
		{
			if (added >= limit)
				return added;

			into.Insert(p);
			added++;
		}

		foreach (int q : inside)
		{
			if (added >= limit)
				return added;

			into.Insert(q);
			added++;
		}

		return added;
	}

	//------------------------------------------------------------------------------------------------
	//! The string key of a category's line. A category word no key names (a pack's own) reads as written.
	static string CategoryKey(string category)
	{
		if (category == "civic")
			return "#LF-MapLegend_Cat_civic";

		if (category == "shop")
			return "#LF-MapLegend_Cat_shop";

		if (category == "work")
			return "#LF-MapLegend_Cat_work";

		if (category == "law")
			return "#LF-MapLegend_Cat_law";

		if (category == "health")
			return "#LF-MapLegend_Cat_health";

		if (category == "travel")
			return "#LF-MapLegend_Cat_travel";

		if (category == "story")
			return "#LF-MapLegend_Cat_story";

		if (category == "underworld")
			return "#LF-MapLegend_Cat_underworld";

		return category;
	}

	//------------------------------------------------------------------------------------------------
	static string RowName(string category)
	{
		return "Cat_" + category;
	}

	//------------------------------------------------------------------------------------------------
	//! The sheet's parts: title, search field and button, the results list, then one status row per category.
	static array<ref LF_UIPart> BuildParts(notnull array<string> categories, notnull array<string> hidden)
	{
		array<ref LF_UIPart> parts = {};
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, "Title", "#LF-MapLegend_Title"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.FIELD, "SearchField", "#LF-MapLegend_Search"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.BUTTON, "SearchButton", "#LF-MapLegend_Find"));
		parts.Insert(LF_UIPart.CreateList("Results"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.HEADING, "KeyHeading", "#LF-MapLegend_Heading"));
		foreach (string category : categories)
		{
			if (IsHidden(hidden, category))
				parts.Insert(LF_UIPart.CreateStatus(RowName(category), CategoryKey(category), "#LF-MapLegend_Off", "#LF-MapLegend_Show"));
			else
				parts.Insert(LF_UIPart.CreateStatus(RowName(category), CategoryKey(category), "#LF-MapLegend_On", "#LF-MapLegend_Hide"));
		}

		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, "Close", "#LF-MapLegend_Close"));
		return parts;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_MapLegend's statics (LF_StaticReset finds it by reflection).
class LF_MapLegend_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_MapLegend";
	}

	override void Apply()
	{
		LF_MapLegend.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! The places layer with the legend's two hooks: it draws only the categories the viewer left on, and it
//! can pin one place's card after a search pick. Kept here so the legend owns its whole feature.
modded class LF_MapPlacesUI
{
	protected ref array<ref LF_MapPoi> m_aAllPois;
	protected string m_sFlashId;

	//------------------------------------------------------------------------------------------------
	override void OnMapOpen(MapConfiguration config)
	{
		super.OnMapOpen(config);

		m_aAllPois = new array<ref LF_MapPoi>();
		foreach (LF_MapPoi poi : m_aPois)
		{
			m_aAllPois.Insert(poi);
		}

		if (LF_MapLegend.Hidden().Count() > 0 || !LF_MapLegend.Solo().IsEmpty())
			ApplyLegendFilter();
	}

	//------------------------------------------------------------------------------------------------
	override void OnMapClose(MapConfiguration config)
	{
		GetGame().GetCallqueue().Remove(FlashNow);
		m_aAllPois = null;
		m_sFlashId = "";
		super.OnMapClose(config);
	}

	//------------------------------------------------------------------------------------------------
	//! Every place the viewer knows, before the category filter (what the legend lists and search reads).
	array<ref LF_MapPoi> GetAllPois()
	{
		return m_aAllPois;
	}

	//------------------------------------------------------------------------------------------------
	//! Draws only the places whose category the viewer has not hidden.
	void ApplyLegendFilter()
	{
		if (!m_aAllPois)
			return;

		m_aPois.Clear();
		int shown = LF_MapLegend.Filter(m_aAllPois, LF_MapLegend.Hidden(), m_aPois);
		LF_Debug.Info(LF_MapLegend.FEATURE, string.Format("filter: %1 of %2 places drawn", shown, m_aAllPois.Count()));
		Rebuild();
	}

	//------------------------------------------------------------------------------------------------
	//! HARNESS ONLY (`maplegend demo`): no world holds POIs yet, so the final picture adds four stand-in
	//! places around a point the proof names. They are drawn through the same layer and filter; nothing
	//! is saved or sent.
	void AddDemoPois(vector centre)
	{
		if (!m_aAllPois)
			return;

		AddDemo("Bank", "civic", "#LF-MapPoi_Bank", LF_EMapPoiState.FULL, centre, 90, 30);
		AddDemo("PoliceStation", "law", "#LF-MapPoi_PoliceStation", LF_EMapPoiState.FULL, centre, -110, 60);
		AddDemo("HospitalSite", "health", "#LF-MapPoi_Hospital", LF_EMapPoiState.FULL, centre, 20, -120);
		AddDemo("Workplace", "work", "#LF-MapPoi_Workplace", LF_EMapPoiState.FADED, centre, -60, -90);
		ApplyLegendFilter();
	}

	//------------------------------------------------------------------------------------------------
	protected void AddDemo(string kind, string category, string label, LF_EMapPoiState state, vector centre, float dx, float dz)
	{
		LF_MapPoi poi = new LF_MapPoi();
		poi.m_sKind = kind;
		poi.m_sId = "demo_" + kind;
		poi.m_sCategory = category;
		poi.m_sLabel = label;
		poi.m_eState = state;
		poi.m_vPos = Vector(centre[0] + dx, 0, centre[2] + dz);
		m_aAllPois.Insert(poi);
	}

	//------------------------------------------------------------------------------------------------
	//! Pans and zooms to a place and pins its card (the zoom runs on, so the card waits for it).
	void FlashPlace(notnull LF_MapPoi poi)
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		m_sFlashId = poi.m_sId;
		float soloWidth = LF_MapClusterRules.WORLD_M / Math.Pow(2, LF_MapClusterRules.SOLO_ZOOM);
		m_MapEntity.ZoomPanSmooth(workspace.GetWidth() / soloWidth, poi.m_vPos[0], poi.m_vPos[2]);
		GetGame().GetCallqueue().Remove(FlashNow);
		GetGame().GetCallqueue().CallLater(FlashNow, 900, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void FlashNow()
	{
		if (m_sFlashId.IsEmpty())
			return;

		// The place may share a crowded mark with others: pin the mark that holds it.
		foreach (int i, LF_MapClusterGroup group : m_aGroups)
		{
			if (!group.HasLead())
				continue;

			foreach (int member : group.m_aMembers)
			{
				if (m_aPois[member].m_sId != m_sFlashId)
					continue;

				m_iPinned = i;
				ShowCard(i);
				m_sFlashId = "";
				return;
			}
		}

		m_sFlashId = "";
	}
}

//------------------------------------------------------------------------------------------------
//! The map UI component: the tab that opens the sheet, and the sheet's buttons. Listed in
//! Configs/Map/LF_MapFullscreen.conf, so it exists only while the map is open.
class LF_MapLegendUI : SCR_MapUIBaseComponent
{
	protected static const string SEARCH_BUTTON = "SearchButton";
	protected static const string CLOSE_BUTTON = "Close";
	protected static const string RESULT_ROW = "Result_";
	protected static const float TAB_LEFT = 8;	// lf-fixed: the tab sits at the left edge of the map
	protected static const float TAB_TOP = 260;	// lf-fixed: under the vanilla tool strip
	protected static const float LIST_HEIGHT = 190;	// lf-fixed: eight rows scroll inside the sheet
	protected static const float KEY_RIGHT = 10;	// lf-fixed: the key column's margin from the right edge
	protected static const float KEY_TOP = 70;	// lf-fixed: under the vanilla top bar
	protected static const float KEY_TAB_GAP = 32;	// lf-fixed: the column starts under its tab
	protected static const float KEY_PAD = 6;	// lf-fixed
	protected static const int KEY_PAPER = 0xC8EEE6D6;	// lf-fixed: the paper theme at about 78 % so the map shows through
	protected static const float KEY_OFF_ALPHA = 0.4;	// lf-fixed: a hidden group or filtered-out type in the key

	protected Widget m_wTab;
	protected Widget m_wKeyTab;
	protected TextWidget m_wKeyTabText;
	protected ref LF_MapKeyTabHandler m_KeyTabHandler;
	protected static const float KEY_TAB_WIDTH = 110;	// lf-fixed: "Key: hide" in the tab type
	protected static const float KEY_TAB_HEIGHT = 28;	// lf-fixed
	protected static const int KEY_CHIP = 0xFFEEE6D6;	// lf-fixed: the paper theme, opaque so the tab reads on any map colour
	protected static const int KEY_RIM = 0xFF3E403A;	// lf-fixed: the card rim (LF_MapPoiCard's Metal)
	protected Widget m_wKey;
	protected bool m_bKeyFolded;
	protected int m_iKeyBuiltFor = -1;
	protected ref array<ref LF_MapKeyRowHandler> m_aKeyHandlers = {};
	protected static float s_fKeyX;
	protected static float s_fKeyY;
	protected static float s_fKeyW;
	protected static float s_fKeyH;
	protected ref LF_UIScreen m_Sheet;
	protected ref array<ref LF_MapPoi> m_aResults = {};

	//------------------------------------------------------------------------------------------------
	override void OnMapOpen(MapConfiguration config)
	{
		super.OnMapOpen(config);

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace || !m_RootWidget)
		{
			LF_Debug.Error(LF_MapLegend.FEATURE, "the map has no root widget; the map key is not shown");
			return;
		}

		m_wTab = workspace.CreateWidgets(LF_UITheme.BUTTON_LAYOUT, m_RootWidget);
		if (!m_wTab)
		{
			LF_Debug.Error(LF_MapLegend.FEATURE, "the map key tab was not created");
			return;
		}

		FrameSlot.SetAnchorMin(m_wTab, 0, 0);
		FrameSlot.SetAnchorMax(m_wTab, 0, 0);
		FrameSlot.SetPos(m_wTab, TAB_LEFT, TAB_TOP);
		FrameSlot.SetSizeToContent(m_wTab, true);

		SCR_ButtonTextComponent tab = SCR_ButtonTextComponent.Cast(m_wTab.FindHandler(SCR_ButtonTextComponent));
		if (tab)
		{
			tab.SetText(LF_Terms.Shown("#LF-MapLegend_Tab"));
			LF_UISkin.Paper().RecolorButton(m_wTab);
			tab.m_OnClicked.Insert(OnTabClicked);
		}
		else
		{
			LF_Debug.Error(LF_MapLegend.FEATURE, "the map key tab has no button component");
		}

		BuildKey(workspace);
	}

	//------------------------------------------------------------------------------------------------
	//! The key column on the right edge of the map (map clean-up 2026-11-22, the maintainer: "a key that shows
	//! what the different things are on the right side without it being too obtrusive"). One header per group
	//! with its badge colour, then the badge and short name of each type this map shows. Hovering a row lifts
	//! those badges on the map and dims the rest; a click on a row shows only that type (again: all), a click
	//! on a header hides or shows the group. The Key tab folds it to a thin tab, remembered per player. It
	//! replaces the Legend slip that opened under Map key: one list of icons.
	protected void BuildKey(notnull WorkspaceWidget workspace)
	{
		m_bKeyFolded = LF_MapHelp.LoadFolded(LF_MapKeyLine.PREF_PATH);
		// The tab: a paper chip with dark ink (the kit button drew near-white on the grey map, 2026-11-22).
		m_wKeyTab = workspace.CreateWidget(WidgetType.FrameWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.NOFOCUS, Color.FromInt(Color.WHITE), 0, m_RootWidget);
		if (!m_wKeyTab)
		{
			LF_Debug.Error(LF_MapLegend.FEATURE, "the key tab was not created; the map shows no key");
			return;
		}

		FrameSlot.SetAnchorMin(m_wKeyTab, 1, 0);
		FrameSlot.SetAnchorMax(m_wKeyTab, 1, 0);
		FrameSlot.SetAlignment(m_wKeyTab, 1, 0);
		FrameSlot.SetPos(m_wKeyTab, -KEY_RIGHT, KEY_TOP);
		FrameSlot.SetSize(m_wKeyTab, KEY_TAB_WIDTH, KEY_TAB_HEIGHT);
		Widget rim = workspace.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.NOFOCUS, Color.FromInt(KEY_RIM), 0, m_wKeyTab);
		if (rim)
		{
			FrameSlot.SetAnchorMin(rim, 0, 0);
			FrameSlot.SetAnchorMax(rim, 1, 1);
			FrameSlot.SetOffsets(rim, 0, 0, 0, 0);
		}

		Widget chip = workspace.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.NOFOCUS, Color.FromInt(KEY_CHIP), 0, m_wKeyTab);
		if (chip)
		{
			FrameSlot.SetAnchorMin(chip, 0, 0);
			FrameSlot.SetAnchorMax(chip, 1, 1);
			FrameSlot.SetOffsets(chip, 2, 2, 2, 2);
		}

		m_wKeyTabText = TextWidget.Cast(workspace.CreateWidgets(LF_MapPlacesUI.TYPE_LAYOUT, m_wKeyTab));
		if (m_wKeyTabText)
		{
			m_wKeyTabText.SetFlags(WidgetFlags.IGNORE_CURSOR);
			m_wKeyTabText.SetExactFontSize(LF_MapKeyLine.FONT_SIZE + 2);
			FrameSlot.SetPos(m_wKeyTabText, 10, 4);
			FrameSlot.SetSize(m_wKeyTabText, KEY_TAB_WIDTH - 14, KEY_TAB_HEIGHT - 6);
		}

		LF_MapKeyTabHandler handler = new LF_MapKeyTabHandler();
		handler.m_Owner = this;
		m_wKeyTab.AddHandler(handler);
		m_KeyTabHandler = handler;
		RebuildKey();
	}

	//------------------------------------------------------------------------------------------------
	void OnKeyTabClicked()
	{
		SetKeyFolded(!m_bKeyFolded);
	}

	//------------------------------------------------------------------------------------------------
	protected void SetKeyFolded(bool folded)
	{
		m_bKeyFolded = folded;
		LF_MapHelp.SaveFolded(folded, LF_MapKeyLine.PREF_PATH);
		LF_Debug.Info(LF_MapLegend.FEATURE, string.Format("key column folded=%1", folded));
		HighlightRow(null);
		RebuildKey();
	}

	//------------------------------------------------------------------------------------------------
	//! Builds the column from the places the map knows now: a soft paper backing, then the lines top down,
	//! flowing into a second column (to the left) when one is full.
	protected void RebuildKey()
	{
		ClearKey();
		if (!m_wKeyTab)
			return;

		if (m_wKeyTabText)
		{
			if (m_bKeyFolded)
				m_wKeyTabText.SetText(WidgetManager.Translate("#LF-MapKey_TabFolded"));
			else
				m_wKeyTabText.SetText(WidgetManager.Translate("#LF-MapKey_Tab"));
		}

		LF_MapPlacesUI places = Places();
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!places || !places.GetAllPois() || !workspace)
			return;

		m_iKeyBuiltFor = places.GetAllPois().Count();
		if (m_bKeyFolded)
			return;

		array<ref LF_MapKeyLine> lines = {};
		int count = LF_MapLegend.KeyLines(places.GetAllPois(), lines);
		if (count == 0)
			return;

		array<int> cols = {};
		array<int> rows = {};
		int perColumn = LF_MapKeyLine.PerColumn(count);
		int columns = LF_MapKeyLine.Place(lines, perColumn, cols, rows);
		float width = columns * LF_MapKeyLine.COLUMN_WIDTH + KEY_PAD * 2;
		float height = perColumn * LF_MapKeyLine.ROW_HEIGHT + KEY_PAD * 2;
		m_wKey = workspace.CreateWidget(WidgetType.FrameWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.NOFOCUS, Color.FromInt(Color.WHITE), 0, m_RootWidget);
		if (!m_wKey)
		{
			LF_Debug.Error(LF_MapLegend.FEATURE, "the key column was not created");
			return;
		}

		FrameSlot.SetAnchorMin(m_wKey, 1, 0);
		FrameSlot.SetAnchorMax(m_wKey, 1, 0);
		FrameSlot.SetAlignment(m_wKey, 1, 0);
		FrameSlot.SetPos(m_wKey, -KEY_RIGHT, KEY_TOP + KEY_TAB_GAP);
		FrameSlot.SetSize(m_wKey, width, height);
		Widget backing = workspace.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.NOFOCUS, Color.FromInt(KEY_PAPER), 0, m_wKey);
		if (backing)
		{
			FrameSlot.SetAnchorMin(backing, 0, 0);
			FrameSlot.SetAnchorMax(backing, 1, 1);
			FrameSlot.SetOffsets(backing, 0, 0, 0, 0);
		}

		foreach (int i, LF_MapKeyLine line : lines)
		{
			float x = KEY_PAD + cols[i] * LF_MapKeyLine.COLUMN_WIDTH;
			float y = KEY_PAD + rows[i] * LF_MapKeyLine.ROW_HEIGHT;
			AddKeyLine(workspace, line, places.GetAllPois(), x, y);
		}

		RefreshKeyDim();
		LF_Debug.Info(LF_MapLegend.FEATURE, string.Format("key column: %1 line(s) in %2 column(s)", count, columns));
	}

	//------------------------------------------------------------------------------------------------
	//! One line: a header (colour swatch and group name) or a row (badge and type name), each a frame that
	//! takes the pointer through LF_MapKeyRowHandler.
	protected void AddKeyLine(notnull WorkspaceWidget workspace, notnull LF_MapKeyLine line, notnull array<ref LF_MapPoi> pois, float x, float y)
	{
		Widget row = workspace.CreateWidget(WidgetType.FrameWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.NOFOCUS, Color.FromInt(Color.WHITE), 0, m_wKey);
		if (!row)
			return;

		FrameSlot.SetPos(row, x, y);
		FrameSlot.SetSize(row, LF_MapKeyLine.COLUMN_WIDTH - 4, LF_MapKeyLine.ROW_HEIGHT);
		float iconSize = LF_MapKeyLine.ROW_HEIGHT - 3;
		string text;
		if (line.m_bHeader)
		{
			Widget swatch = workspace.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.NOFOCUS, Color.FromInt(LF_MapLegend.CategoryColor(line.m_sCategory)), 0, row);
			if (swatch)
			{
				FrameSlot.SetPos(swatch, 2, 4);
				FrameSlot.SetSize(swatch, iconSize - 4, iconSize - 6);
			}

			text = WidgetManager.Translate(LF_MapLegend.CategoryKey(line.m_sCategory));
			text.ToUpper();
		}
		else
		{
			LF_MapPoi poi = pois[line.m_iPoi];
			Widget icon = workspace.CreateWidgets(LF_MapPlacesUI.POI_LAYOUT, row);
			if (icon)
			{
				icon.SetFlags(WidgetFlags.IGNORE_CURSOR);
				FrameSlot.SetPos(icon, 0, 1);
				FrameSlot.SetSize(icon, iconSize, iconSize);
				ImageWidget image = ImageWidget.Cast(icon.FindAnyWidget("Icon"));
				array<string> candidates = {};
				LF_MapPlaceIcons.Candidates(poi.m_sKind, poi.m_sIcon, poi.m_sCategory, candidates);
				if (image)
				{
					LF_MapPlaceCard.FillParent(image);
					LF_MapPlaceIcons.Resolve(image, candidates);
				}

				Widget count = icon.FindAnyWidget("Count");
				if (count)
					count.SetVisible(false);
			}

			text = LF_MapLegend.TypeName(poi);
		}

		TextWidget name = TextWidget.Cast(workspace.CreateWidgets(LF_MapPlacesUI.TYPE_LAYOUT, row));
		if (name)
		{
			name.SetFlags(WidgetFlags.IGNORE_CURSOR);
			name.SetText(text);
			name.SetExactFontSize(LF_MapKeyLine.FONT_SIZE);
			FrameSlot.SetPos(name, iconSize + 5, 2);
			FrameSlot.SetSize(name, LF_MapKeyLine.COLUMN_WIDTH - iconSize - 10, LF_MapKeyLine.ROW_HEIGHT - 2);
		}

		LF_MapKeyRowHandler handler = new LF_MapKeyRowHandler();
		handler.m_Owner = this;
		handler.m_Line = line;
		handler.m_wRow = row;
		row.AddHandler(handler);
		m_aKeyHandlers.Insert(handler);
	}

	//------------------------------------------------------------------------------------------------
	//! A row of a hidden group, or of a type the solo filter leaves out, is faded in the key.
	protected void RefreshKeyDim()
	{
		foreach (LF_MapKeyRowHandler handler : m_aKeyHandlers)
		{
			if (!handler.m_wRow)
				continue;

			bool off = LF_MapLegend.IsHidden(LF_MapLegend.Hidden(), handler.m_Line.m_sCategory);
			string solo = LF_MapLegend.Solo();
			if (!handler.m_Line.m_bHeader && !solo.IsEmpty() && handler.m_Line.m_sType != solo)
				off = true;

			if (off)
				handler.m_wRow.SetOpacity(KEY_OFF_ALPHA);
			else
				handler.m_wRow.SetOpacity(1);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The pointer is on a key line (null: it left). A type row lifts its badges on the map.
	void HighlightRow(LF_MapKeyLine line)
	{
		LF_MapPlacesUI places = Places();
		if (!places)
			return;

		if (line && !line.m_bHeader)
			places.HighlightType(line.m_sType);
		else
			places.HighlightType("");
	}

	//------------------------------------------------------------------------------------------------
	//! A click on a key line: a row shows only its type (again: every type), a header hides or shows its group.
	void ClickRow(notnull LF_MapKeyLine line)
	{
		if (line.m_bHeader)
		{
			bool hidden = LF_MapLegend.Toggle(LF_MapLegend.Hidden(), line.m_sCategory);
			LF_Debug.Info(LF_MapLegend.FEATURE, string.Format("key: group %1 hidden=%2", line.m_sCategory, hidden));
		}
		else
		{
			string solo = LF_MapLegend.ToggleSolo(line.m_sType);
			LF_Debug.Info(LF_MapLegend.FEATURE, string.Format("key: only '%1'", solo));
		}

		LF_MapPlacesUI places = Places();
		if (places)
			places.ApplyLegendFilter();

		RefreshKeyDim();
	}

	//------------------------------------------------------------------------------------------------
	//! True when the map cursor (reference units, SCR_MapCursorInfo) is over the key column or its tab, so a
	//! click there never also lands on a badge or the map under it.
	static bool CursorOverKey(float x, float y)
	{
		return s_fKeyW > 0 && x >= s_fKeyX && x <= s_fKeyX + s_fKeyW && y >= s_fKeyY && y <= s_fKeyY + s_fKeyH;
	}

	//------------------------------------------------------------------------------------------------
	//! Keeps the column's screen box for CursorOverKey (reference units), tab included.
	protected void StoreKeyBox()
	{
		s_fKeyW = 0;
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace || !m_wKeyTab)
			return;

		float x0 = 1e9;
		float y0 = 1e9;
		float x1 = -1e9;
		float y1 = -1e9;
		array<Widget> parts = {m_wKeyTab, m_wKey};
		foreach (Widget part : parts)
		{
			if (!part)
				continue;

			float px;
			float py;
			float pw;
			float ph;
			part.GetScreenPos(px, py);
			part.GetScreenSize(pw, ph);
			x0 = Math.Min(x0, workspace.DPIUnscale(px));
			y0 = Math.Min(y0, workspace.DPIUnscale(py));
			x1 = Math.Max(x1, workspace.DPIUnscale(px + pw));
			y1 = Math.Max(y1, workspace.DPIUnscale(py + ph));
		}

		if (x1 <= x0)
			return;

		s_fKeyX = x0;
		s_fKeyY = y0;
		s_fKeyW = x1 - x0;
		s_fKeyH = y1 - y0;
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearKey()
	{
		HighlightRow(null);
		m_aKeyHandlers.Clear();
		if (m_wKey)
			m_wKey.RemoveFromHierarchy();

		m_wKey = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Places learned after the map opened (the server's private ones) join the key; the pointer box and
	//! the pulse of the lifted badges follow every frame.
	override void Update(float timeSlice)
	{
		super.Update(timeSlice);
		if (!m_wKeyTab)
			return;

		LF_MapPlacesUI places = Places();
		if (places && places.GetAllPois() && places.GetAllPois().Count() != m_iKeyBuiltFor)
			RebuildKey();

		StoreKeyBox();
		if (places)
			places.PulseHighlight(timeSlice);
	}

	//------------------------------------------------------------------------------------------------
	override void OnMapClose(MapConfiguration config)
	{
		CloseSheet();
		ClearKey();
		if (m_wTab)
			m_wTab.RemoveFromHierarchy();

		if (m_wKeyTab)
			m_wKeyTab.RemoveFromHierarchy();

		m_wTab = null;
		m_wKeyTab = null;
		m_wKeyTabText = null;
		m_KeyTabHandler = null;
		m_iKeyBuiltFor = -1;
		s_fKeyW = 0;
		super.OnMapClose(config);
	}

	//------------------------------------------------------------------------------------------------
	protected LF_MapPlacesUI Places()
	{
		if (!m_MapEntity)
			return null;

		return LF_MapPlacesUI.Cast(m_MapEntity.GetMapUIComponent(LF_MapPlacesUI));
	}

	//------------------------------------------------------------------------------------------------
	protected void OnTabClicked()
	{
		if (m_Sheet && m_Sheet.IsOpen())
			CloseSheet();
		else
			OpenSheet();
	}

	//------------------------------------------------------------------------------------------------
	//! The legend only names categories the player's map shows, so an unmet category has no line.
	protected void OpenSheet()
	{
		CloseSheet();
		LF_MapPlacesUI places = Places();
		if (!places || !places.GetAllPois())
		{
			LF_Debug.Error(LF_MapLegend.FEATURE, "the places layer is not up; the map key has nothing to list");
			return;
		}

		array<string> categories = {};
		LF_MapLegend.Categories(places.GetAllPois(), categories);
		m_Sheet = LF_UIScreen.Open(LF_MapLegend.BuildParts(categories, LF_MapLegend.Hidden()));
		if (!m_Sheet)
		{
			LF_Debug.Error(LF_MapLegend.FEATURE, "the map key sheet did not open");
			return;
		}

		m_Sheet.SetListHeight("Results", LIST_HEIGHT);
		m_Sheet.CloseOnDeath();
		m_Sheet.GetOnButton().Insert(OnSheetButton);
		LF_Debug.Info(LF_MapLegend.FEATURE, string.Format("map key open: %1 categories", categories.Count()));
	}

	//------------------------------------------------------------------------------------------------
	protected void CloseSheet()
	{
		if (!m_Sheet)
			return;

		LF_UIScreen sheet = m_Sheet;
		m_Sheet = null;
		sheet.GetOnButton().Remove(OnSheetButton);
		sheet.Close();
		m_aResults.Clear();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnSheetButton(string name)
	{
		if (!m_Sheet)
			return;

		if (name == CLOSE_BUTTON)
		{
			CloseSheet();
			return;
		}

		if (name == SEARCH_BUTTON)
		{
			RunSearch();
			return;
		}

		if (name.StartsWith(RESULT_ROW))
		{
			PickResult(name.Substring(RESULT_ROW.Length(), name.Length() - RESULT_ROW.Length()).ToInt());
			return;
		}

		if (name.StartsWith("Cat_"))
			ToggleCategory(name.Substring(4, name.Length() - 4));
	}

	//------------------------------------------------------------------------------------------------
	protected void RunSearch()
	{
		LF_MapPlacesUI places = Places();
		if (!places || !places.GetAllPois())
			return;

		string query = m_Sheet.GetFieldText("SearchField");
		array<int> hits = {};
		LF_MapLegend.Search(query, places.GetAllPois(), LF_MapLegend.MAX_RESULTS, hits);
		m_Sheet.ClearList("Results");
		m_aResults.Clear();
		foreach (int i, int index : hits)
		{
			LF_MapPoi poi = places.GetAllPois()[index];
			m_aResults.Insert(poi);
			m_Sheet.AddListRow("Results", RESULT_ROW + i.ToString(), LF_MapLegend.NameOf(poi), "", "#LF-MapLegend_Go");
		}

		if (hits.Count() == 0 && !query.IsEmpty())
			m_Sheet.AddListRow("Results", RESULT_ROW + "none", "#LF-MapLegend_NoMatch", "", "");

		LF_Debug.Info(LF_MapLegend.FEATURE, string.Format("search '%1': %2 known places", query, hits.Count()));
	}

	//------------------------------------------------------------------------------------------------
	//! A pick shows its category again if it was hidden, then pans there and pins the place's card.
	protected void PickResult(int index)
	{
		LF_MapPlacesUI places = Places();
		if (!places || index < 0 || index >= m_aResults.Count())
			return;

		LF_MapPoi poi = m_aResults[index];
		if (LF_MapLegend.IsHidden(LF_MapLegend.Hidden(), poi.m_sCategory))
		{
			LF_MapLegend.Toggle(LF_MapLegend.Hidden(), poi.m_sCategory);
			places.ApplyLegendFilter();
		}

		CloseSheet();
		places.FlashPlace(poi);
	}

	//------------------------------------------------------------------------------------------------
	//! The harness's hands (`maplegend`): the same paths a click takes.
	void HarnessOpen()
	{
		OpenSheet();
	}

	//------------------------------------------------------------------------------------------------
	void HarnessClose()
	{
		CloseSheet();
	}

	//------------------------------------------------------------------------------------------------
	//! `maplegend icons [open|folded]`: unfold or fold the key column (default: unfold).
	void HarnessIcons(string word = "")
	{
		SetKeyFolded(word == "folded");
	}

	//------------------------------------------------------------------------------------------------
	//! `maplegend hover <n>`: the pointer on key line n (as the mouse does), -1 to leave it.
	string HarnessKeyLine(int index, bool click)
	{
		if (index < 0 || index >= m_aKeyHandlers.Count())
		{
			HighlightRow(null);
			return string.Format("lines=%1", m_aKeyHandlers.Count());
		}

		LF_MapKeyLine line = m_aKeyHandlers[index].m_Line;
		if (click)
			ClickRow(line);
		else
			HighlightRow(line);

		return string.Format("line=%1 header=%2 type=%3", index, line.m_bHeader, line.m_sType);
	}

	//------------------------------------------------------------------------------------------------
	void HarnessSearch(string word)
	{
		if (!m_Sheet)
			OpenSheet();

		if (!m_Sheet)
			return;

		m_Sheet.SetFieldText("SearchField", word);
		RunSearch();
	}

	//------------------------------------------------------------------------------------------------
	void HarnessToggle(string category)
	{
		if (!m_Sheet)
			OpenSheet();

		if (m_Sheet)
			ToggleCategory(category);
	}

	//------------------------------------------------------------------------------------------------
	void HarnessDemo(vector centre)
	{
		LF_MapPlacesUI places = Places();
		if (places)
			places.AddDemoPois(centre);
	}

	//------------------------------------------------------------------------------------------------
	string HarnessStatus()
	{
		LF_MapPlacesUI places = Places();
		int known;
		float meanX;
		float meanZ;
		if (places && places.GetAllPois())
		{
			known = places.GetAllPois().Count();
			foreach (LF_MapPoi poi : places.GetAllPois())
			{
				meanX += poi.m_vPos[0] / known;
				meanZ += poi.m_vPos[2] / known;
			}
		}

		string hidden;
		foreach (string category : LF_MapLegend.Hidden())
		{
			hidden += category + ",";
		}

		return string.Format("sheet=%1 hidden=%2 known=%3 results=%4 mean_x=%5 mean_z=%6 key_folded=%7 key_lines=%8 solo=%9", m_Sheet != null, hidden, known, m_aResults.Count(), meanX, meanZ, m_bKeyFolded, m_aKeyHandlers.Count(), LF_MapLegend.Solo());
	}

	//------------------------------------------------------------------------------------------------
	protected void ToggleCategory(string category)
	{
		LF_MapPlacesUI places = Places();
		if (!places)
			return;

		bool hidden = LF_MapLegend.Toggle(LF_MapLegend.Hidden(), category);
		string row = LF_MapLegend.RowName(category);
		if (hidden)
			m_Sheet.SetStatusRow(row, LF_MapLegend.CategoryKey(category), "#LF-MapLegend_Off", "#LF-MapLegend_Show");
		else
			m_Sheet.SetStatusRow(row, LF_MapLegend.CategoryKey(category), "#LF-MapLegend_On", "#LF-MapLegend_Hide");

		LF_Debug.Info(LF_MapLegend.FEATURE, string.Format("category %1 %2", category, hidden));
		places.ApplyLegendFilter();
	}
}

//------------------------------------------------------------------------------------------------
//! Session harness for the legend (#2244). FINAL PROOF ONLY (`map-legend-shots`):
//!   maplegend open            open the Map key sheet on the open map
//!   maplegend demo <x> <z>     add four stand-in places around a point (no world has POIs yet)
//!   maplegend search <word>   type the word into the search blank and press Find
//!   maplegend toggle <cat>    Hide / Show a category as its button does
//!   maplegend close           close the sheet
//!   maplegend status          sheet open flag, hidden categories, places drawn and known
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "maplegend")
			return super.Handle(cmd);

		string detail;
		bool ok = LF_MapLegendHarness.Run(cmd.Arg1, cmd.Arg2, cmd.Arg3, detail);
		if (ok)
			LF_AgentHarness.Answer(cmd.Id, "ok", detail);
		else
			LF_AgentHarness.Answer(cmd.Id, "error", detail);

		return true;
	}
}

//------------------------------------------------------------------------------------------------
class LF_MapLegendHarness
{
	//------------------------------------------------------------------------------------------------
	static bool Run(string action, string argument, string argument2, out string detail)
	{
		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (!mapEntity || !mapEntity.IsOpen())
		{
			detail = "map_not_open";
			return false;
		}

		LF_MapLegendUI legend = LF_MapLegendUI.Cast(mapEntity.GetMapUIComponent(LF_MapLegendUI));
		if (!legend)
		{
			detail = "no_legend_component";
			return false;
		}

		if (action == "open")
			legend.HarnessOpen();
		else if (action == "close")
			legend.HarnessClose();
		else if (action == "search")
			legend.HarnessSearch(argument);
		else if (action == "toggle")
			legend.HarnessToggle(argument);
		else if (action == "icons")
			legend.HarnessIcons(argument);
		else if (action == "hover" || action == "solo")
		{
			detail = legend.HarnessKeyLine(argument.ToInt(), action == "solo");
			return true;
		}
		else if (action == "demo")
			legend.HarnessDemo(Vector(argument.ToFloat(), 0, argument2.ToFloat()));
		else if (action != "status")
		{
			detail = "usage: maplegend open|close|search <word>|toggle <category>|icons [folded]|hover <line>|solo <line>|demo <x> <z>|status";
			return false;
		}

		detail = legend.HarnessStatus();
		return true;
	}
}
