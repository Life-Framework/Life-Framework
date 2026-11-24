// red-proof: #4717 names: under `--red` each tier prefab resolves its base prefab's name (the old one), so the nine-distinct-names lines fail. Observed 2026-11-23 (#4415):before the tier rows existed on main, `tools\cli test --no-build --only LF_Test_EveronToolTiers` failed on every `has a row at the miners' supply till` line and on the level-2 refusal; under `--red` the prefab probe is blanked and the same lines fail.
//! #4415 (WK-W12): the miners' supply till, which the hardware store's tool counter also is, sells the
//! pickaxe, the axe and the fishing rod in their three finishes (#4188) at $150, $600 and $1,800. The
//! better finishes sit behind the trade level: a level-2 miner is refused the fine pickaxe and the
//! refusal says it in words. Checked against the merged pack catalogue (pack leg).
[LF_TestInfo(LF_TestTier.LOGIC, pack: true)]
class LF_Test_EveronToolTiers : LF_Test
{
	protected static const string OUTLET = "miners_supply";
	protected static const int PRICE_WORN = 150;
	protected static const int PRICE_STANDARD = 600;
	protected static const int PRICE_FINE = 1800;
	protected static const int LEVEL_STANDARD = 3;
	protected static const int LEVEL_FINE = 6;

	static const ResourceName PICKAXE_WORN = "{D9D9C619E82ECEA7}Prefabs/Tools/Tiers/LF_Tool_Pickaxe_Worn.et";
	static const ResourceName PICKAXE_STANDARD = "{A1E3A11E03AEFC5C}Prefabs/Tools/Tiers/LF_Tool_Pickaxe_Standard.et";
	static const ResourceName PICKAXE_FINE = "{67822853D125045C}Prefabs/Tools/Tiers/LF_Tool_Pickaxe_Fine.et";
	static const ResourceName AXE_WORN = "{473A848D505EC104}Prefabs/Tools/Tiers/LF_Tool_Axe_Worn.et";
	static const ResourceName AXE_STANDARD = "{C17E26FDABD6E423}Prefabs/Tools/Tiers/LF_Tool_Axe_Standard.et";
	static const ResourceName AXE_FINE = "{B97D14FD730FB045}Prefabs/Tools/Tiers/LF_Tool_Axe_Fine.et";
	static const ResourceName ROD_WORN = "{882DF16910A9076F}Prefabs/Tools/Tiers/LF_Tool_Rod_Worn.et";
	static const ResourceName ROD_STANDARD = "{18F6CD1DCF69D861}Prefabs/Tools/Tiers/LF_Tool_Rod_Standard.et";
	static const ResourceName ROD_FINE = "{10849D099541584B}Prefabs/Tools/Tiers/LF_Tool_Rod_Fine.et";
	static const ResourceName BASE_PICKAXE = "{23341E2840F5ACB4}Prefabs/Tools/Pickaxe.et";
	static const ResourceName BASE_AXE = "{9CAC68BA551417A4}Prefabs/Tools/Axe/Axe.et";
	static const ResourceName BASE_ROD = "{0327968D17D249FD}Prefabs/Items/Fishing/LF_FishingRod.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/tool-tiers";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ItemCatalogueConfig catalogue = LF_ItemCatalogueConfig.Get();
		ctx.NotNull(catalogue, "the catalogue loads");
		if (!catalogue)
			return;

