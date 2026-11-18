// red-proof: 2026-11-17 `tools/cli test --no-build --only LF_Test_OrgUpgradeUnwired,LF_Test_OrgFundOverflow,LF_Test_HideoutStashRight,LF_Test_HideoutLoadRelock` on the unfixed code (only the IsRelockScheduled accessor added) failed all four, here "a rank without the stash right is refused". Green with the fix.
//! hideout/stash-right (review 2026-11-17): the rank's stash right (USE_STASH) was saved, shown and
//! editable in the rank config, yet the stash opened for every member. A rank without it is shut out;
//! a rank with it still gets in, and a non-member never does.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_HideoutStashRight : LF_Test
{
	protected static const string SITE = "hideout-stash-right-site";
	protected static const string FOUNDER = "hideout-stash-right-founder";
	protected static const string MEMBER = "hideout-stash-right-member";
	protected static const string OUTSIDER = "hideout-stash-right-outsider";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "hideout/stash-right";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_OrganizationManager.Reset();
		LF_Hideout.Reset();

		LF_Organization org = LF_OrganizationManager.Create(FOUNDER, "Stash Right Crew");
		ctx.NotNull(org, "the organization exists");
		if (org)
		{
			ctx.True(LF_OrganizationManager.Invite(org.GetId(), FOUNDER, MEMBER), "a member joins");
			ctx.True(LF_Hideout.Assign(SITE, org.GetId()), "the organization leases the site");
			ctx.True(LF_Hideout.AllowsStash(MEMBER, SITE, false), "a member rank with the stash right opens the stash");

			// The owner's config took the stash right off the bottom rung.
			ctx.True(org.ApplyRankRow(LF_OrganizationRankRow.Create(org.GetId(), LF_OrganizationConfig.RANK_MEMBER, 2, 0)), "the member rung loses every right");
			ctx.False(LF_Hideout.AllowsStash(MEMBER, SITE, false), "a rank without the stash right is refused");
			ctx.True(LF_Hideout.AllowsStash(FOUNDER, SITE, false), "the founder keeps the stash");
			ctx.False(LF_Hideout.AllowsStash(OUTSIDER, SITE, false), "a non-member is refused");
		}

		LF_OrganizationManager.Reset();
		LF_Hideout.ResetStatics();
	}
}
