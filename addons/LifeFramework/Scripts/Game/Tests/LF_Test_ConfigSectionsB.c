// red-proof: observed 2026-11-13 with `tools/cli test --no-build --only LF_Test_ConfigSectionsB`: removing the `fineMultiplier` read from LF_ZoneConfig.ApplyOwner failed `- the owner's fine multiplier applies to SAFE (expected 2, got 1)`. Restored; green.
// tier: LOGIC
//! #283 (config sections B): the owner's `zones`, `organizations` and `persistence` sections change
//! one field of a shipped value each; a bad value is refused with the shipped one kept; a changed
//! default rank right reaches a new organization's ladder; and `organizations` reaches a client
//! through the config stream.
class LF_Test_ConfigSectionsB : LF_Test
{
	override string GetName()
	{
		return "config/sections-b";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_OrgConfig ShippedOrg()
	{
		Resource container = BaseContainerTools.LoadContainer(LF_OrgConfig.CONFIG_PATH);
		if (!container || !container.GetResource())
			return null;

		return LF_OrgConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
	}

	//------------------------------------------------------------------------------------------------
	protected LF_OrgConfig MergeOrg(string sectionJson)
	{
		LF_ConfigService.Reset();
		LF_ConfigStream.Reset();
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1,\"organizations\":" + sectionJson + "}", "test");
		LF_OrgConfig config = ShippedOrg();
		if (config)
			config.ApplyLayers();

		return config;
	}

	//------------------------------------------------------------------------------------------------
	protected void ZoneOwner(string sectionJson)
	{
		LF_ConfigService.Reset();
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1,\"zones\":" + sectionJson + "}", "test");
		LF_ZoneConfig.Reload();
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// Zones: a rule's fields merge by rule name; the rest of the table is untouched.
		ZoneOwner("{\"ruleSets\":{\"SAFE\":{\"fineMultiplier\":2,\"allowsRobbery\":false},\"LAWLESS\":{\"suppressesWanted\":true},\"NOT_A_RULE\":{\"copsEnter\":false}}}");
		LF_ZoneRuleSet safe = LF_ZoneConfig.RuleSetFor(LF_ZoneRule.SAFE);
		ctx.EqualFloat(2, safe.m_fFineMultiplier, 0.001, "the owner's fine multiplier applies to SAFE");
		ctx.False(safe.m_bAllowsRobbery, "the owner's robbery flag applies to SAFE");
		ctx.True(safe.m_bBlocksPlayerDamage, "a SAFE field the owner left out keeps the shipped value");
		ctx.True(LF_ZoneConfig.RuleSetFor(LF_ZoneRule.LAWLESS).m_bSuppressesWanted, "the owner's wanted flag applies to LAWLESS");
		ctx.False(LF_ZoneConfig.RuleSetFor(LF_ZoneRule.NORMAL).m_bSuppressesWanted, "a rule the owner left out is untouched");

		// Zones: an out-of-range number is refused and the shipped one stays.
		ZoneOwner("{\"ruleSets\":{\"SAFE\":{\"fineMultiplier\":50,\"copsEnter\":false}}}");
		safe = LF_ZoneConfig.RuleSetFor(LF_ZoneRule.SAFE);
		ctx.EqualFloat(1, safe.m_fFineMultiplier, 0.001, "a fine multiplier above the limit is refused");
		ctx.False(safe.m_bCopsEnter, "the valid field of the same record applies");

		// Zones: the legacy flags are read too (the shipped table has entries, so they only matter for a rule with none).
		ZoneOwner("{\"lawlessSuppressesWanted\":true,\"safeBlocksPlayerDamage\":false}");
		LF_ZoneConfig zones = LF_ZoneConfig.Get();
		ctx.NotNull(zones, "the zone config loads");
		if (zones)
		{
			ctx.True(zones.m_bLawlessSuppressesWanted, "the owner's legacy LAWLESS flag is read");
			ctx.False(zones.m_bSafeBlocksPlayerDamage, "the owner's legacy SAFE flag is read");
		}

		// Organizations: fields merge, a range and a name-length contradiction are refused.
		LF_OrgConfig org = MergeOrg("{\"foundingFee\":900,\"priceParkedVehicle\":123,\"deskRange\":99,\"nameMin\":40,\"bogus\":1}");
		ctx.NotNull(org, "the organization config loads");
		if (!org)
			return;

