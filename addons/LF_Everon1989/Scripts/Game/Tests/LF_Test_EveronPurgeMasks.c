// red-proof: RED-OK everon/purge-masks (fails with the hook on, 11 assertions), observed 2026-11-18 with `tools\cli test --no-build --red LF_Test_PurgeMasksWorn,LF_Test_EveronPurgeMasks`: with the hook on the black market lookup is made on the clothing rack, `the black market offers the doll mask` and the hockey mask. Hook off: green.
//! The black market stocks the two face masks beyond the balaclava (purge-masks): a smiling doll mask and a
//! cracked hockey mask. Under the Everon 1989 header (the pack leg of `tools\cli test`), the pack's clothing
//! catalogue holds both rows with `concealsFace`, so `LF_ItemCatalogue.Concealment` over a worn list holding
//! one returns face true and build false, the `black_market` outlet offers each behind the black market
//! licence at 40 to 60, the clothing rack does not, and carrying one is illegal (a search seizes it).
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronPurgeMasks : LF_Test
{
	protected static const ResourceName DOLL_MASK = "{C243CFB581136C99}Prefabs/Characters/HeadGear/LF/DollMask.et";
	protected static const ResourceName HOCKEY_MASK = "{F049C702E4FE8063}Prefabs/Characters/HeadGear/LF/HockeyMask.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/purge-masks";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// The leg boots under the pack header; layer the catalogue again so the clothing overlay applies.
		LF_ItemCatalogueConfig.Reload();
		LF_ItemCatalogueConfig config = LF_ItemCatalogueConfig.Get();
		ctx.NotNull(config, "the item catalogue loads under the pack header");
		if (!config)
			return;

		CheckMask(ctx, config, DOLL_MASK, "the doll mask");
		CheckMask(ctx, config, HOCKEY_MASK, "the hockey mask");

		LF_ItemCatalogueConfig.Reload();
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckMask(LF_TestContext ctx, LF_ItemCatalogueConfig config, ResourceName prefab, string label)
	{
		bool face;
		bool build;
		array<ResourceName> masked = {prefab};
		LF_ItemCatalogue.Concealment(config, masked, face, build);
		ctx.True(face, label + " hides the face when worn");
		ctx.False(build, label + " does not hide the build");

		string outlet = "black_market";
		if (RedProof())
			outlet = "clothing_rack";

		LF_CatalogueItem item;
		LF_OutletOffer offer = LF_ItemCatalogue.OfferFor(config, outlet, prefab, item);
		ctx.NotNull(offer, "the black market offers " + label);
		if (offer)
			ctx.EqualStr("CRIMINAL_BLACKMARKET", offer.m_sLicence, label + " sits behind the black market licence");

		if (item)
		{
			ctx.EqualStr("illegal", item.m_sLegality, "carrying " + label + " is illegal");
			ctx.True(item.m_iPrice >= 40 && item.m_iPrice <= 60, label + " costs 40 to 60");
		}

		LF_CatalogueItem rackItem;
		ctx.True(LF_ItemCatalogue.OfferFor(config, "clothing_rack", prefab, rackItem) == null, "the clothing rack does not sell " + label);
	}
}
