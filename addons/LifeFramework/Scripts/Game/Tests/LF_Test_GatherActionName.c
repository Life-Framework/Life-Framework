// red-proof: 2026-11-23, `--only LF_Test_GatherActionName` before the change: LF_GatherAction.SentenceCaseItem did not exist, so the test could not compile.
//! gather/action-name: the action row reads as a sentence, "Gather apple", not "Gather Apple"
//! (first-2-hours re-review A24).
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_GatherActionName : LF_Test
{
	override string GetName()
	{
		return "gather/action-name";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.EqualStr("apple", LF_GatherAction.SentenceCaseItem("Apple"), "a one-word item name drops its capital");
		ctx.EqualStr("iron ore", LF_GatherAction.SentenceCaseItem("Iron Ore"), "a Title Case item name is lowered whole");
		ctx.EqualStr("", LF_GatherAction.SentenceCaseItem(""), "an empty name stays empty");
	}
}
