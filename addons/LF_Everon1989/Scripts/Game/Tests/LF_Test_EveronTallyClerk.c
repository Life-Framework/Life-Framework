// red-proof (#4381): batched run `tools\cli test --no-build --red LF_Test_EveronTallyClerk` after the fee hook: RedProof() skips loading the conf, so every check behind `the dialogue loads` fails (observed 2026-11-23: RED-OK everon/tally-clerk, "the dialogue loads").
// red-proof (#4199): batched run `tools\cli test --no-build --red LF_Test_EveronTallyClerk`: RedProof() skips loading the conf, so `the dialogue loads` and every check behind it fail (RED-OK everon/tally-clerk). Without LF_Dialogue_TallyClerk.conf (nothing generated yet) the load returns null and the same checks fail.
//! #4199, #4381 (crime B4 of 0.3.0; the tally clerk of the container yard): under the Everon 1989 header (the pack
//! leg of `tools\cli test`) Marcel's dialogue loads, is a playable, fully voiced conversation, and a player can walk it
//! from the greeting to the fee line ("Four hundred dollars..."). Paying runs the core FeeForItem hook for $400 and the
//! container nine key, and the voiced key line ("Here. Brass, on a red string...") is Marcel's answer to a paid fee.
//! The index resolves "tally_clerk" to this conf and Marcel's NPC prefab names it.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronTallyClerk : LF_Test
{
	static const ResourceName DIALOGUE = "{358A253AB64607EF}Configs/Dialogue/LF_Dialogue_TallyClerk.conf";
	static const ResourceName KEY_PREFAB = "{6E6C667869AC0F87}Prefabs/Items/Story/LF_ContainerNineKey.et";
	static const ResourceName NPC_PREFAB = "{06C758FEC5FEF9F2}Configs/Npcs/LF_Npc_Townsfolk_TallyClerk.et";
	static const int FEE = 400;

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
	//! The reply of `node` that runs an action, or null.
	protected static LF_DialogueOption PayReply(LF_DialogueNode node)
	{
		if (!node || !node.m_aOptions)
			return null;

		foreach (LF_DialogueOption option : node.m_aOptions)
		{
			if (option && !option.m_sAction.IsEmpty())
				return option;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! The key item, Marcel's index row and his NPC prefab.
	protected void WiringCheck(LF_TestContext ctx)
	{
		Resource key = Resource.Load(KEY_PREFAB);
		ctx.True(key && key.IsValid(), "the container nine key prefab loads");
		ctx.True(HasText("#LF-Item_ContainerNineKey_Name"), "the key has its name");
		ctx.EqualStr(DIALOGUE, LF_DialogueRegistry.Resolve("tally_clerk"), "the index resolves the tally clerk's dialogue");

		Resource npc = Resource.Load(NPC_PREFAB);
		ctx.True(npc && npc.IsValid(), "Marcel's NPC prefab loads");
		if (!npc || !npc.IsValid())
			return;

		IEntityComponentSource source = SCR_BaseContainerTools.FindComponentSource(npc, LF_DialogueComponent);
		ctx.True(source != null, "Marcel's prefab carries a dialogue");
		string id;
		if (source)
			source.Get("m_sDialogueId", id);
		ctx.EqualStr("tally_clerk", id, "Marcel's prefab names the tally clerk's dialogue");
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

		LF_DialogueOption pay = PayReply(offer);
		ctx.NotNull(pay, "the fee node offers paying");
		ctx.NotNull(ReplyTo(offer, "later"), "the fee node lets the player walk away");
		if (pay)
		{
			ctx.EqualStr("FeeForItem", pay.m_sAction, "paying runs the fee hook");
			int price;
			ResourceName prefab;
			ctx.True(LF_DialogueFeeTrade.ParseParam(pay.m_sParam, price, prefab), "the fee hook's param parses");
			ctx.Equal(FEE, price, "the fee is four hundred dollars");
			ctx.EqualStr(KEY_PREFAB, prefab, "the fee buys the container nine key");
		}

		LF_DialogueLine keyLine = LF_DialogueFlow.FindLine(config, "key");
		ctx.NotNull(keyLine, "the key line exists");
		if (keyLine)
			ctx.True(WidgetManager.Translate(keyLine.m_sTextKey).Contains("Brass"), "the key line hands the brass key over");
		ctx.EqualStr("key", LF_DialogueFlow.SpokenFor(config, LF_DialogueFeeTrade.ANSWER_PAID), "a paid fee is answered with the voiced key line");

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
		WiringCheck(ctx);
	}
}
