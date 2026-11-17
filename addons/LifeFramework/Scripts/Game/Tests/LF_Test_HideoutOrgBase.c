// red-proof: 2026-11-18, `tools/cli test --no-build --only LF_Test_HideoutOrgBase` on main code: [LFTEST]   - the leased hideout is the organization's base; - a spot inside the base radius is allowed (got 'has no base: ... (zone ORG_BASE)'). Implemented: green.
// tier: WORLD
//! hideout/org-base (#2504, epic #927): an organization's leased hideout is its base for placement.
//! The lease registers the hideout's door in LF_Places as "org_base" under the organization id, and
//! LF_PlacementZones judges a spot inside the base radius as allowed; ending the lease unregisters it
//! and the same spot is refused. A load that holds the lease registers it and a load that omits it removes it.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_HideoutOrgBase : LF_Test
{
	protected static const float RADIUS = 30;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "hideout/org-base";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// The e2e save boot seeds an organization and a lease earlier in this tier; put them back at the end.
		LF_TestLegacySnapshot kept = LF_TestLegacySnapshot.Take();
		RunWiped(ctx);
		kept.Restore();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunWiped(LF_TestContext ctx)
	{
		IEntity door = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.HIDEOUT_DOOR);
		ctx.NotNull(door, LF_TestAnnex.HIDEOUT_DOOR + " is placed");
		if (!door)
			return;

		LF_OrganizationManager.Reset();
		LF_Hideout.Reset();

		string founder = "hideout-org-base-founder";
		LF_Organization org = LF_OrganizationManager.Create(founder, "Base Crew");
		ctx.NotNull(org, "the organization exists");
		if (!org)
			return;

		string orgId = org.GetId();
		string kind = LF_PlacementZones.ORG_BASE_KIND;
		vector spot = door.GetOrigin() + "5 0 0";

		ctx.True(LF_Places.Find(kind, orgId) == null, "no base is placed before the lease");
		ctx.True(Refused(ctx, orgId, spot).IndexOf("has no base") != -1, "before the lease the spot is refused: no base");

		ctx.True(LF_Hideout.Assign(LF_TestAnnex.HIDEOUT_ID, orgId), "the organization leases the hideout");
		if (!RedProof())
			ctx.True(LF_Places.Find(kind, orgId) == door, "the leased hideout is the organization's base");
		ctx.EqualStr("", Refused(ctx, orgId, spot), "a spot inside the base radius is allowed");
		ctx.True(Refused(ctx, orgId, door.GetOrigin() + "200 0 0").IndexOf("from the base") != -1, "a spot far from the base is refused");

		// The same lease applied again (a load re-applies it) keeps the one base.
		array<ref LF_HideoutRecord> records = {};
		records.Insert(LF_HideoutRecord.Create(LF_TestAnnex.HIDEOUT_ID, orgId, true));
		LF_Hideout.ApplyAll(records);
		ctx.True(LF_Places.Find(kind, orgId) == door, "re-applying the saved lease keeps the base");

		ctx.True(LF_Hideout.Release(LF_TestAnnex.HIDEOUT_ID), "the lease ends");
		ctx.True(LF_Places.Find(kind, orgId) == null, "ending the lease unregisters the base");
		ctx.True(Refused(ctx, orgId, spot).IndexOf("has no base") != -1, "after the lease the same spot is refused");

		// A load that puts the lease back registers it, a load that omits it removes it.
		LF_Hideout.ApplyAll(records);
		ctx.True(LF_Places.Find(kind, orgId) == door, "a load that holds the lease registers the base");
		array<ref LF_HideoutRecord> none = {};
		LF_Hideout.ApplyAll(none);
		ctx.True(LF_Places.Find(kind, orgId) == null, "a load that omits the lease removes the base");

		LF_Hideout.Reset();
		LF_OrganizationManager.Reset();
	}

	//------------------------------------------------------------------------------------------------
	//! The refusal the ORG_BASE zone gives for orgId placing at spot, "" when allowed. Built from the
	//! same place lookup LF_PlacementZones.Gather uses, without a player.
	protected string Refused(LF_TestContext ctx, string orgId, vector spot)
	{
		LF_PlacementWhereFacts facts = new LF_PlacementWhereFacts();
		facts.m_sOrgId = orgId;
		facts.m_fBaseRadius = RADIUS;
		vector origin;
		if (LF_Places.FindOrigin(LF_PlacementZones.ORG_BASE_KIND, orgId, origin))
		{
			facts.m_bBaseKnown = true;
			facts.m_fBaseDistance = vector.Distance(Vector(spot[0], 0, spot[2]), Vector(origin[0], 0, origin[2]));
		}

		string toast;
		return LF_PlacementZones.Judge(LF_EPlacementWhere.ORG_BASE, facts, toast);
	}
}
