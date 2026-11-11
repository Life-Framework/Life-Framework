// red-proof: Observed 2026-11-11 with `tools/cli test --no-build --only LF_Test_WitnessDialogue`: inverting the `!sawScene` check in LF_WitnessDialogue.AnswerKey failed investigation/witness-dialogue; restored; green.
// tier: LOGIC
//! What a questioned witness says (#615): NPC names are stable per spot and varied across spots,
//! and an NPC that saw nothing answers with one of the noncommittal lines, never "you didn't see
//! anything".
class LF_Test_WitnessDialogue : LF_Test
{
	override string GetName()
	{
		return "investigation/witness-dialogue";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.EqualStr(LF_WitnessDialogue.NameFromSeed(41), LF_WitnessDialogue.NameFromSeed(41), "the same seed gives the same name");
		map<string, bool> seen = new map<string, bool>();
		for (int i = 0; i < 40; i++)
			seen.Set(LF_WitnessDialogue.NameFromSeed(i * 7 + 3), true);

		ctx.True(seen.Count() >= 20, "forty spots give at least twenty different names");
		ctx.True(LF_WitnessDialogue.NameFromSeed(-5).Contains(" "), "a name is first name and surname");

		map<string, bool> lines = new map<string, bool>();
		for (int r = 0; r < LF_WitnessDialogue.UNSURE_VARIANTS * 3; r++)
			lines.Set(LF_WitnessDialogue.UnsureKey(r), true);

		ctx.Equal(LF_WitnessDialogue.UNSURE_VARIANTS, lines.Count(), "every roll lands on one of the noncommittal variants and all of them are reachable");

		string none = LF_WitnessDialogue.AnswerKey(false, 0, 1);
		ctx.EqualStr(LF_WitnessDialogue.UnsureKey(1), none, "a witness who saw nothing answers noncommittally");
		ctx.False(none == "#LF-Witness_NothingSeen", "the answer is not the old 'you didn't see anything'");
		ctx.EqualStr("#LF-Witness_ForgotSome", LF_WitnessDialogue.AnswerKey(true, 0, 1), "a witness who saw it but recalls nothing says so");
		ctx.EqualStr("#LF-Witness_GaveStatement", LF_WitnessDialogue.AnswerKey(true, 2, 1), "a witness who remembers gives a statement");
	}
}
