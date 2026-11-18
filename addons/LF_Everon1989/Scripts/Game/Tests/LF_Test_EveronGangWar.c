// red-proof: before the overlays and the header lines, `tools\cli test --no-build --only LF_Test_EveronGangWar` fails the header, rules and site checks (fee 5000 not 2500, no `quarry_hideout` entry); see the PR body for the observed run.
//! #2687 in game: under the Everon 1989 header (the pack leg of `tools\cli test`), the pack's war rules
//! overlay and sites overlay are listed and picked up, the war rules read the pack values (fee 2500,
//! answer window 900 s, length 7200 s, cooldown 86400 s, two members a side), the lawless quarry's
//! hideout is a contested site with its rent, radius, bank cap and capture time, and the war notices
//! speak in the pack's period voice. The core files keep the neutral defaults.
[LF_TestInfo(LF_TestTier.LOGIC, pack: true)]
class LF_Test_EveronGangWar : LF_Test
{
	static const ResourceName WARS_OVERLAY = "{C19E3948154EE302}Configs/Organizations/LF_OrgWars.conf";
	static const ResourceName SITES_OVERLAY = "{7B9B25114DD34C68}Configs/Sites/LF_Sites.conf";
	static const string QUARRY_HIDEOUT = "quarry_hideout";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/gang-war";
	}

	//------------------------------------------------------------------------------------------------
	protected static bool Picked(ResourceName coreConf, typename type, string section, ResourceName overlay)
	{
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(coreConf, type, section, resources, names);
		return names.Find(overlay) != -1;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MissionHeader header = LF_MissionHeader.GetCurrent();
		ctx.NotNull(header, "the pack leg runs under an LF mission header");
		if (header && header.m_aLF_ConfigOverlays)
		{
			ctx.True(header.m_aLF_ConfigOverlays.Find(WARS_OVERLAY) != -1, "the header lists the Everon war rules overlay");
			ctx.True(header.m_aLF_ConfigOverlays.Find(SITES_OVERLAY) != -1, "the header lists the Everon sites overlay");
		}
		else
		{
			ctx.Fail("the header lists no config overlays");
		}

		ctx.True(Picked(LF_OrgWarConfig.CONFIG_PATH, LF_OrgWarConfig, LF_OrgWarConfig.SECTION, WARS_OVERLAY), "the war rules loader picks up its overlay");
		ctx.True(Picked(LF_SitesConfig.CONFIG_PATH, LF_SitesConfig, LF_SitesConfig.SECTION, SITES_OVERLAY), "the sites loader picks up its overlay");
		ctx.False(Picked(LF_SitesConfig.CONFIG_PATH, LF_SitesConfig, LF_SitesConfig.SECTION, WARS_OVERLAY), "the war overlay is not read as a sites overlay");

		// The act: layer the pack over fresh core instances (each feature drops only its own cache).
		LF_OrgWarConfig.Reload();
		LF_SitesConfig.Reload();
		LF_OrgWarConfig wars = LF_OrgWarConfig.Get();
		ctx.NotNull(wars, "the war rules load under the pack");
		if (wars)
		{
			ctx.Equal(2500, wars.m_iFee, "the pack fee is 2500");
			ctx.Equal(900, wars.m_iAnswerSeconds, "the pack answer window is 900 s");
			ctx.Equal(7200, wars.m_iLengthSeconds, "a war lasts 7200 s");
			ctx.Equal(86400, wars.m_iCooldownSeconds, "the cooldown is 86400 s");
			ctx.Equal(2, wars.m_iMinMembers, "a war needs 2 members a side");
		}

		LF_SitesConfig sites = LF_SitesConfig.Get();
		ctx.NotNull(sites, "the sites config loads under the pack");
		if (sites)
		{
			LF_SiteRuleDef quarry = sites.Find(QUARRY_HIDEOUT);
			ctx.NotNull(quarry, "the quarry hideout is a contested site");
			if (quarry)
			{
				LF_SiteSettings settings = sites.SettingsOf(QUARRY_HIDEOUT);
				ctx.True(settings.m_bConfigured, "the quarry site has an entry");
				ctx.Equal(60, settings.m_iRadius, "the quarry site radius is 60 m");
				ctx.Equal(600, settings.m_iRentPerHour, "the quarry site rent is 600 an hour");
				ctx.Equal(3000, settings.m_iMaxRentBanked, "the quarry site banks at most 3000");
				ctx.Equal(300, settings.m_iCaptureSeconds, "the quarry site captures in 300 s");
			}
		}

		// The core files stay neutral: the overlay changes the layered result only.
		LF_SitesConfig coreSites = LF_SitesConfig.LoadCore();
		ctx.NotNull(coreSites, "the core sites load");
		if (coreSites)
			ctx.True(coreSites.Find(QUARRY_HIDEOUT) == null, "the core sites file names no quarry site");

		// The period wording: the pack overrides the neutral notice lines.
		string declared = WidgetManager.Translate("#LF-OrgWarBoard_Declared");
		ctx.True(declared.IndexOf("NOTICE") != -1, "the war board speaks as a registry notice");
		string announce = WidgetManager.Translate("#LF-OrgWarAnnounce_Declared");
		ctx.True(announce.IndexOf("NOTICE") != -1, "the war toast speaks as a registry notice");
		ctx.False(WidgetManager.Translate("#LF-SiteFlag_Unclaimed") == "Unclaimed", "the flag board is in the pack's wording");
	}
}
