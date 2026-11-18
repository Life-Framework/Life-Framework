// red-proof: 2026-11-18 `tools/cli test --no-build --red LF_Test_OrgHeldSites` printed RED-OK org/held-sites: with the hook on (no RowsFor call) 38 assertions fail, first "A ledger lists its base, then the captured site (expected held-sites-base-a|0;held-sites-site-x|1, got empty)".
// tier: WORLD (hideout leases and the event bus; no door or prefab is spawned)
//! org/held-sites (#2834, gang war #1022): the org ledger's held-site rows and the war board's site
//! lines come from the lease table on the server. An organisation lists its base first, then each
//! captured site; a capture by a rival moves the row; a release drops it; every lease change refreshes
//! the open ledgers (the event listener ran); the war board names the holder of each contested site,
//! or "unclaimed". The rows cross to the client as one string and split back.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_OrgHeldSites : LF_Test
{
	protected static const string BASE_A = "held-sites-base-a";
	protected static const string BASE_B = "held-sites-base-b";
	protected static const string SITE_X = "held-sites-site-x";
	protected static const string SITE_Y = "held-sites-site-y";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "org/held-sites";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_TestLegacySnapshot kept = LF_TestLegacySnapshot.Take();
		RunWiped(ctx);
		kept.Restore();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunWiped(LF_TestContext ctx)
	{
		LF_OrganizationManager.Reset();
		LF_Hideout.Reset();
		LF_OrgHeldSites.ResetStatics();
		ctx.True(LF_OrgHeldSites.Install(), "the held-site listener installs on the server");

		LF_Organization orgA = LF_OrganizationManager.Create("held-sites-founder-a", "Held Crew A");
		LF_Organization orgB = LF_OrganizationManager.Create("held-sites-founder-b", "Held Crew B");
		ctx.True(orgA && orgB, "two organizations are registered");
		if (!orgA || !orgB)
			return;

		string idA = orgA.GetId();
		string idB = orgB.GetId();
		int baseline = LF_OrgHeldSites.Refreshes();

		// 1. An organization with a base and a captured site lists the base first, then the capture.
		ctx.True(LF_Hideout.Assign(BASE_A, idA), "A leases its base");
		ctx.Equal(baseline + 1, LF_OrgHeldSites.Refreshes(), "a lease refreshes the open ledgers");
		ctx.True(LF_Hideout.HandOver(SITE_X, idA, "captured"), "A takes a free site as captured ground");
		string rowsA;
		if (!RedProof())
			rowsA = LF_OrgHeldSites.RowsFor(idA);

		ctx.EqualStr(BASE_A + "|0;" + SITE_X + "|1", rowsA, "A's ledger lists its base, then the captured site");
		ctx.EqualStr("", LF_OrgHeldSites.RowsFor(idB), "B holds nothing yet");

		// 2. The rows cross to the client as one string and split back in order.
		array<string> ids = {};
		array<bool> captured = {};
		LF_OrgHeldSites.Split(rowsA, ids, captured);
		ctx.Equal(2, ids.Count(), "the client reads two rows");
		if (ids.Count() == 2)
		{
			ctx.EqualStr(BASE_A, ids[0], "the first row is the base");
			ctx.False(captured[0], "the base is not marked captured");
			ctx.EqualStr(SITE_X, ids[1], "the second row is the captured site");
			ctx.True(captured[1], "the captured site is marked captured");
		}

		LF_OrgHeldSites.Split("broken;;" + BASE_A + "|0", ids, captured);
		ctx.Equal(1, ids.Count(), "a row without both fields is skipped");
		ctx.EqualStr("#LF-Org_SiteBase", LF_OrgHeldSites.MarkKey(false), "a base reads as a base");
		ctx.EqualStr("#LF-Org_SiteCaptured", LF_OrgHeldSites.MarkKey(true), "a capture reads as captured");

		// 3. A rival with its own base captures the site: A loses the row, B gains it as captured.
		ctx.True(LF_Hideout.Assign(BASE_B, idB), "B leases its base");
		int beforeCapture = LF_OrgHeldSites.Refreshes();
		ctx.True(LF_Hideout.HandOver(SITE_X, idB, "captured"), "B captures the site from A");
		ctx.Equal(beforeCapture + 2, LF_OrgHeldSites.Refreshes(), "a capture refreshes both the old and the new holder");
		ctx.EqualStr(BASE_A + "|0", LF_OrgHeldSites.RowsFor(idA), "A lists only its base after the capture");
		ctx.EqualStr(BASE_B + "|0;" + SITE_X + "|1", LF_OrgHeldSites.RowsFor(idB), "B lists its base and the captured site");

		// 4. The war board names the holder of each contested site, or says it is unclaimed.
		array<string> siteIds = {};
		siteIds.Insert(SITE_X);
		siteIds.Insert(SITE_Y);
		array<ref LF_OrgWarNotice> notices = {};
		LF_OrgWarBoard.AddSiteNotices(siteIds, notices);
		ctx.Equal(2, notices.Count(), "one notice per contested site");
		if (notices.Count() == 2)
		{
			ctx.Equal(LF_EOrgWarNotice.SITE, notices[0].m_eKind, "a site notice is a SITE");
			ctx.EqualStr("Held Crew B", notices[0].m_sDeclarerName, "the notice names the holder");
			ctx.EqualStr(SITE_X, notices[0].m_sTargetName, "the notice carries the site id");
			ctx.EqualStr("", notices[1].m_sDeclarerName, "a site nobody leases has no holder");
		}

		LF_BoardAnswer answer = LF_OrgWarBoard.Pack(notices, 1800000000);
		ctx.Equal(2 * LF_OrgWarBoard.TEXT_STRIDE, answer.m_aTexts.Count(), "two sites make two text rows");
		ctx.Equal(LF_EOrgWarNotice.SITE, LF_OrgWarBoard.KindOf(answer.m_aNumbers[0]), "the answer's first row is a site");
		array<ref LF_UIPart> parts = LF_OrgWarBoard.BuildParts(answer);
		ctx.True(HasPart(parts, "None"), "no war is up, so the quiet line shows beside the sites");
		ctx.True(HasPart(parts, "SitesHeader"), "the sites have a header");
		ctx.True(HasPart(parts, "Site0"), "the first site has a line");
		ctx.True(HasPart(parts, "Site1"), "the second site has a line");
		ctx.True(LF_OrgWarBoard.SiteLineText(SITE_X, "Held Crew B").Contains("Held Crew B"), "a held site's line names its holder");
		ctx.False(LF_OrgWarBoard.SiteLineText(SITE_Y, "").Contains("Held Crew"), "an unclaimed site's line names no holder");

		// A later capture shows on the next read: the board reads the lease table, nothing is cached.
		ctx.True(LF_Hideout.HandOver(SITE_X, idA, "captured"), "A takes the site back");
		notices.Clear();
		LF_OrgWarBoard.AddSiteNotices(siteIds, notices);
		if (notices.Count() == 2)
			ctx.EqualStr("Held Crew A", notices[0].m_sDeclarerName, "the next read names the new holder");

		// 5. A release drops the row and refreshes the ledgers.
		int beforeRelease = LF_OrgHeldSites.Refreshes();
		ctx.True(LF_Hideout.Release(SITE_X, "given up"), "A gives the captured site up");
		ctx.Equal(beforeRelease + 1, LF_OrgHeldSites.Refreshes(), "a release refreshes the open ledgers");
		ctx.EqualStr(BASE_A + "|0", LF_OrgHeldSites.RowsFor(idA), "A lists only its base after the release");
		ctx.True(LF_Hideout.Release(BASE_A, "lapsed"), "A's base lapses");
		ctx.EqualStr("", LF_OrgHeldSites.RowsFor(idA), "A holds nothing after both leases end");

		LF_OrgHeldSites.ResetStatics();
		LF_Hideout.Reset();
		LF_OrganizationManager.Reset();
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasPart(notnull array<ref LF_UIPart> parts, string name)
	{
		foreach (LF_UIPart part : parts)
		{
			if (part.GetName() == name)
				return true;
		}

		return false;
	}
}
