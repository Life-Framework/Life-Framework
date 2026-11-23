// red-proof (#4457): batched run `tools\cli test --no-build --red LF_Test_EveronRegistrar`: RedProof() skips loading the conf, so `the dialogue loads` and every check behind it fail. Without LF_Dialogue_Registrar.conf (nothing generated yet) the load returns null and the same checks fail.
//! #4457 (gangs C14 of 0.3.0; the registry's registrar): under the Everon 1989 header (the pack leg of
//! `tools\cli test`) the registrar's dialogue loads, is a playable and fully voiced conversation, and a player
//! can walk it from the greeting to the founding line ("Name, flag, and two hundred fifty dollars."). The flag
//! wall's two voiced results are his answers to the toast keys: a pick says "picked", a flag held by another crew
//! says "taken". The index resolves "registrar" to this conf and his NPC prefab names it.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronRegistrar : LF_Test
{
	static const ResourceName DIALOGUE = "{85F0D89633B0BE02}Configs/Dialogue/LF_Dialogue_Registrar.conf";
	static const ResourceName NPC_PREFAB = "{A93B5D07E21C4F86}Configs/Npcs/LF_Npc_Townsfolk_Registrar.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/registrar";
	}

	//------------------------------------------------------------------------------------------------
	protected static bool HasText(string key)
	{
		string text = WidgetManager.Translate(key);
		return !key.IsEmpty() && !text.IsEmpty() && text != key;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool HasReplyTo(LF_DialogueNode node, string next)
	{
		if (!node || !node.m_aOptions)
			return false;

		foreach (LF_DialogueOption option : node.m_aOptions)
		{
			if (option && option.m_sNext == next)
				return true;
		}

		return false;
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
	//! greet -> (start a crew) -> found, the founding line.
	protected void TalkCheck(LF_TestContext ctx, LF_DialogueConfig config)
	{
		LF_DialogueNode greet = LF_DialogueFlow.FindNode(config, config.m_sStart);
		ctx.NotNull(greet, "the greeting node exists");
		ctx.True(HasReplyTo(greet, "found"), "the greeting offers starting a crew");

		LF_DialogueLine founding = LF_DialogueFlow.FindLine(config, "found");
		ctx.NotNull(founding, "the founding line exists");
		if (founding)
		{
			ctx.True(HasText(founding.m_sTextKey), "the founding line has its caption");
			ctx.EqualStr("Name, flag, and two hundred fifty dollars.", WidgetManager.Translate(founding.m_sTextKey), "the founding line is name, flag and the fee");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The wall's pick and refusal are spoken: the toast key of each result has a voiced line.
	protected void WallCheck(LF_TestContext ctx, LF_DialogueConfig config)
	{
		ctx.EqualStr("picked", LF_DialogueFlow.SpokenFor(config, LF_OrgFlagWall.MessageKey(LF_EOrgFlagWallResult.PICKED)), "a pick at the wall is answered with the picked line");
		ctx.EqualStr("taken", LF_DialogueFlow.SpokenFor(config, LF_OrgFlagWall.MessageKey(LF_EOrgFlagWallResult.HELD)), "a flag held by another crew is answered with the taken line");
		ctx.True(LF_OrgFlagWall.VoicesResult(LF_EOrgFlagWallResult.PICKED), "the wall voices a pick");
		ctx.True(LF_OrgFlagWall.VoicesResult(LF_EOrgFlagWallResult.HELD), "the wall voices a held flag");
		ctx.True(!LF_OrgFlagWall.VoicesResult(LF_EOrgFlagWallResult.NO_ORG), "a visitor with no crew gets the toast only");
	}

	//------------------------------------------------------------------------------------------------
	protected void WiringCheck(LF_TestContext ctx)
	{
		ctx.EqualStr(DIALOGUE, LF_DialogueRegistry.Resolve(LF_OrgFlagWall.REGISTRAR_DIALOGUE), "the index resolves the registrar's dialogue");

		Resource npc = Resource.Load(NPC_PREFAB);
		ctx.True(npc && npc.IsValid(), "the registrar's NPC prefab loads");
		if (!npc || !npc.IsValid())
			return;

		IEntityComponentSource source = SCR_BaseContainerTools.FindComponentSource(npc, LF_DialogueComponent);
		ctx.True(source != null, "the registrar's prefab carries a dialogue");
		string id;
		if (source)
			source.Get("m_sDialogueId", id);
		ctx.EqualStr(LF_OrgFlagWall.REGISTRAR_DIALOGUE, id, "the registrar's prefab names the registrar's dialogue");
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

		ctx.EqualStr("registrar", config.m_sId, "the dialogue is the registrar's");
		ctx.EqualStr("Gaspard Perrin", WidgetManager.Translate(config.m_sSpeakerKey), "the speaker is Gaspard Perrin");

		array<string> problems = {};
		LF_DialogueFlow.Problems(config, problems);
		ctx.Equal(0, problems.Count(), "the dialogue is playable");

		VoiceCheck(ctx, config);
		TalkCheck(ctx, config);
		WallCheck(ctx, config);
		WiringCheck(ctx);
	}
}
