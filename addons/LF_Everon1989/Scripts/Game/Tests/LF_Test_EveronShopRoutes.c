// red-proof: RED-OK everon/shop-routes, observed 2026-11-27 (#5063) with `tools\cli test --no-build --red LF_Test_EveronShopRoutes`: with the hook on the stock lookup gets no prefab, so every `the <stock> stock sells <prefab>` check fails; green with `--only`.
//! #5063: the three starter and errand lines that send the player to a shop for an item (the lumber axe, the
//! fishing rod, the foundation's gravel and cement) name the shop whose stock really holds it, and the general
//! store's stock holds none of them. The shop is read from the merged pack stock (pack leg), so moving an item
//! between stocks without touching the line fails here.
[LF_TestInfo(LF_TestTier.LOGIC, pack: true)]
class LF_Test_EveronShopRoutes : LF_Test
{
	protected static const ResourceName AXE = "{9CAC68BA551417A4}Prefabs/Tools/Axe/Axe.et";
	protected static const ResourceName ROD = "{0327968D17D249FD}Prefabs/Items/Fishing/LF_FishingRod.et";
	protected static const ResourceName GRAVEL = "{344090890C63F4CA}Prefabs/Items/Resources/GravelBag.et";
	protected static const ResourceName CEMENT = "{D58674633332DC86}Prefabs/Items/Resources/CementBag.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/shop-routes";
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckRoute(LF_TestContext ctx, string key, string shopWords, string stockId, array<ResourceName> goods)
	{
		string text = LF_Terms.Text(key);
		LF_ShopStock stock = LF_ShopStockConfig.Get().Find(stockId);
		LF_ShopStock general = LF_ShopStockConfig.Get().Find("general_store");
		ctx.NotNull(stock, string.Format("%1: the %2 stock exists", key, stockId));
		ctx.NotNull(general, "the general store stock exists");
		if (!stock || !general)
			return;

		ctx.True(text.Contains(shopWords), string.Format("%1 names the shop that really sells it (%2)", key, shopWords));
		foreach (ResourceName good : goods)
		{
			ResourceName probe = good;
			if (RedProof())
				probe = "";

			ctx.NotNull(stock.FindItem(probe), string.Format("%1: the %2 stock sells %3", key, stockId, good));
			ctx.True(general.FindItem(good) == null, string.Format("%1: the general store does not sell %2", key, good));
		}
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		CheckRoute(ctx, "#LF-Activity_Lumber_Axe_Note", "hardware store", "hardware", {AXE});
		CheckRoute(ctx, "#LF-Activity_Fishing_Rod_Note", "seed and feed store", "seed_feed", {ROD});
		CheckRoute(ctx, "#LF-Quest_SettleFoundation_Desc", "hardware store", "hardware", {GRAVEL, CEMENT});
	}
}
