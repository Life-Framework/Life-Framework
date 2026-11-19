// red-proof: observed 2026-11-19, `tools\cli test --no-build --red LF_Test_GunCounterGate` RED-OK: `- a wanted
// permit holder is refused (expected 6, got 0)`, `- a dead, downed or cuffed permit holder is refused (expected 7, got 0)`. Black market (review of #3902): `a wanted
// criminal still buys at the black market` failed on the PR's gate (expected 0, got 6), passes with the fix.
//! The gun counter's server gate (LF_ItemCatalogue.Check) for a firearm offer: no permit is refused,
//! the permit opens it, and a permit holder who is wanted, or dead, downed or cuffed, is refused before
//! the permit is even read. The refusals map to their own slip lines.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_GunCounterGate : LF_Test
{
	override string GetName()
	{
		return "catalogue/gun-counter-gate";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_OutletOffer pistol = new LF_OutletOffer();
		pistol.m_sOutlet = "gun_counter";
		pistol.m_sItem = "pistol_pm";
		pistol.m_sLicence = "FIREARM_PERMIT";

		LF_Test_GunCounterGateBuyer buyer = new LF_Test_GunCounterGateBuyer();
		string missing;

		ctx.Equal(LF_EOutletGate.NEEDS_LICENCE, LF_ItemCatalogue.Check(pistol, buyer, missing), "no permit, no pistol");

		buyer.m_bPermit = true;
		ctx.Equal(LF_EOutletGate.OPEN, LF_ItemCatalogue.Check(pistol, buyer, missing), "the permit opens the counter");

		if (!RedProof())
			buyer.m_iWanted = 2;
		ctx.Equal(LF_EOutletGate.WANTED, LF_ItemCatalogue.Check(pistol, buyer, missing), "a wanted permit holder is refused");
		ctx.Equal(LF_EShopRefusal.WANTED, LF_ItemCatalogue.RefusalOf(LF_EOutletGate.WANTED), "the till names the wanted level");
		buyer.m_iWanted = 0;

		if (!RedProof())
			buyer.m_bCanTrade = false;
		ctx.Equal(LF_EOutletGate.UNFIT, LF_ItemCatalogue.Check(pistol, buyer, missing), "a dead, downed or cuffed permit holder is refused");
		ctx.Equal(LF_EShopRefusal.UNFIT, LF_ItemCatalogue.RefusalOf(LF_EOutletGate.UNFIT), "the till names the state");
		buyer.m_bCanTrade = true;

		// The refusals have slip lines, and the wanted one reads as words once shown.
		ctx.EqualStr("#LF-Shop_RefuseWanted", LF_ShopRefusal.Key(LF_EShopRefusal.WANTED), "the wanted refusal has its line");
		ctx.EqualStr("#LF-Shop_RefuseUnfit", LF_ShopRefusal.Key(LF_EShopRefusal.UNFIT), "the unfit refusal has its line");
		ctx.Equal(LF_EShopRefusal.UNFIT, LF_ShopRefusal.FromWire(LF_EShopRefusal.UNFIT), "the last refusal survives the wire");

		// An unlicensed line (a hat) is still sold to anyone: the state gate is for licensed lines only.
		LF_OutletOffer hat = new LF_OutletOffer();
		hat.m_sOutlet = "gun_counter";
		hat.m_sItem = "hat";
		buyer.m_iWanted = 3;
		ctx.Equal(LF_EOutletGate.OPEN, LF_ItemCatalogue.Check(hat, buyer, missing), "a wanted buyer can still buy an unlicensed line");

		// The black market serves the wanted: a criminal licence line skips the wanted gate, not the fitness one.
		LF_OutletOffer blackPistol = new LF_OutletOffer();
		blackPistol.m_sOutlet = "black_market";
		blackPistol.m_sItem = "pistol_pm";
		blackPistol.m_sLicence = "CRIMINAL_WEAPONS";
		buyer.m_bCriminal = true;
		ctx.Equal(LF_EOutletGate.OPEN, LF_ItemCatalogue.Check(blackPistol, buyer, missing), "a wanted criminal still buys at the black market");
		buyer.m_bCanTrade = false;
		ctx.Equal(LF_EOutletGate.UNFIT, LF_ItemCatalogue.Check(blackPistol, buyer, missing), "a cuffed criminal is refused at the black market");
		buyer.m_bCanTrade = true;

		// A self-service shelf copy is a free pickup until paid: no weapon or licensed item stands on one.
		ResourceName pistolPrefab = "{1353C6EAD1DCFE43}Prefabs/Weapons/Handguns/M9/Handgun_M9.et";
		ResourceName hatPrefab = "{A22C976003AA5BA3}Prefabs/Characters/HeadGear/Hat_Ushanka_01/Hat_Ushanka_01_folded.et";
		ResourceName pouchPrefab = "{9C00CF3A286C048F}Prefabs/Items/Equipment/Accessories/Pouch_ALICE_20rnd_M14/Pouch_ALICE_20rnd_M14.et";
		ctx.False(LF_StoreRules.MayShelve(pistolPrefab, null), "a weapon prefab never goes on a shelf, listed or not");
		ctx.True(LF_StoreRules.MayShelve(hatPrefab, null), "a hat may stand on a shelf");
		ctx.True(LF_StoreRules.MayShelve(pouchPrefab, LF_CatalogueItem.Make("pouch", pouchPrefab, "equipment", 25, 10, "legal")), "a legal item may stand on a shelf");
		ctx.False(LF_StoreRules.MayShelve(pouchPrefab, LF_CatalogueItem.Make("pouch", pouchPrefab, "equipment", 25, 10, "licensed")), "a licensed item never goes on a shelf");
	}
}

//------------------------------------------------------------------------------------------------
//! A buyer with no world: a permit, a wanted level and a fit-to-trade flag the test sets.
class LF_Test_GunCounterGateBuyer : LF_BuyerFacts
{
	bool m_bPermit;
	bool m_bCriminal;
	int m_iWanted;
	bool m_bCanTrade = true;

	//------------------------------------------------------------------------------------------------
	override bool HasLicence(LF_ELicenseType licence)
	{
		if (licence == LF_ELicenseType.CRIMINAL_WEAPONS)
			return m_bCriminal;

		return m_bPermit && licence == LF_ELicenseType.FIREARM_PERMIT;
	}

	//------------------------------------------------------------------------------------------------
	override int WantedLevel()
	{
		return m_iWanted;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanTrade()
	{
		return m_bCanTrade;
	}
}
