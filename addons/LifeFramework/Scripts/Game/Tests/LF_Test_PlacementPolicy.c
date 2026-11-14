// red-proof: pending
// tier: LOGIC
//! Placement rules (#1030): each rule kind of a placement policy refuses with a reason that names the
//! policy and the rule: who (job, rank, licence, organisation right: the catalogue's gate), where (a
//! zone rule, a place of a kind within a radius, the road) and cost (cash, an item). The override
//! model changes a policy by id: an owner patch of the price, an add, a remove.
class LF_Test_PlacementPolicy : LF_Test
{
	static const ResourceName TOKEN = "{1353C6EAD1DCFE43}Prefabs/Weapons/Handguns/M9/Handgun_M9.et";

	override string GetName()
	{
		return "placement/policy";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		TestWho(ctx);
		TestWhere(ctx);
		TestCost(ctx);
		TestOverrides(ctx);
	}

	//------------------------------------------------------------------------------------------------
	protected bool Says(string reason, string text)
	{
		return reason.IndexOf(text) != -1;
	}

	//------------------------------------------------------------------------------------------------
	protected void TestWho(LF_TestContext ctx)
	{
		string reason;
		LF_PlacementPolicy police = LF_PlacementPolicy.Make("cone");
		police.m_sJob = "POLICE";
		LF_PlacementPolicy sergeant = LF_PlacementPolicy.Make("barrier");
		sergeant.m_sJob = "POLICE";
		sergeant.m_iMinRank = 2;
		LF_PlacementPolicy permit = LF_PlacementPolicy.Make("gun_rack");
		permit.m_sLicence = "FIREARM_PERMIT";
		LF_PlacementPolicy stall = LF_PlacementPolicy.Make("stall");
		stall.m_sOrgRight = "USE_STASH";

		LF_StubPlacer officer = new LF_StubPlacer();
		officer.m_iPoliceRank = 1;
		LF_StubPlacer civilian = new LF_StubPlacer();

		ctx.Equal(LF_EPlacementPolicyVerdict.ALLOWED, LF_PlacementPolicies.CheckWho(null, civilian, reason), "no policy: anyone may place it");
		ctx.Equal(LF_EPlacementPolicyVerdict.ALLOWED, LF_PlacementPolicies.CheckWho(police, officer, reason), "an on-duty officer places a cone");
		ctx.Equal(LF_EPlacementPolicyVerdict.NEEDS_JOB, LF_PlacementPolicies.CheckWho(police, civilian, reason), "a civilian cannot place a police cone");
		ctx.True(Says(reason, "policy cone: needs job POLICE"), "the job refusal names the policy and the job: " + reason);
		ctx.Equal(LF_EPlacementPolicyVerdict.NEEDS_RANK, LF_PlacementPolicies.CheckWho(sergeant, officer, reason), "rank 1 is below the barrier's rank 2");
		ctx.True(Says(reason, "policy barrier: needs rank 2"), "the rank refusal names the rank: " + reason);
		officer.m_iPoliceRank = 2;
		ctx.Equal(LF_EPlacementPolicyVerdict.ALLOWED, LF_PlacementPolicies.CheckWho(sergeant, officer, reason), "rank 2 places the barrier");
		ctx.Equal(LF_EPlacementPolicyVerdict.NEEDS_LICENCE, LF_PlacementPolicies.CheckWho(permit, civilian, reason), "no permit, no gun rack");
		ctx.True(Says(reason, "policy gun_rack: needs licence FIREARM_PERMIT"), "the licence refusal names the licence: " + reason);
		civilian.m_bLicence = true;
		ctx.Equal(LF_EPlacementPolicyVerdict.ALLOWED, LF_PlacementPolicies.CheckWho(permit, civilian, reason), "a permit places the gun rack");
		ctx.Equal(LF_EPlacementPolicyVerdict.NEEDS_ORG_RIGHT, LF_PlacementPolicies.CheckWho(stall, civilian, reason), "no organisation right, no stall");
		ctx.True(Says(reason, "policy stall: needs organisation right USE_STASH"), "the right refusal names the right: " + reason);
		civilian.m_iOrgRights = LF_OrgRights.Bit(LF_EOrganizationPermission.USE_STASH);
		ctx.Equal(LF_EPlacementPolicyVerdict.ALLOWED, LF_PlacementPolicies.CheckWho(stall, civilian, reason), "the right places the stall");
	}

