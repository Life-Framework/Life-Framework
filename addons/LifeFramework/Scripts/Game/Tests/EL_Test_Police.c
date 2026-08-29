// red-proof: break a threshold in EL_PoliceUtils (e.g. GetWantedReduction 1000 -> 2000,
// or MAX_FINE 5000 -> 6000), then run the fast tier; every assertion touching the
// perturbed rule goes red.

// tier: LOGIC
class EL_Test_PoliceFineMath : EL_Test
{
	override string GetName()
	{
		return "police/fine-math";
	}

	override void Run(EL_TestContext ctx)
	{
		ctx.Equal(0, EL_PoliceUtils.GetWantedReduction(0), "a zero fine removes no wanted level");
		ctx.Equal(0, EL_PoliceUtils.GetWantedReduction(999), "fines below 1000 remove no wanted level");
		ctx.Equal(1, EL_PoliceUtils.GetWantedReduction(1000), "1000 removes one wanted level");
		ctx.Equal(2, EL_PoliceUtils.GetWantedReduction(2500), "2500 removes two wanted levels");
		ctx.Equal(5, EL_PoliceUtils.GetWantedReduction(5000), "the max fine removes five wanted levels");

		ctx.False(EL_PoliceUtils.IsSaneFineAmount(0), "zero fine is rejected");
		ctx.False(EL_PoliceUtils.IsSaneFineAmount(-100), "negative fine is rejected");
		ctx.True(EL_PoliceUtils.IsSaneFineAmount(1), "smallest fine is accepted");
		ctx.True(EL_PoliceUtils.IsSaneFineAmount(5000), "the max fine is accepted");
		ctx.False(EL_PoliceUtils.IsSaneFineAmount(5001), "fines above the max are rejected");

		EL_PlayerAccount account = EL_PlayerAccount.Create("test-fine-target");
		account.SetWantedLevel(3);
		account.IncreaseWantedLevel(-EL_PoliceUtils.GetWantedReduction(2500));
		ctx.Equal(1, account.GetWantedLevel(), "a 2500 fine drops wanted 3 to 1");
	}
};

//------------------------------------------------------------------------------------------------
// tier: LOGIC
class EL_Test_PoliceOnDutyOfficer : EL_Test
{
	override string GetName()
	{
		return "police/on-duty-officer";
	}

	override void Run(EL_TestContext ctx)
	{
		ctx.False(EL_PoliceUtils.IsOnDutyOfficer(null), "a missing account is never an on-duty officer");

		EL_PlayerAccount civilian = EL_PlayerAccount.Create("test-civilian");
		civilian.SetFaction(EL_Faction.CIVILIAN);
		civilian.SetOnDuty(true);
		ctx.False(EL_PoliceUtils.IsOnDutyOfficer(civilian), "a civilian on duty is not an officer");

		EL_PlayerAccount offDutyPolice = EL_PlayerAccount.Create("test-off-duty-police");
		offDutyPolice.SetFaction(EL_Faction.POLICE);
		offDutyPolice.SetOnDuty(false);
		ctx.False(EL_PoliceUtils.IsOnDutyOfficer(offDutyPolice), "an off-duty officer cannot run police actions");

		EL_PlayerAccount onDutyPolice = EL_PlayerAccount.Create("test-on-duty-police");
		onDutyPolice.SetFaction(EL_Faction.POLICE);
		onDutyPolice.SetOnDuty(true);
		ctx.True(EL_PoliceUtils.IsOnDutyOfficer(onDutyPolice), "an on-duty police account passes the gate");
	}
};