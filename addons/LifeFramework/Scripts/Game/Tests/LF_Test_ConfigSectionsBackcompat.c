// red-proof: observed 2026-11-13 with `tools/cli test --no-build --only LF_Test_ConfigSectionsBackcompat`: setting m_iSpikeStripLifetimeSeconds to 601 in Configs/Police/LF_Police.conf failed `- spike strip lifetime (expected 600, got 601)`. Restored; green.
// tier: LOGIC
//! #282/#283: with a config.json that names none of the new sections (or no config.json), zones,
//! police, organizations, persistence and licences hold the values the code carried before they
//! became owner sections. The sections are known top-level keys, so none warns as unknown.
class LF_Test_ConfigSectionsBackcompat : LF_Test
{
	override string GetName()
	{
		return "config/sections-backcompat";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ConfigService.Reset();
		LF_ConfigStream.Reset();
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1}", "test");
		LF_ZoneConfig.Reload();
		LF_PoliceConfig.Reload();
		LF_OrgConfig.Reload();

		// Zones: SAFE refuses player damage, nothing else is restricted, no fine multiplier.
		LF_ZoneRuleSet safe = LF_ZoneConfig.RuleSetFor(LF_ZoneRule.SAFE);
		ctx.True(safe.m_bBlocksPlayerDamage, "SAFE still blocks player damage");
		ctx.True(safe.m_bAllowsRobbery && safe.m_bAllowsVehicleTheft && safe.m_bAllowsWeaponDraw && safe.m_bCopsEnter, "SAFE still restricts nothing else");
		ctx.EqualFloat(1, safe.m_fFineMultiplier, 0.001, "SAFE fine multiplier");
		LF_ZoneRuleSet lawless = LF_ZoneConfig.RuleSetFor(LF_ZoneRule.LAWLESS);
		ctx.False(lawless.m_bSuppressesWanted, "LAWLESS still keeps wanted on");
		ctx.False(LF_ZoneConfig.RuleSetFor(LF_ZoneRule.NORMAL).m_bBlocksPlayerDamage, "NORMAL blocks nothing");

		// Police: the numbers the call sites carried.
		ctx.EqualFloat(LF_PoliceConfig.DEFAULT_ARREST_REACH, LF_PoliceConfig.ArrestReach(), 0.001, "police arrest reach");
		ctx.EqualFloat(LF_PoliceConfig.DEFAULT_CATCH_IN_ACT_RADIUS, LF_PoliceConfig.CatchInActRadius(), 0.001, "police catch-in-the-act radius");
		ctx.EqualFloat(LF_PoliceConfig.DEFAULT_PLACE_RADIUS, LF_PoliceConfig.PlaceRadius(), 0.001, "police place radius");
		ctx.EqualFloat(LF_PoliceConfig.DEFAULT_BOARD_READ_DISTANCE, LF_PoliceConfig.BoardReadDistance(), 0.001, "wanted board read distance");
		ctx.Equal(LF_PoliceConfig.DEFAULT_BOARD_MAX_POSTERS, LF_PoliceConfig.BoardMaxPosters(), "wanted board posters");
		ctx.EqualFloat(LF_PoliceConfig.DEFAULT_ALARM_SECONDS, LF_PoliceConfig.AlarmSeconds(false), 0.001, "site alarm seconds");
		ctx.EqualFloat(LF_PoliceConfig.DEFAULT_LOUD_ALARM_SECONDS, LF_PoliceConfig.AlarmSeconds(true), 0.001, "loud site alarm seconds");
		ctx.Equal(LF_PoliceConfig.DEFAULT_DISPATCH_LOG_LINES, LF_PoliceConfig.DispatchLogLines(), "dispatch log lines");
		ctx.Equal(LF_PoliceConfig.DEFAULT_TICKET_FIRST_DEADLINE, LF_PoliceConfig.TicketFirstDeadline(), "ticket first deadline");
		ctx.Equal(LF_PoliceConfig.DEFAULT_TICKET_SECOND_DEADLINE, LF_PoliceConfig.TicketSecondDeadline(), "ticket second deadline");
		ctx.EqualFloat(LF_PoliceConfig.DEFAULT_TICKET_LATE_MULTIPLIER, LF_PoliceConfig.TicketLateMultiplier(), 0.001, "ticket late multiplier");
		ctx.Equal(LF_PoliceConfig.DEFAULT_SEARCH_COOLDOWN_SECONDS, LF_PoliceConfig.SearchCooldownSeconds(), "search cooldown");
		ctx.Equal(LF_PoliceConfig.DEFAULT_TICKET_MAX_OPEN, LF_PoliceConfig.TicketMaxOpen(), "open tickets");
		ctx.Equal(LF_PoliceConfig.DEFAULT_TICKET_KEEP_PAID, LF_PoliceConfig.TicketKeepPaid(), "paid tickets kept");
		ctx.EqualFloat(LF_PoliceConfig.DEFAULT_JAIL_ARRIVAL_RADIUS, LF_PoliceConfig.JailArrivalRadius(), 0.001, "jail arrival radius");
		ctx.EqualFloat(LF_PoliceConfig.DEFAULT_APPLY_DESK_RANGE, LF_PoliceConfig.ApplyDeskRange(), 0.001, "apply desk range");
		ctx.Equal(LF_PoliceConfig.DEFAULT_COMPLIANT_SECONDS, LF_PoliceConfig.CompliantSeconds(), "compliant seconds");
		ctx.Equal(LF_PoliceConfig.DEFAULT_SPIKE_STRIP_LIFETIME_SECONDS, LF_PoliceConfig.SpikeStripLifetimeSeconds(), "spike strip lifetime");
		ctx.Equal(LF_PoliceConfig.DEFAULT_ARREST_MIN_RANK, LF_PoliceConfig.ArrestMinRank(), "arrest rank");
		ctx.Equal(LF_PoliceConfig.DEFAULT_ESCORT_MIN_RANK, LF_PoliceConfig.EscortMinRank(), "escort rank");
		array<ref LF_PoliceRank> ranks = {};
		LF_PoliceConfig.Ranks(ranks);
		ctx.Equal(2, ranks.Count(), "the ladder has patrol and senior");
		if (ranks.Count() == 2)
		{
			ctx.Equal(1000, ranks[0].m_iFineCap, "patrol fine cap");
			ctx.Equal(5000, ranks[1].m_iFineCap, "senior fine cap");
			ctx.Equal(3600, ranks[0].m_iDutySecondsForNext, "patrol promotion time");
		}

