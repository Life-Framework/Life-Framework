// red-proof: Observed 2026-11-17 `tools/cli test --no-build --only LF_Test_HideoutRelock` with the relock check and the load release disabled failed `hideout/relock-and-load` with `- the door relocks when the last member has left`. Restored: green.
// tier: LOGIC
//! #2419: a hideout door a member opened relocks once no member is near it, a load that omits a
//! live lease unleases it (the load is idempotent), and the world-end reset forgets every lease.
class LF_Test_HideoutRelock : LF_Test
{
	protected static const string SITE_A = "hideout-relock-site-a";
	protected static const string SITE_B = "hideout-relock-site-b";

	override string GetName()
	{
		return "hideout/relock-and-load";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_OrganizationManager.Reset();
		LF_Hideout.Reset();

		string founderA = "hideout-relock-founder-a";
		string founderB = "hideout-relock-founder-b";
		string member = "hideout-relock-member";
		LF_Organization orgA = LF_OrganizationManager.Create(founderA, "Relock Crew A");
		LF_Organization orgB = LF_OrganizationManager.Create(founderB, "Relock Crew B");
		ctx.NotNull(orgA, "organization A exists");
		ctx.NotNull(orgB, "organization B exists");
		if (!orgA || !orgB)
			return;

		ctx.True(LF_OrganizationManager.Invite(orgA.GetId(), founderA, member), "a member joins A");
		ctx.True(LF_Hideout.Assign(SITE_A, orgA.GetId()), "A leases site A");
		ctx.True(LF_Hideout.Assign(SITE_B, orgB.GetId()), "B leases site B");

		RunRelock(ctx, member);
		RunLoad(ctx, orgA, orgB);
		RunReset(ctx);

		LF_OrganizationManager.Reset();
		LF_Hideout.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunRelock(LF_TestContext ctx, string member)
	{
		ctx.True(LF_Hideout.TryUnlock(member, SITE_A), "a member opens the door");
		ctx.False(LF_Hideout.IsLocked(SITE_A), "the door is open");
		ctx.False(LF_Hideout.RelockIfAbandoned(SITE_A, 1), "a member standing by keeps the door open");
		ctx.False(LF_Hideout.IsLocked(SITE_A), "the door stays open while a member is near");
		ctx.True(LF_Hideout.RelockIfAbandoned(SITE_A, 0), "the door relocks when the last member has left");
		ctx.True(LF_Hideout.IsLocked(SITE_A), "the door is locked again");
		ctx.False(LF_Hideout.RelockIfAbandoned(SITE_A, 0), "a locked door is left alone");
		ctx.False(LF_Hideout.RelockIfAbandoned("hideout-relock-nowhere", 0), "an unknown site is left alone");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunLoad(LF_TestContext ctx, LF_Organization orgA, LF_Organization orgB)
	{
		array<ref LF_HideoutRecord> records = {};
		records.Insert(LF_HideoutRecord.Create(SITE_B, orgB.GetId(), true));
		LF_Hideout.ApplyAll(records);
		ctx.EqualStr(orgB.GetId(), LF_Hideout.GetOwner(SITE_B), "the saved lease is applied");
		ctx.EqualStr("", LF_Hideout.GetOwner(SITE_A), "a lease the save does not hold is released by the load");
		ctx.True(LF_Hideout.IsLocked(SITE_A), "the released site is locked");

		LF_Hideout.ApplyAll(records);
		ctx.EqualStr(orgB.GetId(), LF_Hideout.GetOwner(SITE_B), "applying the same save again changes nothing");
		ctx.EqualStr("", LF_Hideout.GetOwner(SITE_A), "the released site stays released");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunReset(LF_TestContext ctx)
	{
		LF_Hideout.ResetStatics();
		ctx.EqualStr("", LF_Hideout.GetOwner(SITE_B), "the world-end reset forgets every lease");
		ctx.True(LF_Hideout.IsLocked(SITE_B), "an unknown site reads locked");
		ctx.True(LF_Hideout.ExportAll().Count() == 0, "nothing is exported after the reset");
	}
}
