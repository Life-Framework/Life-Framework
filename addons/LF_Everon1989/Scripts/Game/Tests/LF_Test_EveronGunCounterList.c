// red-proof: RedProof() reads the clothing rack's list instead of the gun counter's. RED-OK 2026-11-27, `tools\cli test --no-build --red LF_Test_EveronGunCounterList`: `the gun counter sells exactly the ten pinned rows (expected 10, got 54)`, `the gun counter issues the firearm permit`. Hook off: green.
//! everon/gun-counter-list (gun-counter-overlay, 2026-11-27): the Saint-Philippe gun counter sells exactly what
//! the player manual (05-shops.md) and money-economy.md (#989 weapon split) say, with one owner per row:
//! the PM and the M21 with their magazines behind FIREARM_PERMIT, scopes, holsters and the M14 pouch for anyone,
//! the gang radio, and the permit itself as the licence row. The vz. 58 and the M9 magazine are not on it; the
//! vz. 58 is a black market rifle at $1,800 (LF_Catalogue_Weapons_Everon1989.conf owns it, the gun store
//! file no longer names it, so no load order can put it back on the counter at another price).
//! Lives in the pack because it reads the pack's catalogue (pack leg).
[LF_TestInfo(LF_TestTier.LOGIC, pack: true)]
class LF_Test_EveronGunCounterList : LF_Test
{
	protected static const string OUTLET = "gun_counter";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/gun-counter-list";
	}

	//------------------------------------------------------------------------------------------------
	//! One pinned row: the item is on the list, at this price, behind this licence ("" for anyone).
	protected static void Pin(LF_TestContext ctx, LF_ItemCatalogueConfig catalogue, array<LF_OutletOffer> offers, string id, int price, string licence)
	{
		LF_OutletOffer found;
		foreach (LF_OutletOffer offer : offers)
		{
			if (offer.m_sItem == id)
				found = offer;
		}

		ctx.NotNull(found, "the gun counter sells " + id);
		if (!found)
			return;

		LF_CatalogueItem item = LF_ItemCatalogue.Item(catalogue, id);
		ctx.NotNull(item, "the catalogue lists " + id);
		if (!item)
			return;

		ctx.Equal(price, item.m_iPrice, "the price of " + id);
		ctx.True(item.m_iSellPrice * 2 <= item.m_iPrice, "sell-back is half the price or less: " + id);
		ctx.EqualStr(licence, found.m_sLicence, "the licence on " + id);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ItemCatalogueConfig catalogue = LF_ItemCatalogueConfig.Get();
		ctx.NotNull(catalogue, "the item catalogue loads");
		if (!catalogue)
			return;

		string outlet = OUTLET;
		if (RedProof())
			outlet = "clothing_rack";

		array<LF_OutletOffer> offers = {};
		LF_ItemCatalogue.Offers(catalogue, outlet, offers);

		string permit = "FIREARM_PERMIT";
		Pin(ctx, catalogue, offers, "pistol_pm", 350, permit);
		Pin(ctx, catalogue, offers, "mag_pm_8rnd", 15, permit);
		Pin(ctx, catalogue, offers, "rifle_m21", 1500, permit);
		Pin(ctx, catalogue, offers, "mag_m14_20rnd", 35, permit);
		Pin(ctx, catalogue, offers, "scope_artii", 450, "");
		Pin(ctx, catalogue, offers, "scope_4x20", 300, "");
		Pin(ctx, catalogue, offers, "holster_m12", 30, "");
		Pin(ctx, catalogue, offers, "holster_pm", 25, "");
		Pin(ctx, catalogue, offers, "pouch_m14", 25, "");
		Pin(ctx, catalogue, offers, "gang_radio", 60, "");
		ctx.Equal(10, offers.Count(), "the gun counter sells exactly the ten pinned rows");

		ctx.NotNull(LF_ItemCatalogue.LicenceRowFor(catalogue, outlet, LF_ELicenseType.FIREARM_PERMIT), "the gun counter issues the firearm permit");

		// The vz. 58 is the black market's, at its one price.
		LF_CatalogueItem vz58 = LF_ItemCatalogue.Item(catalogue, "rifle_vz58p");
		ctx.NotNull(vz58, "the catalogue lists the vz. 58");
		if (vz58)
			ctx.Equal(1800, vz58.m_iPrice, "the vz. 58 costs $1,800");

		array<LF_OutletOffer> market = {};
		LF_ItemCatalogue.Offers(catalogue, "black_market", market);
		bool onMarket;
		foreach (LF_OutletOffer offer : market)
		{
			if (offer.m_sItem == "rifle_vz58p")
				onMarket = true;
		}

		ctx.True(onMarket, "the black market sells the vz. 58");
	}
}
