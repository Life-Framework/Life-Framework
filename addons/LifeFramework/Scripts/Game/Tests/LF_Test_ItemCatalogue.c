// red-proof: observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_ItemCatalogue`: the licence
// gate in LF_ItemCatalogue.Check short-circuited (`if (false && !buyer.HasLicence(licence))`) failed
// `- no permit, no pistol (expected 2, got 0)` and `- the refusal names the permit (expected
// 'name:FIREARM_PERMIT', got '')`. Restored; green.
// tier: LOGIC
//! Item catalogue and outlets (#988): each offer's gate (licence, job and rank, organisation right)
//! and the name its refusal carries, legality to a crime catalog offence, what clothing hides, an owner
//! patch of a price merged by id, the load checks (DLC-owned prefab, sell above price, offer for a
//! missing item), and the stream carrying the server's catalogue to a client.
class LF_Test_ItemCatalogue : LF_Test
{
	static const ResourceName PISTOL = "{1353C6EAD1DCFE43}Prefabs/Weapons/Handguns/M9/Handgun_M9.et";
	static const ResourceName RAINCOAT = "{7F1451790F3D75BD}Prefabs/Characters/Uniforms/Jacket_Raincoat_01/Jacket_Raincoat_01_green.et";
	static const ResourceName RIFLE = "{3E413771E1834D2F}Prefabs/Weapons/Rifles/M16/Rifle_M16A2.et";
	static const ResourceName RALLY_HELMET = "{0000000000000001}Prefabs/Characters/HeadGear/Helmet_Rally_01/Helmet_Rally_01_red.et";

	override string GetName()
	{
		return "catalogue/outlets";
	}

