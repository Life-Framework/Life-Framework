// red-proof (#4491): batched run `tools\cli test --no-build --red LF_Test_EveronCoalYardClerk`: RedProof() skips loading the conf, so `the dialogue loads` and every check behind it fail.
//! #4491 (the power plant coal yard): under the Everon 1989 header (the pack leg of `tools\cli test`) the yard clerk's
//! dialogue loads, is a playable, fully voiced conversation (three recorded lines: greeting, the purse answer and the
//! goodbye), and a player can walk it from the greeting to the purse answer and out. The index resolves "coal_yard_clerk"
//! to this conf, and the clerk's NPC prefab names it.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronCoalYardClerk : LF_Test
{
	static const ResourceName DIALOGUE = "{6309329C2A750687}Configs/Dialogue/LF_Dialogue_CoalYardClerk.conf";
	static const ResourceName NPC_PREFAB = "{DFBF063F590C7160}Configs/Npcs/LF_Npc_Townsfolk_CoalYardClerk.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/coal-yard-clerk";
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
	//! The index row and the clerk's NPC prefab.
	protected void WiringCheck(LF_TestContext ctx)
	{
		ctx.EqualStr(DIALOGUE, LF_DialogueRegistry.Resolve("coal_yard_clerk"), "the index resolves the coal yard clerk's dialogue");

		Resource npc = Resource.Load(NPC_PREFAB);
		ctx.True(npc && npc.IsValid(), "the clerk's NPC prefab loads");
		if (!npc || !npc.IsValid())
			return;

		IEntityComponentSource source = SCR_BaseContainerTools.FindComponentSource(npc, LF_DialogueComponent);
		ctx.True(source != null, "the clerk's prefab carries a dialogue");
		string id;
		if (source)
			source.Get("m_sDialogueId", id);
		ctx.EqualStr("coal_yard_clerk", id, "the clerk's prefab names the coal yard clerk's dialogue");
	}

	//------------------------------------------------------------------------------------------------
	protected void VoiceCheck(LF_TestContext ctx, LF_DialogueConfig config)
	{
		ctx.True(!config.m_sSoundProject.IsEmpty(), "the dialogue names an audio project");
		ctx.True(config.m_aLines && config.m_aLines.Count() > 0, "the dialogue has lines");
		if (!config.m_aLines)
			return;

		ctx.Equal(3, config.m_aLines.Count(), "the clerk has exactly three recorded lines");
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
	//! greet -> (how much) -> how, the purse line -> (goodbye) -> bye.
	protected void WalkCheck(LF_TestContext ctx, LF_DialogueConfig config)
	{
		LF_DialogueNode greet = LF_DialogueFlow.FindNode(config, config.m_sStart);
		ctx.NotNull(greet, "the greeting node exists");
		LF_DialogueOption ask = ReplyTo(greet, "how");
		ctx.NotNull(ask, "the greeting offers the question about how much he takes");
		ctx.NotNull(ReplyTo(greet, "bye"), "the greeting lets the player leave");

		LF_DialogueNode how = LF_DialogueFlow.FindNode(config, "how");
		ctx.NotNull(how, "the purse node exists");
		ctx.NotNull(ReplyTo(how, "bye"), "the purse node lets the player leave");

		LF_DialogueLine purse = LF_DialogueFlow.FindLine(config, "how");
		ctx.NotNull(purse, "the purse line exists");
		if (purse)
			ctx.True(WidgetManager.Translate(purse.m_sTextKey).Contains("purse"), "the purse line says why he stops buying");

		if (ask)
			ctx.True(HasText(ask.m_sTextKey), "the question has its reply text");
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

		ctx.EqualStr("coal_yard_clerk", config.m_sId, "the dialogue is the coal yard clerk's");
		ctx.EqualStr("Gaston Roux", WidgetManager.Translate(config.m_sSpeakerKey), "the speaker is Gaston Roux");

		array<string> problems = {};
		LF_DialogueFlow.Problems(config, problems);
		ctx.Equal(0, problems.Count(), "the dialogue is playable");

		VoiceCheck(ctx, config);
		WalkCheck(ctx, config);
		WiringCheck(ctx);
	}
}
