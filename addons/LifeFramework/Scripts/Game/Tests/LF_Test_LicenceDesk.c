// red-proof: RED-OK licence/desk (#1995), observed 2026-11-16 with `tools\cli test --no-build --red LF_Test_LicenceDesk,LF_Test_E2E_LicenceDesk`:
// with no conviction stamped, `- a recently convicted applicant is refused (expected 6, got 1)`,
// `- the conviction refusal took no cash (expected 500, got 200)` and `- a conviction since the application holds the permit back at collection (expected 6, got 2)`. Hook off: green.
// red-proof: RED-OK licence/desk, observed 2026-11-16 with `tools\cli test --no-build --red LF_Test_LicenceDesk`:
// with the in-game day not advanced, `- a day later the permit is issued (expected 0, got 2)` and
// `- with the desk's permit the gun counter sells (expected 0, got 13)`. Hook off: green.
// tier: WORLD
//! The licence desk (#1766) on the server path: refused while wanted, with an open crime report, with a
//! conviction inside the look-back (#1995, at application and again at collection), or
//! short of the fee, each naming the reason and taking nothing; an application takes the fee once
//! and waits one in-game day; then the permit is issued and the annex gun counter sells the pistol.
//! An on-duty officer's revoke takes it again (a non-officer is refused), and the counter refuses.
class LF_Test_LicenceDesk : LF_Test
{
	protected static const string UID = "lf-test-licence-desk";
	protected static const ResourceName PISTOL = "{1353C6EAD1DCFE43}Prefabs/Weapons/Handguns/M9/Handgun_M9.et";
	protected static const ResourceName DESK = "{0970D6C0210F9612}Prefabs/Props/Commercial/LF_LicenceDesk.et";
	protected static const int FEE = 300;
	protected static const int T0 = 14400000;	// in-game minute 10000 days after 1970-01-01
	protected static const int CONVICTION_DAY = 726000;	// an in-game day (LF_ConvictionRecord.DayOrdinal)
	protected static const int LOOK_BACK = 7;	// the permitConvictionDays under test

