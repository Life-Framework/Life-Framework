// red-proof: observed 2026-11-13 with `tools/cli test --no-build --only LF_Test_ConfigSectionsA`: removing the `arrestReach` read from LF_PoliceConfig.ApplyOwner failed `- the owner's arrest reach applies (expected 7, got 5)` and `- the owner's arrest reach reaches the client (expected 7, got 5)`. Restored; green.
// tier: LOGIC
//! #282 (config sections A): the owner's `police` and `licences` sections change one field of a
//! shipped record each, an out-of-range number, a prefab that does not load and a key that is no
//! licence are refused with the shipped value kept, and `police` reaches a client through the
//! config stream with the ranks in ladder order.
class LF_Test_ConfigSectionsA : LF_Test
{
	override string GetName()
	{
		return "config/sections-a";
	}

	//------------------------------------------------------------------------------------------------
	//! The shipped police config, with no layer applied.
	protected LF_PoliceConfig ShippedPolice()
	{
		Resource container = BaseContainerTools.LoadContainer(LF_PoliceConfig.CONFIG_PATH);
		if (!container || !container.GetResource())
			return null;

		return LF_PoliceConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
	}

	//------------------------------------------------------------------------------------------------
	protected LF_PoliceConfig MergePolice(string sectionJson)
	{
		LF_ConfigService.Reset();
		LF_ConfigStream.Reset();
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1,\"police\":" + sectionJson + "}", "test");
		LF_PoliceConfig config = ShippedPolice();
		if (config)
			config.ApplyLayers();

