// red-proof: RED-OK 2026-11-16 `tools/cli test --no-build --only LF_Test_ServicePistolLawful` on the unfixed code (SIDEARM still the plain M9) failed `- the duty sidearm is its own prefab, not the plain M9`, `- the service pistol is not listed as an offence (expected 0, got 1)`, `- an off-duty officer's service pistol is not seized (expected 0, got 1)`. Green after the service prefab and its catalogue row.
// tier: WORLD
//! catalogue/service-pistol-lawful (#2096): the duty kit's service pistol is lawful police issue. An
//! off-duty officer searched with it keeps it and nothing is filed; the plain catalogue M9 carried
//! without a permit is still seized as an unlicensed item.
class LF_Test_ServicePistolLawful : LF_Test
{
	protected static const string UID = "service-pistol-lawful-a";

	override string GetName()
	{
		return "catalogue/service-pistol-lawful";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_ItemCatalogueConfig config = LF_ItemCatalogueConfig.Get();
		ctx.NotNull(config, "the shipped item catalogue loads");
		if (!config)
			return;

		ctx.NotNull(LF_ItemCatalogue.ItemByPrefab(config, LF_PoliceRanks.SIDEARM), "the duty kit's sidearm has a catalogue row");
		ctx.False(LF_Utils.SamePrefab(LF_PoliceRanks.SIDEARM, LF_Test_ItemCatalogue.PISTOL), "the duty sidearm is its own prefab, not the plain M9");

		LF_WantedReports.ClearFor(UID);
		IEntity suspect = LF_TestCharacterFixture.Spawn(ctx);
		if (!suspect)
			return;

		InventoryStorageManagerComponent inventory = LF_InventoryUtils.GetResponsibleStorageManager(suspect);
		ctx.True(inventory && LF_InventoryUtils.AddItem(inventory, LF_PoliceRanks.SIDEARM), "the suspect carries the service pistol");

		// An off-duty officer: no job rank, no civilian permit. The service pistol is lawful anyway.
		LF_Test_CatalogueBuyer offDuty = new LF_Test_CatalogueBuyer();
		array<IEntity> carried = {};
		LF_ItemSearch.CarriedItems(suspect, carried);
		array<IEntity> flagged = {};
		array<string> flaggedOffences = {};
		LF_ItemSearch.Unlawful(config, carried, offDuty, flagged, flaggedOffences);
		ctx.Equal(0, flagged.Count(), "the service pistol is not listed as an offence");

		array<string> offences = {};
		ctx.Equal(0, LF_ItemSearch.SearchAndSeize(suspect, UID, offDuty, config, offences), "an off-duty officer's service pistol is not seized");
		ctx.Equal(0, offences.Count(), "nothing is filed for the service pistol");
		ctx.True(LF_TestCharacterFixture.HasItem(suspect, LF_PoliceRanks.SIDEARM), "the officer keeps the service pistol");
		ctx.Equal(0, LF_WantedReports.CountFor(UID), "no report against the officer");
		LF_TestCharacterFixture.Delete(suspect);

		// The plain catalogue M9 without the permit is still an offence.
		IEntity civilian = LF_TestCharacterFixture.Spawn(ctx);
		if (!civilian)
			return;

		InventoryStorageManagerComponent civilianInventory = LF_InventoryUtils.GetResponsibleStorageManager(civilian);
		ctx.True(civilianInventory && LF_InventoryUtils.AddItem(civilianInventory, LF_Test_ItemCatalogue.PISTOL), "the civilian carries a plain M9");
		LF_Test_CatalogueBuyer unlicensed = new LF_Test_CatalogueBuyer();
		int seized = 0;
		if (!RedProof())
			seized = LF_ItemSearch.SearchAndSeize(civilian, UID, unlicensed, config, offences);
		ctx.Equal(1, seized, "the plain M9 without a permit is seized");
		ctx.False(LF_TestCharacterFixture.HasItem(civilian, LF_Test_ItemCatalogue.PISTOL), "the plain M9 is gone");
		ctx.Equal(1, LF_WantedReports.CountFor(UID), "the plain M9 was filed");

		LF_WantedReports.ClearFor(UID);
		LF_TestCharacterFixture.Delete(civilian);
	}
}