	//------------------------------------------------------------------------------------------------
	//! A core catalogue built in script: one example per outlet type, plus a stash-gated lockpick.
	static LF_ItemCatalogueConfig MakeConfig()
	{
		LF_ItemCatalogueConfig config = new LF_ItemCatalogueConfig();
		config.m_aItems = {};
		config.m_aOffers = {};

		LF_CatalogueItem pistol = LF_CatalogueItem.Make("pistol_m9", PISTOL, "weapon", 500, 250, "licensed");
		pistol.m_sLicence = "FIREARM_PERMIT";
		config.m_aItems.Insert(pistol);

		LF_CatalogueItem raincoat = LF_CatalogueItem.Make("raincoat_green", RAINCOAT, "clothing", 60, 30, "legal");
		raincoat.m_bConcealsBuild = true;
		config.m_aItems.Insert(raincoat);

		LF_CatalogueItem rifle = LF_CatalogueItem.Make("rifle_m16a2", RIFLE, "weapon", 1200, 0, "restricted");
		rifle.m_sLicence = "POLICE_ACCESS";
		config.m_aItems.Insert(rifle);

		config.m_aItems.Insert(LF_CatalogueItem.Make("lockpick", "{6B1D3F5A92C47E08}Prefabs/Items/Fence/LF_Lockpick.et", "tool", 40, 10, "illegal"));

		LF_OutletOffer counter = LF_OutletOffer.Make("gun_counter", "pistol_m9");
		counter.m_sLicence = "FIREARM_PERMIT";
		config.m_aOffers.Insert(counter);

		config.m_aOffers.Insert(LF_OutletOffer.Make("clothing_rack", "raincoat_green"));

		LF_OutletOffer armory = LF_OutletOffer.Make("police_armory", "rifle_m16a2");
		armory.m_sJob = "POLICE";
		armory.m_iMinRank = 1;
		config.m_aOffers.Insert(armory);

		LF_OutletOffer backroom = LF_OutletOffer.Make("org_backroom", "lockpick");
		backroom.m_sOrgRight = "USE_STASH";
		config.m_aOffers.Insert(backroom);
		return config;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_OutletOffer Offer(LF_ItemCatalogueConfig config, string outlet, ResourceName prefab)
	{
		LF_CatalogueItem item;
		return LF_ItemCatalogue.OfferFor(config, outlet, prefab, item);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ConfigService.Reset();
		LF_ConfigStream.Reset();
		LF_ItemCatalogueConfig config = MakeConfig();
		LF_Test_CatalogueBuyer buyer = new LF_Test_CatalogueBuyer();
		string missing;

		// Licence gate: refused without the permit, and the refusal names it; open with it.
		LF_OutletOffer counter = Offer(config, "gun_counter", PISTOL);
		ctx.NotNull(counter, "the gun counter sells the pistol");
		ctx.Equal(LF_EOutletGate.NEEDS_LICENCE, LF_ItemCatalogue.Check(counter, buyer, missing), "no permit, no pistol");
		ctx.EqualStr("name:FIREARM_PERMIT", missing, "the refusal names the permit");
		ctx.Equal(LF_EShopRefusal.NEEDS_LICENCE, LF_ItemCatalogue.RefusalOf(LF_EOutletGate.NEEDS_LICENCE), "the till refuses for the licence");
		buyer.m_aLicences.Insert(LF_ELicenseType.FIREARM_PERMIT);
		ctx.Equal(LF_EOutletGate.OPEN, LF_ItemCatalogue.Check(counter, buyer, missing), "the permit opens the counter");
		ctx.EqualStr("", missing, "an open gate names nothing");

		// A gate naming something that is not a licence refuses rather than opening.
		LF_OutletOffer typo = LF_OutletOffer.Make("gun_counter", "pistol_m9");
		typo.m_sLicence = "FIREARM_PERMITT";
		ctx.Equal(LF_EOutletGate.NEEDS_LICENCE, LF_ItemCatalogue.Check(typo, buyer, missing), "a misspelt licence gate stays shut");

		// Job and rank gate: the armory hands the rifle only to an on-duty officer of rank 1 or more.
		LF_OutletOffer armory = Offer(config, "police_armory", RIFLE);
		ctx.Equal(LF_EOutletGate.NEEDS_JOB, LF_ItemCatalogue.Check(armory, buyer, missing), "a civilian is not served at the armory");
		ctx.EqualStr("#LF-Job_Name_Police", missing, "the refusal names the job");
		buyer.m_iPoliceRank = 0;
		ctx.Equal(LF_EOutletGate.NEEDS_RANK, LF_ItemCatalogue.Check(armory, buyer, missing), "a patrol officer is below the rifle's rank");
		ctx.EqualStr("#" + LF_PoliceRanks.TitleKey(1), missing, "the refusal names the rank that may");
		buyer.m_iPoliceRank = 1;
		ctx.Equal(LF_EOutletGate.OPEN, LF_ItemCatalogue.Check(armory, buyer, missing), "a senior officer draws the rifle");

		// Organisation right gate.
		LF_OutletOffer backroom = Offer(config, "org_backroom", "{6B1D3F5A92C47E08}Prefabs/Items/Fence/LF_Lockpick.et");
		ctx.Equal(LF_EOutletGate.NEEDS_ORG_RIGHT, LF_ItemCatalogue.Check(backroom, buyer, missing), "a member without the stash right is refused");
		ctx.EqualStr("#LF-OrgRight_USE_STASH", missing, "the refusal names the right");
		buyer.m_iOrgRights = LF_OrgRights.Bit(LF_EOrganizationPermission.USE_STASH);
		ctx.Equal(LF_EOutletGate.OPEN, LF_ItemCatalogue.Check(backroom, buyer, missing), "the stash right opens it");

		// An outlet sells only its own offers; a refusal reason survives the wire.
		ctx.True(Offer(config, "gun_counter", RIFLE) == null, "the gun counter does not sell the rifle");
		ctx.Equal(LF_EShopRefusal.NEEDS_ORG_RIGHT, LF_ShopRefusal.FromWire(LF_EShopRefusal.NEEDS_ORG_RIGHT), "the last gate refusal survives the wire");

		// Legality to an offence code, read against the carrier's licences; each code is in the crime catalog.
		LF_Test_CatalogueBuyer civilian = new LF_Test_CatalogueBuyer();
		ctx.EqualStr(LF_ItemCatalogue.OFFENCE_UNLICENSED, LF_ItemCatalogue.OffenceFor(config.FindItem("pistol_m9"), civilian), "a pistol without the permit is an offence");
		ctx.EqualStr("", LF_ItemCatalogue.OffenceFor(config.FindItem("pistol_m9"), buyer), "a pistol with the permit is lawful");
		ctx.EqualStr(LF_ItemCatalogue.OFFENCE_RESTRICTED, LF_ItemCatalogue.OffenceFor(config.FindItem("rifle_m16a2"), civilian), "a police rifle off a civilian is restricted");
		ctx.EqualStr(LF_ItemCatalogue.OFFENCE_ILLEGAL, LF_ItemCatalogue.OffenceFor(config.FindItem("lockpick"), buyer), "an illegal item is an offence for anyone");
		ctx.EqualStr("", LF_ItemCatalogue.OffenceFor(config.FindItem("raincoat_green"), civilian), "a raincoat is lawful");
		LF_CrimeCatalogConfig.Reload();
		LF_CrimeCatalogConfig crimes = LF_CrimeCatalogConfig.Get();
		ctx.NotNull(crimes, "the crime catalog loads");
		if (crimes)
		{
			ctx.NotNull(crimes.Find(LF_ItemCatalogue.OFFENCE_UNLICENSED), "the crime catalog lists the unlicensed offence");
			ctx.NotNull(crimes.Find(LF_ItemCatalogue.OFFENCE_RESTRICTED), "the crime catalog lists the restricted offence");
			ctx.NotNull(crimes.Find(LF_ItemCatalogue.OFFENCE_ILLEGAL), "the crime catalog lists the illegal offence");
		}

		// What a worn item hides from a witness.
		bool face;
		bool build;
		array<ResourceName> worn = {RAINCOAT, PISTOL};
		LF_ItemCatalogue.Concealment(config, worn, face, build);
		ctx.True(build, "a raincoat hides the build");
		ctx.False(face, "a raincoat leaves the face");

		// The owner patches by id: one price, one offer removed, one item added; the rest stays.
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1,\"catalogue\":{\"items\":{\"pistol_m9\":{\"price\":650},\"raincoat_green\":{\"legality\":\"sometimes\"},\"boots\":{\"price\":5},\"ghostcap\":{\"prefab\":\"{2222222222222222}Prefabs/Items/Test/Cap.et\"},\"cap\":{\"prefab\":\"{67662815EFF734DD}Prefabs/Characters/HeadGear/Hat_FlatCap_01/Hat_FlatCap_01_darkgrey.et\",\"category\":\"clothing\",\"concealsFace\":true}},\"offers\":{\"clothing_rack/raincoat_green\":{\"removed\":true},\"clothing_rack/cap\":{},\"nokey\":{}}}}", "test");
		LF_ItemCatalogueConfig patched = MakeConfig();
		patched.ApplyLayers(LF_ItemCatalogueConfig.CONFIG_PATH);
		LF_CatalogueItem patchedPistol = patched.FindItem("pistol_m9");
		LF_CatalogueItem patchedCoat = patched.FindItem("raincoat_green");
		ctx.NotNull(patchedPistol, "the pistol is still listed");
		ctx.NotNull(patchedCoat, "the raincoat is still listed");
		if (patchedPistol)
		{
			ctx.Equal(650, patchedPistol.m_iPrice, "the owner's price applies");
			ctx.Equal(250, patchedPistol.m_iSellPrice, "a field the owner left out keeps the core value");
		}
		if (patchedCoat)
			ctx.EqualStr("legal", patchedCoat.m_sLegality, "an unknown legality word is refused");
		ctx.True(patched.FindItem("boots") == null, "a new item without a prefab is refused");
		ctx.True(patched.FindItem("ghostcap") == null, "a new item whose prefab does not load is refused");
		ctx.NotNull(LF_ItemCatalogue.Item(patched, "cap"), "a new item with a prefab is added");
		ctx.True(Offer(patched, "clothing_rack", RAINCOAT) == null, "a removed offer is no longer sold");
		ctx.NotNull(Offer(patched, "clothing_rack", "{67662815EFF734DD}Prefabs/Characters/HeadGear/Hat_FlatCap_01/Hat_FlatCap_01_darkgrey.et"), "an added offer is sold");
		ctx.NotNull(Offer(patched, "gun_counter", PISTOL), "an offer the owner left out stays");

		// The stream carries the server's catalogue to a client that has only the core one.
		string payload = LF_ConfigStream.BuildPayload(null, true, null, null, null, patched);
		LF_ConfigService.Reset();
		LF_ConfigStream.Reset();
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1}", "test-client");
		ctx.Equal(LF_EConfigStreamStatus.APPLIED, LF_ConfigStream.Receive(LF_ConfigStream.FORMAT_VERSION, payload.Hash(), payload), "the client accepts the stream");
		LF_ItemCatalogueConfig client = MakeConfig();
		client.ApplyLayers(LF_ItemCatalogueConfig.CONFIG_PATH);
		LF_CatalogueItem clientPistol = client.FindItem("pistol_m9");
		ctx.NotNull(clientPistol, "the client lists the pistol");
		if (clientPistol)
			ctx.Equal(650, clientPistol.m_iPrice, "the client shows the server's price");
		ctx.True(Offer(client, "clothing_rack", RAINCOAT) == null, "the client drops the offer the server removed");
		ctx.NotNull(Offer(client, "clothing_rack", "{67662815EFF734DD}Prefabs/Characters/HeadGear/Hat_FlatCap_01/Hat_FlatCap_01_darkgrey.et"), "the client sells the offer the server added");
		LF_ConfigService.Reset();
		LF_ConfigStream.Reset();

