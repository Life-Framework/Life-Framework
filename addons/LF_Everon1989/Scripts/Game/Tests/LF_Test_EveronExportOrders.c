// red-proof: 2026-11-27 (#5064): on the unfixed book (copper_bars_steamer still a template) `--only LF_Test_EveronExportOrders` failed (`the pack opens three kinds` and the copper template's fillable-goods checks); green once the template was removed. Earlier proof, 2026-11-24`tools/cli test --no-build --red LF_Test_EveronExportOrders`: RED-OK (15 assertions); RedProof() swaps in an empty market, so "iron_bars_steamer is priced by the market" and "40 iron bars pay ... = $1040 (expected 1040, got 0)" fail.
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
		ctx.True(config && config.m_aTemplates.Count() == 3, "the pack opens three kinds of export order");
		if (!config)
			return;

		// #5064: an order must be fillable on the island. These are the goods whose whole chain is placed on it (iron
		// from the Coalman's Brow and Highstone faces and the east smelter, coal from the East Rise seam, planks from
		// the lumber chain). Copper has ore, a refinery row and no placed seam, so a copper order could be taken and
		// never filled; add its bar here only in the change that places the copper chain.
		array<ResourceName> fillable = {
			"{BCD1E8CD7F975074}Prefabs/Resources/Mining/IronBar.et",
			"{C2AE9B7E62F0369F}Prefabs/Resources/Mining/CoalLump.et",
			"{45D1B36F99DD2FC3}Prefabs/Resources/WoodCutting/WoodPlank.et"
		};
		foreach (LF_ExportOrderTemplate source : config.m_aTemplates)
		{
			bool known;
			foreach (ResourceName good : fillable)
			{
				if (LF_Utils.SamePrefab(source.m_Goods, good))
					known = true;
			}

			ctx.True(known, source.m_sId + " asks for goods the island can make (no copper order until a copper seam is placed)");
			ctx.False(source.m_sId == "copper_bars_steamer", "the book offers no copper bar order");
		}

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
