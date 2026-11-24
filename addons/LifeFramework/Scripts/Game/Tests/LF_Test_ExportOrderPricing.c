// red-proof: 2026-11-24 `tools/cli test --no-build --red LF_Test_ExportOrderPricing`: RED-OK (9 assertions); RedProof() leaves the market row out, so "a template at 0 reads the market base per unit (expected 20, got 0)" and "pay $1040 (expected 1040, got 0)" fail.
//! Export order pricing (#4844): a template with no wholesale of its own reads the market row of its goods, the row's
//! base per unit (a 2-bar lot at $40 is $20 a bar, not $40), and pays count x wholesale plus the premium plus its
//! order bonus. A template's own wholesale still wins. Goods with neither price open no order, and a book of only
//! unpriced kinds does not spin on its first fill.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_ExportOrderPricing : LF_Test
{
	protected static const ResourceName IRON = "{BCD1E8CD7F975074}Prefabs/Resources/Mining/IronBar.et";
	protected static const ResourceName UNPRICED = "{C2AE9B7E62F0369F}Prefabs/Resources/Mining/CoalLump.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "jobs/export-order-pricing";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_ExportOrderTemplate Template(string id, ResourceName goods, int count, int wholesale, int bonus)
	{
		LF_ExportOrderTemplate template = new LF_ExportOrderTemplate();
		template.m_sId = id;
		template.m_Goods = goods;
		template.m_sGoodsName = id;
		template.m_sDestination = "the steamer";
		template.m_iCount = count;
		template.m_iWholesale = wholesale;
		template.m_iBonus = bonus;
		template.m_iGameMinutes = 600;
		return template;
	}

	//------------------------------------------------------------------------------------------------
	protected void Clean()
	{
		LF_ExportOrderBook.ResetStatics();
		LF_ExportOrderConfig.Reload();
		LF_MarketConfig.Reload();
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		Clean();
		LF_MarketConfig market = new LF_MarketConfig();
		market.m_aGoods = {};
		if (!RedProof())
			market.m_aGoods.Insert(LF_MarketGoodDef.Make(IRON, 40, 28, 40, 1, 30, 2));
		LF_MarketConfig.UseForTest(market);

		LF_ExportOrderConfig config = LF_ExportOrderConfig.CreateDefault();
		LF_ExportOrderTemplate fromMarket = Template("iron", IRON, 40, 0, 120);
		LF_ExportOrderTemplate own = Template("iron_own", IRON, 20, 40, 0);
		LF_ExportOrderTemplate none = Template("coal", UNPRICED, 100, 0, 180);

		ctx.Equal(20, LF_ExportOrderConfig.WholesaleFor(fromMarket), "a template at 0 reads the market base per unit: $40 a 2-bar lot is $20 a bar");
		ctx.Equal(1040, config.PayForTemplate(fromMarket), "40 bars at $20 plus 15 percent, plus the $120 order bonus, pay $1040");
		ctx.Equal(920, config.PayForTemplate(own), "a template's own wholesale still wins: 20 x $40 plus 15 percent is $920");
		ctx.Equal(0, config.PayForTemplate(none), "goods with no template price and no market row price nothing");

		// The book opens only the priced kinds.
		config.m_aTemplates.Insert(fromMarket);
		config.m_aTemplates.Insert(none);
		LF_ExportOrderConfig.UseForTest(config);
		LF_ExportOrderBook.Tick(600);
		array<ref LF_ExportOrder> orders = LF_ExportOrderBook.List();
		ctx.Equal(3, orders.Count(), "the first fill opens three priced orders and skips the unpriced kind");
		foreach (LF_ExportOrder order : orders)
		{
			ctx.EqualStr("iron", order.m_sTemplate, "every open order is the priced kind");
			ctx.Equal(1040, order.m_iPay, "the order carries the market-priced pay with its bonus");
		}

		// A book of only unpriced kinds opens nothing and returns.
		LF_ExportOrderBook.ResetStatics();
		LF_ExportOrderConfig onlyNone = LF_ExportOrderConfig.CreateDefault();
		onlyNone.m_aTemplates.Insert(none);
		LF_ExportOrderConfig.UseForTest(onlyNone);
		LF_ExportOrderBook.Tick(600);
		ctx.Equal(0, LF_ExportOrderBook.List().Count(), "a book of unpriced kinds opens nothing");

		Clean();
	}
}
