// red-proof: Observed 2026-11-10 with `tools/cli test --no-build --only LF_Test_Witness`: letting every distance see clothing in LF_Witness.PresetsFor failed `investigation/witness` with `- a far witness gets no clothing preset`. Restored; green.
// tier: LOGIC
//! The payphone witness (#266): a witness out of range gets no clothing and no plate, a near one
//! gets the clothing and a longer plate fragment than a mid-range one, someone who never stood in
//! sight can say nothing, a thing is said once, and the statement reaches the board entries.
class LF_Test_Witness : LF_Test
{
	protected static const string NEAR_UID = "witness-near";
	protected static const string MID_UID = "witness-mid";
	protected static const string FAR_UID = "witness-far";
	protected static const string AWAY_UID = "witness-away";
	protected static const string PLATE = "AB1234X";

	override string GetName()
	{
		return "investigation/witness";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_Witness.ResetStatics();
		LF_WitnessScene scene = LF_Witness.OpenScene(41, "witness-suspect", LF_WantedReports.OFFENCE_ROBBERY, "#jacket|#pants", "#car", PLATE, "Test Square", 1000);
		ctx.True(LF_Witness.AddWitness(scene, NEAR_UID, false, 5), "a player 5 m away saw the crime");
		ctx.True(LF_Witness.AddWitness(scene, MID_UID, false, 30), "a player 30 m away saw the crime");
		ctx.True(LF_Witness.AddWitness(scene, FAR_UID, false, 55), "a player 55 m away is still in sight");
		ctx.False(LF_Witness.AddWitness(scene, AWAY_UID, false, 500), "a player 500 m away saw nothing");
		ctx.False(LF_Witness.AddWitness(scene, "witness-suspect", false, 1), "the suspect is not their own witness");

		ctx.True(LF_Witness.Give(scene, FAR_UID, LF_EWitnessField.CLOTHING, "payphone") == LF_EWitnessResult.NOT_SEEN, "a far witness gets no clothing preset");
		ctx.True(LF_Witness.Give(scene, FAR_UID, LF_EWitnessField.PLATE, "payphone") == LF_EWitnessResult.NOT_SEEN, "a far witness reads no plate");
		ctx.True(LF_Witness.Give(scene, AWAY_UID, LF_EWitnessField.CLOTHING, "payphone") == LF_EWitnessResult.NOT_A_WITNESS, "someone out of sight is no witness");
		ctx.True(LF_Witness.Give(null, NEAR_UID, LF_EWitnessField.CLOTHING, "payphone") == LF_EWitnessResult.NO_SCENE, "no crime, no statement");

		ctx.True(LF_Witness.Give(scene, NEAR_UID, LF_EWitnessField.CLOTHING, "payphone") == LF_EWitnessResult.OK, "a near witness describes the clothing");
		ctx.True(LF_Witness.Give(scene, NEAR_UID, LF_EWitnessField.CLOTHING, "payphone") == LF_EWitnessResult.ALREADY_GIVEN, "the same thing is said once");
		ctx.True(LF_Witness.Give(scene, NEAR_UID, LF_EWitnessField.PLATE, "payphone") == LF_EWitnessResult.OK, "a near witness reads the plate");
		ctx.True(LF_Witness.Give(scene, MID_UID, LF_EWitnessField.PLATE, "payphone") == LF_EWitnessResult.OK, "a mid-range witness reads part of the plate");

		array<LF_WitnessStatement> statements = {};
		LF_Witness.CollectFor(41, statements);
		ctx.Equal(2, statements.Count(), "two witnesses made statements");
		string near;
		string mid;
		foreach (LF_WitnessStatement statement : statements)
		{
			if (statement.m_sWitnessKey == NEAR_UID)
				near = statement.m_sPlateFragment;
			else
				mid = statement.m_sPlateFragment;
		}

		ctx.True(near.Length() > mid.Length(), "a near witness reads a longer plate fragment than a mid-range one");
		ctx.True(mid.Length() > 0, "a mid-range witness still reads something");
		ctx.EqualStr(PLATE, near, "a near witness reads the whole plate");

		array<string> entries = {};
		LF_Witness.AppendBoardEntries(10, entries);
		ctx.Equal(2, entries.Count(), "both statements reach the board entries");
		ctx.True(LF_Witness.IsBoardEntry(entries[0]), "board entries are marked as witness lines");
		ctx.True(entries[0].Contains("anonymous") || entries[0].Contains("Witness_Anonymous"), "a statement names no caller by default");
		ctx.False(entries[0].Contains(NEAR_UID), "the board never carries the caller's uid");

		LF_Witness.ResetStatics();
	}
}
