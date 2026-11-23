// red-proof: observed 2026-11-23 with the test command, `test --no-build --no-cache --red LF_Test_FenceVoice,LF_Test_EveronFenceVoice`: RED PROOF OK (RedProof() skips loading the fence conversation, so the role has no playable dialogue and neither line is found); green with the hook off.
//! Vince's two record-gate lines are in the conversation the fence speaks from (#4620): under the Everon 1989
//! header his role dialogue, with the pack's patch over it, holds `refuse_drill` and `sell_drill`, each with a
//! recorded clip and its caption row. The shop's choice of line is `LF_Test_FenceVoice`.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronFenceVoice : LF_Test
{
	override string GetName()
	{
		return "everon/fence-voice";
	}

	protected void CheckLine(LF_TestContext ctx, LF_DialogueConfig config, string id)
	{
		LF_DialogueLine line = LF_DialogueFlow.FindLine(config, id);
		ctx.NotNull(line, id + ": is in the fence's conversation");
		if (!line)
			return;

		ctx.False(line.IsTextOnly(), id + ": has a recorded clip");
		ctx.True(!line.m_sTextKey.IsEmpty(), id + ": has a caption row");
		ctx.True(line.m_fSeconds > 0, id + ": has a length");
	}

	override void Run(LF_TestContext ctx)
	{
		LF_DialogueConfig config;
		if (!RedProof())
			config = LF_Roles.DialogueFor("fence");

		ctx.NotNull(config, "the fence role has a playable conversation");
		CheckLine(ctx, config, LF_FenceVoice.LINE_REFUSE);
		CheckLine(ctx, config, LF_FenceVoice.LINE_SELL);
	}
}
