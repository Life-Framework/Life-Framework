// red-proof: with the `butcher` stock left out of LF_ShopStock_Everon1989.conf, `tools\cli test --no-build --only LF_Test_EveronButcherStock` fails `the butcher has a stock under the pack header` (RedProof() looks up a missing id).
//! #3228: the Saint-Philippe butcher's till sells the pack stock `butcher`: dried meat and tinned meat,
//! every prefab resolves, no row sells below what the shop pays back (no money loop) or below the market's
//! ceiling for a curved good (no shop-to-trader loop, #3121). The till and the meat display prefabs load.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronButcherStock : LF_Test
{
	protected static const ResourceName SAUCISSON = "{A2A09084A440CB11}Prefabs/Items/Food/LF_Saucisson.et";
	protected static const ResourceName HAM = "{753BC751EC3BABC0}Prefabs/Items/Food/LF_Ham.et";
	protected static const ResourceName DRIED_MEAT = "{486317A3719FD166}Prefabs/Items/Food/LF_DriedMeat.et";
	protected static const ResourceName CANNED_MEAT = "{A0A5D34D6F8232B4}Prefabs/Items/Store1989/LF_CannedMeat.et";
	protected static const ResourceName TILL = "{7CA6A8139045323E}Prefabs/Props/Commercial/LF_ButcherTill.et";
	protected static const ResourceName DISPLAY = "{F1E319DF62725A34}Prefabs/Props/Commercial/LF_ButcherMeatDisplay.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/butcher-stock";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ShopManagerComponent manager = LF_ShopManagerComponent.GetInstance();
		ctx.NotNull(manager, "the shop manager exists");
		if (!manager)
			return;

		string lookup = "butcher";
		if (RedProof())
			lookup = "butcher_missing";

		LF_ShopStock stock = LF_ShopStockConfig.Get().Find(lookup);
		ctx.NotNull(stock, "the butcher has a stock under the pack header");
		if (!stock)
			return;

		array<ResourceName> wanted = {DRIED_MEAT, CANNED_MEAT, SAUCISSON, HAM};
		ctx.True(stock.m_aItems && stock.m_aItems.Count() == wanted.Count(), string.Format("the butcher stock lists %1 items", wanted.Count()));
		foreach (ResourceName prefab : wanted)
		{
			Resource resource = Resource.Load(prefab);
			ctx.True(resource && resource.IsValid(), string.Format("butcher: %1 loads", prefab));

			LF_ShopStockItem row = stock.FindItem(prefab);
			ctx.NotNull(row, string.Format("butcher: %1 is in the stock", prefab));
			if (!row)
				continue;

			ctx.True(row.m_iPrice > 0, string.Format("butcher: %1 has a price (got %2)", prefab, row.m_iPrice));
			if (manager.GetItem(prefab))
			{
				int buyPrice = 0;
				int sellPrice = 0;
				bool hidden = true;
				manager.GetPrices(prefab, buyPrice, sellPrice, hidden);
				ctx.True(row.m_iPrice >= sellPrice, string.Format("butcher: %1 sells for %2 and buys back for %3: no profit loop", prefab, row.m_iPrice, sellPrice));
			}

			LF_MarketGoodDef curve = LF_MarketBook.Curved(prefab);
			if (curve)
				ctx.True(row.m_iPrice >= curve.m_iCeiling, string.Format("butcher: %1 sells for %2 and the market pays up to %3", prefab, row.m_iPrice, curve.m_iCeiling));
		}

		ResourceName till = TILL;
		ResourceName display = DISPLAY;
		Resource tillResource = Resource.Load(till);
		Resource displayResource = Resource.Load(display);
		ctx.True(tillResource && tillResource.IsValid(), "the butcher till prefab loads");
		ctx.True(displayResource && displayResource.IsValid(), "the meat display prefab loads");
	}
}
