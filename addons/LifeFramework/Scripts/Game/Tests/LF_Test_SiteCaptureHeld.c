// red-proof: 2026-11-18 `tools/cli test --no-build --red LF_Test_SiteCaptureHeld,LF_Test_SiteCapturePersist` printed RED PROOF OK: with the hook on (no HandOver) the capturer takes no site, so "a capturer that already leases a base takes the site" and the captured-ground checks fail. On the unfixed #2680 HandOver the same capture is refused ("organization already owns a hideout").
// tier: WORLD (the annex hideout door is the captured site; its org_base place is what the test watches)
//! sites/capture-held (#2816, gang war #1022): a captured contested site is held under a CAPTURED
//! tenancy that does not count against the one-hideout-lease limit. An organization with its base
//! captures a second site and holds both, a third too, and a second base lease at the desk is still
//! refused. The old tenant's lease ends, the captured site never becomes the capturer's org_base,
//! and an organization with only captured sites can still take a base. Releasing a captured site
//! clears its tenancy.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_SiteCaptureHeld : LF_Test
{
	protected static const string BASE_B = "capture-held-base-b";
	protected static const string BASE_A = "capture-held-base-a";
	protected static const string SITE_TWO = "capture-held-site-two";
	protected static const string BASE_C = "capture-held-base-c";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "sites/capture-held";
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

		string annex = LF_TestAnnex.HIDEOUT_ID;
		string baseKind = LF_PlacementZones.ORG_BASE_KIND;
		LF_Organization orgA = LF_OrganizationManager.Create("capture-held-founder-a", "Capture Crew A");
		LF_Organization orgB = LF_OrganizationManager.Create("capture-held-founder-b", "Capture Crew B");
		LF_Organization orgC = LF_OrganizationManager.Create("capture-held-founder-c", "Capture Crew C");
		ctx.NotNull(orgA, "organization A exists");
		ctx.NotNull(orgB, "organization B exists");
		ctx.NotNull(orgC, "organization C exists");
		if (!orgA || !orgB || !orgC)
			return;

		string idA = orgA.GetId();
		string idB = orgB.GetId();
		string idC = orgC.GetId();

		// A leases the annex site as its base; B has its own base elsewhere.
		ctx.True(LF_Hideout.Assign(annex, idA), "A leases the annex site");
		ctx.True(LF_Hideout.Assign(BASE_B, idB), "B leases its base");
		ctx.True(LF_Hideout.SetHeldGround(annex, idA, 120, 40), "A holds ground on the annex site");
		ctx.True(LF_Places.Find(baseKind, idA) != null, "the annex door is A's base place");

		// 1. B, with a base, captures A's site and holds both.
		bool handed;
		if (!RedProof())
			handed = LF_Hideout.HandOver(annex, idB, "captured");

		ctx.True(handed, "a capturer that already leases a base takes the site");
		ctx.EqualStr(idB, LF_Hideout.GetOwner(annex), "the lease moved to B");
		ctx.EqualStr(idB, LF_Hideout.GetOwner(BASE_B), "B still leases its base");
		ctx.True(LF_Hideout.IsCaptured(annex), "the annex site is held as captured ground");
		ctx.False(LF_Hideout.IsCaptured(BASE_B), "B's base is not captured ground");
		ctx.Equal(2, LF_Hideout.SitesOf(idB).Count(), "B holds its base and the captured site");
		ctx.Equal(1, LF_Hideout.SitesOf(idB, true).Count(), "one of them is captured");

		// 2. The old tenant's lease ended, its base place went with it, and the capturer got none from it.
		ctx.False(LF_Hideout.GetOwner(annex) == idA, "A's lease on the annex site ended");
		ctx.True(LF_Hideout.SitesOf(idA).IsEmpty(), "A holds no site now");
		ctx.True(LF_Places.Find(baseKind, idA) == null, "A's base place is gone");
		ctx.True(LF_Places.Find(baseKind, idB) == null, "a captured site never becomes B's base place");
		LF_HeldGroundRow held = LF_Hideout.HeldGroundOf(annex);
		if (held)
		{
			ctx.EqualStr(idB, held.m_sHolderOrgId, "the held ground moved to B");
			ctx.Equal(0, held.m_iHoldPoints, "B starts at zero points");
		}

		// 3. The desk limit still holds for bases: B cannot lease a second base.
		ctx.False(LF_Hideout.Assign(BASE_A, idB), "B cannot take a second base lease at the desk");

		// 4. A second capture: B holds three sites.
		ctx.True(LF_Hideout.Assign(SITE_TWO, idA), "A leases another site");
		ctx.True(LF_Hideout.HandOver(SITE_TWO, idB, "captured"), "B captures a second site");
		ctx.Equal(3, LF_Hideout.SitesOf(idB).Count(), "B holds a base and two captured sites");
		ctx.Equal(2, LF_Hideout.SitesOf(idB, true).Count(), "two of them are captured");

		// 5. An organization with no site takes a captured one, then still takes a base.
		ctx.True(LF_Hideout.Assign(BASE_A, idA), "A, with no lease left, takes a base");
		ctx.True(LF_Hideout.HandOver(SITE_TWO, idC, "captured"), "C captures a site with no base");
		ctx.True(LF_Hideout.Assign(BASE_C, idC), "C, holding only captured ground, still takes a base");
		ctx.False(LF_Hideout.Assign("capture-held-extra", idC), "and only one base");

		// 6. Giving a captured site up clears its tenancy: the site is free and not captured.
		ctx.True(LF_Hideout.Release(annex), "B gives the annex site up");
		ctx.False(LF_Hideout.IsCaptured(annex), "a free site is not captured ground");
		ctx.True(LF_Hideout.Assign(annex, idA) == false, "A already has a base, so the freed site is not its second");
		ctx.True(LF_Hideout.Release(BASE_A), "A gives its base up");
		ctx.True(LF_Hideout.Assign(annex, idA), "A leases the freed site as a base");
		ctx.False(LF_Hideout.IsCaptured(annex), "a desk lease is a base lease");
		ctx.True(LF_Places.Find(baseKind, idA) != null, "the annex door is A's base place again");

		// 7. An emptied organization loses captured sites too.
		ctx.True(LF_Hideout.ReleaseAllOf(idC, "test") >= 2, "releasing an organization frees its base and captured sites");
		ctx.True(LF_Hideout.SitesOf(idC).IsEmpty(), "C holds nothing after the release");

		LF_Hideout.Reset();
		LF_OrganizationManager.Reset();
	}
}