	//------------------------------------------------------------------------------------------------
	protected void TestWhere(LF_TestContext ctx)
	{
		string reason;
		LF_StubSpot spot = new LF_StubSpot();

		LF_PlacementPolicy outsideSafe = LF_PlacementPolicy.Make("stall");
		outsideSafe.m_sBlockedZones = "SAFE, LAWLESS";
		ctx.Equal(LF_EPlacementPolicyVerdict.ALLOWED, LF_PlacementPolicies.CheckWhere(outsideSafe, spot, reason), "a normal zone takes a stall");
		spot.m_eZone = LF_ZoneRule.SAFE;
		ctx.Equal(LF_EPlacementPolicyVerdict.ZONE, LF_PlacementPolicies.CheckWhere(outsideSafe, spot, reason), "a safe zone refuses it");
		ctx.True(Says(reason, "policy stall: not in a SAFE zone"), "the zone refusal names the zone: " + reason);
		spot.m_eZone = LF_ZoneRule.LAWLESS;
		ctx.Equal(LF_EPlacementPolicyVerdict.ZONE, LF_PlacementPolicies.CheckWhere(outsideSafe, spot, reason), "the second blocked zone of the list refuses it too");

		LF_PlacementPolicy onlyLawless = LF_PlacementPolicy.Make("fence_stash");
		onlyLawless.m_sOnlyZone = "LAWLESS";
		ctx.Equal(LF_EPlacementPolicyVerdict.ALLOWED, LF_PlacementPolicies.CheckWhere(onlyLawless, spot, reason), "a lawless zone takes it");
		spot.m_eZone = LF_ZoneRule.NORMAL;
		ctx.Equal(LF_EPlacementPolicyVerdict.ZONE, LF_PlacementPolicies.CheckWhere(onlyLawless, spot, reason), "a normal zone does not");
		ctx.True(Says(reason, "policy fence_stash: only in a LAWLESS zone"), "the only-zone refusal names the rule: " + reason);

		LF_PlacementPolicy house = LF_PlacementPolicy.Make("chair");
		house.m_sPlaceKind = "house";
		house.m_fPlaceRadius = 20;
		ctx.Equal(LF_EPlacementPolicyVerdict.PLACE, LF_PlacementPolicies.CheckWhere(house, spot, reason), "no house placed: refused");
		spot.m_fPlace = 35;
		ctx.Equal(LF_EPlacementPolicyVerdict.PLACE, LF_PlacementPolicies.CheckWhere(house, spot, reason), "35 m from the house is outside 20 m");
		ctx.True(Says(reason, "policy chair: not within 20 m of a house"), "the place refusal names the kind and radius: " + reason);
		spot.m_fPlace = 12;
		ctx.Equal(LF_EPlacementPolicyVerdict.ALLOWED, LF_PlacementPolicies.CheckWhere(house, spot, reason), "12 m from the house is inside");

		LF_PlacementPolicy offRoad = LF_PlacementPolicy.Make("tent");
		offRoad.m_sRoad = LF_PlacementPolicy.ROAD_OFF;
		LF_PlacementPolicy onRoad = LF_PlacementPolicy.Make("cone");
		onRoad.m_sRoad = LF_PlacementPolicy.ROAD_ON;
		ctx.Equal(LF_EPlacementPolicyVerdict.ALLOWED, LF_PlacementPolicies.CheckWhere(offRoad, spot, reason), "a tent off the road");
		ctx.Equal(LF_EPlacementPolicyVerdict.ROAD, LF_PlacementPolicies.CheckWhere(onRoad, spot, reason), "a road-only cone off the road is refused");
		ctx.True(Says(reason, "policy cone: only on a road"), "the road refusal names the rule: " + reason);
		spot.m_bRoad = true;
		ctx.Equal(LF_EPlacementPolicyVerdict.ROAD, LF_PlacementPolicies.CheckWhere(offRoad, spot, reason), "a tent across a road is refused");
		ctx.True(Says(reason, "policy tent: not on a road"), "the not-on-road refusal names the rule: " + reason);
		ctx.Equal(LF_EPlacementPolicyVerdict.ALLOWED, LF_PlacementPolicies.CheckWhere(onRoad, spot, reason), "a cone on the road");
	}

