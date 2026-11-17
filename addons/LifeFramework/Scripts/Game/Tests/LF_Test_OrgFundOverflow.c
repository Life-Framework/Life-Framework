// red-proof: 2026-11-17 `tools/cli test --no-build --only LF_Test_OrgUpgradeUnwired,LF_Test_OrgFundOverflow,LF_Test_HideoutStashRight,LF_Test_HideoutLoadRelock` on the unfixed code (only the IsRelockScheduled accessor added) failed all four, here "the fund did not wrap (expected 2147483637, got -2147483559)". Green with the fix.
// tier: LOGIC
//! organization/fund-overflow (review 2026-11-17): the fund is an int and a deposit added to it with no
//! ceiling, so a fund near int.MAX wrapped negative on the next deposit. A deposit that would pass the
//! ceiling is refused and the fund is unchanged (LF_OrganizationPlayer.DepositCash then gives the cash back).
class LF_Test_OrgFundOverflow : LF_Test
{
	protected static const string FOUNDER = "org-overflow-founder";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "organization/fund-overflow";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_TestLegacySnapshot kept = LF_TestLegacySnapshot.Take();
		LF_OrganizationManager.Reset();

		LF_Organization org = LF_OrganizationManager.Create(FOUNDER, "Overflow Test Crew");
		ctx.True(org != null, "the founder registers an organization");
		if (org)
		{
			int nearTop = int.MAX - 10;
			org.SetFund(nearTop);
			bool deposited = LF_OrganizationManager.Deposit(org.GetId(), FOUNDER, 100);

			ctx.False(deposited, "a deposit past the fund's ceiling is refused");
			ctx.Equal(nearTop, org.GetFund(), "the fund did not wrap");
			ctx.True(org.GetFund() > 0, "the fund stays positive");
			ctx.True(LF_OrganizationManager.Deposit(org.GetId(), FOUNDER, 10), "a deposit that fits is taken");
			ctx.Equal(int.MAX, org.GetFund(), "the fund holds its ceiling exactly");
		}

		LF_OrganizationManager.Reset();
		kept.Restore();
	}
}
