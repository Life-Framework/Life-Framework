// red-proof: 2026-11-19, cli test --no-build --red LF_Test_EveronMapPoisContested: RED-OK (fails with the hook on, 3 assertions: RedProof() reads the table without the contested-site row). Green without it under the pack header.
//! everon/map-pois-contested (#3552): under the Everon 1989 header (the pack leg), the contested-site place
//! kind (LF_ContestedSites.PLACE_KIND, one place per held ground: the farm compound, the hilltop mast, the
//! quarry) has a POI row, so held ground is on the map. The row is public (a newcomer finds the sites without
//! having heard of them) and named by a pack string that resolves.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronMapPoisContested : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/map-pois-contested";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MapPoiConfig.Reload();
		LF_MapPoiConfig config = LF_MapPoiConfig.Get();
		ctx.NotNull(config, "the POI table loads under the pack");
		if (!config)
			return;

		if (RedProof())
		{
			// What the map did before: the table without the contested-site row.
			LF_MapPoiConfig bare = new LF_MapPoiConfig();
			bare.m_aKinds = new array<ref LF_MapPoiKind>();
			foreach (LF_MapPoiKind other : config.m_aKinds)
			{
				if (other && other.m_sKind != LF_ContestedSites.PLACE_KIND)
					bare.m_aKinds.Insert(other);
			}

			config = bare;
		}

		LF_MapPoiKind row = config.Find(LF_ContestedSites.PLACE_KIND);
		ctx.NotNull(row, "the pack has a POI row for the contested-site kind");
		if (!row)
			return;

		ctx.EqualStr("#LF-MapPoi_ContestedSite", row.m_sLabel, "held ground is named by its own string");
		ctx.True(row.m_eVisibility == LF_EMapPoiVisibility.PUBLIC, "held ground is on every player's map");
		string caption = WidgetManager.Translate(row.m_sLabel);
		ctx.True(caption != row.m_sLabel && !caption.IsEmpty(), "the contested-ground name is in the string table");

		array<string> hideouts = {"farm_compound", "hilltop_mast", "quarry_hideout"};
		array<ref LF_MapPoiPlace> places = {};
		foreach (string hideout : hideouts)
			places.Insert(LF_MapPoiPlace.Make(LF_ContestedSites.PLACE_KIND, hideout, "1 0 1", true));

		LF_MapPoiAudience stranger = new LF_MapPoiAudience();
		array<ref LF_MapPoi> pois = {};
		LF_MapPois.Collect(config, stranger, places, pois);
		ctx.Equal(hideouts.Count(), pois.Count(), "a player who never heard of the sites sees all of them");
		foreach (LF_MapPoi poi : pois)
			ctx.EqualStr("#LF-MapPoi_ContestedSite", poi.m_sLabel, "each site draws as contested ground");
	}
}