		ctx.Equal(900, org.m_iFoundingFee, "the owner's founding fee applies");
		ctx.Equal(123, org.m_iPriceParkedVehicle, "the owner's parked vehicle price applies");
		ctx.Equal(LF_OrgConfig.DEFAULT_PRICE_EXTRA_STORAGE, org.m_iPriceExtraStorage, "a field the owner left out keeps the shipped value");
		ctx.EqualFloat(LF_OrgConfig.DEFAULT_DESK_RANGE, org.m_fDeskRange, 0.001, "a desk range above the limit is refused");
		ctx.Equal(LF_OrgConfig.DEFAULT_NAME_MIN, org.m_iNameMin, "a name minimum above the maximum is refused");

		// Organizations: the default ranks' rights, through the function a new organization uses.
		org = MergeOrg("{\"rankDefaults\":{\"officer\":{\"withdraw\":true,\"kick\":false},\"captain\":{\"invite\":true}}}");
		LF_OrgConfig.UseForTest(org);
		int shippedOfficer = LF_OrganizationConfig.ShippedRights(LF_OrganizationConfig.RANK_OFFICER);
		int officer = LF_OrganizationConfig.DefaultRights(LF_OrganizationConfig.RANK_OFFICER);
		int kickBit = LF_OrgRights.Bit(LF_EOrganizationPermission.KICK);
		ctx.True((shippedOfficer & kickBit) != 0, "the shipped officer may kick, so the test can turn it off");
		int expected = (shippedOfficer | LF_OrgRights.Bit(LF_EOrganizationPermission.WITHDRAW)) - kickBit;
		ctx.Equal(expected, officer, "the officer rung gets the owner's rights over the shipped ones");
		ctx.Equal(LF_OrganizationConfig.ShippedRights(LF_OrganizationConfig.RANK_MEMBER), LF_OrganizationConfig.DefaultRights(LF_OrganizationConfig.RANK_MEMBER), "a rung the owner left out keeps its shipped rights");
		ctx.Equal(0, LF_OrganizationConfig.DefaultRights("captain"), "an id outside the default ladder is refused and gets no rights");
		LF_OrgConfig.Reload();

		// Organizations on a client: the server's values reach a client that has only the shipped ones.
		LF_OrgConfig server = MergeOrg("{\"foundingFee\":900,\"nameMax\":20}");
		string payload = LF_ConfigStream.BuildPayload(null, true, null, null, server);
		LF_ConfigService.Reset();
		LF_ConfigStream.Reset();
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1}", "test-client");
		ctx.Equal(LF_EConfigStreamStatus.APPLIED, LF_ConfigStream.Receive(LF_ConfigStream.FORMAT_VERSION, payload.Hash(), payload), "the client accepts the stream");
		LF_OrgConfig client = ShippedOrg();
		if (client)
		{
			client.ApplyLayers();
			ctx.Equal(900, client.m_iFoundingFee, "the owner's founding fee reaches the client");
			ctx.Equal(20, client.m_iNameMax, "the owner's name limit reaches the client");
		}

		// Persistence: the owner's autosave settings over the component's own values.
		bool enabled = true;
		float interval = 600;
		LF_ConfigService.Reset();
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1,\"persistence\":{\"autosaveInterval\":120}}", "test");
		LF_PersistenceConfig.Resolve(enabled, interval);
		ctx.True(enabled, "the autosave flag the owner left out keeps the component's value");
		ctx.EqualFloat(120, interval, 0.001, "the owner's autosave interval applies");

		enabled = true;
		interval = 600;
		LF_ConfigService.Reset();
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1,\"persistence\":{\"autosaveInterval\":5,\"enableAutosave\":false}}", "test");
		LF_PersistenceConfig.Resolve(enabled, interval);
		ctx.False(enabled, "the owner can turn the autosave off");
		ctx.EqualFloat(600, interval, 0.001, "an interval below the limit is refused");

		LF_ConfigService.Reset();
		LF_ConfigStream.Reset();
		LF_ZoneConfig.Reload();
		LF_OrgConfig.Reload();
	}
}
