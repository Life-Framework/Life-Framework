// red-proof: observed 2026-11-23 (#4474): with the stock and price rows unedited, `--only LF_Test_EveronNewsagent` failed `the Courier is in the newsagent stock`, `the road map is in the newsagent stock` and `the old placeholder newspaper is no longer sold`; `--red LF_Test_EveronNewsagent` then printed RED PROOF OK (the missing stock id and the skipped HearAll fail every town line).
//! #4474 (WO-W2.4): the Saint-Philippe newsagent sells the Courier ($1) and a road map ($15), and reading the
//! road map marks every live town "heard of" through LF_MapKnown, never a job site or a cove.
//! Checked against the merged pack config (pack leg): the stock rows and their prefabs, then the road map's
//! town rule and the store, driven with a fixture POI table (the pack has no town places to mark yet).
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronNewsagent : LF_Test
{
	protected static const ResourceName COURIER = "{4D7E2B91A6C35F08}Prefabs/Items/Paper/LF_Newspaper.et";
	protected static const ResourceName OLD_PLACEHOLDER = "{87592893752A28D6}Prefabs/Items/Newsagent/LF_Newspaper.et";
	protected static const ResourceName ROAD_MAP = "{7C3E5A92D41F6B08}Prefabs/Items/Paper/LF_RoadMap.et";
	protected static const string CHARACTER = "newsagent-test-character";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/newsagent";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		RunStock(ctx);
		RunRoadMap(ctx);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunStock(LF_TestContext ctx)
	{
		string lookup = "newsagent";
		if (RedProof())
			lookup = "newsagent_missing";

		LF_ShopStock stock = LF_ShopStockConfig.Get().Find(lookup);
		ctx.NotNull(stock, "the newsagent has a stock under the pack header");
		if (!stock)
			return;

		CheckRow(ctx, stock, COURIER, 1, "the Courier");
		CheckRow(ctx, stock, ROAD_MAP, 15, "the road map");
		ctx.True(stock.FindItem(OLD_PLACEHOLDER) == null, "the old placeholder newspaper is no longer sold");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckRow(LF_TestContext ctx, LF_ShopStock stock, ResourceName prefab, int price, string label)
	{
		Resource resource = Resource.Load(prefab);
		ctx.True(resource && resource.IsValid(), string.Format("%1 loads: %2", label, prefab));

		LF_ShopStockItem row = stock.FindItem(prefab);
		ctx.NotNull(row, string.Format("%1 is in the newsagent stock", label));
		if (row)
			ctx.Equal(price, row.m_iPrice, string.Format("%1 sells for $%2", label, price));
	}

	//------------------------------------------------------------------------------------------------
	protected void RunRoadMap(LF_TestContext ctx)
	{
		array<ref LF_MapKnownRow> saved = LF_MapKnown.ExportAll();
		LF_MapKnown.ApplyAll(null);

		LF_MapPoiConfig config = new LF_MapPoiConfig();
		config.m_aKinds = {};
		LF_MapPoiKind town = LF_MapPoiKind.Make("NsTown", LF_EMapPoiVisibility.DISCOVER);
		town.m_sCategory = LF_RoadMapRules.TOWN_CATEGORY;
		LF_MapPoiKind job = LF_MapPoiKind.Make("NsJob", LF_EMapPoiVisibility.DISCOVER);
		job.m_sCategory = "work";
		LF_MapPoiKind cove = LF_MapPoiKind.Make("NsCove", LF_EMapPoiVisibility.DISCOVER);
		cove.m_sCategory = "fishing";
		LF_MapPoiKind hiddenTown = LF_MapPoiKind.Make("NsHiddenTown", LF_EMapPoiVisibility.HIDDEN);
		hiddenTown.m_sCategory = LF_RoadMapRules.TOWN_CATEGORY;
		config.m_aKinds.Insert(town);
		config.m_aKinds.Insert(job);
		config.m_aKinds.Insert(cove);
		config.m_aKinds.Insert(hiddenTown);

		array<ref LF_MapPoiPlace> places = {};
		places.Insert(LF_MapPoiPlace.Make("NsTown", "ns-town-a", Vector(100, 0, 0), true));
		places.Insert(LF_MapPoiPlace.Make("NsTown", "ns-town-b", Vector(2500, 0, 900), true));
		places.Insert(LF_MapPoiPlace.Make("NsTown", "ns-town-dark", Vector(50, 0, 0), false));
		places.Insert(LF_MapPoiPlace.Make("NsJob", "ns-job", Vector(60, 0, 0), true));
		places.Insert(LF_MapPoiPlace.Make("NsCove", "ns-cove", Vector(70, 0, 0), true));
		places.Insert(LF_MapPoiPlace.Make("NsHiddenTown", "ns-hidden", Vector(80, 0, 0), true));

		array<string> ids = {};
		int found = LF_RoadMapRules.TownsToHear(config, places, ids);
		ctx.Equal(2, found, "the road map lists the two live towns");

		if (!RedProof())
			LF_MapPlacesKnown.HearAll(CHARACTER, ids);

		ctx.True(LF_MapKnown.KnownOf(CHARACTER, "ns-town-a") == LF_EMapPoiKnown.HEARD, "town a is heard of");
		ctx.True(LF_MapKnown.KnownOf(CHARACTER, "ns-town-b") == LF_EMapPoiKnown.HEARD, "town b is heard of");
		ctx.True(LF_MapKnown.KnownOf(CHARACTER, "ns-town-dark") == LF_EMapPoiKnown.NONE, "a town that is not live is not marked");
		ctx.True(LF_MapKnown.KnownOf(CHARACTER, "ns-job") == LF_EMapPoiKnown.NONE, "a job site is not marked");
		ctx.True(LF_MapKnown.KnownOf(CHARACTER, "ns-cove") == LF_EMapPoiKnown.NONE, "a cove is not marked");
		ctx.True(LF_MapKnown.KnownOf(CHARACTER, "ns-hidden") == LF_EMapPoiKnown.NONE, "an underworld place never comes from a printed map");

		LF_MapKnown.ApplyAll(saved);
	}
}
