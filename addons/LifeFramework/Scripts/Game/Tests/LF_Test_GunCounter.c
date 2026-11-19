// red-proof: observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_GunCounter` before
// LF_LicenseManagerComponent was on Character_Roleplay.et (no player body could hold a licence):
// `- the refusal names the firearm permit (expected '#LF-License_FirearmPermit_Name', got
// 'FIREARM_PERMIT')` and `- the test character has a licence manager`. With the component: green.
//! The annex gun counter (#988) on the server path: a character without the firearm permit is refused
//! with the permit's name and keeps its cash; a cuffed permit holder is refused; with the permit the
//! same order is paid and delivered.
// red-proof (cuffed, 2026-11-19): `tools\cli test --no-build --red LF_Test_GunCounter` RED-OK: `- a cuffed
// permit holder is refused (expected 21, got 0)`, `- the cuffed buyer got no pistol (expected 0, got 1)`.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_GunCounter : LF_Test
{
	protected static const ResourceName PISTOL = "{1353C6EAD1DCFE43}Prefabs/Weapons/Handguns/M9/Handgun_M9.et";
	protected static const ResourceName RIFLE = "{3E413771E1834D2F}Prefabs/Weapons/Rifles/M16/Rifle_M16A2.et";

	override string GetName()
	{
		return "catalogue/gun-counter";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_ConfigService.Reset();
		LF_ConfigStream.Reset();
		LF_ItemCatalogueConfig.Reload();

		IEntity counter = GetGame().GetWorld().FindEntityByName("Annex_GunCounter");
		ctx.NotNull(counter, "Annex_GunCounter resolves in the DebugWorld");
		if (!counter)
			return;

		LF_ShopComponent shop = LF_Component<LF_ShopComponent>.Find(counter);
		ctx.NotNull(shop, "the gun counter carries LF_ShopComponent");
		if (!shop)
			return;

		ctx.EqualStr("gun_counter", shop.GetOutletId(), "the counter is the gun_counter outlet");
		ctx.Equal(1, shop.GetShopItems().Count(), "the counter lists its one offer");
		ctx.True(shop.IsItemSoldHere(PISTOL), "the counter sells the pistol");
		ctx.False(shop.IsItemSoldHere(RIFLE), "the counter does not sell the armory's rifle");
		int price = shop.GetBuyPrice(PISTOL);
		ctx.Equal(500, price, "the pistol costs the catalogue price");

		IEntity buyer = LF_TestCharacterFixture.Spawn(ctx);
		if (!buyer)
			return;

		LF_MoneyUtils.AddCash(buyer, price);
		int charged;
		string missing;
		ctx.Equal(LF_EShopRefusal.NEEDS_LICENCE, shop.Buy(PISTOL, 1, buyer, charged, missing), "no permit, no pistol");
		ctx.EqualStr("#LF-License_FirearmPermit_Name", missing, "the refusal names the firearm permit");
		ctx.Equal(0, charged, "a refused order charges nothing");
		ctx.Equal(price, LF_MoneyUtils.GetCash(buyer), "the refusal moved no cash");
		ctx.Equal(0, LF_InventoryUtils.GetAmount(buyer, PISTOL), "the refusal delivered nothing");

		LF_LicenseManagerComponent licences = LF_Component<LF_LicenseManagerComponent>.Find(buyer);
		ctx.NotNull(licences, "the test character has a licence manager");
		if (licences)
		{
			ctx.True(licences.UnlockLicense(LF_ELicenseType.FIREARM_PERMIT, true), "the permit is granted");

			// A cuffed permit holder is not served: the client can send the order from any state.
			SCR_ChimeraCharacter chimera = SCR_ChimeraCharacter.Cast(buyer);
			if (chimera && !RedProof())
				chimera.LF_SetRestrained(true);
			ctx.Equal(LF_EShopRefusal.UNFIT, shop.Buy(PISTOL, 1, buyer, charged, missing), "a cuffed permit holder is refused");
			ctx.Equal(0, LF_InventoryUtils.GetAmount(buyer, PISTOL), "the cuffed buyer got no pistol");
			ctx.Equal(price, LF_MoneyUtils.GetCash(buyer), "the cuffed buyer kept the cash");
			if (chimera)
				chimera.LF_SetRestrained(false);

			ctx.Equal(LF_EShopRefusal.NONE, shop.Buy(PISTOL, 1, buyer, charged, missing), "with the permit the order is paid");
			ctx.EqualStr("", missing, "a paid order names nothing missing");
			ctx.Equal(price, charged, "the slip reports the catalogue price");
			ctx.Equal(0, LF_MoneyUtils.GetCash(buyer), "the buyer paid the price");
			ctx.Equal(1, LF_InventoryUtils.GetAmount(buyer, PISTOL), "the buyer holds the pistol");
		}

		LF_TestCharacterFixture.Delete(buyer);
	}
}
