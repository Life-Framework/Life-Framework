// red-proof: 2026-11-17 `tools/cli test --no-build --only LF_Test_OrgUpgradeUnwired,LF_Test_OrgFundOverflow,LF_Test_HideoutStashRight,LF_Test_HideoutLoadRelock` on the unfixed code (only the IsRelockScheduled accessor added) failed all four, here "extra storage, which nothing reads, is not sold". Green with the fix (--only, 20 org and hideout tests).
// tier: LOGIC
//! organization/upgrade-unwired (review 2026-11-17): extra storage and the parked vehicle are flags no
//! system reads, yet the ledger RPC sold them and took the fund. They are refused and charge nothing;
//! the processing slot, which LF_Hideout reads, still sells.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_OrgUpgradeUnwired : LF_Test
{
	protected static const string FOUNDER = "org-unwired-founder";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "organization/upgrade-unwired";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_TestLegacySnapshot kept = LF_TestLegacySnapshot.Take();
		LF_OrganizationManager.Reset();

		LF_Organization org = LF_OrganizationManager.Create(FOUNDER, "Unwired Test Crew");
		ctx.True(org != null, "the founder registers an organization");
		if (org)
		{
			org.SetFund(10000);
			bool storage = LF_OrganizationManager.BuyUpgrade(org.GetId(), FOUNDER, LF_EOrganizationUpgrade.EXTRA_STORAGE);
			bool vehicle = LF_OrganizationManager.BuyUpgrade(org.GetId(), FOUNDER, LF_EOrganizationUpgrade.PARKED_VEHICLE);

			ctx.False(storage, "extra storage, which nothing reads, is not sold");
			ctx.False(vehicle, "the parked vehicle, which nothing reads, is not sold");
			ctx.False(org.HasUpgrade(LF_EOrganizationUpgrade.EXTRA_STORAGE), "no extra storage flag is set");
			ctx.Equal(10000, org.GetFund(), "the refused upgrades charged the fund nothing");

			ctx.True(LF_OrganizationManager.BuyUpgrade(org.GetId(), FOUNDER, LF_EOrganizationUpgrade.PROCESSING_SLOT), "the processing slot still sells");
			ctx.Equal(10000 - LF_OrganizationManager.UpgradePrice(LF_EOrganizationUpgrade.PROCESSING_SLOT), org.GetFund(), "the processing slot took its price");
		}

		LF_OrganizationManager.Reset();
		kept.Restore();
	}
}
