// red-proof: RedProof() reads the table without the island rows file and skips the study, so no `Town` kind resolves, the road map lists no town and the map shows none. Observed on 2026-11-23 (#4655): `tools\cli test --no-build --red LF_Test_EveronTownPlaces` printed RED-OK; green without the hook.
//! everon/town-places (#4655): under the Everon 1989 header (the pack leg) the pack's POI table has a `Town` kind
//! (category town, shown once heard of, the town hall pictogram, a name that resolves), so the road map the newsagent
//! sells has towns to mark. Driven with the real row and the five live town ids of the island layer: a fresh player's
//! map shows no town; after the road map is studied (the same rule and store the item's action calls) every live town
//! is heard of and the map draws it, its card naming the nearest place beside the kind; a job site, a cove or the
//! underworld is never marked. Where each town stands is the island layer's job (MainWorld_Layers/Towns_everon.layer):
//! the proof of that is an island session.
[LF_TestInfo(LF_TestTier.LOGIC, pack: true)]
class LF_Test_EveronTownPlaces : LF_Test
{
	protected static const string CHARACTER = "town-places-test-character";
	protected static const string KIND = "Town";

	protected ref array<string> m_aSlugs = {};
	protected ref array<string> m_aNames = {};
	protected static const string KIND_LABEL = "Settlement";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/town-places";
	}

	//------------------------------------------------------------------------------------------------
	//! The live towns of the island, the same five the road sign rows name (#4478).
	protected void Fill()
	{
		m_aSlugs.Insert("saint-philippe");
		m_aNames.Insert("Saint-Philippe");
		m_aSlugs.Insert("meaux");
		m_aNames.Insert("Meaux");
		m_aSlugs.Insert("montignac");
		m_aNames.Insert("Montignac");
		m_aSlugs.Insert("chotain");
		m_aNames.Insert("Chotain");
		m_aSlugs.Insert("levie");
		m_aNames.Insert("Levie");
	}

	//------------------------------------------------------------------------------------------------
	protected static string PlaceId(string slug)
	{
		return "town." + slug;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		Fill();
		LF_MapPoiConfig.Reload();
		LF_MapPoiConfig config = LF_MapPoiConfig.Get();
		ctx.NotNull(config, "the POI table loads under the pack");
		if (!config)
			return;

		if (RedProof())
		{
			// What the map did before: no island rows file, so the Town kind has no row.
			LF_MapPoiConfig bare = new LF_MapPoiConfig();
			bare.m_aKinds = new array<ref LF_MapPoiKind>();
			config = bare;
		}

		CheckKinds(ctx, config);

		array<ref LF_MapPoiPlace> places = {};
		foreach (int i, string slug : m_aSlugs)
			places.Insert(LF_MapPoiPlace.Make(KIND, PlaceId(slug), Vector(1000 * (i + 1), 0, 500 * (i + 1)), true));

		// Never marked by a road map: a job site, a cove and a hidden place, beside the towns.
		places.Insert(LF_MapPoiPlace.Make("Workplace", "coalmans-brow.mine", Vector(60, 0, 0), true));
		places.Insert(LF_MapPoiPlace.Make("FishingSpot", "fishermansbay.cove", Vector(70, 0, 0), true));
		places.Insert(LF_MapPoiPlace.Make("Fence", "saint-philippe.fence", Vector(80, 0, 0), true));

		RunStudy(ctx, config, places);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckKinds(LF_TestContext ctx, LF_MapPoiConfig config)
	{
		LF_MapPoiKind base = config.Find(KIND);
		ctx.NotNull(base, "the pack has a Town POI row");
		if (base)
		{
			ctx.EqualStr(LF_RoadMapRules.TOWN_CATEGORY, base.m_sCategory, "the Town row is in the town category the road map reads");
			ctx.True(base.m_eVisibility == LF_EMapPoiVisibility.DISCOVER, "a town shows once it is heard of, never on a stranger's map");
			ctx.True(base.m_fMinZoom <= 0, "a town shows at every zoom");
			ctx.EqualStr("poi_TownHall", base.m_sIcon, "a town draws the pack's town pictogram");
			ctx.EqualStr(KIND_LABEL, WidgetManager.Translate(base.m_sLabel), "the Town kind is named in the string table");
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void RunStudy(LF_TestContext ctx, LF_MapPoiConfig config, array<ref LF_MapPoiPlace> places)
	{
		array<ref LF_MapKnownRow> saved = LF_MapKnown.ExportAll();
		LF_MapKnown.ApplyAll(null);

		// A fresh player: the five towns are on no map yet.
		array<ref LF_MapPoi> before = {};
		LF_MapPois.Collect(config, AudienceOf(places), places, before);
		ctx.Equal(0, TownMarks(before).Count(), "a player who has not studied a road map sees no town");

		array<string> ids = {};
		int found = LF_RoadMapRules.TownsToHear(config, places, ids);
		ctx.Equal(m_aSlugs.Count(), found, "the road map lists every live town and nothing else");

		// The item's action does exactly this on the server: record the ids through LF_MapKnown.
		if (!RedProof())
			LF_MapPlacesKnown.HearAll(CHARACTER, ids);

		foreach (int i, string slug : m_aSlugs)
			ctx.True(LF_MapKnown.KnownOf(CHARACTER, PlaceId(slug)) == LF_EMapPoiKnown.HEARD, string.Format("%1 is heard of after the study", m_aNames[i]));

		ctx.True(LF_MapKnown.KnownOf(CHARACTER, "coalmans-brow.mine") == LF_EMapPoiKnown.NONE, "a job site is not marked");
		ctx.True(LF_MapKnown.KnownOf(CHARACTER, "fishermansbay.cove") == LF_EMapPoiKnown.NONE, "a cove is not marked");
		ctx.True(LF_MapKnown.KnownOf(CHARACTER, "saint-philippe.fence") == LF_EMapPoiKnown.NONE, "the underworld is never marked");

		// The map draws each one, by its own name.
		array<ref LF_MapPoi> after = {};
		LF_MapPois.Collect(config, AudienceOf(places), places, after);
		array<ref LF_MapPoi> marks = TownMarks(after);
		ctx.Equal(m_aSlugs.Count(), marks.Count(), "after the study the map draws every live town");
		foreach (LF_MapPoi poi : marks)
		{
			int at = m_aSlugs.Find(poi.m_sId.Substring(5, poi.m_sId.Length() - 5));
			ctx.True(at != -1, string.Format("a drawn mark is one of the live towns: %1", poi.m_sId));
			if (at == -1)
				continue;

			ctx.EqualStr(LF_RoadMapRules.TOWN_CATEGORY, poi.m_sCategory, string.Format("%1 is in the town category", m_aNames[at]));
			ctx.EqualStr(KIND_LABEL + ", " + m_aNames[at], LF_MapPlaceCard.Title(WidgetManager.Translate(poi.m_sLabel), m_aNames[at]), string.Format("%1's card names the kind and the place", m_aNames[at]));
		}

		LF_MapKnown.ApplyAll(saved);
	}

	//------------------------------------------------------------------------------------------------
	//! The town marks among what the map draws (public job sites and coves draw for everyone and are not the point here).
	protected array<ref LF_MapPoi> TownMarks(array<ref LF_MapPoi> drawn)
	{
		array<ref LF_MapPoi> marks = {};
		foreach (LF_MapPoi poi : drawn)
		{
			if (poi.m_sKind == KIND)
				marks.Insert(poi);
		}

		return marks;
	}

	//------------------------------------------------------------------------------------------------
	//! The player's audience as the store holds it for CHARACTER now.
	protected LF_MapPoiAudience AudienceOf(array<ref LF_MapPoiPlace> places)
	{
		LF_MapPoiAudience audience = new LF_MapPoiAudience();
		foreach (LF_MapPoiPlace place : places)
		{
			audience.SetKnown(place.m_sId, LF_MapKnown.KnownOf(CHARACTER, place.m_sId));
		}

		return audience;
	}
}
