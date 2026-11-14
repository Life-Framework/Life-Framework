// red-proof: observed 2026-11-14 with `tools\cli test --no-build --local --only LF_Test_ConfigListsOverlay`: making LF_PoliceRankList.RemoveRefusal skip the shipped-rung check (`index < 0`) failed `- police: the last rung is still refused when it is a shipped one` and `- police: that remove is a refused record (expected 1, got 0)`. Restored; green.
// tier: LOGIC
//! #1108: police ranks, organization rank defaults, zone rule sets, licence rules and the system
//! catalog on the override model (#962). Per list: `op: remove` takes an entry out (or is refused
//! with a reason where removing makes no sense), and `op: patch` of an id no lower layer has is
//! refused, not added. Police ranks also keep every saved index: a shipped rung and a rung under
//! another cannot go.
class LF_Test_ConfigListsOverlay : LF_Test
{
	override string GetName()
	{
		return "config/lists-overlay";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_ConfigReader Layer(string section, string sectionJson)
	{
		LF_ConfigService.Reset();
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1,\"" + section + "\":" + sectionJson + "}", "test");
		return LF_ConfigService.OpenOwnerSection(section);
	}

	//------------------------------------------------------------------------------------------------
	protected LF_PoliceConfig MakePolice()
	{
		LF_PoliceConfig config = new LF_PoliceConfig();
		config.m_aRanks = {};
		config.m_aRanks.Insert(LF_PoliceRank.Create("patrol", "EL-Rank_Patrol", 500, -1, 100, ResourceName.Empty));
		config.m_aRanks.Insert(LF_PoliceRank.Create("senior", "EL-Rank_Senior", 5000, -1, 0, ResourceName.Empty));
		config.m_iShippedRanks = 2;
		return config;
	}

	//------------------------------------------------------------------------------------------------
	protected void Police(LF_TestContext ctx)
	{
		LF_PoliceConfig config = MakePolice();
		string json = "{\"ranks\":{";
		json += "\"chief\":{\"op\":\"add\",\"fineCap\":9000},";
		json += "\"deputy\":{\"op\":\"add\",\"fineCap\":7000},";
		json += "\"patrol\":{\"op\":\"remove\"},";
		json += "\"senior\":{\"op\":\"remove\"},";
		json += "\"ghost\":{\"op\":\"patch\",\"fineCap\":1}";
		json += "}}";
		LF_ConfigReader reader = Layer("police", json);
		config.ApplyOwner(reader);

		ctx.True(config.FindRank("patrol") != null, "police: a shipped rung cannot be removed");
		ctx.True(config.FindRank("senior") != null, "police: the last shipped rung cannot be removed either");
		ctx.True(config.FindRank("ghost") == null, "police: patch of a missing rank id is refused, not added");
		ctx.Equal(4, config.m_aRanks.Count(), "police: two shipped rungs plus chief and deputy");
		ctx.Equal(2, config.m_aRanks.Find(config.FindRank("chief")), "police: an added rung is appended, so no index moves");

		// The last rung of a ladder that adds nothing is shipped: still not removable.
		LF_PoliceConfig shipped = MakePolice();
		reader = Layer("police", "{\"ranks\":{\"senior\":{\"op\":\"remove\"}}}");
		shipped.ApplyOwner(reader);
		ctx.True(shipped.FindRank("senior") != null, "police: the last rung is still refused when it is a shipped one");
		ctx.Equal(1, reader.GetRejectedCount(), "police: that remove is a refused record");

		// A rung under another cannot go (it would move the saved index above it); the last one can.
		reader = Layer("police", "{\"ranks\":{\"chief\":{\"op\":\"remove\"}}}");
		config.ApplyOwner(reader);
		ctx.True(config.FindRank("chief") != null, "police: removing a rung that is not the last is refused");
		reader = Layer("police", "{\"ranks\":{\"deputy\":{\"op\":\"remove\"}}}");
		config.ApplyOwner(reader);
		ctx.True(config.FindRank("deputy") == null, "police: the last added rung can be removed");
		ctx.Equal(3, config.m_aRanks.Count(), "police: three rungs left");
		ctx.Equal(2, config.m_aRanks.Find(config.FindRank("chief")), "police: chief keeps its index");

		reader = Layer("police", "{\"ranks\":{\"senior\":{\"op\":\"replace\",\"fineCap\":123}}}");
		config.ApplyOwner(reader);
		LF_PoliceRank senior = config.FindRank("senior");
		if (senior)
		{
			ctx.Equal(123, senior.m_iFineCap, "police: replace applies the given field");
			ctx.Equal(0, senior.m_iDutySecondsForNext, "police: replace drops what the layer leaves out");
		}
		ctx.Equal(1, config.m_aRanks.Find(senior), "police: replace keeps the rung's index");
	}

