// red-proof: 2026-11-18, cli test --no-build --red LF_Test_EveronMapShopLabels: RED-OK (RedProof() reads the table without the pack overlay's slot rows, so no shop has a row and every shop-specific label check fails). Green without it under the pack header.
//! everon/map-shop-labels (#3515): under the Everon 1989 header (the pack leg), each shop slot of
//! Saint-Philippe has its own POI slot row, so the map names the bakery, the hardware store and the rest
//! instead of calling every StoreCounter a general store. A shop is on every player's map from the first
//! look (rc-map-places: a new player could not find a shop that stayed hidden until heard of).
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronMapShopLabels : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/map-shop-labels";
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckShop(LF_TestContext ctx, LF_MapPoiConfig config, string slot, string label, array<string> seenLabels)
	{
		string placeId = "saint-philippe." + slot;
		LF_MapPoiKind row = config.FindSlotRow("StoreCounter", placeId);
		ctx.NotNull(row, "the pack has a POI slot row for " + slot);
		if (!row)
			return;

		ctx.EqualStr(label, row.m_sLabel, slot + " is named by its own string");
		ctx.True(seenLabels.Find(row.m_sLabel) == -1, slot + " has a label no other shop uses");
		seenLabels.Insert(row.m_sLabel);

		array<ref LF_MapPoiPlace> places = {};
		places.Insert(LF_MapPoiPlace.Make("StoreCounter", placeId, "1 0 1", true));
		LF_MapPoiAudience stranger = new LF_MapPoiAudience();
		array<ref LF_MapPoi> pois = {};
		LF_MapPois.Collect(config, stranger, places, pois);
		ctx.Equal(1, pois.Count(), slot + " shows to a player who never heard of it");
		if (pois.Count() == 1)
			ctx.EqualStr(label, pois[0].m_sLabel, slot + " draws under its own name");
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
			// What the map did before: no slot rows, so a shop slot finds nothing of its own.
			LF_MapPoiConfig bare = new LF_MapPoiConfig();
			bare.m_aKinds = new array<ref LF_MapPoiKind>();
			LF_MapPoiKind base = config.Find("StoreCounter");
			if (base)
				bare.m_aKinds.Insert(base);

			config = bare;
		}

		array<string> seen = {};
		CheckShop(ctx, config, "general-store", "#LF-MapPoi_GeneralStore", seen);
		CheckShop(ctx, config, "supermarket", "#LF-MapPoi_Supermarket", seen);
		CheckShop(ctx, config, "hardware", "#LF-MapPoi_Hardware", seen);
		CheckShop(ctx, config, "pharmacy", "#LF-MapPoi_Pharmacy", seen);
		CheckShop(ctx, config, "newsagent", "#LF-MapPoi_Newsagent", seen);
		CheckShop(ctx, config, "seed-feed", "#LF-MapPoi_SeedFeed", seen);
		CheckShop(ctx, config, "bakery", "#LF-MapPoi_Bakery", seen);
		CheckShop(ctx, config, "butcher", "#LF-MapPoi_Butcher", seen);
		CheckShop(ctx, config, "furniture", "#LF-MapPoi_Furniture", seen);

		LF_MapPoiKind barber = config.Find("BarberChair");
		ctx.NotNull(barber, "the pack has a Barber POI row");
		if (barber)
		{
			ctx.EqualStr("#LF-MapPoi_Barber", barber.m_sLabel, "the barber's chair is named Barber");
			ctx.True(barber.m_eVisibility == LF_EMapPoiVisibility.PUBLIC, "the barber is on every player's map, like the other shops");
		}
	}
}
