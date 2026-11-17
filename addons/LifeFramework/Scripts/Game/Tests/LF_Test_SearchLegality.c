// red-proof: RED-OK 2026-11-16 `tools/cli test --no-build --red LF_Test_SearchLegality`: with the seizure skipped it failed `- the unlicensed pistol is seized (expected 1, got 0)`, `- one offence is filed (expected 1, got 0)`, `- the offence reached the crime reports (expected 1, got 0)`. Green without the hook.
// tier: WORLD
//! catalogue/search-legality (#1764): a police search reads item legality. A pistol carried without
//! FIREARM_PERMIT is flagged, seized and filed as #LF-Offence_UnlicensedItem; the same pistol with
//! the permit is left alone and nothing is filed.
class LF_Test_SearchLegality : LF_Test
{
	protected static const string UID = "search-legality-a";

	override string GetName()
	{
		return "catalogue/search-legality";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_ItemCatalogueConfig config = LF_Test_ItemCatalogue.MakeConfig();
		LF_WantedReports.ClearFor(UID);

		IEntity suspect = LF_TestCharacterFixture.Spawn(ctx);
		if (!suspect)
			return;

		InventoryStorageManagerComponent inventory = LF_InventoryUtils.GetResponsibleStorageManager(suspect);
		ctx.True(inventory && LF_InventoryUtils.AddItem(inventory, LF_Test_ItemCatalogue.PISTOL), "the suspect carries a pistol");

		// With the permit: the pistol is lawful, nothing is flagged, seized or filed.
		LF_Test_CatalogueBuyer licensed = new LF_Test_CatalogueBuyer();
		licensed.m_aLicences.Insert(LF_ELicenseType.FIREARM_PERMIT);
		array<string> offences = {};
		ctx.Equal(0, LF_ItemSearch.SearchAndSeize(suspect, UID, licensed, config, offences), "a permit holder's pistol is not seized");
		ctx.Equal(0, offences.Count(), "a permit holder files nothing");
		ctx.True(LF_TestCharacterFixture.HasItem(suspect, LF_Test_ItemCatalogue.PISTOL), "the permit holder keeps the pistol");
		ctx.Equal(0, LF_WantedReports.CountFor(UID), "no report against a permit holder");

		// An on-duty officer's issued M9 is not an offence, though they hold no civilian permit.
		LF_Test_CatalogueBuyer officer = new LF_Test_CatalogueBuyer();
		officer.m_iPoliceRank = 0;
		ctx.Equal(0, LF_ItemSearch.SearchAndSeize(suspect, UID, officer, config, offences), "an on-duty officer's service pistol is not seized");
		ctx.True(LF_TestCharacterFixture.HasItem(suspect, LF_Test_ItemCatalogue.PISTOL), "the officer keeps the service pistol");

		// Without the permit: the pistol is listed, seized and the offence filed once.
		LF_Test_CatalogueBuyer civilian = new LF_Test_CatalogueBuyer();
		array<IEntity> carried = {};
		LF_ItemSearch.CarriedItems(suspect, carried);
		array<IEntity> flagged = {};
		array<string> flaggedOffences = {};
		LF_ItemSearch.Unlawful(config, carried, civilian, flagged, flaggedOffences);
		ctx.Equal(1, flagged.Count(), "the search lists the unlicensed pistol");
		if (flaggedOffences.Count() == 1)
			ctx.EqualStr(LF_ItemCatalogue.OFFENCE_UNLICENSED, flaggedOffences[0], "the listing names the unlicensed offence");

		int seized = 0;
		if (!RedProof())
			seized = LF_ItemSearch.SearchAndSeize(suspect, UID, civilian, config, offences);
		ctx.Equal(1, seized, "the unlicensed pistol is seized");
		ctx.False(LF_TestCharacterFixture.HasItem(suspect, LF_Test_ItemCatalogue.PISTOL), "the pistol is gone from the suspect");
		ctx.Equal(1, offences.Count(), "one offence is filed");
		if (offences.Count() == 1)
			ctx.EqualStr(LF_ItemCatalogue.OFFENCE_UNLICENSED, offences[0], "the filed offence is the unlicensed one");

		ctx.Equal(1, LF_WantedReports.CountFor(UID), "the offence reached the crime reports");
		array<LF_WantedReportRecord> reports = {};
		LF_WantedReports.CollectNewestFirst(reports);
		string filed;
		foreach (LF_WantedReportRecord report : reports)
		{
			if (report && report.m_sPlayerUid == UID)
			{
				filed = report.m_sOffence;
				break;
			}
		}
		ctx.EqualStr(LF_ItemCatalogue.OFFENCE_UNLICENSED, filed, "the report names the unlicensed offence");

		LF_WantedReports.ClearFor(UID);
		LF_TestCharacterFixture.Delete(suspect);
	}
}
