// red-proof: with the `RedProof()` hook swapping the pack source for an empty header, the overlays never load: `the overlay sets m_bCrimePaysMarked` fails (core ships it off), the informant fee and tier read as core values, and the moonshine price falls back to the core 60; run `tools\cli test --no-build --red LF_Test_EveronDirtyMoney`.
//! #2922 (B4 of #1025): dirty money on the 1989 server, read through the overlays under the Everon 1989 header
//! (the pack leg). Robberies pay marked notes, the laundering cut and cap are the pack's, the informant's fee and
//! tier are set, the buyer's list names only chain goods and pays each above what its ingredients cost in the
//! supermarket, the night-shift worker offers the informant's reply once asked about work, and the informant's and station's words are
//! pack strings. Tobacco and paper chains do not exist yet, so only the moonshine is priced.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronDirtyMoney : LF_Test
{
	static const ResourceName LAUNDER_OVERLAY = "{26ABAEC805252936}Configs/Crime/LF_Launder_Everon1989.conf";
	static const ResourceName PRICES_OVERLAY = "{866D768C1DF804F7}Configs/Crime/LF_BuyerPrices_Everon1989.conf";
	static const ResourceName MOONSHINE = "{5280F09CEB005042}Prefabs/Items/Drinks/Moonshine.et";
	static const ResourceName PLUM = "{C3E35D690317B5BE}Prefabs/Items/Food/Plum.et";
	static const ResourceName CABBAGE = "{9FEBBF75B1D525EE}Prefabs/Items/Food/Cabbage.et";
	static const string SUPERMARKET = "supermarket";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/dirty-money";
	}

	//------------------------------------------------------------------------------------------------
	//! What the raw ingredients of one priced good cost in the supermarket; -1 for a good this test has no
	//! recipe for (a new priced good adds its recipe here) or an ingredient the supermarket does not sell.
	protected static int IngredientCost(ResourceName good)
	{
		array<ResourceName> raw = {};
		array<int> amounts = {};
		if (LF_Utils.SamePrefab(good, MOONSHINE))
		{
			// The moonshine chain: mash 4 plums and 2 cabbages into water, distill the water.
			raw.Insert(PLUM);
			amounts.Insert(4);
			raw.Insert(CABBAGE);
			amounts.Insert(2);
		}
		else
		{
			return -1;
		}

		LF_ShopStock stock = LF_ShopStockConfig.Get().Find(SUPERMARKET);
		if (!stock)
			return -1;

		int total;
		foreach (int i, ResourceName prefab : raw)
		{
			LF_ShopStockItem row = stock.FindItem(prefab);
			if (!row)
				return -1;

			total += row.m_iPrice * amounts[i];
		}

		return total;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MissionHeader header = LF_MissionHeader.GetCurrent();
		ctx.NotNull(header, "the pack leg runs under an LF mission header");
		if (header && header.m_aLF_ConfigOverlays)
		{
			ctx.True(header.m_aLF_ConfigOverlays.Find(LAUNDER_OVERLAY) != -1, "the header lists the launder overlay");
			ctx.True(header.m_aLF_ConfigOverlays.Find(PRICES_OVERLAY) != -1, "the header lists the buyer price overlay");
		}

		if (RedProof())
		{
			// No pack layer: an empty header as the pack source.
			LF_MissionConfigPackSource none = new LF_MissionConfigPackSource();
			none.SetHeader(new LF_MissionHeader());
			LF_ConfigService.SetPackSource(none);
		}

		LF_LaunderConfig.ResetStatics();
		LF_BuyerPrices.ResetStatics();
		LF_LaunderConfig config = LF_LaunderConfig.Get();
		LF_BuyerPrices prices = LF_BuyerPrices.Get();
		bool paysMarked = LF_LaunderConfig.CrimePaysMarked();
		LF_LaunderConfig rules = LF_LaunderConfig.GetRules();
		if (RedProof())
		{
			LF_ConfigService.Reset();
			LF_LaunderConfig.ResetStatics();
			LF_BuyerPrices.ResetStatics();
		}

		ctx.NotNull(config, "the launder config loads under the pack");
		ctx.True(paysMarked, "the overlay sets m_bCrimePaysMarked: robberies pay marked notes");
		ctx.NotNull(rules, "the wash rules (cut, batch, cap) pass validation");
		if (config)
		{
			ctx.Equal(35, config.m_iCutPct, "the house cut is the pack's 35 %");
			ctx.Equal(6000, config.m_iDailyCap, "the daily cap is the pack's");
			ctx.Equal(150, config.m_iInformantFee, "the informant fee is set");
			ctx.Equal(1, config.m_iInformantMinTier, "the informant talks from tier 1");
		}

		ctx.NotNull(prices, "the buyer price list loads under the pack");
		if (prices && prices.m_aPrices)
		{
			ctx.True(prices.m_aPrices.Count() > 0, "the buyer prices at least one good");
			foreach (LF_BuyerPriceRow row : prices.m_aPrices)
			{
				int bulk;
				ctx.True(LF_ChainSearch.IsGood(row.m_Prefab, bulk), string.Format("%1 is a chain goods item", row.m_Prefab));
				int cost = IngredientCost(row.m_Prefab);
				ctx.True(cost > 0, string.Format("%1: its ingredients have a supermarket cost", row.m_Prefab));
				if (cost > 0)
					ctx.True(row.m_iPrice > cost, string.Format("%1 pays %2, above its ingredients' %3", row.m_Prefab, row.m_iPrice, cost));
			}

			ctx.Equal(80, LF_BuyerPrices.PriceOf(MOONSHINE), "the buyer pays the pack's moonshine price");
		}

		// The informant: after he has talked about work the night-shift worker offers the reply, only through the Informant hook
		// (a menu shows at most four replies, so it takes the place of the work reply's own repeat).
		string path = LF_DialogueRegistry.Resolve("night_shift");
		LF_DialogueConfig dialogue = LF_DialogueConfig.Layered(path);
		ctx.NotNull(dialogue, "the night-shift dialogue loads");
		if (dialogue)
		{
			array<string> problems = {};
			LF_DialogueFlow.Problems(dialogue, problems);
			ctx.Equal(0, problems.Count(), "the night-shift dialogue stays playable with the informant reply");
			array<string> greetings = {"work"};
			foreach (string nodeId : greetings)
			{
				LF_DialogueNode node = LF_DialogueFlow.FindNode(dialogue, nodeId);
				ctx.NotNull(node, nodeId + " exists");
				if (!node || !node.m_aOptions)
					continue;

				int tips;
				foreach (LF_DialogueOption option : node.m_aOptions)
				{
					if (option.m_sAction != "Informant")
						continue;

					tips++;
					ctx.EqualStr("Informant", option.m_sCondition, nodeId + ": the reply shows only to a player he would tell");
					ctx.EqualStr("tip", option.m_sNext, nodeId + ": the reply leads to the tip node");
				}

				ctx.Equal(1, tips, nodeId + " offers the informant reply once");
			}
		}

		// Strings are pack keys: the reply and the informant's and station's words resolve to the pack's text.
		ctx.True(WidgetManager.Translate("#LF-Dialogue_NightShift_SayTip").IndexOf("buying") != -1, "the informant reply is a pack string");
		ctx.True(WidgetManager.Translate("#LF-Informant_Told").IndexOf("tonight") != -1, "the informant's answer is in the pack's words");
		ctx.True(WidgetManager.Translate("#LF-Launder_DailyCap").IndexOf("town") != -1, "the daily cap toast is in the pack's words");
	}
}
