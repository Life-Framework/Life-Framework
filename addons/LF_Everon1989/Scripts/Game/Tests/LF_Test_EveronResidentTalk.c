// red-proof: 2026-11-19 (fix-missing-dialogue): `--red LF_Test_EveronResidentTalk` (RedProof() skips the spawns) fails every new `<who> spawns` (observed: RED PROOF OK, 2026-11-19); on main the four residents talked through the core `resident` sheet and the scene walkers had no talk at all (validate-dialogue-refs counted 24 such failures there). Older: RedProof() skips the spawn and the greeting lookups, so `<who> spawns` and `<id> opens differently for <word>` fail; the batched `--red` run is in the PR. Before the prefab and dialogue change (#2998) the four generic residents had no dialogue component or talk action and the three townsfolk dialogues had no greet_<word> node, so every word fell back to `greet`.
//! #2998 (residents you can talk to, epic #1012): under the Everon 1989 header (the pack leg) the four generic
//! residents (teller, store clerk, two neighbours) and every scene walker (square and anniversary crowd, relocated
//! arrivals, dockworkers, the raid gendarme) carry a dialogue of their own and a talk action, and the
//! townsfolk and resident dialogues open on a `greet_<word>` node, so the
//! opener follows the player's standing (`LF_ResidentMemory.Tier`); neutral keeps the dialogue's own `greet`.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronResidentTalk : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/resident-talk";
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
	protected void CheckTalk(LF_TestContext ctx, ResourceName prefab, string who, string dialogueId)
	{
		IEntity npc;
		if (!RedProof())
			npc = LF_Utils.SpawnEntityPrefab(prefab, "0 -50 0");

		ctx.True(npc != null, who + " spawns");
		if (!npc)
			return;

		ctx.NotNull(npc.FindComponent(LF_DialogueComponent), who + " has a dialogue component");
		ctx.True(HasTalkAction(npc), who + " has a talk action");
		LF_DialogueConfig config = LF_DialogueComponent.ConfigOf(npc);
		ctx.NotNull(config, who + " has a playable dialogue");
		if (config)
		{
			ctx.EqualStr(dialogueId, config.m_sId, who + " talks with its own dialogue");
			LF_DialogueNode greet = LF_DialogueFlow.FindNode(config, config.m_sStart);
			ctx.True(greet != null && greet.m_aOptions && greet.m_aOptions.Count() >= 2, who + " offers more than Goodbye");
		}

		SCR_EntityHelper.DeleteEntityAndChildren(npc);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckGreetings(LF_TestContext ctx, string id)
	{
		ResourceName path = LF_DialogueRegistry.Resolve(id);
		ctx.True(!path.IsEmpty(), id + " dialogue resolves through the pack index");
		LF_DialogueConfig config;
		if (!RedProof())
			config = LF_DialogueConfig.Layered(path);

		ctx.NotNull(config, id + " dialogue loads");
		if (!config)
			return;

		array<string> problems = {};
		LF_DialogueFlow.Problems(config, problems);
		ctx.True(problems.IsEmpty(), id + " dialogue has no problems");

		// Neutral keeps the dialogue's own opener; every other word opens on a line of its own.
		string neutral = LF_ResidentTalk.LineFor(config, id, "neutral");
		ctx.EqualStr("greet", neutral, id + " neutral opens on its own greeting");
		array<string> words = {"refuses", "wary", "friendly", "regular"};
		array<string> seen = {neutral};
		foreach (string word : words)
		{
			string line = LF_ResidentTalk.LineFor(config, id, word);
			ctx.True(!line.IsEmpty() && seen.Find(line) == -1, string.Format("%1 opens differently for %2 (%3)", id, word, line));
			seen.Insert(line);
			LF_DialogueLine caption = LF_DialogueFlow.FindLine(config, line);
			ctx.NotNull(caption, string.Format("%1 %2 line has its caption", id, word));
			if (caption)
			{
				string text = WidgetManager.Translate(caption.m_sTextKey);
				ctx.True(!text.IsEmpty() && text != caption.m_sTextKey, string.Format("%1 %2 caption key resolves (%3)", id, word, caption.m_sTextKey));
			}
		}

		LF_DialogueNode refuses = LF_DialogueFlow.FindNode(config, LF_ResidentTalk.StartNodeFor(config, id, "refuses"));
		ctx.True(refuses != null && (!refuses.m_aOptions || refuses.m_aOptions.IsEmpty()), id + " refuses offers no replies");
		LF_DialogueNode friendly = LF_DialogueFlow.FindNode(config, LF_ResidentTalk.StartNodeFor(config, id, "friendly"));
		ctx.True(friendly != null && friendly.m_aOptions && friendly.m_aOptions.Count() >= 2, id + " friendly can still ask about things");
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MissionHeader header = LF_MissionHeader.GetCurrent();
		ctx.NotNull(header, "the pack leg runs under an LF mission header");

		CheckTalk(ctx, "{7C27C5AD4C1C5D2D}Configs/Residents/Npcs/LF_Resident_Teller.et", "the teller", "resident_teller");
		CheckTalk(ctx, "{559ECE12ABB3673F}Configs/Residents/Npcs/LF_Resident_StoreClerk.et", "the store clerk", "resident_store_clerk");
		CheckTalk(ctx, "{312B29D7CD3C51BF}Configs/Residents/Npcs/LF_Resident_NeighbourCardigan.et", "the cardigan neighbour", "resident_anselme");
		CheckTalk(ctx, "{3AF4A6EF424C916C}Configs/Residents/Npcs/LF_Resident_NeighbourCoat.et", "the coat neighbour", "resident_mireille");
		CheckTalk(ctx, "{DE8256F994DD6BA3}Prefabs/Characters/Npcs/LF_Passerby_DenimJacket_1.et", "a square passerby", "passerby");
		CheckTalk(ctx, "{0D5FF1C869239694}Prefabs/Characters/Npcs/LF_Walker_Relocated_Coat.et", "a relocated arrival", "relocated_walker");
		CheckTalk(ctx, "{1C869AB4232D876C}Prefabs/Characters/Npcs/LF_Dockworker_1.et", "a dockworker", "dockworker");
		CheckTalk(ctx, "{B29C75414B43DCCA}Prefabs/Characters/Npcs/LF_Gendarme_1.et", "the raid gendarme", "gendarme");

		CheckGreetings(ctx, "old_man");
		CheckGreetings(ctx, "cafe_owner");
		CheckGreetings(ctx, "night_shift");
		CheckGreetings(ctx, "resident_teller");
		CheckGreetings(ctx, "resident_store_clerk");
		CheckGreetings(ctx, "resident_anselme");
		CheckGreetings(ctx, "resident_mireille");
	}
}
