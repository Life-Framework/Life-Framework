// red-proof: 2026-11-17 `tools\cli test --no-build --red LF_Test_OrgLeaseRelease` (RedProof hook skips the Release call): RED PROOF OK, "the ledger row reads FREE again (expected 0, got 1)", "the yard has no owner", "a second release finds nothing to give up". Green with the verb.
// tier: WORLD (needs the organization hideout door: GridWorld's Grid_orgs_vehicles.layer, as LF_Test_OrgRegistry)
//! organization/lease-release (#2572): a member whose rank holds BUY_UPGRADE gives the hideout lease
//! back; the lease reads FREE and the gate is shut. A member without the right, another organization,
//! and a second release are refused and the lease stays.
class LF_Test_OrgLeaseRelease : LF_Test
{
	protected static const string FOUNDER = "org-release-founder";
	protected static const string MEMBER = "org-release-member";
	protected static const string RIVAL = "org-release-rival";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "organization/lease-release";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_OrganizationManager.Reset();
		LF_OrgInvites.Reset();
		LF_Hideout.Reset();

		string hideout = LF_Hideout.OrgHideoutId();
		LF_Organization org = LF_OrganizationManager.Create(FOUNDER, "Release Test Crew");
		LF_Organization rival = LF_OrganizationManager.Create(RIVAL, "Release Rival Crew");
		ctx.NotNull(org, "the founder registers an organization");
		ctx.NotNull(rival, "a rival registers an organization");
		if (!org || !rival)
		{
			Cleanup();
			return;
		}

		ctx.Equal(LF_EOrgOfferResult.OFFERED, LF_OrgInvites.Offer(FOUNDER, 1, MEMBER, 1000), "the founder offers a place");
		ctx.Equal(LF_EOrgOfferResult.JOINED, LF_OrgInvites.Accept(MEMBER, FOUNDER, 1000), "the member joins");
		ctx.True(LF_OrgHideoutLease.Lease(FOUNDER, hideout), "the founder takes the lease");
		ctx.Equal(LF_EOrgLease.OURS, LF_OrgHideoutLease.StateFor(org.GetId(), hideout), "the yard is ours");

		// Refused: a member without the right, someone with no organization, the other organization.
		ctx.False(LF_OrgHideoutLease.Release(MEMBER, hideout), "a member without BUY_UPGRADE cannot give the lease up");
		ctx.False(LF_OrgHideoutLease.Release("org-release-nobody", hideout), "someone with no organization cannot");
		ctx.False(LF_OrgHideoutLease.Release(RIVAL, hideout), "the other organization cannot end a lease it does not hold");
		ctx.Equal(LF_EOrgLease.OURS, LF_OrgHideoutLease.StateFor(org.GetId(), hideout), "the lease stays after the refusals");

		// Accepted: the founder holds BUY_UPGRADE.
		bool gaveUp;
		if (!RedProof())
			gaveUp = LF_OrgHideoutLease.Release(FOUNDER, hideout);

		ctx.True(gaveUp, "a holder of BUY_UPGRADE gives the lease up");
		ctx.Equal(LF_EOrgLease.FREE, LF_OrgHideoutLease.StateFor(org.GetId(), hideout), "the ledger row reads FREE again");
		ctx.True(LF_Hideout.GetOwner(hideout).IsEmpty(), "the yard has no owner");
		ctx.True(LF_Hideout.IsLocked(hideout), "the gate is shut");
		ctx.False(LF_OrgHideoutLease.Release(FOUNDER, hideout), "a second release finds nothing to give up");

		// The ledger row offers the verb to the holder of the right only, and offers the lease again once FREE.
		ctx.EqualStr("#LF-Org_GiveUpLease", LF_OrgLedger.HideoutAction(LF_OrgLedgerPage.Create(true, "X", 0, LF_OrgRules.RIGHT_BUY, false, LF_EOrgLease.OURS, "")), "the holder sees Give up the lease");
		ctx.EqualStr("", LF_OrgLedger.HideoutAction(LF_OrgLedgerPage.Create(true, "X", 0, 0, false, LF_EOrgLease.OURS, "")), "a member without the right sees no button");
		ctx.EqualStr("#LF-Org_TakeLease", LF_OrgLedger.HideoutAction(LF_OrgLedgerPage.Create(true, "X", 0, LF_OrgRules.RIGHT_BUY, false, LF_EOrgLease.FREE, "")), "a FREE yard offers Take the lease again");

		// The yard can be leased again, by anyone with the right.
		ctx.True(LF_OrgHideoutLease.Lease(RIVAL, hideout), "the rival now takes the free yard");

		Cleanup();
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup()
	{
		LF_OrgInvites.Reset();
		LF_Hideout.Reset();
		LF_OrganizationManager.Reset();
	}
}