		CheckLadder(ctx, catalogue, "pickaxe", PICKAXE_WORN, PICKAXE_STANDARD, PICKAXE_FINE, "MINER");
		CheckLadder(ctx, catalogue, "axe", AXE_WORN, AXE_STANDARD, AXE_FINE, "LUMBERJACK");
		CheckLadder(ctx, catalogue, "rod", ROD_WORN, ROD_STANDARD, ROD_FINE, "");
		CheckLevelGate(ctx, catalogue);
		CheckNames(ctx);
	}

	//------------------------------------------------------------------------------------------------
	//! #4717: the till sheet, the inventory and the tool-wear line show an item's UI name, so the nine
	//! tier prefabs must resolve nine different names, each in words (no raw key, no base-item name).
	protected void CheckNames(LF_TestContext ctx)
	{
		array<ResourceName> prefabs = {PICKAXE_WORN, PICKAXE_STANDARD, PICKAXE_FINE, AXE_WORN, AXE_STANDARD, AXE_FINE, ROD_WORN, ROD_STANDARD, ROD_FINE};
		array<ResourceName> bases = {BASE_PICKAXE, BASE_PICKAXE, BASE_PICKAXE, BASE_AXE, BASE_AXE, BASE_AXE, BASE_ROD, BASE_ROD, BASE_ROD};
		ref array<string> seen = {};
		foreach (int i, ResourceName prefab : prefabs)
		{
			ResourceName probe = prefab;
			if (RedProof())
				probe = bases[i];

			string name = WidgetManager.Translate(LF_ShopManagerComponent.ResolveDisplayName(probe));
			ctx.False(name.IsEmpty() || name.StartsWith("#"), string.Format("%1 resolves a name in words (%2)", prefab, name));
			bool fresh = seen.Find(name) < 0;
			ctx.True(fresh, string.Format("%1 has a name no other tier prefab has (%2)", prefab, name));
			if (fresh)
				seen.Insert(name);
		}
		ctx.Equal(9, seen.Count(), "nine tier prefabs resolve nine names");
	}

	//------------------------------------------------------------------------------------------------
	//! One tool kind: three rows at the till, priced $150, $600 and $1,800, each below its sell-back
	//! price's ceiling, with the standard and fine finishes gated by the job level when the kind has a job.
	protected void CheckLadder(LF_TestContext ctx, LF_ItemCatalogueConfig catalogue, string kind, ResourceName worn, ResourceName standard, ResourceName fine, string job)
	{
		LF_OutletOffer wornOffer = OfferOf(ctx, catalogue, kind + " (worn)", worn, PRICE_WORN);
		LF_OutletOffer standardOffer = OfferOf(ctx, catalogue, kind + " (standard)", standard, PRICE_STANDARD);
		LF_OutletOffer fineOffer = OfferOf(ctx, catalogue, kind + " (fine)", fine, PRICE_FINE);
		if (!wornOffer || !standardOffer || !fineOffer)
			return;

		if (job.IsEmpty())
			return;

		ctx.EqualStr(job, standardOffer.m_sJob, string.Format("the standard %1 is gated to the %2 job", kind, job));
		ctx.EqualStr(job, fineOffer.m_sJob, string.Format("the fine %1 is gated to the %2 job", kind, job));
		ctx.Equal(LEVEL_STANDARD, standardOffer.m_iMinRank, string.Format("the standard %1 needs level %2", kind, LEVEL_STANDARD));
		ctx.Equal(LEVEL_FINE, fineOffer.m_iMinRank, string.Format("the fine %1 needs level %2", kind, LEVEL_FINE));
	}

	//------------------------------------------------------------------------------------------------
	//! The offer at the till for a tier prefab and its catalogue price; null (and a failed check) when
	//! the till does not sell it.
	protected LF_OutletOffer OfferOf(LF_TestContext ctx, LF_ItemCatalogueConfig catalogue, string label, ResourceName prefab, int price)
	{
		Resource resource = Resource.Load(prefab);
		ctx.True(resource && resource.IsValid(), string.Format("the %1 prefab loads", label));

		ResourceName probe = prefab;
		if (RedProof())
			probe = "";

		LF_CatalogueItem item;
		LF_OutletOffer offer = LF_ItemCatalogue.OfferFor(catalogue, OUTLET, probe, item);
		ctx.NotNull(offer, string.Format("the %1 has a row at the miners' supply till", label));
		if (!offer || !item)
			return null;

		ctx.Equal(price, item.m_iPrice, string.Format("the %1 costs $%2", label, price));
		ctx.True(item.m_iSellPrice < item.m_iPrice, string.Format("the %1 sells back for less than it costs (%2 against %3)", label, item.m_iSellPrice, item.m_iPrice));
		ctx.True(item.GetLegality() == LF_EItemLegality.LEGAL, string.Format("the %1 is legal to carry", label));
		return offer;
	}

	//------------------------------------------------------------------------------------------------
	//! A level-2 miner is refused the fine pickaxe in words; a level-6 miner is let through, and a
	//! level-3 miner buys the standard one but not the fine one.
	protected void CheckLevelGate(LF_TestContext ctx, LF_ItemCatalogueConfig catalogue)
	{
		LF_CatalogueItem item;
		LF_OutletOffer fine = LF_ItemCatalogue.OfferFor(catalogue, OUTLET, PICKAXE_FINE, item);
		LF_OutletOffer standard = LF_ItemCatalogue.OfferFor(catalogue, OUTLET, PICKAXE_STANDARD, item);
		if (!fine || !standard)
			return;

		LF_Test_CatalogueBuyer novice = new LF_Test_CatalogueBuyer();
		novice.m_mJobLevels.Set("MINER", 2);

		string missing;
		LF_EOutletGate gate = LF_ItemCatalogue.Check(fine, novice, missing);
		if (RedProof())
			gate = LF_EOutletGate.OPEN;

		ctx.True(gate == LF_EOutletGate.NEEDS_RANK, "a level-2 miner is refused the fine pickaxe for rank");
		LF_EShopRefusal refusal = LF_ItemCatalogue.RefusalOf(gate);
		ctx.EqualStr("#LF-Shop_RefuseNeedsRank", LF_ShopRefusal.Key(refusal), "the refusal is the needs-rank sentence");
		ctx.False(missing.IsEmpty(), "the refusal names what the buyer lacks");
		string words = LF_ShopRefusal.Text(refusal, missing);
		ctx.False(words.IsEmpty(), "the refusal reads in words");

		ctx.True(LF_ItemCatalogue.Check(standard, novice, missing) == LF_EOutletGate.NEEDS_RANK, "a level-2 miner is refused the standard pickaxe too");

		LF_Test_CatalogueBuyer journeyman = new LF_Test_CatalogueBuyer();
		journeyman.m_mJobLevels.Set("MINER", LEVEL_STANDARD);
		ctx.True(LF_ItemCatalogue.Check(standard, journeyman, missing) == LF_EOutletGate.OPEN, "a level-3 miner buys the standard pickaxe");
		ctx.True(LF_ItemCatalogue.Check(fine, journeyman, missing) == LF_EOutletGate.NEEDS_RANK, "a level-3 miner is still refused the fine pickaxe");

		LF_Test_CatalogueBuyer master = new LF_Test_CatalogueBuyer();
		master.m_mJobLevels.Set("MINER", LEVEL_FINE);
		ctx.True(LF_ItemCatalogue.Check(fine, master, missing) == LF_EOutletGate.OPEN, "a level-6 miner buys the fine pickaxe");

		LF_Test_CatalogueBuyer farmer = new LF_Test_CatalogueBuyer();
		farmer.m_mJobLevels.Set("FARMER", 10);
		ctx.True(LF_ItemCatalogue.Check(fine, farmer, missing) == LF_EOutletGate.NEEDS_JOB, "a farmer is not sold the fine pickaxe, whatever the level");
	}
}
