// red-proof: RED-OK licence/counter, observed 2026-11-18 with `tools\cli test --no-build --red LF_Test_LicenceCounter` (batched with the pack's gun store permit test):
// with no licence row put on the outlet, `- the counter shows one licence row (expected 1, got 0)`,
// `- a clean buyer with the fee applies at the counter (expected 1, got 10)` and `- the fee is taken (expected 200, got 500)`. Hook off: green.
// tier: WORLD
//! The firearm permit over a shop counter (maintainer 2026-11-17: "get the firearm permit in the gun
//! store"). A catalogue licence row (LF_OutletOffer.m_sGrantsLicence) on the annex gun counter's outlet
//! runs the licence office's visit: a counter without the row refuses, a wanted buyer or one short of
//! the fee is refused and charged nothing, an application takes the fee once and waits, collection a
//! day later issues the permit and the same counter then sells the pistol. With the row the licence
//! desk no longer takes permit applications, and a row for a licence the office does not issue is
//! dropped at load.
class LF_Test_LicenceCounter : LF_Test
{
	protected static const string UID = "lf-test-licence-counter";
	protected static const string OUTLET = "gun_counter";
	protected static const ResourceName PISTOL = "{1353C6EAD1DCFE43}Prefabs/Weapons/Handguns/M9/Handgun_M9.et";
	protected static const int FEE = 300;
	protected static const int T0 = 14400000;	// in-game minute 10000 days after 1970-01-01

