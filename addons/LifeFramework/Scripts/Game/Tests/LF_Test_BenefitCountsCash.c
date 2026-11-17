// red-proof: observed 2026-11-17 with `tools\cli test --no-build --only LF_Test_BenefitCountsCash` on the
// unfixed means test (bank only):
// [LFTEST]   - carried cash above the threshold collects nothing (expected 0, got 100)
// [LFTEST]   - the refused claim left the cash as it was (expected 5000, got 5100)
// Green once LF_UnemploymentBenefit.CollectFor adds the carried cash.

//------------------------------------------------------------------------------------------------
//! The benefit's means test counts carried cash as well as the bank (economy review 2026-11-17). A
//! player who keeps their money in their pockets and the bank empty must not collect the safety net.
// tier: WORLD
class LF_Test_BenefitCountsCash : LF_Test
{
	protected static const string RICH_UID = "benefit-cash-rich";
	protected static const string BROKE_UID = "benefit-cash-broke";

	override string GetName()
	{
		return "jobs/benefit-counts-cash";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_JobsConfig config = LF_JobsConfig.Get();
		if (!config || config.m_iBenefitAmount <= 0 || config.m_iBenefitBalanceThreshold <= 0)
		{
			ctx.Fail("the jobs config sets a benefit amount and threshold");
			return;
		}

		LF_UnemploymentBenefit.ForgetCollection(RICH_UID);
		LF_UnemploymentBenefit.ForgetCollection(BROKE_UID);

		IEntity rich = LF_ExploitGuardsWorldFixture.SpawnCharacter(ctx, "92 0 90", true);
		IEntity broke = LF_ExploitGuardsWorldFixture.SpawnCharacter(ctx, "94 0 90", true);
		if (!rich || !broke)
			return;

		// Both have an empty bank; the rich one carries ten times the threshold in cash.
		int pocket = config.m_iBenefitBalanceThreshold * 10;
		ctx.Equal(pocket, LF_MoneyUtils.AddCash(rich, pocket, "test"), "the rich body carries its cash");
		ctx.Equal(0, LF_ATMManager.GetOrCreate(RICH_UID).GetBalance(), "the rich body's bank is empty");
		ctx.Equal(0, LF_ATMManager.GetOrCreate(BROKE_UID).GetBalance(), "the broke body's bank is empty");

		ctx.Equal(0, LF_UnemploymentBenefit.CollectFor(RICH_UID, rich), "carried cash above the threshold collects nothing");
		ctx.Equal(pocket, LF_MoneyUtils.GetCash(rich), "the refused claim left the cash as it was");
		ctx.Equal(config.m_iBenefitAmount, LF_UnemploymentBenefit.CollectFor(BROKE_UID, broke), "a broke body still collects the benefit");

		LF_UnemploymentBenefit.ForgetCollection(RICH_UID);
		LF_UnemploymentBenefit.ForgetCollection(BROKE_UID);
		SCR_EntityHelper.DeleteEntityAndChildren(rich);
		SCR_EntityHelper.DeleteEntityAndChildren(broke);
	}
}
