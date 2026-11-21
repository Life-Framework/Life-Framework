// red-proof: Observed 2026-11-19: the old `Equal(1, offers.Count())` failed on main once the beehive kit (#3448) joined the counter (main guard, 0fe3cdd3); the reaper lookup by item id passes with it and fails under `--red` (the offer lookup skipped).
//! everon/farm-supply (#3295): under the Everon 1989 header (the pack leg), the farm_supply outlet sells the reaper
//! for $1,200 from its own file under Configs/Catalogue/LF_Catalogue_Kit_Everon1989/, and the offer's prefab is
//! the implement the rack hands out (LF_OwnedImplements reads it as the 'reaper').
[LF_TestInfo(LF_TestTier.LOGIC, pack: true)]
class LF_Test_EveronFarmSupply : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/farm-supply";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ItemCatalogueConfig catalogue = LF_ItemCatalogueConfig.Get();
		ctx.NotNull(catalogue, "the catalogue loads");
		if (!catalogue)
			return;

		array<LF_OutletOffer> offers = {};
		if (!RedProof())
			LF_ItemCatalogue.Offers(catalogue, "farm_supply", offers);

		// The counter also sells other stock (the beehive kit, #3448); this test owns only the reaper offer.
		LF_CatalogueItem item = null;
		foreach (LF_OutletOffer offer : offers)
		{
			if (offer && offer.m_sItem == "farm_reaper")
				item = LF_ItemCatalogue.Item(catalogue, offer.m_sItem);
		}

		ctx.NotNull(item, "the farm supply counter offers the reaper");
		if (!item)
			return;

		ctx.Equal(1200, item.m_iPrice, "the reaper costs $1,200");
		ctx.True(LF_OwnedImplements.IdForPrefab(item.m_Prefab) == LF_OwnedImplements.ID_REAPER, "the offer sells the reaper implement");
	}
}
