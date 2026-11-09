// red-proof: make LF_PoliceUtils.IsOnDutyOfficer ignore the duty flag, then run the fast tier;
// "an off-duty officer cannot run police actions" goes red.

// tier: LOGIC
class LF_Test_PoliceOnDutyOfficer : LF_Test
{
	override string GetName()
	{
		return "police/on-duty-officer";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.False(LF_PoliceUtils.IsOnDutyOfficer(null), "a missing account is never an on-duty officer");

		LF_PlayerAccount civilian = LF_PlayerAccount.Create("test-civilian");
		civilian.SetFaction(LF_Faction.CIVILIAN);
		civilian.SetOnDuty(true);
		ctx.False(LF_PoliceUtils.IsOnDutyOfficer(civilian), "a civilian on duty is not an officer");

		LF_PlayerAccount offDutyPolice = LF_PlayerAccount.Create("test-off-duty-police");
		offDutyPolice.SetFaction(LF_Faction.POLICE);
		offDutyPolice.SetOnDuty(false);
		ctx.False(LF_PoliceUtils.IsOnDutyOfficer(offDutyPolice), "an off-duty officer cannot run police actions");

		LF_PlayerAccount onDutyPolice = LF_PlayerAccount.Create("test-on-duty-police");
		onDutyPolice.SetFaction(LF_Faction.POLICE);
		onDutyPolice.SetOnDuty(true);
		ctx.True(LF_PoliceUtils.IsOnDutyOfficer(onDutyPolice), "an on-duty police account passes the gate");
	}
};