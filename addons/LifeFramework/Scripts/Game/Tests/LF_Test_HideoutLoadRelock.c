// red-proof: 2026-11-17 `tools/cli test --no-build --only LF_Test_OrgUpgradeUnwired,LF_Test_OrgFundOverflow,LF_Test_HideoutStashRight,LF_Test_HideoutLoadRelock` on the unfixed code (only the IsRelockScheduled accessor added) failed all four, here "a load with the gate open queues the relock sweep". Green with the fix.
// tier: LOGIC
//! hideout/load-relock (review 2026-11-17): a gate saved open loaded open, and only a member's unlock
//! started the relock sweep, so after a restart the gate stayed open for anyone. A load with an open
//! gate queues the sweep; a load with every gate shut queues nothing.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_HideoutLoadRelock : LF_Test
{
	protected static const string SITE = "hideout-load-relock-site";
	protected static const string FOUNDER = "hideout-load-relock-founder";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "hideout/load-relock";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_OrganizationManager.Reset();
		LF_Hideout.ResetStatics();

		LF_Organization org = LF_OrganizationManager.Create(FOUNDER, "Load Relock Crew");
		ctx.NotNull(org, "the organization exists");
		if (org)
		{
			array<ref LF_HideoutRecord> shut = {};
			shut.Insert(LF_HideoutRecord.Create(SITE, org.GetId(), true));
			LF_Hideout.ApplyAll(shut);
			ctx.False(LF_Hideout.IsRelockScheduled(), "a load with the gate shut queues no sweep");

			array<ref LF_HideoutRecord> open = {};
			open.Insert(LF_HideoutRecord.Create(SITE, org.GetId(), false));
			LF_Hideout.ApplyAll(open);
			ctx.False(LF_Hideout.IsLocked(SITE), "the gate loads as it was saved");
			ctx.True(LF_Hideout.IsRelockScheduled(), "a load with the gate open queues the relock sweep");
		}

		LF_OrganizationManager.Reset();
		LF_Hideout.ResetStatics();
	}
}
