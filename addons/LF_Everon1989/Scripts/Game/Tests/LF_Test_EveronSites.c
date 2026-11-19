// red-proof: before the two new rules and the `LF-SiteName_*` rows exist, `tools\cli test --no-build --only LF_Test_EveronSites` fails "farm_compound is a contested site" and the name checks; see the PR body for the observed run.
//! #3406 in game, under the Everon 1989 header (the pack leg of `tools\cli test`): the merged site rules
//! list three contested sites (the quarry hideout, the farm compound, the hilltop mast), each with its
//! own radius, rent an hour, bank cap and capture time, and each has a `LF-SiteName_<id>` row that
//! resolves to a real name. A held site pays a gang less an hour than two members on civilian jobs, so
//! every rent stays under the cheapest civilian run twice over.
[LF_TestInfo(LF_TestTier.LOGIC, pack: true)]
class LF_Test_EveronSites : LF_Test
{
	//! The cheapest civilian run pays 400 an hour (a courier parcel run); two members earn 800. A rent at or above it makes holding a site pay better than working.
	static const int TWO_CIVILIAN_PER_HOUR = 800;	// lf-fixed: the duty-pay rule, `tools\cli economy` parcel run x 2

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/sites";
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckSite(LF_TestContext ctx, LF_SitesConfig sites, string id, int radius, int rent, int cap, int captureSeconds)
	{
		LF_SiteRuleDef rule = sites.Find(id);
		ctx.NotNull(rule, id + " is a contested site");
		if (!rule)
			return;

		LF_SiteSettings settings = sites.SettingsOf(id);
		ctx.True(settings.m_bConfigured, id + " has an entry");
		ctx.Equal(radius, settings.m_iRadius, id + " radius");
		ctx.Equal(rent, settings.m_iRentPerHour, id + " rent an hour");
		ctx.Equal(cap, settings.m_iMaxRentBanked, id + " bank cap");
		ctx.Equal(captureSeconds, settings.m_iCaptureSeconds, id + " capture seconds");
		ctx.True(settings.m_iRentPerHour < TWO_CIVILIAN_PER_HOUR, id + " rent stays under two civilian jobs");
		ctx.True(settings.m_iMaxRentBanked >= settings.m_iRentPerHour, id + " banks at least an hour of rent");

		string key = "#LF" + "-SiteName_" + id;	// built from two parts: the key is per hideout id (the same as LF_SiteCaptureHud.SiteLabel)
		string name = WidgetManager.Translate(key);
		ctx.True(!name.IsEmpty() && name != key && name != key.Substring(1, key.Length() - 1), id + " name resolves");
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_SitesConfig.Reload();
		LF_SitesConfig sites = LF_SitesConfig.Get();
		ctx.NotNull(sites, "the sites config loads under the pack");
		if (!sites)
			return;

		ctx.Equal(3, sites.m_aRules.Count(), "the pack defines three contested sites");
		CheckSite(ctx, sites, "quarry_hideout", 60, 600, 3000, 300);
		CheckSite(ctx, sites, "farm_compound", 80, 500, 2500, 240);
		CheckSite(ctx, sites, "hilltop_mast", 45, 700, 3500, 420);

		// Three sites, three different names.
		string quarry = LF_SiteCaptureHud.SiteLabel("quarry_hideout");
		string farm = LF_SiteCaptureHud.SiteLabel("farm_compound");
		string mast = LF_SiteCaptureHud.SiteLabel("hilltop_mast");
		ctx.True(quarry != farm && farm != mast && quarry != mast, "each site has its own name");
	}
}
