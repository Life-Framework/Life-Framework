// red-proof: make EL_PoliceUtils.IsOnDutyOfficer ignore the duty flag, then run the fast tier;
// "an off-duty officer cannot run police actions" goes red.

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