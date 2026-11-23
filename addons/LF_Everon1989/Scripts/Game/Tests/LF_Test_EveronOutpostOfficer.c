// red-proof: RED-OK 2026-11-23 `test --no-build --red LF_Test_OutpostOfficerVoice,LF_Test_EveronOutpostOfficer`: RedProof() skips loading the conversations, so each `loads` assertion fails (RED-OK everon/outpost-officer, 6 assertions); green without the hook.
//! #4640 (follow-up of #4459), pack leg: in the quartermaster's, the buyer's and the gate guard's real conversations
//! (the pack confs, loaded the way an NPC loads them) every reply that leads to the officer answer carries the
//! `OutpostOfficer` condition, no other reply does, and the condition names a hook that exists. The quartermaster and
//! the buyer hold the voiced `for_key` refusal line the trader's refusal path plays (LF_OutpostVoice).
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronOutpostOfficer : LF_Test
{
	protected static const string CONDITION = "OutpostOfficer";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/outpost-officer";
	}

	//------------------------------------------------------------------------------------------------
	//! \param officerNode The node the officer reply leads to.
	//! \param refusalLine True when the speaker holds the voiced refusal line.
	protected void Check(LF_TestContext ctx, string id, string officerNode, bool refusalLine)
	{
		LF_DialogueConfig config;
		string path = LF_DialogueRegistry.Resolve(id);
		if (!path.IsEmpty() && !RedProof())
			config = LF_DialogueConfig.Layered(path);

		ctx.NotNull(config, id + ": loads");
		if (!config || !config.m_aNodes)
			return;

		int officerReplies;
		foreach (LF_DialogueNode node : config.m_aNodes)
		{
			if (!node || !node.m_aOptions)
				continue;

			foreach (LF_DialogueOption option : node.m_aOptions)
			{
				if (!option)
					continue;

				if (option.m_sNext == officerNode)
				{
					officerReplies++;
					ctx.EqualStr(CONDITION, option.m_sCondition, id + "/" + node.m_sId + ": the officer reply is gated on duty");
				}
				else
				{
					ctx.EqualStr("", option.m_sCondition, id + "/" + node.m_sId + ": a plain reply is offered to everyone");
				}
			}
		}

		ctx.True(officerReplies > 0, id + ": has an officer reply");
		ctx.NotNull(LF_DialogueHook.Find(CONDITION), id + ": the condition names a hook that exists");
		bool voiced;
		foreach (LF_DialogueLine line : config.m_aLines)
		{
			if (line && line.m_sForKey == LF_Outpost.REFUSE_KEY && !line.IsTextOnly())
				voiced = true;
		}

		ctx.Equal(refusalLine, voiced, id + ": the voiced refusal line is where it belongs");
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		Check(ctx, "outpost_quartermaster", "refuse", true);
		Check(ctx, "outpost_buyer", "refuse", true);
		Check(ctx, "outpost_guard", "uniform", false);
	}
}
