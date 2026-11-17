// red-proof: Observed 2026-11-17 `tools/cli test --no-build --only LF_Test_NpcWitness` on the unfixed LF_Witness.Ask failed `investigation/npc-witness` with `- asked again, the NPC is not re-rolled and says nothing more (#2419) (expected 0, got 2)`. Fixed: green. Earlier proof, 2026-11-10: dropping the range check in LF_Witness.AddWitness failed it with `- an NPC 80 m away saw nothing`.
// tier: LOGIC
//! Officers question witnesses in person (#267): a town NPC that stood in range gives a statement,
//! one out of range saw nothing, a low-accuracy roll leaves a statement partial, a player witness
//! says everything it could see, and the statement reaches the board.
class LF_Test_NpcWitness : LF_Test
{
	protected static const string NPC_NEAR = "npc:test-near";
	protected static const string NPC_FAR = "npc:test-far";
	protected static const string PLAYER = "npc-witness-player";

	override string GetName()
	{
		return "investigation/npc-witness";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_Witness.ResetStatics();
		LF_WitnessScene scene = LF_Witness.OpenScene(52, "npc-witness-suspect", LF_WantedReports.OFFENCE_ROBBERY, "#jacket|#pants", "#car", "ZX9876Q", "Shop", 5000);
		ctx.True(LF_Witness.AddWitness(scene, NPC_NEAR, true, 10), "an NPC 10 m away saw the crime");
		ctx.False(LF_Witness.AddWitness(scene, NPC_FAR, true, 80), "an NPC 80 m away saw nothing");
		ctx.True(LF_Witness.AddWitness(scene, PLAYER, false, 6), "a player 6 m away saw the crime");

		ctx.True(LF_Witness.SceneFor(NPC_FAR, 6000) == null, "the far NPC has no scene to talk about");
		ctx.Equal(0, LF_Witness.Ask(LF_Witness.SceneFor(NPC_FAR, 6000), NPC_FAR, null, "officer"), "the far NPC says it saw nothing");
		ctx.True(LF_Witness.AskKey(false, 0, 2) == LF_WitnessDialogue.UnsureKey(2), "the NPC that saw nothing answers noncommittally");

		array<float> partial = {0.0, 0.99, 0.99};
		ctx.Equal(1, LF_Witness.Ask(LF_Witness.SceneFor(NPC_NEAR, 6000), NPC_NEAR, partial, "officer"), "a forgetful NPC remembers only what its roll allows");
		array<LF_WitnessStatement> statements = {};
		LF_Witness.CollectFor(52, statements);
		ctx.Equal(1, statements.Count(), "the NPC's partial statement is on file");
		ctx.True(!statements[0].m_sClothing.IsEmpty() && statements[0].m_sVehicle.IsEmpty() && statements[0].m_sPlateFragment.IsEmpty(), "the statement holds the clothing and nothing else");

		array<float> sharp = {0.0, 0.0, 0.0};
		ctx.Equal(0, LF_Witness.Ask(LF_Witness.SceneFor(NPC_NEAR, 6000), NPC_NEAR, sharp, "officer"), "asked again, the NPC is not re-rolled and says nothing more (#2419)");
		LF_Witness.CollectFor(52, statements);
		ctx.Equal(1, statements.Count(), "the second ask leaves no second statement");
		ctx.True(!statements[0].m_sClothing.IsEmpty() && statements[0].m_sVehicle.IsEmpty() && statements[0].m_sPlateFragment.IsEmpty(), "the statement is still the clothing and nothing else");

		ctx.Equal(3, LF_Witness.Ask(LF_Witness.SceneFor(PLAYER, 6000), PLAYER, null, "officer"), "a player witness says everything it could see");

		array<string> entries = {};
		LF_Witness.AppendBoardEntries(10, entries);
		ctx.Equal(2, entries.Count(), "both statements reach the board");
		ctx.True(LF_Witness.SceneFor(NPC_NEAR, 5000 + LF_WitnessConfig.WindowSeconds() * 1000 + 1) == null, "an old crime is forgotten");
		LF_Witness.ResetStatics();
	}
}
