// red-proof: 2026-11-18, `tools/cli test --no-build --red LF_Test_HideoutLeasePerSite` (RedProof offers the fixed primary id): RED PROOF OK, "the save holds both leases (expected 2, got 1)", "after load org B holds the second site (got '')". Green with BaseSiteFor.
//! hideout/lease-per-site (#3408): every door marked as an organization hideout is a base site the
//! desk leases, not only the one "primary" place. With two base sites, org A leases the first, org B
//! is offered and leases the second, org A is refused the second (one base lease each, #2816), and the
//! saved leases (keyed by each door's own hideout id) load back onto both sites.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_HideoutLeasePerSite : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "hideout/lease-per-site";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// The e2e save boot seeds an organization and a lease earlier in this tier; put them back at the end.
		LF_TestLegacySnapshot kept = LF_TestLegacySnapshot.Take();
		IEntity annexDoor = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.HIDEOUT_DOOR);
		bool marked = false;
		if (annexDoor && !LF_Places.Find(LF_Hideout.PLACE_KIND_BASE_DOOR, LF_TestAnnex.HIDEOUT_ID))
			marked = LF_Hideout.MarkBaseDoor(LF_TestAnnex.HIDEOUT_ID, annexDoor);

		RunWiped(ctx, annexDoor);

		if (marked)
			LF_Places.Unregister(LF_Hideout.PLACE_KIND_BASE_DOOR, LF_TestAnnex.HIDEOUT_ID);

		kept.Restore();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunWiped(LF_TestContext ctx, IEntity annexDoor)
	{
		ctx.NotNull(annexDoor, LF_TestAnnex.HIDEOUT_DOOR + " is placed");
		string first = LF_Hideout.OrgHideoutId();
		ctx.False(first.IsEmpty(), "the world places the primary organization hideout");
		if (!annexDoor || first.IsEmpty())
			return;

		string second = LF_TestAnnex.HIDEOUT_ID;
		ctx.True(first != second, "the two base sites have different ids");

		LF_OrganizationManager.Reset();
		LF_Hideout.Reset();

		array<string> sites = {};
		LF_Hideout.BaseSiteIds(sites);
		ctx.True(sites.Find(first) == 0, "the primary is the first base site");
		ctx.True(sites.Find(second) != -1, "the second marked door is a base site");

		LF_Organization orgA = LF_OrganizationManager.Create("hideout-per-site-a", "Site Crew A");
		LF_Organization orgB = LF_OrganizationManager.Create("hideout-per-site-b", "Site Crew B");
		ctx.NotNull(orgA, "organization A exists");
		ctx.NotNull(orgB, "organization B exists");
		if (!orgA || !orgB)
			return;

		string a = orgA.GetId();
		string b = orgB.GetId();

		// Case 1: org A is offered the primary and leases it.
		ctx.EqualStr(first, OfferFor(a), "org A is offered the first base site");
		ctx.True(LF_Hideout.Assign(OfferFor(a), a), "org A leases the first base site");
		ctx.EqualStr(a, LF_Hideout.GetOwner(first), "org A holds the first site");

		// Case 2: org B is offered the second, free site and leases it.
		ctx.EqualStr(second, OfferFor(b), "org B is offered the free second base site");
		ctx.True(LF_Hideout.Assign(OfferFor(b), b), "org B leases the second base site");
		ctx.EqualStr(b, LF_Hideout.GetOwner(second), "org B holds the second site");

		// Case 3: one base lease each.
		ctx.False(LF_Hideout.Assign(second, a), "org A is refused the second site");
		ctx.EqualStr(first, OfferFor(a), "org A is still offered its own base");

		// Case 4: the saved leases load back onto both sites.
		array<ref LF_HideoutRecord> records = LF_Hideout.ExportAll();
		ctx.Equal(2, records.Count(), "the save holds both leases");
		LF_Hideout.Reset();
		ctx.EqualStr("", LF_Hideout.GetOwner(first), "the wipe frees the first site");
		LF_Hideout.ApplyAll(records);
		ctx.EqualStr(a, LF_Hideout.GetOwner(first), "after load org A holds the first site");
		ctx.EqualStr(b, LF_Hideout.GetOwner(second), "after load org B holds the second site");
		ctx.EqualStr(second, OfferFor(b), "after load org B is offered its own base");

		LF_Hideout.Reset();
		LF_OrganizationManager.Reset();
	}

	//------------------------------------------------------------------------------------------------
	//! The site the desk offers the organization; the red proof uses the old fixed id.
	protected string OfferFor(string organizationId)
	{
		if (RedProof())
			return LF_Hideout.OrgHideoutId();

		return LF_Hideout.BaseSiteFor(organizationId);
	}
}
