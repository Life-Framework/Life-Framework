// red-proof: observed 2026-11-16 with `tools\cli test --no-build --red LF_Test_CatalogueStock`: RedProof() skips
// the Take calls, so the stock never counts down; RED-OK, failing `the next sale refuses at zero`, `still
// empty a second before the restock` and `no restock time, no restock`. Green without the hook.
// tier: LOGIC
//! Catalogue stock and restock (#1064): an offer with a stock counts down per sale, the next sale
//! refuses at zero, the shelf is full again after the restock time, an unlimited offer never runs
//! out, an offer is counted on its own, and the owner fields stock and restockSeconds patch by key.
class LF_Test_CatalogueStock : LF_Test
{
	override string GetName()
	{
		return "catalogue/stock";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ConfigService.Reset();
		LF_ConfigStream.Reset();
		LF_CatalogueStock.Reset();

		LF_OutletOffer offer = LF_OutletOffer.Make("gun_counter", "pistol_m9");
		offer.m_iStock = 3;
		offer.m_iRestockSeconds = 60;
		int t = 1000;

		ctx.Equal(3, LF_CatalogueStock.Left(offer, t), "a new shelf is full");
		ctx.True(LF_CatalogueStock.CanSupply(offer, 3, t), "the whole shelf can go in one order");
		ctx.True(!LF_CatalogueStock.CanSupply(offer, 4, t), "an order beyond the shelf is refused");

		// Counts down per sale.
		if (!RedProof())
			LF_CatalogueStock.Take(offer, 1, t);
		ctx.Equal(2, LF_CatalogueStock.Left(offer, t + 1000), "one sale leaves two");
		if (!RedProof())
			LF_CatalogueStock.Take(offer, 2, t + 2000);
		ctx.Equal(0, LF_CatalogueStock.Left(offer, t + 3000), "the shelf is empty after three sales");
		ctx.True(!LF_CatalogueStock.CanSupply(offer, 1, t + 3000), "the next sale refuses at zero");

		// Another offer is counted on its own.
		LF_OutletOffer other = LF_OutletOffer.Make("gun_counter", "rifle_m16a2");
		other.m_iStock = 1;
		ctx.True(LF_CatalogueStock.CanSupply(other, 1, t + 3000), "an empty pistol shelf does not empty the rifle shelf");

		// The restock clock starts at the first sale, not at each one: 60 s after t, the shelf is full.
		ctx.True(!LF_CatalogueStock.CanSupply(offer, 1, t + 59000), "still empty a second before the restock");
		ctx.Equal(3, LF_CatalogueStock.Left(offer, t + 60000), "the stock is back after the restock time");
		ctx.True(LF_CatalogueStock.CanSupply(offer, 1, t + 60000), "the next sale goes through after the restock");

		// A restock of 0 never comes back until a restart.
		LF_OutletOffer once = LF_OutletOffer.Make("clothing_rack", "raincoat_green");
		once.m_iStock = 1;
		once.m_iRestockSeconds = 0;
		if (!RedProof())
			LF_CatalogueStock.Take(once, 1, t);
		ctx.True(!LF_CatalogueStock.CanSupply(once, 1, t + 86400000), "no restock time, no restock");
		LF_CatalogueStock.Reset();
		ctx.True(LF_CatalogueStock.CanSupply(once, 1, t), "a restart refills every outlet");

		// No stock set is no limit.
		LF_OutletOffer open = LF_OutletOffer.Make("clothing_rack", "scarf");
		ctx.Equal(-1, LF_CatalogueStock.Left(open, t), "no stock is no limit");
		LF_CatalogueStock.Take(open, 50, t);
		ctx.True(LF_CatalogueStock.CanSupply(open, 50, t), "an unlimited offer never runs out");

		// Owner patch: stock and restockSeconds merge by outlet/item like the rest of the section.
		LF_ItemCatalogueConfig config = LF_Test_ItemCatalogue.MakeConfig();
		LF_OutletOffer counter = config.FindOffer("gun_counter", "pistol_m9");
		ctx.NotNull(counter, "the test catalogue has the gun counter offer");
		ctx.Equal(0, counter.m_iStock, "an offer is unlimited unless a layer sets a stock");
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1,\"catalogue\":{\"offers\":{\"gun_counter/pistol_m9\":{\"stock\":5,\"restockSeconds\":120}}}}", "test");
		LF_ItemCatalogueConfig patched = LF_Test_ItemCatalogue.MakeConfig();
		patched.ApplyLayers(LF_ItemCatalogueConfig.CONFIG_PATH);
		LF_OutletOffer patchedOffer = patched.FindOffer("gun_counter", "pistol_m9");
		ctx.NotNull(patchedOffer, "the patched offer is still listed");
		if (patchedOffer)
		{
			ctx.Equal(5, patchedOffer.m_iStock, "the owner patch sets the stock");
			ctx.Equal(120, patchedOffer.m_iRestockSeconds, "the owner patch sets the restock time");
			ctx.EqualStr("FIREARM_PERMIT", patchedOffer.m_sLicence, "a stock patch keeps the licence gate");
		}
		LF_ConfigService.Reset();
		LF_CatalogueStock.Reset();
	}
}
