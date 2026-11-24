// red-proof: 2026-11-24 `tools/cli test --no-build --red LF_Test_EveronExportOrders`: RED-OK (15 assertions); RedProof() swaps in an empty market, so "iron_bars_steamer is priced by the market" and "40 iron bars pay ... = $1040 (expected 1040, got 0)" fail.
//! #4844: the 1989 pack's export order book under the Everon header (the pack leg). Every kind is priced from the
//! market's own row (the template's wholesale is unset), so 40 iron bars read the iron row's $40 for a 2-bar lot as
//! $20 a bar and pay 40 x $20 + 15 percent + the $120 order bonus = $1040, not the $46 a bar the old typed $40 paid.
//! The band itself ($650 to $800/h per kind) is `tools/validation/economy-export-band-selftest.mjs`.
[LF_TestInfo(LF_TestTier.LOGIC, pack: true)]
class LF_Test_EveronExportOrders : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/export-orders";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ExportOrderConfig.Reload();
		LF_MarketConfig.Reload();
		if (RedProof())
		{
			LF_MarketConfig empty = new LF_MarketConfig();
			empty.m_aGoods = {};
			LF_MarketConfig.UseForTest(empty);
		}

		LF_ExportOrderConfig config = LF_ExportOrderConfig.Get();
		ctx.True(config && config.m_aTemplates.Count() == 4, "the pack opens four kinds of export order");
		if (!config)
			return;

		foreach (LF_ExportOrderTemplate template : config.m_aTemplates)
		{
			ctx.Equal(0, template.m_iWholesale, template.m_sId + " leaves its wholesale to the market row");
			ctx.True(LF_ExportOrderConfig.WholesaleFor(template) > 0, template.m_sId + " is priced by the market");
			if (template.m_sId == "iron_bars_steamer")
				ctx.Equal(1040, config.PayForTemplate(template), "40 iron bars pay 40 x $20 + 15 percent + $120 = $1040");
		}

		LF_ExportOrderConfig.Reload();
		LF_MarketConfig.Reload();
	}
}
