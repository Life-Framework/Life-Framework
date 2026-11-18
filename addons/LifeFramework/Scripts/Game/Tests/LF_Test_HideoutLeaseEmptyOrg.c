// red-proof: 2026-11-17, `tools/cli test --no-build --only LF_Test_HideoutLeaseEmptyOrg` on main code (no caller of Release) failed `hideout/lease-empty-org` with `- the lease is free once A is empty (expected '', got '01a1eb08-...')`, `- A's org_base place is gone`, `- the door forgot A: its house identifier is empty`, `- B can lease the freed hideout`. Implemented: green.
//! hideout/lease-empty-org (#2571, epic #2533): an organization that loses its last member (Leave,
//! LeaveDirect or Kick) publishes LF_OnOrganizationEmptied and LF_Hideout releases every site it
//! leases: the lease is free, the org_base place is gone, the door lock no longer carries the old
//! organization id (an empty id reads "Unowned" to everyone), and another organization can lease it.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_HideoutLeaseEmptyOrg : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "hideout/lease-empty-org";
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

		LF_LockComponent lock = LF_Component<LF_LockComponent>.Find(door);
		ctx.NotNull(lock, "the door has a lock");
		if (!lock)
			return;

		LF_OrganizationManager.Reset();
		LF_Hideout.Reset();

		string site = LF_TestAnnex.HIDEOUT_ID;
		string founderA = "hideout-empty-founder-a";
		string founderB = "hideout-empty-founder-b";
		LF_Organization orgA = LF_OrganizationManager.Create(founderA, "Empty Crew A");
		LF_Organization orgB = LF_OrganizationManager.Create(founderB, "Empty Crew B");
		ctx.NotNull(orgA, "organization A exists");
		ctx.NotNull(orgB, "organization B exists");
		if (!orgA || !orgB)
			return;

		string idA = orgA.GetId();
		string baseKind = LF_PlacementZones.ORG_BASE_KIND;

		ctx.True(LF_Hideout.Assign(site, idA), "A leases the hideout");
		ctx.EqualStr(idA, LF_Hideout.GetOwner(site), "A owns the lease");
		ctx.EqualStr(idA, lock.GetHouseIdentifier(), "the door carries A's id");
		ctx.True(LF_Places.Find(baseKind, idA) == door, "the hideout is A's base");

		// A's only member leaves (the founder stays through Leave, so the playtest verb ends it).
		ctx.True(LF_OrganizationManager.LeaveDirect(founderA), "A's only member leaves");

		ctx.EqualStr("", LF_Hideout.GetOwner(site), "the lease is free once A is empty");
		ctx.True(LF_Places.Find(baseKind, idA) == null, "A's org_base place is gone");
		ctx.EqualStr("", lock.GetHouseIdentifier(), "the door forgot A: its house identifier is empty");
		ctx.True(LF_Hideout.IsLocked(site), "the freed hideout is locked");
		ctx.True(LF_Hideout.Assign(site, orgB.GetId()), "B can lease the freed hideout");
		ctx.EqualStr(orgB.GetId(), lock.GetHouseIdentifier(), "the door carries B's id");

		LF_Hideout.Reset();
		LF_OrganizationManager.Reset();
	}
}
