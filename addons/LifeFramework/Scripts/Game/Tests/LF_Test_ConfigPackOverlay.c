// red-proof: in LF_ShopPricesConfig.ApplyContainer drop the IsVariableSetDirectly check on a
// rule's m_iSellPrice and config/pack-overlay fails "a rule field the pack file leaves out stays
// core (expected 4, got 50)" (observed 2026-11-12).

//------------------------------------------------------------------------------------------------
//! A stand-in for the D4 pack source: overlays the shops core config with a test fixture.
class LF_Test_ConfigPackSourceStub : LF_ConfigPackSource
{
	static const ResourceName OVERLAY = "{B1FB6EB28E9194A6}Configs/Tests/LF_Test_ShopPricesOverlay.conf";

	override void GetOverlays(ResourceName coreConf, notnull array<ResourceName> overlays)
	{
		if (coreConf == LF_Test_ConfigPackOverlay.CORE)
			overlays.Insert(OVERLAY);
	}

	override string GetName()
	{
		return "test-stub";
	}
};

//------------------------------------------------------------------------------------------------
//! Owner over pack over core, on the real core ShopPrices.conf. The pack overlay changes only the
//! fields it sets in its file (IsVariableSetDirectly), so its food rule changes the buy price and
//! leaves the core sell price; the owner then changes the food buy price again.
// tier: WORLD
class LF_Test_ConfigPackOverlay : LF_Test
{
	static const ResourceName CORE = "{A547CE324ADDD178}Configs/Shop/ShopPrices.conf";

	override string GetName()
	{
		return "config/pack-overlay";
	}

	protected static LF_ShopPricesConfig LoadCore()
	{
		Resource resource = BaseContainerTools.LoadContainer(CORE);
		if (!resource || !resource.IsValid())
			return null;
		return LF_ShopPricesConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(resource.GetResource().ToBaseContainer()));
	}

	override void Run(LF_TestContext ctx)
	{
		LF_ConfigService.Reset();
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1}", "test");
		LF_ConfigService.SetPackSource(new LF_Test_ConfigPackSourceStub());

		LF_ShopPricesConfig core = LoadCore();
		ctx.NotNull(core, "core ShopPrices.conf loads");
		if (!core)
		{
			LF_ConfigService.Reset();
			return;
		}

		int coreRules = core.m_aPriceRules.Count();
		int coreExtras = core.m_aExtraItemPrefabs.Count();
		LF_ShopPriceRule coreFood = LF_Test_ConfigMergeFixture.Find(core, LF_Test_ConfigMergeFixture.FOOD);
		ctx.Equal(8, coreFood.m_iBuyPrice, "core food buy price");
		ctx.Equal(4, coreFood.m_iSellPrice, "core food sell price");

		// Pack over core.
		LF_ShopPricesConfig packed = LoadCore();
		packed.ApplyLayers(CORE);
		ctx.Equal(50, packed.m_iDefaultBuyPrice, "a default the pack file leaves out stays core");
		ctx.Equal(30, packed.m_iDefaultSellPrice, "a default the pack file sets wins over core");
		LF_ShopPriceRule food = LF_Test_ConfigMergeFixture.Find(packed, LF_Test_ConfigMergeFixture.FOOD);
		ctx.Equal(9, food.m_iBuyPrice, "the pack's food buy price wins over core");
		ctx.Equal(4, food.m_iSellPrice, "a rule field the pack file leaves out stays core");
		ctx.Equal(coreRules + 1, packed.m_aPriceRules.Count(), "the pack's new rule is appended");
		if (packed.m_aPriceRules.Count() != coreRules + 1)
		{
			LF_ConfigService.Reset();
			return;
		}

		LF_ShopPriceRule packOnly = packed.m_aPriceRules[coreRules];
		ctx.EqualStr("Prefabs/Items/LF_PackOnly/", packOnly.m_sFind, "the appended rule is the pack's");
		ctx.Equal(3, packOnly.m_iSellPrice, "the pack's new rule keeps its sell price");
		ctx.Equal(coreExtras, packed.m_aExtraItemPrefabs.Count(), "a pack extra prefab already in core is not doubled");

		// Owner over pack over core.
		LF_ConfigService.LoadOwnerString("{\"shops\":{\"defaultBuyPrice\":70,\"priceRules\":{\"Prefabs/Items/Food/\":{\"buyPrice\":11},\"Prefabs/Items/LF_PackOnly/\":{\"sellPrice\":2}}}}", "test");
		LF_ShopPricesConfig layered = LoadCore();
		layered.ApplyLayers(CORE);
		ctx.Equal(70, layered.m_iDefaultBuyPrice, "owner default wins over core");
		ctx.Equal(30, layered.m_iDefaultSellPrice, "pack default stays where the owner is silent");
		food = LF_Test_ConfigMergeFixture.Find(layered, LF_Test_ConfigMergeFixture.FOOD);
		ctx.Equal(11, food.m_iBuyPrice, "owner food buy price wins over pack and core");
		ctx.Equal(4, food.m_iSellPrice, "core food sell price stays where pack and owner are silent");
		packOnly = LF_Test_ConfigMergeFixture.Find(layered, "Prefabs/Items/LF_PackOnly/");
		ctx.Equal(7, packOnly.m_iBuyPrice, "the pack rule's buy price stays where the owner is silent");
		ctx.Equal(2, packOnly.m_iSellPrice, "the owner overrides a rule the pack added");
		ctx.Equal(coreRules + 1, layered.m_aPriceRules.Count(), "the owner adds no rule for keys that exist");

		LF_ConfigService.Reset();
	}
};
