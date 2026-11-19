// red-proof: RedProof() skips loading the catalogue and the price overlay, so every assertion on the range fails (RED-OK). Also red on main before #3193: 14 price rules for 18 clothing-counter rows, 31 priced rows and no row above $90. Green in the pack leg.
//! shop/clothing-range (#3193): the clothing shop has something to save up for. The pack's clothing
//! catalogue lists at least 45 priced items and at least five of them cost $150 or more (a good suit,
//! a raincoat), so an hour-4 income has something to buy. The clothing counters (clothing_rack,
//! workwear_counter) each carry a price rule in the pack's `LF_ShopPrices_ClothingStore.conf`, every
//! rule names a catalogued item, the rule's buy price is the catalogue price and its sell-back is half
//! the price or less (the #3121 no-loop rule), and every prefab loads.
//! Lives in the pack because it reads the pack's overlays (pack leg).
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronClothingRange : LF_Test
{
	protected static const ResourceName PRICES_CONF = "{B16F9287FF8F753B}Configs/Shop/LF_ShopPrices_ClothingStore.conf";
	protected static const int MIN_ON_SALE = 45;
	protected static const int MIN_TOP_TIER = 5;
	protected static const int TOP_TIER_PRICE = 150;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/clothing-range";
	}

	//------------------------------------------------------------------------------------------------
	//! The pack's price overlay as an instance, or null when it does not load.
	protected static LF_ShopPricesConfig LoadPrices()
	{
		Resource resource = BaseContainerTools.LoadContainer(PRICES_CONF);
		if (!resource || !resource.IsValid())
			return null;

		return LF_ShopPricesConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(resource.GetResource().ToBaseContainer()));
	}

	//------------------------------------------------------------------------------------------------
	//! The prefab rule for this prefab in the overlay, or null.
	protected static LF_ShopPriceRule FindRule(LF_ShopPricesConfig prices, ResourceName prefab)
	{
		foreach (LF_ShopPriceRule rule : prices.m_aPriceRules)
		{
			if (rule && LF_Utils.SamePrefab(rule.m_Prefab, prefab))
				return rule;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a clothing counter sells the item with this prefab.
	protected static bool SoldAtCounters(LF_ItemCatalogueConfig catalogue, ResourceName prefab)
	{
		array<string> outlets = {"clothing_rack", "workwear_counter"};
		foreach (string outlet : outlets)
		{
			LF_CatalogueItem item;
			if (LF_ItemCatalogue.OfferFor(catalogue, outlet, prefab, item))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ItemCatalogueConfig catalogue;
		LF_ShopPricesConfig prices;
		if (!RedProof())
		{
			catalogue = LF_ItemCatalogueConfig.Get();
			prices = LoadPrices();
		}

		ctx.NotNull(catalogue, "the item catalogue loads");
		ctx.NotNull(prices, "the clothing price overlay loads");
		if (!catalogue || !prices || !prices.m_aPriceRules)
			return;

		// The range: priced items some outlet sells, and how many sit in the top tier.
		array<string> offered = {};
		foreach (LF_OutletOffer offer : catalogue.m_aOffers)
		{
			if (offer && !offer.m_bRemoved && !offer.IsLicenceRow() && offered.Find(offer.m_sItem) == -1)
				offered.Insert(offer.m_sItem);
		}

		int onSale;
		int topTier;
		foreach (LF_CatalogueItem item : catalogue.m_aItems)
		{
			if (!item || item.m_bRemoved || item.m_iPrice <= 0 || offered.Find(item.m_sId) == -1)
				continue;

			onSale++;
			if (item.m_iPrice >= TOP_TIER_PRICE)
				topTier++;
		}

		ctx.True(onSale >= MIN_ON_SALE, string.Format("at least %1 items are on sale (%2 are)", MIN_ON_SALE, onSale));
		ctx.True(topTier >= MIN_TOP_TIER, string.Format("at least %1 items cost $%2 or more (%3 do)", MIN_TOP_TIER, TOP_TIER_PRICE, topTier));

		// Every item a clothing counter sells has a price rule that agrees with the catalogue.
		foreach (LF_CatalogueItem item : catalogue.m_aItems)
		{
			if (!item || item.m_bRemoved || !SoldAtCounters(catalogue, item.m_Prefab))
				continue;

			ctx.True(item.m_Prefab.Length() > 0 && Resource.Load(item.m_Prefab).IsValid(), "the prefab loads: " + item.m_sId);
			LF_ShopPriceRule rule = FindRule(prices, item.m_Prefab);
			ctx.NotNull(rule, "the counter item has a price rule: " + item.m_sId);
			if (!rule)
				continue;

			ctx.Equal(item.m_iPrice, rule.m_iBuyPrice, "the rule buy price is the catalogue price: " + item.m_sId);
			ctx.True(item.m_iSellPrice * 2 <= item.m_iPrice, "sell-back is half the price or less: " + item.m_sId);
			ctx.True(rule.m_iSellPrice * 2 <= rule.m_iBuyPrice, "the rule sell-back is half the price or less: " + item.m_sId);
		}

		// Every price rule names a catalogued item that a counter sells.
		foreach (LF_ShopPriceRule rule : prices.m_aPriceRules)
		{
			if (!rule)
				continue;

			LF_CatalogueItem row = LF_ItemCatalogue.ItemByPrefab(catalogue, rule.m_Prefab);
			ctx.NotNull(row, "the price rule has a catalogue row: " + rule.m_Prefab);
			if (row)
				ctx.True(SoldAtCounters(catalogue, rule.m_Prefab), "a counter sells the rule's item: " + row.m_sId);
		}
	}
}