		// The load checks: a DLC-owned prefab, a sell price above the price, an offer for no item.
		LF_ItemCatalogueConfig checkedConfig = MakeConfig();
		checkedConfig.m_aItems.Insert(LF_CatalogueItem.Make("rally_helmet", RALLY_HELMET, "clothing", 90, 45, "legal"));
		checkedConfig.m_aOffers.Insert(LF_OutletOffer.Make("clothing_rack", "rally_helmet"));
		LF_CatalogueItem checkedCoat = checkedConfig.FindItem("raincoat_green");
		if (checkedCoat)
			checkedCoat.m_iSellPrice = 80;
		checkedConfig.m_aOffers.Insert(LF_OutletOffer.Make("clothing_rack", "ghost"));
		ctx.Equal(3, checkedConfig.Check(false), "three records are fixed by the load checks");
		ctx.True(LF_ItemCatalogue.IsDlcOwned(RALLY_HELMET), "a Rally helmet is DLC-owned");
		ctx.True(Offer(checkedConfig, "clothing_rack", RALLY_HELMET) == null, "no outlet sells a DLC-owned prefab");
		if (checkedCoat)
			ctx.Equal(60, checkedCoat.m_iSellPrice, "a sell price above the price is lowered to it");
		LF_OutletOffer ghost = checkedConfig.FindOffer("clothing_rack", "ghost");
		ctx.True(ghost && ghost.m_bRemoved, "an offer for a missing item is removed");
		ctx.False(LF_ItemCatalogue.IsDlcOwned(PISTOL), "a base game pistol is not DLC-owned");
	}
}

//------------------------------------------------------------------------------------------------
//! A buyer with no world: the licences, police rank and organisation rights the test sets.
class LF_Test_CatalogueBuyer : LF_BuyerFacts
{
	ref array<LF_ELicenseType> m_aLicences = {};
	int m_iPoliceRank = -1;
	int m_iOrgRights;

	//------------------------------------------------------------------------------------------------
	override bool HasLicence(LF_ELicenseType licence)
	{
		return m_aLicences.Contains(licence);
	}

	//------------------------------------------------------------------------------------------------
	override int JobRank(string job)
	{
		if (job == LF_EntityBuyerFacts.JOB_POLICE)
			return m_iPoliceRank;

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	override int OrgRights()
	{
		return m_iOrgRights;
	}

	//------------------------------------------------------------------------------------------------
	override string LicenceName(LF_ELicenseType licence)
	{
		return "name:" + typename.EnumToString(LF_ELicenseType, licence);
	}
}
