// red-proof: 2026-11-17, cli test --no-build --red LF_Test_MapSearch: RED-OK (RedProof() skips the first search and the first toggle, so the known-only search and the filter checks fail). Green without it.

//------------------------------------------------------------------------------------------------
//! map/search (#2244, epic #2223, card M16): the legend's rules. Search finds only the places this
//! player knows (the heard-of and visited ones; never a place the visibility rule hid), by part of the
//! name, case-insensitive, at most 8; a category filter takes its POIs off the map, and the layer's
//! cluster build then has nothing of that category. Pure script, fixture places only.
// tier: LOGIC
class LF_Test_MapSearch : LF_Test
{
	override string GetName()
	{
		return "map/search";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_MapLegend.ResetStatics();

		array<ref LF_MapPoi> pois = {};
		BuildPois(pois);
		RunKnownOnly(ctx, pois);
		RunSearchRules(ctx, pois);
		RunFilter(ctx, pois);
		RunParts(ctx, pois);

		LF_MapLegend.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	//! Five fixture places of four kinds for one player: a public bank, a heard-of workplace, a workplace
	//! the player has never heard of, a police armoury (ROLE, no role held) and a hidden fence.
	protected void BuildPois(notnull array<ref LF_MapPoi> into)
	{
		LF_MapPoiConfig config = new LF_MapPoiConfig();
		config.m_aKinds = new array<ref LF_MapPoiKind>();
		AddKind(config, "FxBank", "civic", "Fixture Bank", LF_EMapPoiVisibility.PUBLIC, "");
		AddKind(config, "FxWork", "work", "Fixture Quarry", LF_EMapPoiVisibility.DISCOVER, "");
		AddKind(config, "FxArmoury", "law", "Fixture Armoury", LF_EMapPoiVisibility.ROLE, "police");
		AddKind(config, "FxFence", "underworld", "Fixture Fence", LF_EMapPoiVisibility.HIDDEN, "");

		LF_MapPoiAudience audience = new LF_MapPoiAudience();
		audience.SetKnown("quarry-heard", LF_EMapPoiKnown.HEARD);

		array<ref LF_MapPoiPlace> places = {};
		places.Insert(LF_MapPoiPlace.Make("FxBank", "bank-1", "12100 0 12100", true));
		places.Insert(LF_MapPoiPlace.Make("FxWork", "quarry-heard", "12200 0 12200", true));
		places.Insert(LF_MapPoiPlace.Make("FxWork", "quarry-unknown", "12300 0 12300", true));
		places.Insert(LF_MapPoiPlace.Make("FxArmoury", "armoury-1", "12400 0 12400", true));
		places.Insert(LF_MapPoiPlace.Make("FxFence", "fence-1", "12500 0 12500", true));
		LF_MapPois.Collect(config, audience, places, into);
	}

	//------------------------------------------------------------------------------------------------
	protected void AddKind(LF_MapPoiConfig config, string kind, string category, string label, LF_EMapPoiVisibility visibility, string role)
	{
		LF_MapPoiKind row = LF_MapPoiKind.Make(kind, visibility, role);
		row.m_sCategory = category;
		row.m_sLabel = label;
		config.m_aKinds.Insert(row);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunKnownOnly(LF_TestContext ctx, notnull array<ref LF_MapPoi> pois)
	{
		// The rule keeps the bank (public) and the heard-of workplace; the unmet workplace, the armoury and the fence are out.
		ctx.Equal(2, pois.Count(), "the fixture player sees two places");

		array<int> hits = {};
		int found;
		if (!RedProof())
			found = LF_MapLegend.Search("quarry", pois, LF_MapLegend.MAX_RESULTS, hits);

		ctx.Equal(1, found, "'quarry' finds only the workplace the player has heard of, not the unmet one");
		if (hits.Count() > 0)
			ctx.EqualStr("quarry-heard", pois[hits[0]].m_sId, "the match is the heard-of workplace");

		hits.Clear();
		ctx.Equal(0, LF_MapLegend.Search("armoury", pois, LF_MapLegend.MAX_RESULTS, hits), "a ROLE place the player has no role for is not found");
		ctx.Equal(0, LF_MapLegend.Search("fence", pois, LF_MapLegend.MAX_RESULTS, hits), "a hidden place is not found");

		// A place the table could not hide (state HIDDEN by hand) never matches either.
		array<ref LF_MapPoi> withHidden = {};
		LF_MapPoi ghost = new LF_MapPoi();
		ghost.m_sId = "ghost";
		ghost.m_sLabel = "Fixture Ghost";
		ghost.m_eState = LF_EMapPoiState.HIDDEN;
		withHidden.Insert(ghost);
		ctx.Equal(0, LF_MapLegend.Search("ghost", withHidden, LF_MapLegend.MAX_RESULTS, hits), "a place in the hidden state never matches");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunSearchRules(LF_TestContext ctx, notnull array<ref LF_MapPoi> pois)
	{
		array<int> hits = {};
		ctx.Equal(1, LF_MapLegend.Search("BANK", pois, LF_MapLegend.MAX_RESULTS, hits), "search ignores case");
		hits.Clear();
		ctx.Equal(1, LF_MapLegend.Search("  ban ", pois, LF_MapLegend.MAX_RESULTS, hits), "a part of a name matches and spaces around it are dropped");
		hits.Clear();
		ctx.Equal(0, LF_MapLegend.Search("", pois, LF_MapLegend.MAX_RESULTS, hits), "an empty word finds nothing");
		ctx.Equal(0, LF_MapLegend.Search("zzz", pois, LF_MapLegend.MAX_RESULTS, hits), "a word no known place has finds nothing");

		// Names that begin with the word come before names that only contain it; the list is capped.
		array<ref LF_MapPoi> many = {};
		AddPoi(many, "late", "Corner Shop");
		for (int i = 0; i < 10; i++)
		{
			AddPoi(many, "shop" + i.ToString(), "Shop " + i.ToString());
		}

		hits.Clear();
		int count = LF_MapLegend.Search("shop", many, LF_MapLegend.MAX_RESULTS, hits);
		ctx.Equal(LF_MapLegend.MAX_RESULTS, count, "search lists at most 8 places");
		ctx.Equal(LF_MapLegend.MAX_RESULTS, hits.Count(), "the list holds the 8");
		ctx.EqualStr("shop0", many[hits[0]].m_sId, "a name that begins with the word comes first");
		bool lateListed;
		foreach (int hit : hits)
		{
			if (many[hit].m_sId == "late")
				lateListed = true;
		}

		ctx.False(lateListed, "a name that only contains the word is cut by the cap, after the names that begin with it");
	}

	//------------------------------------------------------------------------------------------------
	protected void AddPoi(notnull array<ref LF_MapPoi> into, string id, string label)
	{
		LF_MapPoi poi = new LF_MapPoi();
		poi.m_sId = id;
		poi.m_sKind = id;
		poi.m_sLabel = label;
		poi.m_sCategory = "shop";
		poi.m_eState = LF_EMapPoiState.FULL;
		poi.m_vPos = "12010 0 12010";
		into.Insert(poi);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunFilter(LF_TestContext ctx, notnull array<ref LF_MapPoi> pois)
	{
		array<string> hidden = {};
		array<ref LF_MapPoi> shown = {};
		LF_MapLegend.Filter(pois, hidden, shown);
		ctx.Equal(2, shown.Count(), "no category hidden: every place is drawn");

		bool nowHidden;
		if (!RedProof())
			nowHidden = LF_MapLegend.Toggle(hidden, "civic");

		ctx.True(nowHidden, "toggling a shown category hides it");
		shown.Clear();
		LF_MapLegend.Filter(pois, hidden, shown);
		ctx.Equal(1, shown.Count(), "hiding civic takes the bank off the map");
		if (shown.Count() > 0)
			ctx.EqualStr("work", shown[0].m_sCategory, "the other category stays");

		// The layer's cluster build sees only what the filter lets through.
		array<ref LF_MapClusterGroup> groups = {};
		LF_MapClusterRules.BuildFromPois(shown, 5, groups);
		ctx.Equal(1, groups.Count(), "the map draws one icon, not two");

		ctx.False(LF_MapLegend.Toggle(hidden, "civic"), "toggling again shows it");
		shown.Clear();
		LF_MapLegend.Filter(pois, hidden, shown);
		ctx.Equal(2, shown.Count(), "showing civic brings the bank back");

		hidden.Insert("nonesuch");
		shown.Clear();
		LF_MapLegend.Filter(pois, hidden, shown);
		ctx.Equal(2, shown.Count(), "hiding a category no place has changes nothing");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunParts(LF_TestContext ctx, notnull array<ref LF_MapPoi> pois)
	{
		// The legend names only categories the player's map holds, in the printed order, and nothing else.
		array<string> categories = {};
		LF_MapLegend.Categories(pois, categories);
		ctx.Equal(2, categories.Count(), "the legend has a line per category the player sees");
		if (categories.Count() == 2)
		{
			ctx.EqualStr("civic", categories[0], "civic is first in the printed order");
			ctx.EqualStr("work", categories[1], "work follows");
		}

		array<string> hidden = {};
		hidden.Insert("work");
		array<ref LF_UIPart> parts = LF_MapLegend.BuildParts(categories, hidden);
		bool field;
		bool list;
		bool close;
		int status;
		string workStatus;
		foreach (LF_UIPart part : parts)
		{
			if (part.GetKind() == LF_EUIPartKind.FIELD)
				field = true;
			else if (part.GetKind() == LF_EUIPartKind.LIST)
				list = true;
			else if (part.GetKind() == LF_EUIPartKind.CLOSE)
				close = true;
			else if (part.GetKind() == LF_EUIPartKind.STATUS)
			{
				status++;
				if (part.GetName() == LF_MapLegend.RowName("work"))
					workStatus = part.GetStatus();
			}
		}

		ctx.True(field && list && close, "the sheet is a search field, a results list and Close");
		ctx.Equal(2, status, "one status row per category");
		ctx.EqualStr("#LF-MapLegend_Off", workStatus, "a hidden category's row reads off");
	}
}
