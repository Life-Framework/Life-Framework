// red-proof: 2026-11-17, `--only LF_Test_GatherStockConfig` on the unfixed ResourceChains.conf (apple, plum, cement, gravel, sand all m_iMaxGathers 0, no regrow) failed "apple spot has a stock cap".
// tier: WORLD
//! gather/stock-config (#2409): the free gather spots carry a stock and a regrow time in
//! ResourceChains.conf, and ripe crops stay on their own growth timer.
class LF_Test_GatherStockConfig : LF_Test
{
	override string GetName()
	{
		return "gather/stock-config";
	}

	override void Run(LF_TestContext ctx)
	{
		array<string> ids = {"apple", "plum", "cement", "gravel", "sand"};
		foreach (string id : ids)
		{
			LF_ResourceNodeDef node = LF_ResourceChainCatalog.FindNode(id);
			ctx.NotNull(node, id + " node exists");
			if (!node)
				continue;

			ctx.True(node.m_iMaxGathers > 0, id + " spot has a stock cap");
			ctx.True(node.m_fRegrowMs > 0, id + " spot regrows");
			// Lead decision: about 10, one back per 3 minutes. A wide band so a rebalance does not break the test.
			ctx.InRange(node.m_iMaxGathers, 3, 20, id + " stock is a handful, not a stockpile");
			ctx.InRange(node.m_fRegrowMs, 60000, 600000, id + " regrows in minutes, not seconds");
		}

		LF_ResourceNodeDef cabbage = LF_ResourceChainCatalog.FindNode("cabbage");
		ctx.NotNull(cabbage, "cabbage node exists");
		if (cabbage)
			ctx.Equal(0, cabbage.m_iMaxGathers, "a ripe crop is limited by its growth stage, not a stock");
	}
};
