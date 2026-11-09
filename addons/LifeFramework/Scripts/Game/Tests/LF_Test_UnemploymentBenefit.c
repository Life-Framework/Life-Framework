// red-proof: observed 2026-11-08 with `tools\cli test --no-build --tier fast`, both perturbations
// restored afterwards: `bankBalance > threshold` in Refusal and dropping
// `s_mLastCollectedAt.Set(uid, now);` in Claim printed
// [LFTEST]   - repeated collection spam pays nothing more (expected 0, got 500)
// [LFTEST]   - one second short of the cooldown is refused (expected 0, got 100)
// [LFTEST]   - a balance at the threshold is refused (expected 0, got 100)

// tier: LOGIC
class LF_Test_UnemploymentBenefit : LF_Test
{
	override string GetName()
	{
		return "jobs/unemployment-benefit";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_JobsConfig config = LF_JobsConfig.Get();
		ctx.True(config != null, "the jobs config loads");
		if (config)
		{
			ctx.True(config.m_iBenefitAmount > 0, "the config sets a benefit amount");
			ctx.True(config.m_iBenefitBalanceThreshold > 0, "the config sets a balance threshold");
			ctx.True(config.m_fBenefitCooldownSeconds > 0, "the config sets a cooldown");
		}

		int threshold = 500;
		int amount = 100;
		float cooldown = 1200;
		float t0 = 1000;
		string reason;

		// Below the threshold pays once; spam right after pays nothing.
		ctx.Equal(amount, LF_UnemploymentBenefit.Claim("benefit-broke", 499, false, t0, threshold, amount, cooldown, reason), "a balance below the threshold collects the benefit");
		int spamPaid = 0;
		for (int i = 0; i < 5; i++)
		{
			spamPaid += LF_UnemploymentBenefit.Claim("benefit-broke", 0, false, t0 + i, threshold, amount, cooldown, reason);
		}
		ctx.Equal(0, spamPaid, "repeated collection spam pays nothing more");
		ctx.EqualStr(LF_UnemploymentBenefit.REFUSED_COOLDOWN, reason, "the spam refusal is the cooldown");

		// Cooldown boundary.
		ctx.Equal(0, LF_UnemploymentBenefit.Claim("benefit-broke", 0, false, t0 + cooldown - 1, threshold, amount, cooldown, reason), "one second short of the cooldown is refused");
		ctx.Equal(amount, LF_UnemploymentBenefit.Claim("benefit-broke", 0, false, t0 + cooldown, threshold, amount, cooldown, reason), "after the cooldown the benefit pays again");

		// At or above the threshold is refused.
		ctx.Equal(0, LF_UnemploymentBenefit.Claim("benefit-at", 500, false, t0, threshold, amount, cooldown, reason), "a balance at the threshold is refused");
		ctx.EqualStr(LF_UnemploymentBenefit.REFUSED_BALANCE, reason, "the at-threshold refusal is the balance");
		ctx.Equal(0, LF_UnemploymentBenefit.Claim("benefit-rich", 99999, false, t0, threshold, amount, cooldown, reason), "a balance above the threshold is refused");

		// Working is refused.
		ctx.Equal(0, LF_UnemploymentBenefit.Claim("benefit-working", 0, true, t0, threshold, amount, cooldown, reason), "a working player is refused");
		ctx.EqualStr(LF_UnemploymentBenefit.REFUSED_WORKING, reason, "the working refusal names work");
		ctx.Equal(0, LF_UnemploymentBenefit.Claim("benefit-off", 0, false, t0, threshold, 0, cooldown, reason), "a zero amount disables the benefit");
		ctx.Equal(0, LF_UnemploymentBenefit.Claim("", 0, false, t0, threshold, amount, cooldown, reason), "no account id is refused");

		LF_PlayerAccount onDuty = LF_PlayerAccountManager.GetOrCreate("benefit-cop-on");
		onDuty.SetFaction(LF_Faction.POLICE);
		onDuty.SetOnDuty(true);
		ctx.True(LF_UnemploymentBenefit.IsWorking("benefit-cop-on"), "an on-duty officer is working");
		LF_PlayerAccount offDuty = LF_PlayerAccountManager.GetOrCreate("benefit-cop-off");
		offDuty.SetFaction(LF_Faction.POLICE);
		offDuty.SetOnDuty(false);
		ctx.False(LF_UnemploymentBenefit.IsWorking("benefit-cop-off"), "an off-duty officer is not working");
		LF_PlayerAccount civilian = LF_PlayerAccountManager.GetOrCreate("benefit-civilian");
		civilian.SetFaction(LF_Faction.CIVILIAN);
		ctx.False(LF_UnemploymentBenefit.IsWorking("benefit-civilian"), "a civilian with no run is not working");
	}
}