		// Organizations: the shipped numbers, and the default ranks keep their shipped rights.
		ctx.Equal(LF_OrgConfig.DEFAULT_FOUNDING_FEE, LF_OrgConfig.FoundingFee(), "founding fee");
		ctx.Equal(LF_OrgConfig.DEFAULT_NAME_MIN, LF_OrgConfig.NameMin(), "name minimum");
		ctx.Equal(LF_OrgConfig.DEFAULT_NAME_MAX, LF_OrgConfig.NameMax(), "name maximum");
		ctx.Equal(LF_OrgConfig.DEFAULT_OFFER_MS, LF_OrgConfig.OfferMs(), "offer lifetime");
		ctx.Equal(LF_OrgConfig.DEFAULT_MAX_CASH_MOVE, LF_OrgConfig.MaxCashMove(), "largest cash move");
		ctx.Equal(LF_OrgConfig.DEFAULT_PRICE_EXTRA_STORAGE, LF_OrgConfig.PriceExtraStorage(), "extra storage price");
		ctx.Equal(LF_OrgConfig.DEFAULT_PRICE_PROCESSING_SLOT, LF_OrgConfig.PriceProcessingSlot(), "processing slot price");
		ctx.Equal(LF_OrgConfig.DEFAULT_PRICE_PARKED_VEHICLE, LF_OrgConfig.PriceParkedVehicle(), "parked vehicle price");
		ctx.EqualFloat(LF_OrgConfig.DEFAULT_DESK_RANGE, LF_OrgConfig.DeskRange(), 0.001, "desk range");
		array<string> ladder = {};
		LF_OrganizationConfig.DefaultLadder(ladder);
		foreach (string rankId : ladder)
			ctx.Equal(LF_OrganizationConfig.ShippedRights(rankId), LF_OrganizationConfig.DefaultRights(rankId), "default rights of " + rankId);

		// Persistence: the component's own values pass through.
		bool enabled = true;
		float interval = 600;
		LF_PersistenceConfig.Resolve(enabled, interval);
		ctx.True(enabled, "the autosave stays on");
		ctx.EqualFloat(600, interval, 0.001, "the autosave interval stays 600 s");
		enabled = false;
		interval = 90;
		LF_PersistenceConfig.Resolve(enabled, interval);
		ctx.False(enabled, "a prefab with the autosave off stays off");
		ctx.EqualFloat(90, interval, 0.001, "a prefab interval passes through");

		// Licences: the catalog is left as built.
		map<LF_ELicenseType, ref LF_LicenseConfig> catalog = new map<LF_ELicenseType, ref LF_LicenseConfig>();
		LF_LicenseConfig first = new LF_LicenseConfig();
		first.m_bIsInitialLicense = true;
		catalog.Set(LF_ELicenseType.UNEMPLOYED, first);
		LF_LicenseConfig second = new LF_LicenseConfig();
		second.m_bRequiresWhitelist = true;
		catalog.Set(LF_ELicenseType.POLICE_ACCESS, second);
		LF_LicencesConfig.ApplyTo(catalog);
		ctx.True(first.m_bIsInitialLicense && !first.m_bRequiresWhitelist, "an initial licence is untouched");
		ctx.True(second.m_bRequiresWhitelist && !second.m_bIsInitialLicense, "a whitelisted licence is untouched");

		// The five sections are known keys: the file below warns about nothing but its one typo.
		ctx.Equal(1, LF_ConfigService.WarnUnknownSections("{\"schemaVersion\":1,\"zones\":{},\"police\":{},\"organizations\":{},\"persistence\":{},\"licences\":{},\"zone\":{}}", "test"), "only a misspelt section warns as unknown");

		LF_ConfigService.Reset();
		LF_ZoneConfig.Reload();
		LF_PoliceConfig.Reload();
		LF_OrgConfig.Reload();
	}
}