	override string GetName()
	{
		return "licence/desk";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_ConfigService.Reset();
		LF_ConfigStream.Reset();
		LF_ItemCatalogueConfig.Reload();
		LF_Progression.Forget(UID);
		LF_LicencesConfig.SetDeskForTest(FEE, 1, LOOK_BACK);
		LF_LicenceDesk.SetNowForTest(T0);
		array<ref LF_ConvictionRow> convictions = LF_ConvictionRecord.ExportAll();
		LF_ConvictionRecord.ApplyAll(null);
		LF_ConvictionRecord.SetTodayForTest(CONVICTION_DAY);

		ctx.Equal(0, LF_LicenceDesk.DaysFromCivil(1970, 1, 1), "day 0 is 1970-01-01");
		ctx.Equal(7305, LF_LicenceDesk.DaysFromCivil(1990, 1, 1), "1990-01-01 is day 7305");
		ctx.Equal(7364, LF_LicenceDesk.DaysFromCivil(1990, 3, 1), "the day after 1990-02-28 is 1990-03-01");

		Resource deskResource = Resource.Load(DESK);
		ctx.True(deskResource && deskResource.IsValid(), "the licence desk prefab loads");

		IEntity buyer = LF_TestCharacterFixture.Spawn(ctx);
		if (!buyer)
			return;

		LF_LicenseManagerComponent licences = LF_Component<LF_LicenseManagerComponent>.Find(buyer);
		ctx.NotNull(licences, "the test character has a licence manager");
		LF_PlayerAccount account = LF_PlayerAccountManager.GetOrCreate(UID);
		LF_EntityCashWallet wallet = new LF_EntityCashWallet(buyer, LF_LicenceDesk.LEDGER_REASON, string.Empty);
		int left;

		LF_MoneyUtils.AddCash(buyer, 500);
		account.SetWantedLevel(2);
		ctx.Equal(LF_ELicenceDesk.WANTED, LF_LicenceDesk.VisitFor(buyer, UID, wallet, left), "a wanted player is refused");
		ctx.EqualStr("#LF-LicenceDesk_Wanted", LF_LicenceDesk.AnswerKey(LF_ELicenceDesk.WANTED), "the refusal names the wanted level");
		account.SetWantedLevel(0);

		array<ref LF_WantedReportRecord> saved = LF_WantedReports.ExportAll();
		array<ref LF_WantedReportRecord> withMine = LF_WantedReports.ExportAll();
		LF_WantedReportRecord report = new LF_WantedReportRecord();
		report.m_sPlayerUid = UID;
		report.m_sOffence = LF_WantedReports.OFFENCE_ROBBERY;
		withMine.Insert(report);
		LF_WantedReports.ApplyAll(withMine);
		ctx.Equal(LF_ELicenceDesk.OPEN_CRIME, LF_LicenceDesk.VisitFor(buyer, UID, wallet, left), "an open crime report refuses the application");
		LF_WantedReports.ApplyAll(saved);
		ctx.Equal(500, LF_MoneyUtils.GetCash(buyer), "the refusals took no cash");

		if (!RedProof())
			LF_ConvictionRecord.Record(UID);

		ctx.Equal(LF_ELicenceDesk.CONVICTED, LF_LicenceDesk.VisitFor(buyer, UID, wallet, left), "a recently convicted applicant is refused");
		ctx.EqualStr("#LF-LicenceDesk_Convicted", LF_LicenceDesk.AnswerKey(LF_ELicenceDesk.CONVICTED), "the refusal names the conviction");
		ctx.Equal(500, LF_MoneyUtils.GetCash(buyer), "the conviction refusal took no cash");
		ctx.Equal(-1, LF_LicenceDesk.DueMinutes(UID), "and opened no application");
		LF_LicencesConfig.SetDeskForTest(FEE, 1, 0);
		ctx.True(LF_LicenceDesk.RecordRefusal(UID) == LF_ELicenceDesk.ISSUED, "a look-back of 0 days turns the conviction check off");
		LF_LicencesConfig.SetDeskForTest(FEE, 1, LOOK_BACK);
		LF_ConvictionRecord.SetTodayForTest(CONVICTION_DAY + LOOK_BACK);
		ctx.True(LF_LicenceDesk.RecordRefusal(UID) == LF_ELicenceDesk.ISSUED, "once the look-back has passed the conviction no longer counts");

		LF_LicencesConfig.SetDeskForTest(1000, 1, LOOK_BACK);
		ctx.Equal(LF_ELicenceDesk.NOT_ENOUGH_CASH, LF_LicenceDesk.VisitFor(buyer, UID, wallet, left), "short of the fee, refused");
		LF_LicencesConfig.SetDeskForTest(FEE, 1, LOOK_BACK);
		ctx.Equal(500, LF_MoneyUtils.GetCash(buyer), "a short fee takes nothing");

		ctx.Equal(LF_ELicenceDesk.APPLIED, LF_LicenceDesk.VisitFor(buyer, UID, wallet, left), "a clean player with the fee applies");
		ctx.Equal(1440, left, "the permit is due in one in-game day");
		ctx.Equal(500 - FEE, LF_MoneyUtils.GetCash(buyer), "the fee is taken");
		ctx.Equal(LF_ELicenceDesk.WAITING, LF_LicenceDesk.VisitFor(buyer, UID, wallet, left), "before the day is up the desk says wait");
		ctx.Equal(1440, left, "with the full day left");
		ctx.False(LF_LicenceDesk.Holds(buyer, UID), "no permit while waiting");
		LF_LicenceDesk.SetNowForTest(T0 - 5000);
		ctx.Equal(LF_ELicenceDesk.WAITING, LF_LicenceDesk.VisitFor(buyer, UID, wallet, left), "a clock set back still says wait");
		ctx.Equal(1440, left, "but never longer than one term");
		ctx.Equal(500 - FEE, LF_MoneyUtils.GetCash(buyer), "and charges nothing again");
		LF_Progression.NoteFlag(UID + "-bad", LF_LicenceDesk.DUE_FLAG + "soon");
		ctx.Equal(-1, LF_LicenceDesk.DueMinutes(UID + "-bad"), "an unreadable due minute is no application");
		LF_Progression.Forget(UID + "-bad");

		if (!RedProof())
			LF_LicenceDesk.SetNowForTest(T0 + 1440);

		// A conviction after the application holds the permit back at collection, and lets it go once it ages out.
		if (!RedProof())
			LF_ConvictionRecord.Record(UID);

		ctx.Equal(LF_ELicenceDesk.CONVICTED, LF_LicenceDesk.VisitFor(buyer, UID, wallet, left), "a conviction since the application holds the permit back at collection");
		ctx.False(LF_LicenceDesk.Holds(buyer, UID), "so no permit is issued");
		ctx.True(LF_LicenceDesk.DueMinutes(UID) >= 0, "the application stays open");
		ctx.Equal(500 - FEE, LF_MoneyUtils.GetCash(buyer), "and charges nothing again");
		LF_ConvictionRecord.SetTodayForTest(CONVICTION_DAY + 2 * LOOK_BACK);

		ctx.Equal(LF_ELicenceDesk.ISSUED, LF_LicenceDesk.VisitFor(buyer, UID, wallet, left), "a day later the permit is issued");
		ctx.True(LF_LicenceDesk.Holds(buyer, UID), "the player holds the permit");
		ctx.Equal(500 - FEE, LF_MoneyUtils.GetCash(buyer), "the fee was taken once");
		ctx.Equal(-1, LF_LicenceDesk.DueMinutes(UID), "the application is closed");
		array<LF_ELicenseType> stored = {};
		LF_Progression.GetLicenses(UID, stored);
		ctx.True(stored.Contains(LF_ELicenseType.FIREARM_PERMIT), "the saved licence list holds the permit");
		ctx.Equal(LF_ELicenceDesk.ALREADY_HELD, LF_LicenceDesk.VisitFor(buyer, UID, wallet, left), "a holder is not charged again");

		IEntity counter = GetGame().GetWorld().FindEntityByName("Annex_GunCounter");
		LF_ShopComponent shop = LF_Component<LF_ShopComponent>.Find(counter);
		ctx.NotNull(shop, "the annex gun counter resolves");
		int charged;
		string missing;
		if (shop)
		{
			LF_MoneyUtils.AddCash(buyer, shop.GetBuyPrice(PISTOL));
			ctx.Equal(LF_EShopRefusal.NONE, shop.Buy(PISTOL, 1, buyer, charged, missing), "with the desk's permit the gun counter sells");
			ctx.Equal(1, LF_InventoryUtils.GetAmount(buyer, PISTOL), "the buyer holds the pistol");
		}

		ctx.False(LF_LicenceDesk.Revoke(buyer, buyer), "a player who is not an on-duty officer cannot revoke");
		ctx.True(LF_LicenceDesk.Holds(buyer, UID), "so the permit stays");
		ctx.True(LF_LicenceDesk.RevokeFor(buyer, UID, "lf-test-officer"), "the officer's revoke takes the permit");
		ctx.False(LF_LicenceDesk.Holds(buyer, UID), "the permit is gone from the body");
		LF_Progression.GetLicenses(UID, stored);
		ctx.False(stored.Contains(LF_ELicenseType.FIREARM_PERMIT), "and from the saved list");
		if (shop)
		{
			LF_MoneyUtils.AddCash(buyer, shop.GetBuyPrice(PISTOL));
			ctx.Equal(LF_EShopRefusal.NEEDS_LICENCE, shop.Buy(PISTOL, 1, buyer, charged, missing), "after the revoke the counter refuses");
		}

		LF_TestCharacterFixture.Delete(buyer);
		LF_Progression.Forget(UID);
		LF_LicenceDesk.SetNowForTest(-1);
		LF_LicencesConfig.SetDeskForTest(-1, -1);
		LF_ConvictionRecord.SetTodayForTest(-1);
		LF_ConvictionRecord.ApplyAll(convictions);
	}
}
