// red-proof: 2026-11-17, a bug fix: one boot of this test against the unfixed LF_Jail.Arrest (no self check): `- an officer cannot arrest themself`, `- no sentence starts`. Green on the fixed code.
// tier: WORLD (needs a cell origin, like jail/work)
//! An on-duty officer who is also wanted cannot arrest themself (crime review 2026-11-17). The police
//! menu RPC passed any uid in reach, and the officer's own body is always in reach.
class LF_Test_JailSelfArrest : LF_Test
{
	override string GetName()
	{
		return "jail/self-arrest";
	}

	override void Run(LF_TestContext ctx)
	{
		string officerUid = "self-arrest-officer";

		LF_Jail.Configure(60, 300, 1, 5);
		LF_Jail.SetCellOrigin("48 0 64");

		LF_PlayerAccount officer = LF_PlayerAccountManager.GetOrCreate(officerUid);
		officer.SetFaction(LF_Faction.POLICE);
		officer.SetOnDuty(true);
		officer.SetPoliceRank(LF_PoliceRanks.SENIOR);
		officer.SetWantedLevel(3);

		ctx.False(LF_Jail.Arrest(officerUid, officerUid, null), "an officer cannot arrest themself");
		ctx.False(LF_Jail.IsServing(officerUid), "no sentence starts");

	}
}
