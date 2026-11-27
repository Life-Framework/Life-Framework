// red-proof: observed with the unfixed LF_MapLegend.NameOf (a bare WidgetManager.Translate of the label): the run failed with "the station name holds no raw placeholder: {Police} station" and the real-table sweep named PoliceStation ({Police} station) and PoliceArmoury ({Police} armory). NameOf now goes through TypeName (LF_Terms.Expand); green.

//------------------------------------------------------------------------------------------------
//! map/search-terms: no place name the map search lists, matches or hovers holds a raw `{Term}`
//! placeholder (the 0.3.0 "Find a place" list showed "{Police} station"). Every kind row of the real
//! POI table is named through LF_MapLegend.NameOf, and the search finds a place by its expanded word,
//! so an owner who renames the term finds it under the new word.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_MapSearchTerms : LF_Test
{
	protected static const string STATION_KEY = "#LF-MapPoi_PoliceStation";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "map/search-terms";
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_MapPoi MakePoi(string kind, string label, string town)
	{
		LF_MapPoi poi = new LF_MapPoi();
		poi.m_sKind = kind;
		poi.m_sId = "mst_" + kind;
		poi.m_sLabel = label;
		poi.m_sTown = town;
		poi.m_eState = LF_EMapPoiState.FULL;
		return poi;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_Terms.Reload();

		LF_MapPoi station = MakePoi("PoliceStation", STATION_KEY, "");
		string name = LF_MapLegend.NameOf(station);
		ctx.False(name.Contains("{"), "the station name holds no raw placeholder: " + name);
		ctx.True(name.Contains("station"), "the station name still reads as a station: " + name);

		// Every kind row of the real table (core plus the loaded pack), named as the search list names it.
		LF_MapPoiConfig config = LF_MapPoiConfig.Get();
		ctx.True(config != null && config.m_aKinds != null && config.m_aKinds.Count() > 0, "the POI table loads");
		if (config && config.m_aKinds)
		{
			foreach (LF_MapPoiKind row : config.m_aKinds)
			{
				if (!row || row.m_sLabel.IsEmpty())
					continue;

				string shown = LF_MapLegend.NameOf(MakePoi(row.m_sKind, row.m_sLabel, ""));
				ctx.False(shown.Contains("{"), "a raw placeholder is left in the search name of " + row.m_sKind + ": " + shown);
				ctx.False(LF_MapLegend.TypeName(MakePoi(row.m_sKind, row.m_sLabel, "")).Contains("{"), "a raw placeholder is left in the legend name of " + row.m_sKind);
			}
		}

		// The search matches the words a player reads, not the token.
		array<ref LF_MapPoi> pois = {};
		pois.Insert(station);
		pois.Insert(MakePoi("Bank", "#LF-MapPoi_Bank", ""));
		array<int> hits = {};
		LF_MapLegend.Search("police", pois, 10, hits);
		ctx.True(hits.Count() == 1 && hits[0] == 0, "searching police finds the station");

		hits.Clear();
		LF_MapLegend.Search("{police", pois, 10, hits);
		ctx.True(hits.IsEmpty(), "the token text is not searchable");

		LF_TermBook renamed = LF_Terms.Get();
		renamed.SetOwner("police", "singular", "", "Sheriff");
		ctx.True(LF_MapLegend.NameOf(station).Contains("Sheriff"), "the station name follows the owner's word");
		hits.Clear();
		LF_MapLegend.Search("sheriff", pois, 10, hits);
		ctx.True(hits.Count() == 1, "searching the owner's word finds the station");
		LF_Terms.Reload();
	}
}
