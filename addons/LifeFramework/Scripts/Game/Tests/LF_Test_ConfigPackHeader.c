// red-proof: in LF_MissionConfigPackSource.GetOverlays insert the overlays in reverse order and
// config/pack-header fails "the later overlay in the header wins over the earlier one (expected
// 10, got 9)" (observed 2026-11-12).

//------------------------------------------------------------------------------------------------
//! D4 (b): the mission header lists the pack overlays and the config service merges them. A test
//! header (Configs/Tests/LF_Test_PackHeader.conf, no pack resources) lists two shops overlays;
//! precedence is owner over later overlay over earlier overlay over core.
// tier: WORLD
class LF_Test_ConfigPackHeader : LF_Test
{
	static const ResourceName HEADER = "{5D83B0A6C417E92F}Configs/Tests/LF_Test_PackHeader.conf";

	override string GetName()
	{
		return "config/pack-header";
	}

	protected static LF_MissionHeader LoadHeader()
	{
		Resource resource = BaseContainerTools.LoadContainer(HEADER);
		if (!resource || !resource.IsValid())
			return null;
		return LF_MissionHeader.Cast(BaseContainerTools.CreateInstanceFromContainer(resource.GetResource().ToBaseContainer()));
	}

	override void Run(LF_TestContext ctx)
	{
		LF_ConfigService.Reset();
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1}", "test");

		LF_MissionHeader header = LoadHeader();
		ctx.NotNull(header, "the test mission header loads");
		if (!header)
		{
			LF_ConfigService.Reset();
			return;
		}
		ctx.Equal(2, header.m_aLF_ConfigOverlays.Count(), "the header lists two overlays");

		LF_MissionConfigPackSource source = new LF_MissionConfigPackSource();
		source.SetHeader(header);
		ctx.EqualStr("mission-header", source.GetName(), "the header source names itself");
		LF_ConfigService.SetPackSource(source);

		array<ResourceName> listed = {};
		LF_ConfigService.GetPackOverlays(LF_Test_ConfigPackOverlay.CORE, listed);
		ctx.Equal(2, listed.Count(), "the source supplies the header's overlays");

		// Overlays over core.
		Resource coreResource = BaseContainerTools.LoadContainer(LF_Test_ConfigPackOverlay.CORE);
		LF_ShopPricesConfig layered = LF_ShopPricesConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(coreResource.GetResource().ToBaseContainer()));
		layered.ApplyLayers(LF_Test_ConfigPackOverlay.CORE);
		LF_ShopPriceRule food = LF_Test_ConfigMergeFixture.Find(layered, LF_Test_ConfigMergeFixture.FOOD);
		ctx.Equal(10, food.m_iBuyPrice, "the later overlay in the header wins over the earlier one");
		ctx.Equal(4, food.m_iSellPrice, "a field neither overlay sets stays core");
		ctx.Equal(55, layered.m_iDefaultBuyPrice, "a default only the second overlay sets");
		ctx.Equal(30, layered.m_iDefaultSellPrice, "a default only the first overlay sets survives the second");
		LF_ShopPriceRule packOnly = LF_Test_ConfigMergeFixture.Find(layered, "Prefabs/Items/LF_PackOnly/");
		ctx.NotNull(packOnly, "the first overlay's new rule is appended");

		// Owner over overlays.
		LF_ConfigService.LoadOwnerString("{\"shops\":{\"defaultBuyPrice\":70,\"priceRules\":{\"Prefabs/Items/Food/\":{\"buyPrice\":11}}}}", "test");
		Resource coreAgain = BaseContainerTools.LoadContainer(LF_Test_ConfigPackOverlay.CORE);
		LF_ShopPricesConfig ownerLayered = LF_ShopPricesConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(coreAgain.GetResource().ToBaseContainer()));
		ownerLayered.ApplyLayers(LF_Test_ConfigPackOverlay.CORE);
		food = LF_Test_ConfigMergeFixture.Find(ownerLayered, LF_Test_ConfigMergeFixture.FOOD);
		ctx.Equal(11, food.m_iBuyPrice, "the owner wins over both overlays");
		ctx.Equal(70, ownerLayered.m_iDefaultBuyPrice, "the owner default wins over the overlays");
		ctx.Equal(30, ownerLayered.m_iDefaultSellPrice, "an overlay default stays where the owner is silent");

		// No header, no overlays: the default behaviour of a mission that lists none.
		source.SetHeader(new LF_MissionHeader());
		listed.Clear();
		LF_ConfigService.GetPackOverlays(LF_Test_ConfigPackOverlay.CORE, listed);
		ctx.Equal(0, listed.Count(), "a header with no overlays supplies none");

		LF_ConfigService.Reset();
	}
};
