// red-proof: observed 2026-11-10 with `toolsi test --no-build --only LF_Test_SirenDuty`: dropping the onDuty term from DutyAllows failed it with `- off-duty police are refused`. Restored; green.

//------------------------------------------------------------------------------------------------
// tier: LOGIC
//! vehicles/siren-duty (#464): only an on-duty member of the vehicle's service runs the siren.
class LF_Test_SirenDuty : LF_Test
{
	override string GetName()
	{
		return "vehicles/siren-duty";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.True(LF_SirenRules.DutyAllows(LF_Faction.POLICE, true, LF_Faction.POLICE, true), "on-duty police run a police siren");
		ctx.True(LF_SirenRules.DutyAllows(LF_Faction.MEDIC, true, LF_Faction.MEDIC, true), "an on-duty medic runs an ambulance siren");
		ctx.False(LF_SirenRules.DutyAllows(LF_Faction.POLICE, true, LF_Faction.POLICE, false), "off-duty police are refused");
		ctx.False(LF_SirenRules.DutyAllows(LF_Faction.POLICE, true, LF_Faction.CIVILIAN, true), "a thief in a police car is refused");
		ctx.False(LF_SirenRules.DutyAllows(LF_Faction.POLICE, true, LF_Faction.MEDIC, true), "a medic is refused on a police car");
		ctx.False(LF_SirenRules.DutyAllows(LF_Faction.MEDIC, true, LF_Faction.POLICE, true), "police are refused on an ambulance");
		ctx.False(LF_SirenRules.DutyAllows(LF_Faction.POLICE, false, LF_Faction.POLICE, true), "no account is refused");
		ctx.False(LF_SirenRules.DutyAllows(LF_Faction.CIVILIAN, true, LF_Faction.CIVILIAN, true), "a vehicle with no service admits nobody");
	}
}
