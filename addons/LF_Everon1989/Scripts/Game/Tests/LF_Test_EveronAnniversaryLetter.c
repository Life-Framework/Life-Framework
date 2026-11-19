// red-proof: (#3793) RedProof() skips the spawn of Gaspard and the real talk, so the giver, hand-in and memory assertions fail; run `--red` once. Earlier, observed 2026-11-19 (#3465) before the pack data existed: `tools\cli test --no-build --only LF_Test_EveronAnniversaryLetter` failed every assertion (no letter prefab, no quest row, no dialogue, no memory change); green once the four pack files landed. The act (reading the tables, the hand-in) is skipped under RedProof().
//! #3465 (M8 arc 4, beat 3 of #1039): "A letter from the past". A letter found in the memorial's flowers names Gaspard
//! Lenoir (`old_man`); reading it (the item's inspect text) says whom to bring it to, the quest is to hand it in to him,
//! his dialogue tells the old secret, and the hand-in raises his memory of the player. Under the Everon 1989 header (the
//! pack leg of `tools\cli test`): the item, the quest row and the dialogue load, and the real turn-in hook raises the
//! named resident's memory.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronAnniversaryLetter : LF_Test
{
	static const ResourceName LETTER = "{3465A00000000003}Prefabs/Items/Story/LF_AnniversaryLetter.et";
	static const string RESIDENT = "old_man";
	static const string QUEST_ID = "anniversary_letter";
	static const string DIALOGUE_ID = "old_man";
	static const ResourceName OLD_MAN = "{ED4DD018891346D0}Configs/Npcs/LF_Npc_Townsfolk_OldMan.et";
	static const string UID = "t3465_letter_carrier";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/anniversary-letter";
	}

	//------------------------------------------------------------------------------------------------
	protected static bool HasText(string key)
	{
		string text = WidgetManager.Translate(key);
		return !key.IsEmpty() && !text.IsEmpty() && text != key;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_QuestDefinition FindQuest(LF_QuestGiverRow row, string questId)
	{
		if (!row || !row.m_aQuests)
			return null;

		foreach (LF_QuestDefinition quest : row.m_aQuests)
		{
			if (quest && quest.m_sQuestId == questId)
				return quest;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected void ItemCheck(LF_TestContext ctx)
	{
		Resource prefab;
		if (!RedProof())
			prefab = Resource.Load(LETTER);

		ctx.True(prefab && prefab.IsValid(), "the letter is a real prefab");
	}

	//------------------------------------------------------------------------------------------------
	protected LF_QuestDefinition QuestCheck(LF_TestContext ctx)
	{
		LF_QuestGiverConfig table;
		if (!RedProof())
			table = LF_QuestGiverTable.Get();

		ctx.NotNull(table, "the quest giver table loads under the pack header");
		if (!table)
			return null;

		LF_QuestGiverRow row = table.Find(RESIDENT);
		ctx.NotNull(row, "the named resident has a quest row");
		LF_QuestDefinition quest = FindQuest(row, QUEST_ID);
		ctx.NotNull(quest, "the resident offers the letter quest");
		if (!quest)
			return null;

		ctx.True(HasText(quest.m_sTitleKey), "the quest has its title in the string table");
		ctx.True(HasText(quest.m_sDescriptionKey), "the quest has its description in the string table");
		ctx.True(HasText(quest.m_sTurnInKey), "the quest has its turn-in line in the string table");
		ctx.EqualStr(RESIDENT, quest.m_sMemoryResident, "the hand-in is remembered by the resident the letter names");

		LF_QuestObjective hand = quest.GetObjective(0);
		ctx.True(hand && hand.m_eType == LF_EQuestEventType.COLLECT_REPORT, "bringing the letter is a collect objective");
		if (hand)
			ctx.EqualStr(LETTER, hand.m_sFilter, "the objective names the letter item");

		return quest;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_DialogueOption FindReply(LF_DialogueNode node, string next, string condition)
	{
		if (!node || !node.m_aOptions)
			return null;

		foreach (LF_DialogueOption option : node.m_aOptions)
		{
			if (option.m_sNext == next && option.m_sCondition == condition)
				return option;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Gaspard's own dialogue, patched by the pack (#3793): the letter replies sit on his greetings next to the
	//! talk he already had, the hand-in is offered only to a player who holds the letter, and the secret only
	//! after the hand-in.
	protected void DialogueCheck(LF_TestContext ctx)
	{
		ResourceName path;
		if (!RedProof())
			path = LF_DialogueRegistry.Resolve(DIALOGUE_ID);

		ctx.True(!path.IsEmpty(), "the dialogue id " + DIALOGUE_ID + " resolves");
		if (path.IsEmpty())
			return;

		LF_DialogueConfig config = LF_DialogueConfig.Layered(path);
		ctx.NotNull(config, "Gaspard's dialogue loads with the pack patch");
		if (!config)
			return;

		ctx.EqualStr(DIALOGUE_ID, config.m_sId, "the dialogue is Gaspard's talk");
		array<string> problems = {};
		LF_DialogueFlow.Problems(config, problems);
		LF_DialogueFlow.HookProblems(config, problems);
		ctx.Equal(0, problems.Count(), "Gaspard's talk with the letter is playable");

		array<string> ids = {"anniversary.letter.thanks", "anniversary.letter.secret"};
		foreach (string id : ids)
		{
			ctx.NotNull(LF_DialogueFlow.FindNode(config, id), "the dialogue has the node " + id);
			LF_DialogueLine line = LF_DialogueFlow.FindLine(config, id);
			ctx.NotNull(line, "the dialogue has the line " + id);
			if (line)
				ctx.True(HasText(line.m_sTextKey), id + " has its caption");
		}

		foreach (LF_DialogueNode node : config.m_aNodes)
		{
			if (!node.m_aOptions)
				continue;

			foreach (LF_DialogueOption option : node.m_aOptions)
				ctx.True(HasText(option.m_sTextKey), "reply " + option.m_sTextKey + " has its text");
		}

		array<string> greetings = {"greet", "greet_regular", "greet_friendly"};
		foreach (string greeting : greetings)
		{
			LF_DialogueNode node = LF_DialogueFlow.FindNode(config, greeting);
			ctx.NotNull(node, "Gaspard still has the greeting " + greeting);
			LF_DialogueOption hand = FindReply(node, "anniversary.letter.thanks", "QuestItemsHeld");
			ctx.NotNull(hand, greeting + " offers the hand-in only to a player holding the letter");
			if (hand)
			{
				ctx.EqualStr("QuestConverse", hand.m_sAction, greeting + ": the hand-in is a talk with his quest giver");
				ctx.EqualStr(QUEST_ID, hand.m_sParam, greeting + ": the hand-in names the letter quest");
				ctx.True(LF_DialogueHook.Find(hand.m_sCondition) != null && LF_DialogueHook.Find(hand.m_sAction) != null, greeting + ": the hand-in hooks have classes");
			}

			ctx.NotNull(FindReply(node, "bye", ""), greeting + " keeps the goodbye");
			ctx.NotNull(FindReply(node, "zone", ""), greeting + " keeps his talk of the Zone");
		}

		// The secret sits behind the hand-in on every talk node that has room (four replies a node), and his other talk stays.
		array<string> talk = {"zone", "move", "start"};
		foreach (string id : talk)
		{
			LF_DialogueNode talkNode = LF_DialogueFlow.FindNode(config, id);
			ctx.NotNull(FindReply(talkNode, "anniversary.letter.secret", "QuestTurnedIn"), id + " tells the secret only after the hand-in");
			ctx.NotNull(FindReply(talkNode, "bye", ""), id + " keeps the goodbye");
		}

		ctx.NotNull(FindReply(LF_DialogueFlow.FindNode(config, "zone"), "move", ""), "his talk of moving stays one reply from the greeting");
		ctx.NotNull(FindReply(LF_DialogueFlow.FindNode(config, "anniversary.letter.thanks"), "anniversary.letter.secret", ""), "he tells the secret straight after taking the letter");
	}

	//------------------------------------------------------------------------------------------------
	//! The letter prefab's Read action carries two keys with text, and Gaspard's prefab is the quest giver.
	protected void PrefabCheck(LF_TestContext ctx)
	{
		IEntity gaspard;
		if (!RedProof())
			gaspard = LF_Utils.SpawnEntityPrefab(OLD_MAN, "60 0 60");

		ctx.NotNull(gaspard, "Gaspard's prefab spawns");
		if (!gaspard)
			return;

		LF_QuestGiverComponent giver = LF_Component<LF_QuestGiverComponent>.Find(gaspard);
		ctx.NotNull(giver, "Gaspard is a quest giver");
		if (giver)
		{
			ctx.EqualStr(RESIDENT, giver.GetGiverId(), "his giver id is the resident the quest row names");
			ctx.True(giver.IsConverse(), "he gives and takes in a talk");
		}

		SCR_EntityHelper.DeleteEntityAndChildren(gaspard);

		IEntity letter = LF_Utils.SpawnEntityPrefab(LETTER, "62 0 60");
		ctx.NotNull(letter, "the letter spawns");
		if (!letter)
			return;

		BaseActionsManagerComponent manager = BaseActionsManagerComponent.Cast(letter.FindComponent(BaseActionsManagerComponent));
		LF_ReadNoteAction read;
		if (manager)
		{
			array<BaseUserAction> actions = {};
			manager.GetActionsList(actions);
			foreach (BaseUserAction action : actions)
			{
				if (LF_ReadNoteAction.Cast(action))
					read = LF_ReadNoteAction.Cast(action);
			}
		}

		ctx.NotNull(read, "the letter has a Read action");
		if (read)
			ctx.True(HasText(read.GetTextKey()), "the Read action's text is in the string table");

		ctx.True(HasText("#LF-Item_AnniversaryLetter_ReadTitle") && HasText("#LF-Item_AnniversaryLetter_Read"), "the Read title and prompt are in the string table");
		SCR_EntityHelper.DeleteEntityAndChildren(letter);
	}

	//------------------------------------------------------------------------------------------------
	//! The talk: without the letter the hand-in reply is hidden; holding it, the hook offers the reply, and the real
	//! talk (RequestConverse) hands the letter in, takes it from the bag and raises Gaspard's memory of the player.
	protected void FlowCheck(LF_TestContext ctx)
	{
		LF_QuestManager manager = LF_QuestManager.GetInstance();
		LF_QuestGiverTable.ResetStatics();
		IEntity gaspard = LF_Utils.SpawnEntityPrefab(OLD_MAN, "60 0 60");
		ctx.NotNull(gaspard, "Gaspard spawns for the talk");
		if (!gaspard)
			return;

		LF_QuestGiverComponent giver = manager.GetGiver(RESIDENT);
		if (!giver)
			giver = LF_Component<LF_QuestGiverComponent>.Find(gaspard);

		ctx.NotNull(giver, "Gaspard is registered as the giver");
		if (!giver)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(gaspard);
			return;
		}

		IEntity player = LF_ExploitGuardsWorldFixture.SpawnCharacter(ctx, giver.GetOwner().GetOrigin() + "0 0 1.5", true);
		LF_QuestTrackerComponent tracker;
		if (player)
			tracker = LF_QuestTrackerComponent.Cast(player.FindComponent(LF_QuestTrackerComponent));

		ctx.NotNull(tracker, "the visitor carries a quest tracker");
		if (tracker)
		{
			LF_DialogueHook offered = LF_DialogueHook.Find("QuestItemsHeld");
			ctx.NotNull(offered, "the QuestItemsHeld condition has a class");
			if (offered)
				ctx.False(offered.Allows(player, giver.GetOwner(), QUEST_ID), "the hand-in reply is hidden from a player with no letter");

			ctx.Equal(1, LF_InventoryUtils.AddAmount(player, LETTER, 1), "the visitor holds the letter");
			if (offered)
				ctx.True(offered.Allows(player, giver.GetOwner(), QUEST_ID), "the hand-in reply is offered to the player who holds it");

			string uid = "fixture-" + player.GetID().ToString();
			int before = LF_ResidentMemory.Score(RESIDENT, uid);
			if (!RedProof())
				tracker.RequestConverse(RESIDENT);

			ctx.True(tracker.GetTracker().GetStatus(QUEST_ID) == LF_EQuestStatus.TURNED_IN, "the talk hands the letter in");
			ctx.Equal(0, LF_InventoryUtils.GetAmount(player, LETTER), "Gaspard keeps the letter");
			ctx.True(LF_ResidentMemory.Score(RESIDENT, uid) > before, "Gaspard remembers the player who brought it");
			if (offered)
				ctx.False(offered.Allows(player, giver.GetOwner(), QUEST_ID), "the hand-in reply is gone once the letter is in");
		}

		if (player)
			SCR_EntityHelper.DeleteEntityAndChildren(player);

		SCR_EntityHelper.DeleteEntityAndChildren(gaspard);
	}

	//------------------------------------------------------------------------------------------------
	//! The hand-in: the real turn-in hook raises the named resident's memory of the carrier.
	protected void HandInCheck(LF_TestContext ctx, LF_QuestDefinition quest)
	{
		ctx.NotNull(quest, "the letter quest is there to hand in");
		if (!quest)
			return;

		LF_QuestManager.GetInstance().RegisterDefinition(quest);
		int before = LF_ResidentMemory.Score(RESIDENT, UID);
		bool remembered;
		if (!RedProof())
			remembered = LF_ResidentNeeds.OnTurnedIn(QUEST_ID, UID);

		ctx.True(remembered, "the hand-in is remembered by the resident");
		ctx.True(LF_ResidentMemory.Score(RESIDENT, UID) > before, "the resident's memory of the carrier rose");
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MissionHeader header = LF_MissionHeader.GetCurrent();
		ctx.NotNull(header, "the pack leg runs under an LF mission header");

		ItemCheck(ctx);
		LF_QuestDefinition quest = QuestCheck(ctx);
		DialogueCheck(ctx);
		HandInCheck(ctx, quest);
		PrefabCheck(ctx);
		FlowCheck(ctx);
	}
}
