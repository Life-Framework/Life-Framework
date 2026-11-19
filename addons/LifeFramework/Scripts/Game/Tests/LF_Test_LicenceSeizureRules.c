// red-proof: observed 2026-11-19 `tools\cli test --no-build --red LF_Test_LicenceSeizureRules` printed `RED-OK licence/seizure-rules (fails with the hook on, 18 assertions)`; `--only` then passed=1 failed=0.
//! #3428: licence seizure rules. A mapped crime code returns its bans, an unmapped one none, and two
//! codes that take the same licence give the longest ban for it. The shipped table maps a firearm
//! crime to the firearm permit and drunk driving to the driving licence.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_LicenceSeizureRules : LF_Test
{
	override string GetName()
	{
		return "licence/seizure-rules";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_LicenceSeizureRule Rule(string crimeId, array<string> licences, int days)
	{
		LF_LicenceSeizureRule rule = new LF_LicenceSeizureRule();
		rule.m_sCrimeId = crimeId;
		rule.m_aLicences = licences;
		rule.m_iBanDays = days;
		return rule;
	}

	//------------------------------------------------------------------------------------------------
	protected int DaysOf(notnull array<ref LF_LicenceBan> bans, LF_ELicenseType licence)
	{
		foreach (LF_LicenceBan ban : bans)
		{
			if (ban.m_eLicence == licence)
				return ban.m_iDays;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_LicenceSeizureRules rules = new LF_LicenceSeizureRules();
		rules.m_aRules = new array<ref LF_LicenceSeizureRule>();
		rules.m_aRules.Insert(Rule("gun", {"FIREARM_PERMIT"}, 10));
		rules.m_aRules.Insert(Rule("drunk", {"DRIVING_LICENCE"}, 5));
		rules.m_aRules.Insert(Rule("both", {"FIREARM_PERMIT", "DRIVING_LICENCE"}, 30));
		rules.m_aRules.Insert(Rule("typo", {"NO_SUCH_LICENCE"}, 9));

		array<ref LF_LicenceBan> bans = {};
		if (!RedProof())
			rules.CollectBans({"gun"}, bans);

		ctx.Equal(1, bans.Count(), "a mapped code takes one licence");
		ctx.Equal(10, DaysOf(bans, LF_ELicenseType.FIREARM_PERMIT), "the firearm crime bans the firearm permit for its days");

		rules.CollectBans({"nothing-mapped"}, bans);
		ctx.Equal(0, bans.Count(), "an unmapped code takes nothing");

		rules.CollectBans({}, bans);
		ctx.Equal(0, bans.Count(), "no codes take nothing");

		rules.CollectBans({"gun", "both"}, bans);
		ctx.Equal(2, bans.Count(), "two codes taking overlapping licences give one ban per licence");
		ctx.Equal(30, DaysOf(bans, LF_ELicenseType.FIREARM_PERMIT), "the longer ban wins on the shared licence");
		ctx.Equal(30, DaysOf(bans, LF_ELicenseType.DRIVING_LICENCE), "the other licence keeps its ban");

		rules.CollectBans({"both", "gun"}, bans);
		ctx.Equal(30, DaysOf(bans, LF_ELicenseType.FIREARM_PERMIT), "the order of the codes does not matter");

		rules.CollectBans({"gun", "drunk"}, bans);
		ctx.Equal(10, DaysOf(bans, LF_ELicenseType.FIREARM_PERMIT), "separate licences keep their own days");
		ctx.Equal(5, DaysOf(bans, LF_ELicenseType.DRIVING_LICENCE), "drunk driving bans the driving licence");

		rules.CollectBans({"typo"}, bans);
		ctx.Equal(0, bans.Count(), "a name that is not a licence is skipped, not a VME");

		// The shipped table.
		LF_LicenceSeizureRules.ResetStatics();
		LF_LicenceSeizureRules shipped = LF_LicenceSeizureRules.Get();
		ctx.NotNull(shipped, "the shipped Configs/Crime/LF_LicenceSeizure.conf resolves");
		if (shipped)
		{
			array<string> brandish = {"#LF-Ticket_OffenceBrandishing"};
			array<string> drunkDriving = {"#LF-Offence_DrunkDriving"};
			array<string> robbery = {"#LF-Offence_Robbery"};
			ctx.True(LF_LicenceSeizureRules.BanDays(brandish, LF_ELicenseType.FIREARM_PERMIT) > 0, "brandishing a weapon bans the firearm permit");
			ctx.True(LF_LicenceSeizureRules.BanDays(drunkDriving, LF_ELicenseType.DRIVING_LICENCE) > 0, "drunk driving bans the driving licence");
			ctx.Equal(-1, LF_LicenceSeizureRules.BanDays(brandish, LF_ELicenseType.DRIVING_LICENCE), "brandishing leaves the driving licence alone");
			ctx.Equal(-1, LF_LicenceSeizureRules.BanDays(robbery, LF_ELicenseType.FIREARM_PERMIT), "a crime with no row takes no licence");
		}

		LF_LicenceSeizureRules.ResetStatics();
	}
}