		return config;
	}

	//------------------------------------------------------------------------------------------------
	protected map<LF_ELicenseType, ref LF_LicenseConfig> Catalog()
	{
		map<LF_ELicenseType, ref LF_LicenseConfig> catalog = new map<LF_ELicenseType, ref LF_LicenseConfig>();
		LF_LicenseConfig unemployed = new LF_LicenseConfig();
		unemployed.m_bIsInitialLicense = true;
		catalog.Set(LF_ELicenseType.UNEMPLOYED, unemployed);
		LF_LicenseConfig policeAccess = new LF_LicenseConfig();
		policeAccess.m_bRequiresWhitelist = true;
		catalog.Set(LF_ELicenseType.POLICE_ACCESS, policeAccess);
		catalog.Set(LF_ELicenseType.MINER_GOLD, new LF_LicenseConfig());
		return catalog;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// Police: fields merge, a rank merges by id, an unlisted id appends a rung.
		LF_PoliceConfig config = MergePolice("{\"arrestReach\":7,\"ticketMaxOpen\":5,\"bogus\":1,\"ranks\":{\"patrol\":{\"fineCap\":1500},\"captain\":{\"fineCap\":9000,\"titleKey\":\"Test-Rank_Captain\"}}}");
		ctx.NotNull(config, "the police config loads");
		if (!config)
			return;

		ctx.EqualFloat(7, config.m_fArrestReach, 0.001, "the owner's arrest reach applies");
		ctx.Equal(5, config.m_iTicketMaxOpen, "the owner's open ticket limit applies");
		ctx.EqualFloat(LF_PoliceConfig.DEFAULT_PLACE_RADIUS, config.m_fPlaceRadius, 0.001, "a field the owner left out keeps the shipped value");
		LF_PoliceRank patrol = config.FindRank("patrol");
		ctx.NotNull(patrol, "the patrol rank is still there");
		if (patrol)
		{
			ctx.Equal(1500, patrol.m_iFineCap, "the owner's fine cap applies to patrol");
			ctx.Equal(3600, patrol.m_iDutySecondsForNext, "a rank field the owner left out keeps the shipped value");
			ctx.Equal(2, patrol.m_aKit.Count(), "the patrol kit is untouched");
		}
		LF_PoliceRank senior = config.FindRank("senior");
		ctx.True(senior && senior.m_iFineCap == 5000, "a rank the owner left out is untouched");
		ctx.Equal(3, config.m_aRanks.Count(), "an unlisted rank id appends a rung");
		LF_PoliceRank captain = config.FindRank("captain");
		ctx.True(captain && captain.m_iFineCap == 9000 && captain.m_sTitleKey == "Test-Rank_Captain", "the appended rung has the owner's fields");
		ctx.Equal(2, config.m_aRanks.Find(captain), "the appended rung is last, so saved ranks keep their index");

		// Police: range, type and resource checks keep the shipped value and let the rest apply.
		config = MergePolice("{\"arrestReach\":500,\"ticketMaxOpen\":\"many\",\"placeRadius\":120,\"alarmPrefab\":\"{0000000000000000}Prefabs/Police/Not_There.et\",\"loudAlarmPrefab\":\"" + LF_PoliceConfig.DEFAULT_ALARM_PREFAB + "\"}");
		if (config)
		{
			ctx.EqualFloat(LF_PoliceConfig.DEFAULT_ARREST_REACH, config.m_fArrestReach, 0.001, "an arrest reach above the limit is refused");
			ctx.Equal(LF_PoliceConfig.DEFAULT_TICKET_MAX_OPEN, config.m_iTicketMaxOpen, "a ticket limit of the wrong type is refused");
			ctx.EqualFloat(120, config.m_fPlaceRadius, 0.001, "a valid field beside the bad ones applies");
			ctx.EqualStr(LF_PoliceConfig.DEFAULT_ALARM_PREFAB, config.m_sAlarmPrefab, "an alarm prefab that does not load is refused");
			ctx.EqualStr(LF_PoliceConfig.DEFAULT_ALARM_PREFAB, config.m_sLoudAlarmPrefab, "an alarm prefab that loads applies");
		}

		// Through the live singleton: the arrest reach the actions read.
		LF_ConfigService.Reset();
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1,\"police\":{\"arrestReach\":9,\"arrestMinRank\":1}}", "test");
		LF_PoliceConfig.Reload();
		ctx.EqualFloat(9, LF_PoliceConfig.ArrestReach(), 0.001, "LF_PoliceConfig.ArrestReach reads the owner's value");
		ctx.Equal(1, LF_PoliceConfig.ArrestMinRank(), "LF_PoliceConfig.ArrestMinRank reads the owner's value");

		// Police on a client: the server's values and its ladder reach a client that has only the shipped ones.
		LF_PoliceConfig server = MergePolice("{\"arrestReach\":7,\"applyDeskRange\":4,\"ranks\":{\"patrol\":{\"fineCap\":1500},\"captain\":{\"fineCap\":9000}}}");
		string payload = LF_ConfigStream.BuildPayload(null, true, null, server);
		LF_ConfigService.Reset();
		LF_ConfigStream.Reset();
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1}", "test-client");
		ctx.Equal(LF_EConfigStreamStatus.APPLIED, LF_ConfigStream.Receive(LF_ConfigStream.FORMAT_VERSION, payload.Hash(), payload), "the client accepts the stream");
		LF_PoliceConfig client = ShippedPolice();
		if (client)
		{
			client.ApplyLayers();
			ctx.EqualFloat(7, client.m_fArrestReach, 0.001, "the owner's arrest reach reaches the client");
			ctx.EqualFloat(4, client.m_fApplyDeskRange, 0.001, "the owner's apply desk range reaches the client");
			ctx.Equal(3, client.m_aRanks.Count(), "the client has the server's ladder");
			LF_PoliceRank clientPatrol = client.FindRank("patrol");
			ctx.True(clientPatrol && clientPatrol.m_iFineCap == 1500, "the owner's fine cap reaches the client");
			if (clientPatrol)
				ctx.Equal(2, clientPatrol.m_aKit.Count(), "the client keeps its shipped kit: the stream does not carry it");
		}
		ctx.False(payload.Contains("alarmPrefab"), "the stream leaves out the alarm prefabs");

		// Licences: flags merge per licence name; a name that is no licence is refused.
		LF_ConfigService.Reset();
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1,\"licences\":{\"rules\":{\"POLICE_ACCESS\":{\"requiresWhitelist\":false},\"MINER_GOLD\":{\"isInitialLicense\":true},\"NOT_A_LICENCE\":{\"isInitialLicense\":true}}}}", "test");
		map<LF_ELicenseType, ref LF_LicenseConfig> catalog = Catalog();
		LF_LicencesConfig.ApplyTo(catalog);
		ctx.False(catalog.Get(LF_ELicenseType.POLICE_ACCESS).m_bRequiresWhitelist, "the owner lifts the whitelist off a licence");
		ctx.True(catalog.Get(LF_ELicenseType.MINER_GOLD).m_bIsInitialLicense, "the owner makes a licence initial");
		ctx.True(catalog.Get(LF_ELicenseType.UNEMPLOYED).m_bIsInitialLicense, "a licence the owner left out keeps its flags");
		array<LF_ELicenseType> initial = {};
		LF_LicencesConfig.InitialLicences(catalog, initial);
		ctx.Equal(2, initial.Count(), "two licences are initial now");
		if (initial.Count() == 2)
		{
			ctx.Equal(LF_ELicenseType.UNEMPLOYED, initial[0], "initial licences come in catalog order, first");
			ctx.Equal(LF_ELicenseType.MINER_GOLD, initial[1], "initial licences come in catalog order, second");
		}

		LF_ConfigService.Reset();
		LF_ConfigStream.Reset();
		LF_PoliceConfig.Reload();
	}
}
