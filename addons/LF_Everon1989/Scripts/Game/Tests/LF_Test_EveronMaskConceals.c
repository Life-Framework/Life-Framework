// red-proof: RED-OK everon/mask-conceals (fails with the hook on, 7 assertions), observed 2026-11-18 with
// `toolscli test --no-build --red LF_Test_EveronMaskConceals`: looking the offer up on the clothing rack, `the black market offers the balaclava`. Hook off: green.
//! The 1989 black market sells a balaclava that hides the face (#2439, Refs #1997, #990). Under the Everon 1989
//! header (the pack leg of `tools\cli test`), the pack's clothing catalogue holds the `balaclava` row with
//! `concealsFace`, so `LF_ItemCatalogue.Concealment` over a worn list holding it returns face true (a plain
//! hat does not), the `black_market` outlet offers it behind the black market licence, and the clothing
//! rack does not. It is illegal to carry: a search seizes it.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronMaskConceals : LF_Test
{
	protected static const ResourceName BALACLAVA = "{F1971C6519B5DC9F}Prefabs/Characters/HeadGear/LF/Balaclava.et";
	protected static const ResourceName KNITTED_HAT = "{B9687B8829272E14}Prefabs/Characters/HeadGear/Hat_Knitted_01/Hat_Knitted_01_brown.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/mask-conceals";
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

		bool face;
		bool build;
		array<ResourceName> masked = {BALACLAVA};
		LF_ItemCatalogue.Concealment(config, masked, face, build);
		ctx.True(face, "a worn balaclava hides the face");
		ctx.False(build, "and not the build");

		array<ResourceName> hat = {KNITTED_HAT};
		LF_ItemCatalogue.Concealment(config, hat, face, build);
		ctx.False(face, "a knitted hat leaves the face showing");

		string outlet = "black_market";
		if (RedProof())
			outlet = "clothing_rack";

		LF_CatalogueItem item;
		LF_OutletOffer offer = LF_ItemCatalogue.OfferFor(config, outlet, BALACLAVA, item);
		ctx.NotNull(offer, "the black market offers the balaclava");
		if (offer)
			ctx.EqualStr("CRIMINAL_BLACKMARKET", offer.m_sLicence, "behind the black market licence");

		LF_CatalogueItem rackItem;
		ctx.True(LF_ItemCatalogue.OfferFor(config, "clothing_rack", BALACLAVA, rackItem) == null, "the clothing rack does not sell it");

		if (item)
			ctx.EqualStr("illegal", item.m_sLegality, "carrying it is illegal");

		LF_ItemCatalogueConfig.Reload();
	}
}
