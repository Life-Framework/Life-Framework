// red-proof: 2026-11-17, a bug fix: one boot of this test against the unfixed LF_Jail.Imprison (no LF_JailEscort.Drop): `- the cell ends the escort hold`, `- no hold is left for the officer to drop (expected 0, got 1)`. Green on the fixed code.
// tier: WORLD (needs a cell origin, like jail/work)
//! An arrest ends the escort hold (crime review 2026-11-17). The hold used to stay active after an
//! action or menu arrest, so the freed prisoner's next logout counted as escaping custody.
class LF_Test_JailEscortDrop : LF_Test
{
	override string GetName()
	{
		return "jail/escort-drop";
	}

	override void Run(LF_TestContext ctx)
	{
		string officerUid = "escort-drop-officer";
		string prisonerUid = "escort-drop-prisoner";

		LF_Jail.Configure(60, 300, 1, 5);
		LF_Jail.SetCellOrigin("48 0 64");
		LF_JailEscort.ResetStatics();

		LF_PlayerAccount officer = LF_PlayerAccountManager.GetOrCreate(officerUid);
		officer.SetFaction(LF_Faction.POLICE);
		officer.SetOnDuty(true);
		officer.SetPoliceRank(LF_PoliceRanks.SENIOR);
		LF_PlayerAccount prisoner = LF_PlayerAccountManager.GetOrCreate(prisonerUid);
		prisoner.SetWantedLevel(2);

		ctx.True(LF_JailEscort.Escort(officerUid, prisonerUid), "the officer takes the suspect by the arm");
		ctx.True(LF_JailEscort.IsEscorted(prisonerUid), "the suspect is escorted");

		ctx.True(LF_Jail.Arrest(officerUid, prisonerUid, null), "the arrest starts a sentence");
		ctx.False(LF_JailEscort.IsEscorted(prisonerUid), "the cell ends the escort hold");
		ctx.Equal(0, LF_JailEscort.ReleasePlayer(officerUid), "no hold is left for the officer to drop");

		LF_JailEscort.ResetStatics();
	}
}
