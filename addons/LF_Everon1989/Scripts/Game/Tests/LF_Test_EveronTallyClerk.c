// red-proof: batched run `tools\cli test --no-build --red LF_Test_EveronTallyClerk`: RedProof() skips loading the conf, so `the dialogue loads` and every check behind it fail (RED-OK everon/tally-clerk). Without LF_Dialogue_TallyClerk.conf (nothing generated yet) the load returns null and the same checks fail.
//! #4199 (crime B4 of 0.3.0; the tally clerk of the container yard): under the Everon 1989 header (the pack leg of
//! `tools\cli test`) Marcel's dialogue loads, is a playable, fully voiced conversation, and a player can walk it from the
//! greeting through the fee line ("Four hundred dollars...") to the key line ("Here. Brass, on a red string..."): the
//! greeting offers the talk about container nine, the fee node offers paying, and paying leads to the key node. The
//! dialogue is loaded by its conf, not through the index: no NPC body or index row exists yet (a later card). The fee is
//! only spoken here; taking the money and handing the key item is the transaction hook, a follow-up card.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronTallyClerk : LF_Test
{
	static const ResourceName DIALOGUE = "{358A253AB64607EF}Configs/Dialogue/LF_Dialogue_TallyClerk.conf";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/tally-clerk";
	}

	//------------------------------------------------------------------------------------------------
	protected static bool HasText(string key)
	{
		string text = WidgetManager.Translate(key);
		return !key.IsEmpty() && !text.IsEmpty() && text != key;
	}

	//------------------------------------------------------------------------------------------------
	//! The reply of `node` that leads to `next`, or null when none does.
	protected static LF_DialogueOption ReplyTo(LF_DialogueNode node, string next)
	{
		if (!node || !node.m_aOptions)
			return null;

		foreach (LF_DialogueOption option : node.m_aOptions)
		{
			if (option && option.m_sNext == next)
				return option;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected void VoiceCheck(LF_TestContext ctx, LF_DialogueConfig config)
	{
		ctx.True(!config.m_sSoundProject.IsEmpty(), "the dialogue names an audio project");
		ctx.True(config.m_aLines && config.m_aLines.Count() > 0, "the dialogue has lines");
		if (!config.m_aLines)
			return;

		foreach (LF_DialogueLine line : config.m_aLines)
		{
			if (!line)
				continue;

			string where = "line " + line.m_sId;
			ctx.True(!line.m_sEvent.IsEmpty(), where + ": has a sound event");
			ctx.True(line.m_fSeconds > 0, where + ": has a length");
			ctx.True(!line.m_sMouth.IsEmpty(), where + ": has a mouth envelope");
			ctx.True(HasText(line.m_sTextKey), where + ": has its caption");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! greet -> (container nine) -> offer, the fee line -> (pay) -> key, the key line.
	protected void WalkCheck(LF_TestContext ctx, LF_DialogueConfig config)
	{
		LF_DialogueNode greet = LF_DialogueFlow.FindNode(config, config.m_sStart);
		ctx.NotNull(greet, "the greeting node exists");
		LF_DialogueOption ask = ReplyTo(greet, "offer");
		ctx.NotNull(ask, "the greeting offers the talk about container nine");

		LF_DialogueNode offer = LF_DialogueFlow.FindNode(config, "offer");
		ctx.NotNull(offer, "the fee node exists");
		LF_DialogueLine fee = LF_DialogueFlow.FindLine(config, "offer");
		ctx.NotNull(fee, "the fee line exists");
		if (fee)
		{
			ctx.True(HasText(fee.m_sTextKey), "the fee line has its caption");
			ctx.True(WidgetManager.Translate(fee.m_sTextKey).Contains("dollars"), "the fee line names the price in dollars");
		}

		LF_DialogueOption pay = ReplyTo(offer, "key");
		ctx.NotNull(pay, "the fee node offers paying");
		ctx.NotNull(ReplyTo(offer, "later"), "the fee node lets the player walk away");

		LF_DialogueNode key = LF_DialogueFlow.FindNode(config, "key");
		ctx.NotNull(key, "the key node exists");
		LF_DialogueLine keyLine = LF_DialogueFlow.FindLine(config, "key");
		ctx.NotNull(keyLine, "the key line exists");
		if (keyLine)
			ctx.True(WidgetManager.Translate(keyLine.m_sTextKey).Contains("Brass"), "the key line hands the brass key over");

		if (ask)
			ctx.True(HasText(ask.m_sTextKey), "the container nine reply has its text");
		if (pay)
			ctx.True(HasText(pay.m_sTextKey), "the pay reply has its text");
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// The act: load the conversation from its conf.
		LF_DialogueConfig config;
		if (!RedProof())
			config = LF_DialogueConfig.Load(DIALOGUE);

		ctx.NotNull(config, "the dialogue loads");
		if (!config)
			return;

		ctx.EqualStr("tally_clerk", config.m_sId, "the dialogue is the tally clerk's");
		ctx.EqualStr("Marcel Brun", WidgetManager.Translate(config.m_sSpeakerKey), "the speaker is Marcel Brun");

		array<string> problems = {};
		LF_DialogueFlow.Problems(config, problems);
		ctx.Equal(0, problems.Count(), "the dialogue is playable");

		VoiceCheck(ctx, config);
		WalkCheck(ctx, config);
	}
}
