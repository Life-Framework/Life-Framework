// red-proof: 2026-11-27 `tools/cli test --no-build --red LF_Test_ExportOrderDraw`: RedProof() skips the draw, so "the drawn order is the book's first line" fails.
//! The harness-only `exportorder draw <kind>` (#4580 proof): OpenFirst opens one order of the template whose id
//! starts with the kind as the FIRST line of the book (the board's first take row takes it), leaves the
//! rotation alone, and opens nothing for an unknown kind.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_ExportOrderDraw : LF_Test
{
	protected static const ResourceName IRON = "{BCD1E8CD7F975074}Prefabs/Resources/Mining/IronBar.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "jobs/export-order-draw";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_ExportOrderTemplate Template(string id, int count)
	{
		LF_ExportOrderTemplate template = new LF_ExportOrderTemplate();
		template.m_sId = id;
		template.m_Goods = IRON;
		template.m_sGoodsName = id;
		template.m_sDestination = "the steamer";
		template.m_iCount = count;
		template.m_iWholesale = 40;
		template.m_iGameMinutes = 180;
		return template;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ExportOrderBook.ResetStatics();
		LF_ExportOrderConfig config = LF_ExportOrderConfig.CreateDefault();
		config.m_aTemplates.Insert(Template("iron_bars", 40));
		config.m_aTemplates.Insert(Template("planks_freighter", 120));
		LF_ExportOrderConfig.UseForTest(config);

		LF_ExportOrderBook.Tick(600);
		array<ref LF_ExportOrder> orders = LF_ExportOrderBook.List();
		int open = orders.Count();
		ctx.Equal(3, open, "a fresh book opens three orders");

		LF_ExportOrder drawn;
		if (!RedProof())
			drawn = LF_ExportOrderBook.OpenFirst("planks", 600);

		ctx.True(drawn != null, "a kind that matches a template opens an order");
		ctx.Equal(open + 1, orders.Count(), "the drawn order joins the book");
		if (drawn && orders.Count() > 0)
		{
			ctx.True(orders[0] == drawn, "the drawn order is the book's first line");
			ctx.Equal(120, drawn.m_iCount, "it is the plank order (120 units, a truck's load)");
			ctx.True(!drawn.IsTaken(), "it is open");
		}

		ctx.True(LF_ExportOrderBook.OpenFirst("no_such_kind", 600) == null, "an unknown kind opens nothing");
		ctx.Equal(open + 1, orders.Count(), "an unknown kind leaves the book as it was");

		LF_ExportOrderBook.ResetStatics();
		LF_ExportOrderConfig.Reload();
	}
}
