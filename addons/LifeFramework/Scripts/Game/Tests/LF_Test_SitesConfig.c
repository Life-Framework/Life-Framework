// red-proof: the batched `--red` run (RedProof() skips the pack overlay and the owner JSON) fails "the overlay adds one site (expected 1, got 0)"; see the PR for the RED-OK line. Before LF_SitesConfig existed the file did not compile.
//! The contested-site rules (#2679, #1022): LF_SitesConfig loads Configs/Sites/LF_Sites.conf (the core
//! has no sites and only the default rules), merges pack overlays, then the owner's `sites` keys. A
//! test overlay adds one site and overrides its rent; an owner key lowers its capture time; a bad
//! owner value keeps the value below it.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_SitesConfig : LF_Test
{
	static const ResourceName OVERLAY = "{EED4A5D4D70DD88B}Configs/Tests/LF_Test_SitesOverlay.conf";

	override string GetName()
	{
		return "sites/config";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_ConfigReader Owner(string json)
	{
		LF_ConfigService.Reset();
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1,\"sites\":" + json + "}", "test:sites");
		return LF_ConfigService.OpenOwnerSection("sites");
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// The core conf: no sites, and every hideout id has the default rules.
		LF_SitesConfig config = LF_SitesConfig.LoadCore();
		ctx.True(config != null, "the core sites conf loads");
		if (!config)
			return;

		ctx.Equal(0, config.m_aRules.Count(), "the core conf has no sites");
		LF_SiteSettings fallback = config.SettingsOf("anywhere");
		ctx.False(fallback.m_bConfigured, "a hideout with no entry is not configured");
		ctx.Equal(60, fallback.m_iRadius, "the default radius is 60 m");
		ctx.Equal(20, fallback.m_iInnerRadius, "the default inner radius is 20 m");
		ctx.Equal(2, fallback.m_iMinMembersPresent, "the default member minimum is 2");
		ctx.Equal(300, fallback.m_iCaptureSeconds, "the default capture time is 300 s");
		ctx.Equal(600, fallback.m_iDecayPerHour, "the default decay is 600 s an hour");
		ctx.Equal(1800, fallback.m_iFlipCooldownSeconds, "the default flip cooldown is 1800 s");
		ctx.Equal(600, fallback.m_iRentPerHour, "the default rent is 600 an hour");
		ctx.Equal(3000, fallback.m_iMaxRentBanked, "the default banked rent cap is 3000");
		ctx.Equal(300, fallback.m_iActiveWindowSeconds, "the default active window is 300 s");

		// A pack overlay adds one site and sets its rent and radius; the rest are the default rules.
		if (!RedProof())
		{
			Resource overlay = BaseContainerTools.LoadContainer(OVERLAY);
			ctx.True(overlay && overlay.IsValid() && overlay.GetResource(), "the test overlay loads");
			if (overlay && overlay.IsValid() && overlay.GetResource())
				config.ApplyPackContainer(overlay.GetResource().ToBaseContainer(), "test:overlay");
		}

		ctx.Equal(1, config.m_aRules.Count(), "the overlay adds one site");
		LF_SiteSettings site = config.SettingsOf("test_hideout");
		ctx.True(site.m_bConfigured, "the overlay's site is configured");
		ctx.Equal(900, site.m_iRentPerHour, "the overlay sets the site's rent");
		ctx.Equal(75, site.m_iRadius, "the overlay sets the site's radius");
		ctx.Equal(300, site.m_iCaptureSeconds, "a field the overlay leaves out has the default capture time");
		ctx.False(config.SettingsOf("anywhere").m_bConfigured, "another hideout still has the default rules");

		// The owner lowers the capture time and patches nothing else; a bad value keeps the one below.
		string json = "{\"rules\":{\"test_hideout\":{\"captureSeconds\":120},\"other_hideout\":{\"op\":\"add\",\"minMembersPresent\":4,\"decayPerHour\":-5,\"radius\":0}}}";
		if (RedProof())
			json = "{}";

		config.ApplyOwner(Owner(json));
		site = config.SettingsOf("test_hideout");
		ctx.Equal(120, site.m_iCaptureSeconds, "the owner's capture time wins");
		ctx.Equal(900, site.m_iRentPerHour, "a key the owner leaves out keeps the overlay's rent");
		ctx.Equal(75, site.m_iRadius, "a key the owner leaves out keeps the overlay's radius");

		LF_SiteSettings other = config.SettingsOf("other_hideout");
		ctx.True(other.m_bConfigured, "the owner adds a second site");
		ctx.Equal(4, other.m_iMinMembersPresent, "the owner's member minimum wins");
		ctx.Equal(600, other.m_iDecayPerHour, "a negative decay is refused and keeps the default");
		ctx.Equal(60, other.m_iRadius, "a radius of 0 is refused and keeps the default");

		// An inner ring wider than the site is refused: it is made as wide as the site.
		config.ApplyOwner(Owner("{\"rules\":{\"test_hideout\":{\"innerRadius\":500}}}"));
		ctx.Equal(75, config.SettingsOf("test_hideout").m_iInnerRadius, "an inner ring wider than the site is cut to the radius");

		// The rules a system reads are a copy, and the hold rules carry the site's scoring values.
		LF_SiteSettings copy = config.SettingsOf("test_hideout");
		copy.m_iCaptureSeconds = 1;
		ctx.Equal(120, config.SettingsOf("test_hideout").m_iCaptureSeconds, "SettingsOf hands out a copy");
		LF_SiteRules hold = config.SettingsOf("test_hideout").ToHoldRules();
		ctx.True(hold.m_fCaptureSeconds == 120, "the hold rules take the site's capture time");
		ctx.Equal(2, hold.m_iMinMembersPresent, "the hold rules take the site's member minimum");
		ctx.True(hold.m_fFlipCooldownSeconds == 1800, "the hold rules take the site's flip cooldown");

		// The owner can remove a site: its hideout is back on the default rules.
		config.ApplyOwner(Owner("{\"rules\":{\"other_hideout\":{\"op\":\"remove\"}}}"));
		ctx.False(config.SettingsOf("other_hideout").m_bConfigured, "the owner's remove takes the site out");

		LF_ConfigService.Reset();
		LF_SitesConfig.Reload();
	}
}
