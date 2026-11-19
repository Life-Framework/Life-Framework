// red-proof: observed 2026-11-19 (#3794) with `tools\cli test --no-build --red LF_Test_EveronAnniversaryMayor`: RED-OK everon/anniversary-mayor (3 assertions failed, first `the mayor spawns`); RedProof() skips the spawn, so the NPC, quest giver, dialogue and outfit checks read as missing. Green on the same pack data.
//! #3794 (M8 arc 4, beat 1 of #1039): under the Everon 1989 header (the pack leg of `tools\cli test`) the mayor Bastien
//! Girard is a dressed townsfolk NPC with a talk action, the quest giver `mayor` that offers the anniversary errands
//! (chairs, flowers) from the pack's giver table, and a dialogue (`anniversary_mayor`) that resolves through the pack's
//! index overlay, is playable, has a caption on every line and reply, and hands the errands out through the
//! `QuestConverse` hook. Found by prefab, never by a town coordinate.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronAnniversaryMayor : LF_Test
{
	static const ResourceName MAYOR = "{5D554143B2EB77B6}Configs/Npcs/LF_Npc_Townsfolk_Mayor.et";
	static const string GIVER_ID = "mayor";
	static const string DIALOGUE_ID = "anniversary_mayor";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/anniversary-mayor";
	}

	//------------------------------------------------------------------------------------------------
	protected static bool HasTalkAction(IEntity npc)
	{
		BaseActionsManagerComponent manager = BaseActionsManagerComponent.Cast(npc.FindComponent(BaseActionsManagerComponent));
		if (!manager)
			return false;

		array<BaseUserAction> actions = {};
		manager.GetActionsList(actions);
		foreach (BaseUserAction action : actions)
		{
			if (LF_DialogueTalkAction.Cast(action))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool HasText(string key)
	{
		string text = WidgetManager.Translate(key);
		return !key.IsEmpty() && !text.IsEmpty() && text != key;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool OffersQuest(array<ref LF_QuestDefinition> quests, string questId)
	{
		if (!quests)
			return false;

		foreach (LF_QuestDefinition quest : quests)
		{
			if (quest && quest.m_sQuestId == questId)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected void DialogueCheck(LF_TestContext ctx, IEntity npc)
	{
		string path = LF_DialogueRegistry.Resolve(DIALOGUE_ID);
		ctx.True(!path.IsEmpty(), "the dialogue id " + DIALOGUE_ID + " resolves through the pack index");

		LF_DialogueConfig config = LF_DialogueComponent.ConfigOf(npc);
		ctx.NotNull(config, "the mayor has a playable dialogue");
		if (!config)
			return;

		ctx.EqualStr(DIALOGUE_ID, config.m_sId, "the dialogue is the mayor's");
		ctx.EqualStr("Bastien Girard", WidgetManager.Translate(config.m_sSpeakerKey), "the speaker is Bastien Girard");

		array<string> problems = {};
		LF_DialogueFlow.Problems(config, problems);
		LF_DialogueFlow.HookProblems(config, problems);
		ctx.Equal(0, problems.Count(), "the mayor's dialogue is playable");

		foreach (LF_DialogueLine line : config.m_aLines)
			ctx.True(HasText(line.m_sTextKey), line.m_sId + " has its caption");

		bool offers;
		foreach (LF_DialogueNode node : config.m_aNodes)
		{
			if (!node.m_aOptions)
				continue;

			foreach (LF_DialogueOption option : node.m_aOptions)
			{
				ctx.True(HasText(option.m_sTextKey), "reply " + option.m_sTextKey + " has its text");
				if (option.m_sAction == "QuestConverse")
					offers = true;
			}
		}

		ctx.True(offers, "a reply hands the errands out through the QuestConverse hook");
		ctx.True(LF_DialogueHook.Find("QuestConverse") != null, "the QuestConverse hook has its class");
		ctx.NotNull(LF_DialogueFlow.FindNode(config, "day"), "the mayor tells what the day is for");
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MissionHeader header = LF_MissionHeader.GetCurrent();
		ctx.NotNull(header, "the pack leg runs under an LF mission header");

		// The act: spawn the mayor as the pack ships him. Red proof: no spawn.
		IEntity npc;
		if (!RedProof())
			npc = LF_Utils.SpawnEntityPrefab(MAYOR, "0 -50 0");

		ctx.True(npc != null, "the mayor spawns");
		if (!npc)
			return;

		ctx.NotNull(LF_DialogueComponent.Cast(npc.FindComponent(LF_DialogueComponent)), "the mayor has a dialogue component");
		ctx.True(HasTalkAction(npc), "the mayor has a talk action");

		// A dressed 1989 official: a suit jacket, trousers and boots, not the default naked body.
		int worn;
		IEntity child = npc.GetChildren();
		while (child)
		{
			if (child.FindComponent(BaseLoadoutClothComponent))
				worn++;

			child = child.GetSibling();
		}

		ctx.True(worn >= 3, string.Format("the mayor wears a jacket, trousers and boots (%1 worn)", worn));

		// The quest giver `mayor` offers both anniversary errands from the pack's giver table.
		LF_QuestGiverComponent giver = LF_Component<LF_QuestGiverComponent>.Find(npc);
		ctx.NotNull(giver, "the mayor is a quest giver");
		if (giver)
		{
			ctx.EqualStr(GIVER_ID, giver.GetGiverId(), "the giver id is mayor");
			ctx.True(giver.IsConverse(), "the mayor talks the errands over in person");
			array<ref LF_QuestDefinition> quests = giver.GetQuests();
			ctx.True(OffersQuest(quests, "anniversary_chairs"), "the mayor offers the chairs errand");
			ctx.True(OffersQuest(quests, "anniversary_flowers"), "the mayor offers the flowers errand");
		}

		DialogueCheck(ctx, npc);
		SCR_EntityHelper.DeleteEntityAndChildren(npc);
	}
}
