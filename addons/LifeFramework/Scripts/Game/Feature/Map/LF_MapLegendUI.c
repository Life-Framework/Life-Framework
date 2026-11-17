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
//! World-end reset of LF_MapLegend's statics (registered at script load; Core/LF_StaticReset.c).
class LF_MapLegend_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_MapLegend_StaticResetHook());

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

		if (LF_MapLegend.Hidden().Count() > 0)
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

		foreach (int i, LF_MapClusterGroup group : m_aGroups)
		{
			if (group.IsSingle() && m_aPois[group.m_aMembers[0]].m_sId == m_sFlashId)
			{
				m_iPinned = i;
				ShowCard(i);
				break;
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

	protected Widget m_wTab;
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
	}

	//------------------------------------------------------------------------------------------------
	override void OnMapClose(MapConfiguration config)
	{
		CloseSheet();
		if (m_wTab)
			m_wTab.RemoveFromHierarchy();

		m_wTab = null;
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

		return string.Format("sheet=%1 hidden=%2 known=%3 results=%4 mean_x=%5 mean_z=%6", m_Sheet != null, hidden, known, m_aResults.Count(), meanX, meanZ);
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
		else if (action == "demo")
			legend.HarnessDemo(Vector(argument.ToFloat(), 0, argument2.ToFloat()));
		else if (action != "status")
		{
			detail = "usage: maplegend open|close|search <word>|toggle <category>|demo <x> <z>|status";
			return false;
		}

		detail = legend.HarnessStatus();
		return true;
	}
}
