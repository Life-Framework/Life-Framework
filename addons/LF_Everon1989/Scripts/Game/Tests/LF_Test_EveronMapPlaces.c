// red-proof: 2026-11-21, `tools\cli test --no-build --red LF_Test_EveronMapPlaces,LF_Test_PayphonePlace`: RED-OK everon/map-places (55 assertions fail with the hook on), RED-OK map/payphone-place (4); green without it.
// red-proof: RedProof() reads the table without the island rows file, so the new place kinds have no row and the checks fail.
//! everon/map-places (rc-map-places): under the Everon 1989 header (the pack leg), every important place of
//! the island has a PUBLIC POI row with a pack icon and a name that resolves, so a player who has never been
//! there still sees it on the map: the registry clerk (ID card icon, named for what it does), the hostel,
//! the shops, ATMs, the garage, fuel, farm and lumber sites, the scrapyard, the harbour and payphones.
[LF_TestInfo(LF_TestTier.LOGIC, pack: true)]
class LF_Test_EveronMapPlaces : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/map-places";
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckKind(LF_TestContext ctx, LF_MapPoiConfig config, string kind, string icon)
	{
		LF_MapPoiKind row = config.Find(kind);
		ctx.NotNull(row, string.Format("the pack has a POI row for %1", kind));
		if (!row)
			return;

		ctx.True(row.m_eVisibility == LF_EMapPoiVisibility.PUBLIC, string.Format("%1 is on every player's map, not hidden until heard of", kind));
		ctx.True(row.m_fMinZoom <= 0, string.Format("%1 has no minimum zoom", kind));
		if (!icon.IsEmpty())
			ctx.EqualStr(icon, row.m_sIcon, string.Format("%1 draws its own pictogram", kind));

		string caption = WidgetManager.Translate(row.m_sLabel);
		ctx.True(caption != row.m_sLabel && !caption.IsEmpty(), string.Format("the %1 name is in the string table", kind));
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
			// What the map did before: no island rows file, so these kinds have no row of their own.
			LF_MapPoiConfig bare = new LF_MapPoiConfig();
			bare.m_aKinds = new array<ref LF_MapPoiKind>();
			config = bare;
		}

		CheckKind(ctx, config, "Clerk", "poi_IdCard");
		CheckKind(ctx, config, "HostelBed", "poi_Bed");
		CheckKind(ctx, config, "StoreCounter", "poi_Shop");
		CheckKind(ctx, config, "ClothingCounter", "poi_Clothing");
		CheckKind(ctx, config, "GunCounter", "poi_Guns");
		CheckKind(ctx, config, "BarberChair", "poi_Barber");
		CheckKind(ctx, config, "CafeTable", "poi_Cafe");
		CheckKind(ctx, config, "WholesaleCounter", "poi_Wholesale");
		CheckKind(ctx, config, "TimberCounter", "poi_TimberMerchant");
		CheckKind(ctx, config, "ParcelDepot", "poi_Depot");
		CheckKind(ctx, config, "ATM", "poi_Atm");
		CheckKind(ctx, config, "Garage", "poi_Garage");
		CheckKind(ctx, config, "FuelTill", "poi_Fuel");
		CheckKind(ctx, config, "FuelDepot", "poi_FuelDepot");
		CheckKind(ctx, config, "RoadFuel", "poi_Fuel");
		CheckKind(ctx, config, "Payphone", "poi_Payphone");
		CheckKind(ctx, config, "FarmCompound", "poi_Farm");
		CheckKind(ctx, config, "Orchard", "poi_Orchard");
		CheckKind(ctx, config, "WheatField", "poi_Wheat");
		CheckKind(ctx, config, "ChopYard", "poi_LoggingYard");
		CheckKind(ctx, config, "ScrapRecycler", "poi_Scrap");
		CheckKind(ctx, config, "FishingSpot", "poi_Fish");
		CheckKind(ctx, config, "Workplace", "poi_Workplace");
		CheckKind(ctx, config, "Bank", "");
		CheckKind(ctx, config, "PoliceStation", "");
		CheckKind(ctx, config, "HospitalSite", "");
		CheckKind(ctx, config, "HarbourMaster", "poi_Harbour");

		// The map layer draws the pack's pictograms only when the pack names its imageset: without it every place
		// fell back to a neutral category glyph (the clerk drew as a town-hall column icon).
		ResourceName icons;
		if (!RedProof())
			icons = LF_MapPlaceIcons.PackImageSet();

		ctx.False(icons.IsEmpty(), "the pack names its map icon imageset");
		ctx.True(icons.GetPath().EndsWith("LF_MapIcons_Everon1989.imageset"), "the pack's icon imageset is its own pictogram set");

		// The clerk is named for what a new player looks for, and a stranger sees it.
		LF_MapPoiKind clerk = config.Find("Clerk");
		if (clerk)
		{
			ctx.EqualStr("#LF-MapPoi_Clerk", clerk.m_sLabel, "the clerk row is named for the ID cards");
			array<ref LF_MapPoiPlace> places = {};
			places.Insert(LF_MapPoiPlace.Make("Clerk", "town.hall", "1 0 1", true));
			places.Insert(LF_MapPoiPlace.Make("HostelBed", "town.hostel", "5 0 5", true));
			array<ref LF_MapPoi> pois = {};
			LF_MapPois.Collect(config, new LF_MapPoiAudience(), places, pois);
			ctx.Equal(2, pois.Count(), "a player who never heard of them sees the clerk and the hostel");
		}
	}
}
