// red-proof: observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_PoliceArmoryIssue`: the issue flag
// skipped in LF_PoliceRanks.ArmoryIssue (`if (false && !offer.m_bIssue)`) failed
// `- a patrol officer is issued the sidearm and the spike strip (expected 2, got 3)` and
// `- a priced offer is not issued free, and an issue offer elsewhere is not the armory's (expected 1, got 2)`.
// Restored; green.
// tier: LOGIC
//! The police armory is the catalogue's police_armory outlet (#1066): only its issue offers are the
//! kit, each gated by job and minimum rank, and a refusal names the rank that may draw the item.
class LF_Test_PoliceArmoryIssue : LF_Test
{
	static const ResourceName PISTOL = "{1353C6EAD1DCFE43}Prefabs/Weapons/Handguns/M9/Handgun_M9.et";
	static const ResourceName RIFLE = "{3E413771E1834D2F}Prefabs/Weapons/Rifles/M16/Rifle_M16A2.et";
	static const ResourceName STRIP = "{4B8BDB727DEE59B8}Prefabs/Props/Roleplay/SpikeStrip/SpikeStrip_Item.et";

	override string GetName()
	{
		return "police/armory-issue";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_ItemCatalogueConfig config = new LF_ItemCatalogueConfig();
		config.m_aItems = {};
		config.m_aOffers = {};
		config.m_aItems.Insert(LF_CatalogueItem.Make("pistol_m9", PISTOL, "weapon", 500, 250, "licensed"));
		config.m_aItems.Insert(LF_CatalogueItem.Make("rifle_m16a2", RIFLE, "weapon", 1200, 0, "restricted"));
		config.m_aItems.Insert(LF_CatalogueItem.Make("spike_strip", STRIP, "tool", 150, 0, "legal"));
		config.m_aOffers.Insert(Offer("police_armory", "pistol_m9", 0, true));
		config.m_aOffers.Insert(Offer("police_armory", "spike_strip", 0, true));
		config.m_aOffers.Insert(Offer("police_armory", "rifle_m16a2", 1, true));
		// A priced offer at the same outlet and an issue offer at another outlet are not the kit.
		config.m_aItems.Insert(LF_CatalogueItem.Make("pistol_spare", PISTOL, "weapon", 500, 250, "legal"));
		config.m_aOffers.Insert(Offer("police_armory", "pistol_spare", 0, false));
		config.m_aOffers.Insert(Offer("gun_counter", "pistol_m9", 0, true));

		array<ResourceName> kit = {};
		array<ResourceName> refused = {};
		array<string> missing = {};
		LF_Test_CatalogueBuyer patrol = new LF_Test_CatalogueBuyer();
		patrol.m_iPoliceRank = LF_PoliceRanks.PATROL;
		LF_PoliceRanks.ArmoryIssue(config, patrol, kit, refused, missing);
		ctx.Equal(2, kit.Count(), "a patrol officer is issued the sidearm and the spike strip");
		ctx.True(kit.Contains(PISTOL), "the patrol kit has the sidearm");
		ctx.True(kit.Contains(STRIP), "the patrol kit has the spike strip");
		ctx.Equal(0, CountOf(kit, RIFLE), "a patrol officer is not issued the rifle");
		ctx.Equal(1, refused.Count(), "the rifle is the one refusal");
		if (refused.Count() == 1)
		{
			ctx.True(refused.Get(0) == RIFLE, "the refusal is for the rifle");
			ctx.EqualStr("#" + LF_PoliceRanks.TitleKey(LF_PoliceRanks.SENIOR), missing.Get(0), "the refusal names the senior rank");
		}
		ctx.Equal(1, CountOf(kit, PISTOL), "a priced offer is not issued free, and an issue offer elsewhere is not the armory's");

		LF_Test_CatalogueBuyer senior = new LF_Test_CatalogueBuyer();
		senior.m_iPoliceRank = LF_PoliceRanks.SENIOR;
		LF_PoliceRanks.ArmoryIssue(config, senior, kit, refused, missing);
		ctx.Equal(3, kit.Count(), "a senior officer draws all three");
		ctx.Equal(1, CountOf(kit, RIFLE), "a senior officer draws the rifle");
		ctx.Equal(0, refused.Count(), "a senior officer is refused nothing");

		// A civilian (or an off-duty officer, rank -1) draws nothing and the refusal names the job.
		LF_Test_CatalogueBuyer civilian = new LF_Test_CatalogueBuyer();
		LF_PoliceRanks.ArmoryIssue(config, civilian, kit, refused, missing);
		ctx.Equal(0, kit.Count(), "a civilian is issued nothing");
		ctx.Equal(3, refused.Count(), "every issue offer refuses a civilian");

		// The owner's patch retunes the table: the rifle opens to patrol, the strip closes to senior.
		LF_OutletOffer rifleOffer = config.FindOffer("police_armory", "rifle_m16a2");
		LF_OutletOffer stripOffer = config.FindOffer("police_armory", "spike_strip");
		ctx.NotNull(rifleOffer, "the rifle offer is found by outlet and item");
		ctx.NotNull(stripOffer, "the spike strip offer is found by outlet and item");
		if (!rifleOffer || !stripOffer)
			return;

		rifleOffer.m_iMinRank = 0;
		stripOffer.m_iMinRank = 1;
		LF_PoliceRanks.ArmoryIssue(config, patrol, kit, refused, missing);
		ctx.Equal(1, CountOf(kit, RIFLE), "an owner who sets the rifle's rank to 0 arms patrol");
		ctx.Equal(0, CountOf(kit, STRIP), "an owner who raises the strip's rank closes it to patrol");

		// An offer taken out of the outlet is no longer issued.
		rifleOffer.m_bRemoved = true;
		LF_PoliceRanks.ArmoryIssue(config, senior, kit, refused, missing);
		ctx.Equal(0, CountOf(kit, RIFLE), "a removed offer is not issued");

		// The shipped catalogue: the same table the flagship plays.
		LF_ItemCatalogueConfig shipped = LF_ItemCatalogueConfig.Get();
		LF_PoliceRanks.ArmoryIssue(shipped, patrol, kit, refused, missing);
		ctx.Equal(2, kit.Count(), "the shipped patrol kit is the sidearm and the spike strip");
		ctx.Equal(0, CountOf(kit, RIFLE), "the shipped patrol kit has no rifle");
		LF_PoliceRanks.ArmoryIssue(shipped, senior, kit, refused, missing);
		ctx.Equal(3, kit.Count(), "the shipped senior kit adds the rifle");
		ctx.Equal(1, CountOf(kit, RIFLE), "the shipped senior kit has the rifle");
	}

	//------------------------------------------------------------------------------------------------
	protected LF_OutletOffer Offer(string outlet, string item, int minRank, bool issue)
	{
		LF_OutletOffer offer = LF_OutletOffer.Make(outlet, item);
		offer.m_sJob = LF_EntityBuyerFacts.JOB_POLICE;
		offer.m_iMinRank = minRank;
		offer.m_bIssue = issue;
		return offer;
	}

	//------------------------------------------------------------------------------------------------
	protected int CountOf(notnull array<ResourceName> kit, ResourceName prefab)
	{
		int count;
		foreach (ResourceName candidate : kit)
		{
			if (candidate == prefab)
				count++;
		}

		return count;
	}
}