	//------------------------------------------------------------------------------------------------
	protected void TestCost(LF_TestContext ctx)
	{
		string reason;
		LF_PlacementPolicy paid = LF_PlacementPolicy.Make("stall");
		paid.m_iCash = 150;
		LF_StubPayer payer = new LF_StubPayer();
		payer.m_iCash = 100;
		ctx.Equal(LF_EPlacementPolicyVerdict.CANNOT_PAY, LF_PlacementPolicies.CheckCost(paid, payer, reason), "100 cash cannot pay 150");
		ctx.True(Says(reason, "policy stall: costs 150 cash, the player has 100"), "the cost refusal names the price: " + reason);
		payer.m_iCash = 150;
		ctx.Equal(LF_EPlacementPolicyVerdict.ALLOWED, LF_PlacementPolicies.CheckCost(paid, payer, reason), "150 cash pays 150");

		LF_PlacementPolicy material = LF_PlacementPolicy.Make("barricade");
		material.m_CostItem = TOKEN;
		material.m_iCostItemCount = 2;
		payer.m_iItems = 1;
		ctx.Equal(LF_EPlacementPolicyVerdict.CANNOT_PAY, LF_PlacementPolicies.CheckCost(material, payer, reason), "one item cannot pay two");
		payer.m_iItems = 2;
		ctx.Equal(LF_EPlacementPolicyVerdict.ALLOWED, LF_PlacementPolicies.CheckCost(material, payer, reason), "two items pay two");

		LF_PlacementPolicy free = LF_PlacementPolicy.Make("cone");
		payer.m_iCash = 0;
		payer.m_iItems = 0;
		ctx.Equal(LF_EPlacementPolicyVerdict.ALLOWED, LF_PlacementPolicies.CheckCost(free, payer, reason), "a free kind needs nothing");

		// Judge runs who, where, cost in that order: the first refusal is the answer.
		LF_PlacementPolicy all = LF_PlacementPolicy.Make("all");
		all.m_sJob = "POLICE";
		all.m_sBlockedZones = "SAFE";
		all.m_iCash = 10;
		LF_StubPlacer civilian = new LF_StubPlacer();
		LF_StubSpot safe = new LF_StubSpot();
		safe.m_eZone = LF_ZoneRule.SAFE;
		ctx.Equal(LF_EPlacementPolicyVerdict.NEEDS_JOB, LF_PlacementPolicies.Judge(all, civilian, safe, payer, reason), "who is judged before where and cost");
		LF_StubPlacer officer = new LF_StubPlacer();
		officer.m_iPoliceRank = 0;
		ctx.Equal(LF_EPlacementPolicyVerdict.ZONE, LF_PlacementPolicies.Judge(all, officer, safe, payer, reason), "where is judged before cost");
		LF_StubSpot normal = new LF_StubSpot();
		ctx.Equal(LF_EPlacementPolicyVerdict.CANNOT_PAY, LF_PlacementPolicies.Judge(all, officer, normal, payer, reason), "cost last");
		payer.m_iCash = 10;
		ctx.Equal(LF_EPlacementPolicyVerdict.ALLOWED, LF_PlacementPolicies.Judge(all, officer, normal, payer, reason), "every rule met");
	}

	//------------------------------------------------------------------------------------------------
	protected LF_PlacementPolicyConfig MakeConfig()
	{
		LF_PlacementPolicyConfig config = new LF_PlacementPolicyConfig();
		config.m_aPolicies = {};
		LF_PlacementPolicy strip = LF_PlacementPolicy.Make("spike_strip");
		strip.m_sJob = "POLICE";
		config.m_aPolicies.Insert(strip);
		config.m_aPolicies.Insert(LF_PlacementPolicy.Make("chair"));
		return config;
	}

