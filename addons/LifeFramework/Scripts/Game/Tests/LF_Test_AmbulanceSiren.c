// red-proof: LoopWanted returned hasOwnLoop alone -> [LFTEST] FAIL but not in the default mode
// tier: LOGIC
//! The ambulance's siren (#923): its own loop plays in every mode but default and only when the
//! vehicle has one, and the duty rule that admits only an on-duty medic.
class LF_Test_AmbulanceSiren : LF_Test
{
	override string GetName()
	{
		return "sirenlights/ambulance-siren";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.True(LF_AmbulanceSiren.LoopWanted(true, LF_AmbulanceSiren.MODE_ON), "a vehicle with its own loop plays it in the on mode");
		ctx.True(LF_AmbulanceSiren.LoopWanted(true, "Mode1"), "and in any other running mode");
		ctx.False(LF_AmbulanceSiren.LoopWanted(true, LF_AmbulanceSiren.MODE_OFF), "but not in the default mode");
		ctx.False(LF_AmbulanceSiren.LoopWanted(false, LF_AmbulanceSiren.MODE_ON), "a vehicle without its own loop leaves the shared siren to play");

		ctx.True(LF_SirenRules.DutyAllows(LF_Faction.MEDIC, true, LF_Faction.MEDIC, true), "an on-duty medic runs the ambulance siren");
		ctx.False(LF_SirenRules.DutyAllows(LF_Faction.MEDIC, true, LF_Faction.MEDIC, false), "a medic off duty does not");
		ctx.False(LF_SirenRules.DutyAllows(LF_Faction.MEDIC, true, LF_Faction.POLICE, true), "an on-duty officer does not run the ambulance's");
		ctx.False(LF_SirenRules.DutyAllows(LF_Faction.MEDIC, false, LF_Faction.MEDIC, true), "nobody with no account does");
	}
}
