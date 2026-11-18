// red-proof: 2026-11-18 `test --no-build --red LF_Test_SiteAlerts` printed `RED PROOF OK (all 1 listed test(s) fail with the hook on)`; with the hook on the first contest is never raised, so the "tells the two holder members" assertions fail and the 60 s repeat rule goes wrong.
//! Site alerts (#2686): who is told when a site is contested or captured, with no world and no
//! players. A fake notifier stands in for the organisations and the toast.
class LF_Test_SiteAlertsNotifier : LF_SiteAlertNotifier
{
	ref map<string, ref array<string>> m_mMembers = new map<string, ref array<string>>();
	ref array<string> m_aTold = {};

	//------------------------------------------------------------------------------------------------
	void AddMember(string orgId, string characterId)
	{
		array<string> list = m_mMembers.Get(orgId);
		if (!list)
		{
			list = {};
			m_mMembers.Set(orgId, list);
		}

		list.Insert(characterId);
	}

	//------------------------------------------------------------------------------------------------
	override void MembersOf(string orgId, notnull array<string> into)
	{
		into.Clear();
		array<string> list = m_mMembers.Get(orgId);
		if (!list)
			return;

		into.Copy(list);
	}

	//------------------------------------------------------------------------------------------------
	override bool Alert(string characterId, string messageKey)
	{
		m_aTold.Insert(characterId + ":" + messageKey);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	int Count(string characterId, string messageKey)
	{
		int count;
		string line = characterId + ":" + messageKey;
		foreach (string told : m_aTold)
		{
			if (told == line)
				count++;
		}

		return count;
	}
}

//------------------------------------------------------------------------------------------------
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_SiteAlerts : LF_Test
{
	override string GetName()
	{
		return "sites/site-alerts";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_Test_SiteAlertsNotifier fake = new LF_Test_SiteAlertsNotifier();
		fake.AddMember("orgA", "a1");
		fake.AddMember("orgA", "a2");
		fake.AddMember("orgB", "b1");
		LF_SiteAlerts.SetNotifier(fake);

		// A contest alerts each holder member once, wherever they are.
		int told;
		// Red-proof hook: with --red the contest is never raised, so nobody is told.
		if (!RedProof())
			told = LF_SiteAlerts.OnContested(LF_OnSiteContested.Create("site1", "orgA", 2), 1000);
		ctx.Equal(2, told, "a contest tells the two holder members");
		ctx.Equal(1, fake.Count("a1", LF_SiteAlerts.KEY_CONTESTED), "the first holder member is told once");
		ctx.Equal(1, fake.Count("a2", LF_SiteAlerts.KEY_CONTESTED), "the second holder member is told once");
		ctx.Equal(0, fake.Count("b1", LF_SiteAlerts.KEY_CONTESTED), "the challenger is not told the holder's alert");

		// A repeat inside 60 s does not alert again; at 60 s it does.
		ctx.Equal(0, LF_SiteAlerts.OnContested(LF_OnSiteContested.Create("site1", "orgA", 2), 1030), "a repeat 30 s later tells nobody");
		ctx.Equal(0, LF_SiteAlerts.OnContested(LF_OnSiteContested.Create("site1", "orgA", 2), 1059), "a repeat 59 s later tells nobody");
		ctx.Equal(1, fake.Count("a1", LF_SiteAlerts.KEY_CONTESTED), "the holder member still has one alert after the repeats");
		ctx.Equal(2, LF_SiteAlerts.OnContested(LF_OnSiteContested.Create("site1", "orgA", 2), 1060), "a new contest 60 s later tells the holders again");
		ctx.Equal(2, fake.Count("a1", LF_SiteAlerts.KEY_CONTESTED), "the holder member has two alerts after the second contest");

		// Another site has its own clock.
		ctx.Equal(1, LF_SiteAlerts.OnContested(LF_OnSiteContested.Create("site2", "orgB", 2), 1061), "a contest at another site tells its holder");

		// Nobody holds the site: nobody to tell.
		ctx.Equal(0, LF_SiteAlerts.OnContested(LF_OnSiteContested.Create("site3", "", 2), 1070), "an unheld site has nobody to alert");

		// A capture alerts the old and the new holders.
		told = LF_SiteAlerts.OnCaptured(LF_OnSiteCaptured.Create("site1", "orgA", "orgB"));
		ctx.Equal(3, told, "a capture tells the two old holders and the new one");
		ctx.Equal(1, fake.Count("a1", LF_SiteAlerts.KEY_LOST), "an old holder member hears the site was lost");
		ctx.Equal(1, fake.Count("a2", LF_SiteAlerts.KEY_LOST), "the other old holder member hears it too");
		ctx.Equal(1, fake.Count("b1", LF_SiteAlerts.KEY_CAPTURED), "the new holder hears it was captured");

		// A capture of an unheld site tells only the capturer.
		told = LF_SiteAlerts.OnCaptured(LF_OnSiteCaptured.Create("site3", "", "orgA"));
		ctx.Equal(2, told, "a capture of unheld ground tells the capturer's members");
		ctx.Equal(1, fake.Count("a1", LF_SiteAlerts.KEY_CAPTURED), "the capturer member is told it was captured");

		// The bar's progress is a whole percent of the capture time.
		ctx.Equal(0, LF_SiteCaptureFeed.Percent(0, 300), "no progress is 0%");
		ctx.Equal(50, LF_SiteCaptureFeed.Percent(150, 300), "half the capture time is 50%");
		ctx.Equal(100, LF_SiteCaptureFeed.Percent(900, 300), "progress past the capture time stops at 100%");
		ctx.Equal(0, LF_SiteCaptureFeed.Percent(10, 0), "no capture time reads 0% instead of dividing by zero");

		// Detach the fake; the real listener (if the sweep started it) keeps its subscription.
		LF_SiteAlerts.SetNotifier(null);
	}
}
