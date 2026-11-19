// red-proof: Observed 2026-11-19 with `--no-build --red`: `RED-OK everon/farm-supply (3 assertions)`, the offer lookup skipped.
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

		ctx.Equal(1, offers.Count(), "the farm supply counter has one offer");
		if (offers.IsEmpty())
			return;

		LF_CatalogueItem item = LF_ItemCatalogue.Item(catalogue, offers[0].m_sItem);
		ctx.NotNull(item, "the offer names a catalogue item");
		if (!item)
			return;

		ctx.Equal(1200, item.m_iPrice, "the reaper costs $1,200");
		ctx.True(LF_OwnedImplements.IdForPrefab(item.m_Prefab) == LF_OwnedImplements.ID_REAPER, "the offer sells the reaper implement");
	}
}