	override string GetName()
	{
		return "licence/counter";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_ConfigService.Reset();
		LF_ConfigStream.Reset();
		LF_ItemCatalogueConfig.Reload();
		LF_Progression.Forget(UID);
		LF_LicencesConfig.SetDeskForTest(FEE, 1, 0);
		LF_LicenceDesk.SetNowForTest(T0);

		LF_ItemCatalogueConfig config = LF_ItemCatalogueConfig.Get();
		ctx.NotNull(config, "the item catalogue loads");
		IEntity counter = GetGame().GetWorld().FindEntityByName("Annex_GunCounter");
		LF_ShopComponent shop = LF_Component<LF_ShopComponent>.Find(counter);
		ctx.NotNull(shop, "the annex gun counter resolves");
		if (!config || !shop)
			return;

		ctx.EqualStr(OUTLET, shop.GetOutletId(), "the annex counter is the gun_counter outlet");
		IEntity buyer = LF_TestCharacterFixture.Spawn(ctx);
		if (!buyer)
			return;

		LF_EntityCashWallet wallet = new LF_EntityCashWallet(buyer, LF_LicenceDesk.LEDGER_REASON, string.Empty);
		LF_ELicenceDesk outcome;
		int left;
		LF_MoneyUtils.AddCash(buyer, 500);

		// The core ships no licence row: the counter refuses and the desk keeps the permit.
		ctx.True(LF_LicenceDesk.DeskIssuesPermit(), "with no licence row the desk takes permit applications");
		ctx.Equal(LF_EShopRefusal.NOT_SOLD_HERE, shop.ApplyForLicenceFor(buyer, UID, wallet, LF_ELicenseType.FIREARM_PERMIT, outcome, left), "a counter with no licence row refuses");

		// A row for a licence the office does not issue is dropped at load.
		LF_OutletOffer bogus = LF_OutletOffer.Make(OUTLET, "police_access");
		bogus.m_sGrantsLicence = "POLICE_ACCESS";
		config.m_aOffers.Insert(bogus);
		config.Check(false);
		ctx.True(bogus.m_bRemoved, "a row granting a licence the office does not issue is removed");

		LF_OutletOffer row = LF_OutletOffer.Make(OUTLET, "firearm_permit");
		row.m_sGrantsLicence = "FIREARM_PERMIT";
		if (!RedProof())
			config.m_aOffers.Insert(row);

		config.Check(false);
		ctx.False(row.m_bRemoved, "the firearm permit row survives the load checks");
		array<LF_OutletOffer> rows = {};
		shop.GetLicenceRows(rows);
		ctx.Equal(1, rows.Count(), "the counter shows one licence row");
		if (rows.Count() == 1)
			ctx.Equal(FEE, rows[0].LicenceFee(), "the row's fee is the licence office's permit fee");

		ctx.False(LF_LicenceDesk.DeskIssuesPermit(), "with the row the desk leaves the permit to the counter");
		if (LF_Systems.SwitchOff(LF_ItemCatalogue.SYSTEM, "licence/counter test"))
		{
			ctx.True(LF_LicenceDesk.DeskIssuesPermit(), "with the Catalogue off no counter serves the row, so the desk takes it back");
			LF_Systems.ClearRuntimeOff(LF_ItemCatalogue.SYSTEM);
		}
		ctx.True(shop.IsItemSoldHere(PISTOL), "the counter still sells the pistol");

		LF_PlayerAccount account = LF_PlayerAccountManager.GetOrCreate(UID);
		account.SetWantedLevel(2);
		ctx.Equal(LF_EShopRefusal.NONE, shop.ApplyForLicenceFor(buyer, UID, wallet, LF_ELicenseType.FIREARM_PERMIT, outcome, left), "the counter asks the licence office");
		ctx.Equal(LF_ELicenceDesk.WANTED, outcome, "a wanted buyer is refused the permit");
		ctx.EqualStr("#LF-LicenceDesk_Wanted", LF_LicenceDesk.CounterAnswerKey(outcome), "the slip names the wanted level");
		account.SetWantedLevel(0);
		ctx.Equal(500, LF_MoneyUtils.GetCash(buyer), "the refusal took no cash");

		LF_LicencesConfig.SetDeskForTest(1000, 1, 0);
		shop.ApplyForLicenceFor(buyer, UID, wallet, LF_ELicenseType.FIREARM_PERMIT, outcome, left);
		ctx.Equal(LF_ELicenceDesk.NOT_ENOUGH_CASH, outcome, "short of the fee, refused");
		ctx.EqualStr(LF_MoneyFormat.FormatMoney(1000), LF_LicenceDesk.AnswerArg(outcome, left), "the refusal names the fee");
		LF_LicencesConfig.SetDeskForTest(FEE, 1, 0);
		ctx.Equal(500, LF_MoneyUtils.GetCash(buyer), "a short fee takes nothing");

		shop.ApplyForLicenceFor(buyer, UID, wallet, LF_ELicenseType.FIREARM_PERMIT, outcome, left);
		ctx.Equal(LF_ELicenceDesk.APPLIED, outcome, "a clean buyer with the fee applies at the counter");
		ctx.EqualStr("#LF-LicenceCounter_Applied", LF_LicenceDesk.CounterAnswerKey(outcome), "the slip says to come back to this counter");
		ctx.Equal(500 - FEE, LF_MoneyUtils.GetCash(buyer), "the fee is taken");
		shop.ApplyForLicenceFor(buyer, UID, wallet, LF_ELicenseType.FIREARM_PERMIT, outcome, left);
		ctx.Equal(LF_ELicenceDesk.WAITING, outcome, "before the day is up the counter says wait");
		ctx.Equal(500 - FEE, LF_MoneyUtils.GetCash(buyer), "and charges nothing again");

		LF_LicenceDesk.SetNowForTest(T0 + 1440);
		shop.ApplyForLicenceFor(buyer, UID, wallet, LF_ELicenseType.FIREARM_PERMIT, outcome, left);
		ctx.Equal(LF_ELicenceDesk.ISSUED, outcome, "a day later the counter issues the permit");
		ctx.True(LF_LicenceDesk.Holds(buyer, UID), "the buyer holds the permit");
		ctx.Equal(500 - FEE, LF_MoneyUtils.GetCash(buyer), "the fee was taken once");

		int charged;
		string missing;
		LF_MoneyUtils.AddCash(buyer, shop.GetBuyPrice(PISTOL));
		ctx.Equal(LF_EShopRefusal.NONE, shop.Buy(PISTOL, 1, buyer, charged, missing), "with the counter's permit the counter sells the pistol");

		LF_TestCharacterFixture.Delete(buyer);
		LF_Progression.Forget(UID);
		LF_LicenceDesk.SetNowForTest(-1);
		LF_LicencesConfig.SetDeskForTest(-1, -1);
		LF_ItemCatalogueConfig.Reload();
	}
}