	//------------------------------------------------------------------------------------------------
	protected void Organizations(LF_TestContext ctx)
	{
		LF_OrgConfig config = new LF_OrgConfig();
		config.m_aRankDefaults = {};
		config.m_aRankDefaults.Insert(LF_OrgRankDefault.FromRights("member", LF_OrganizationConfig.ShippedRights("member")));
		config.m_aRankDefaults.Insert(LF_OrgRankDefault.FromRights("officer", LF_OrganizationConfig.ShippedRights("officer")));

		string json = "{\"rankDefaults\":{";
		json += "\"officer\":{\"op\":\"remove\"},";
		json += "\"member\":{\"op\":\"patch\",\"withdraw\":true},";
		json += "\"founder\":{\"op\":\"patch\",\"kick\":false},";
		json += "\"cook\":{\"op\":\"add\",\"withdraw\":true}";
		json += "}}";
		LF_ConfigReader reader = Layer("organizations", json);
		config.ApplyOwner(reader);

		ctx.True(config.FindRankDefault("officer") == null, "organizations: remove drops the entry, so the rank is back to its shipped rights");
		LF_OrgRankDefault member = config.FindRankDefault("member");
		ctx.True(member && member.m_bWithdraw && member.m_bUseStash, "organizations: patch changes the field given and keeps the rest");
		ctx.True(config.FindRankDefault("founder") == null, "organizations: patch of a rank with no entry is refused, not added");
		ctx.True(config.FindRankDefault("cook") == null, "organizations: a key that is not a default rank is refused");
		ctx.Equal(1, config.m_aRankDefaults.Count(), "organizations: one entry left");
		ctx.Equal(2, reader.GetRejectedCount(), "organizations: two records refused (patch of a rank with no entry, a key that is not a rank)");
	}

	//------------------------------------------------------------------------------------------------
	protected void Zones(LF_TestContext ctx)
	{
		LF_ZoneConfig config = LF_ZoneConfig.CreateDefault();
		config.m_aRuleSets = {};
		LF_ZoneRuleSet safe = LF_ZoneRuleSet.CreateDefault(LF_ZoneRule.SAFE);
		safe.m_bBlocksPlayerDamage = true;
		config.m_aRuleSets.Insert(safe);

		string json = "{\"ruleSets\":{";
		json += "\"SAFE\":{\"op\":\"remove\"},";
		json += "\"LAWLESS\":{\"op\":\"patch\",\"suppressesWanted\":true},";
		json += "\"NORMAL\":{\"op\":\"add\",\"fineMultiplier\":2}";
		json += "}}";
		LF_ConfigReader reader = Layer("zones", json);
		config.ApplyOwner(reader);

		ctx.True(config.FindRuleSet(LF_ZoneRule.SAFE) != null, "zones: remove of a rule set is refused (every zone names a rule)");
		ctx.True(config.FindRuleSet(LF_ZoneRule.LAWLESS) == null, "zones: patch of a rule with no entry is refused, not added");
		LF_ZoneRuleSet normal = config.FindRuleSet(LF_ZoneRule.NORMAL);
		ctx.True(normal != null, "zones: add of a rule with no entry adds it");
		if (normal)
			ctx.EqualFloat(2, normal.m_fFineMultiplier, 0.001, "zones: the added entry takes the given field");

		reader = Layer("zones", "{\"ruleSets\":{\"SAFE\":{\"op\":\"replace\",\"allowsRobbery\":false}}}");
		config.ApplyOwner(reader);
		safe = config.FindRuleSet(LF_ZoneRule.SAFE);
		if (safe)
		{
			ctx.False(safe.m_bAllowsRobbery, "zones: replace applies the given field");
			ctx.True(safe.m_bBlocksPlayerDamage, "zones: replace rebuilds from the shipped defaults (SAFE blocks damage)");
		}
	}

	//------------------------------------------------------------------------------------------------
	protected map<LF_ELicenseType, ref LF_LicenseConfig> Catalog()
	{
		map<LF_ELicenseType, ref LF_LicenseConfig> catalog = new map<LF_ELicenseType, ref LF_LicenseConfig>();
		catalog.Set(LF_ELicenseType.UNEMPLOYED, new LF_LicenseConfig());
		LF_LicenseConfig policeAccess = new LF_LicenseConfig();
		policeAccess.m_bRequiresWhitelist = true;
		catalog.Set(LF_ELicenseType.POLICE_ACCESS, policeAccess);
		catalog.Set(LF_ELicenseType.MINER_GOLD, new LF_LicenseConfig());
		catalog.Set(LF_ELicenseType.MINER_COAL, new LF_LicenseConfig());
		return catalog;
	}