	//------------------------------------------------------------------------------------------------
	//! The override model: the owner's section patches, adds and removes policies by id.
	protected void TestOverrides(LF_TestContext ctx)
	{
		LF_PlacementPolicyConfig config = MakeConfig();
		string json = "{\"policies\":{";
		json += "\"spike_strip\":{\"op\":\"patch\",\"minRank\":2,\"cash\":40},";
		json += "\"market_stall\":{\"op\":\"add\",\"placeKind\":\"market\",\"placeRadius\":15,\"blockedZones\":\"SAFE\",\"cash\":25,\"road\":\"off\"},";
		json += "\"chair\":{\"op\":\"remove\"},";
		json += "\"ghost\":{\"op\":\"patch\",\"cash\":1}}}";
		LF_ConfigService.Reset();
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1,\"placement\":" + json + "}", "test");
		LF_ConfigReader reader = LF_ConfigService.OpenOwnerSection(LF_PlacementPolicyConfig.SECTION);
		ctx.NotNull(reader, "the placement section opens");
		if (!reader)
			return;

		config.ApplyOwner(reader, "test-owner.json");
		LF_PlacementPolicy strip = config.Find("spike_strip");
		ctx.NotNull(strip, "the strip's policy stays");
		if (strip)
		{
			ctx.Equal(2, strip.m_iMinRank, "the owner raised the strip's rank");
			ctx.Equal(40, strip.m_iCash, "the owner priced the strip");
			ctx.EqualStr("POLICE", strip.m_sJob, "a patch keeps the fields it does not name");
		}

		LF_PlacementPolicy stall = config.Find("market_stall");
		ctx.NotNull(stall, "the owner added a market stall policy");
		if (stall)
		{
			ctx.EqualStr("market", stall.m_sPlaceKind, "the stall's place kind");
			ctx.InRange(stall.m_fPlaceRadius, 14.9, 15.1, "the stall's radius");
			ctx.EqualStr("off", stall.m_sRoad, "the stall's road rule");
			ctx.Equal(25, stall.m_iCash, "the stall's price");
		}

		ctx.True(config.Find("chair") == null, "the owner removed the chair policy");
		ctx.True(config.Find("ghost") == null, "a patch of an unknown id is refused, not added");
		ctx.Equal(2, config.m_aPolicies.Count(), "the list holds the strip and the stall");

		LF_PlacementPolicyConfig.UseForTest(config);
		bool known;
		ctx.True(LF_PlacementPolicies.Find("market_stall", known) != null && known, "the service finds a layered policy by id");
		ctx.True(LF_PlacementPolicies.Find("chair", known) == null && !known, "a removed policy is unknown to the service, so its placeable is refused");
		ctx.True(LF_PlacementPolicies.Find("", known) == null && known, "a placeable with no policy is anyone, anywhere, free");
		LF_PlacementPolicyConfig.Reload();
		LF_PlacementPolicies.ResetStatics();
		LF_ConfigService.Reset();
	}
}

//------------------------------------------------------------------------------------------------
class LF_StubPlacer : LF_BuyerFacts
{
	int m_iPoliceRank = -1;
	bool m_bLicence;
	int m_iOrgRights;

	override bool HasLicence(LF_ELicenseType licence)
	{
		return m_bLicence;
	}

	override int JobRank(string job)
	{
		if (job == "POLICE")
			return m_iPoliceRank;

		return -1;
	}

	override int OrgRights()
	{
		return m_iOrgRights;
	}
}

//------------------------------------------------------------------------------------------------
class LF_StubSpot : LF_PlacementSpotFacts
{
	LF_ZoneRule m_eZone;
	float m_fPlace = -1;
	bool m_bRoad;

	override LF_ZoneRule ZoneRule()
	{
		return m_eZone;
	}

	override float NearestPlace(string kind)
	{
		return m_fPlace;
	}

	override bool OnRoad()
	{
		return m_bRoad;
	}
}

//------------------------------------------------------------------------------------------------
class LF_StubPayer : LF_PlacementPayer
{
	int m_iCash;
	int m_iItems;

	override int Cash()
	{
		return m_iCash;
	}

	override int ItemCount(ResourceName prefab)
	{
		return m_iItems;
	}
}