	//------------------------------------------------------------------------------------------------
	protected void Licences(LF_TestContext ctx)
	{
		map<LF_ELicenseType, ref LF_LicenseConfig> catalog = Catalog();
		string json = "{\"rules\":{";
		json += "\"POLICE_ACCESS\":{\"op\":\"replace\",\"isInitialLicense\":true},";
		json += "\"MINER_GOLD\":{\"op\":\"remove\"},";
		json += "\"MINER_COPPER\":{\"op\":\"patch\",\"requiresWhitelist\":true},";
		json += "\"FIREARM_PERMIT\":{\"op\":\"add\",\"requiresWhitelist\":true},";
		json += "\"NOT_A_LICENCE\":{\"op\":\"patch\",\"isInitialLicense\":true},";
		json += "\"MINER_COAL\":{\"op\":\"add\",\"isInitialLicense\":true}";
		json += "}}";
		Layer("licences", json);
		LF_LicencesConfig.ApplyTo(catalog);

		ctx.True(catalog.Get(LF_ELicenseType.POLICE_ACCESS).m_bIsInitialLicense, "licences: replace applies the given flag");
		ctx.True(catalog.Get(LF_ELicenseType.MINER_COAL).m_bIsInitialLicense == false, "licences: add of a licence the catalog has is refused");
		ctx.False(catalog.Get(LF_ELicenseType.POLICE_ACCESS).m_bRequiresWhitelist, "licences: replace drops the flag the layer leaves out");
		ctx.True(catalog.Get(LF_ELicenseType.MINER_GOLD) != null, "licences: a licence cannot be removed");
		ctx.True(catalog.Get(LF_ELicenseType.FIREARM_PERMIT) == null, "licences: a licence the catalog lacks is neither patched nor added");
		ctx.True(catalog.Get(LF_ELicenseType.MINER_COPPER) == null, "licences: patch of a licence the catalog lacks is refused");
		ctx.False(catalog.Get(LF_ELicenseType.MINER_GOLD).m_bIsInitialLicense, "licences: a refused remove changes nothing");
	}

	//------------------------------------------------------------------------------------------------
	protected void Systems(LF_TestContext ctx)
	{
		LF_SystemCatalog catalog = new LF_SystemCatalog();
		catalog.Add("Core", LF_ESystemSwitch.LOCKED);
		catalog.Add("Farming", LF_ESystemSwitch.SWITCHABLE);
		LF_SystemDef pager = catalog.Add("Pager", LF_ESystemSwitch.SWITCHABLE, "Comms");
		catalog.Add("Comms", LF_ESystemSwitch.SWITCHABLE);
		catalog.Add("Radio", LF_ESystemSwitch.SWITCHABLE).m_aRequires.Insert("Farming");

		LF_SystemDefList list = new LF_SystemDefList(catalog);
		LF_ConfigClaims claims = new LF_ConfigClaims();
		ctx.False(LF_ConfigOverlay.Apply(list, "Comms", LF_ConfigOverlay.OP_REMOVE, "Pack.conf", claims, null, null), "systems: a system with another one as part of it cannot be removed");
		ctx.True(LF_ConfigOverlay.Apply(list, "Pager", LF_ConfigOverlay.OP_REMOVE, "Pack.conf", claims, null, null), "systems: remove of a leaf system applies");
		ctx.True(catalog.Find("Pager") == null, "systems: remove takes the system out");
		ctx.False(LF_ConfigOverlay.Apply(list, "Core", LF_ConfigOverlay.OP_REMOVE, "Pack.conf", claims, null, null), "systems: a framework (LOCKED) system cannot be removed");
		ctx.False(LF_ConfigOverlay.Apply(list, "Farming", LF_ConfigOverlay.OP_REMOVE, "Pack.conf", claims, null, null), "systems: a system another one needs cannot be removed");
		ctx.False(LF_ConfigOverlay.Apply(list, "Ghost", LF_ConfigOverlay.OP_PATCH, "Pack.conf", claims, null, null), "systems: patch of an id the catalog lacks is refused");
		ctx.True(catalog.Find("Ghost") == null, "systems: nothing is added by that patch");
		ctx.Equal(4, catalog.m_aSystems.Count(), "systems: four systems left");
		ctx.True(pager != null, "systems: the removed record is still held by the test");
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		Police(ctx);
		Organizations(ctx);
		Zones(ctx);
		Licences(ctx);
		Systems(ctx);
		LF_ConfigService.Reset();
	}
}
