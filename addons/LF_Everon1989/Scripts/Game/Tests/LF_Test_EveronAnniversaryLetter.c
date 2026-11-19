// red-proof: observed 2026-11-19 (#3465) before the pack data existed: `tools\cli test --no-build --only LF_Test_EveronAnniversaryLetter` failed every assertion (no letter prefab, no quest row, no dialogue, no memory change); green once the four pack files landed. The act (reading the tables, the hand-in) is skipped under RedProof().
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
	static const string DIALOGUE_ID = "anniversary_old_man";
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
	protected void DialogueCheck(LF_TestContext ctx)
	{
		ResourceName path;
		if (!RedProof())
			path = LF_DialogueRegistry.Resolve(DIALOGUE_ID);

		ctx.True(!path.IsEmpty(), "the dialogue id " + DIALOGUE_ID + " resolves through the pack index");
		if (path.IsEmpty())
			return;

		LF_DialogueConfig config = LF_DialogueConfig.Load(path);
		ctx.NotNull(config, "the letter dialogue loads");
		if (!config)
			return;

		ctx.EqualStr(DIALOGUE_ID, config.m_sId, "the dialogue is the letter talk");
		array<string> problems = {};
		LF_DialogueFlow.Problems(config, problems);
		LF_DialogueFlow.HookProblems(config, problems);
		ctx.Equal(0, problems.Count(), "the letter dialogue is playable");

		array<string> ids = {"anniversary.letter.ask", "anniversary.letter.secret"};
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

		// The secret is told only to the player who handed the letter in.
		bool gated;
		LF_DialogueNode start = LF_DialogueFlow.FindNode(config, config.m_sStart);
		if (start && start.m_aOptions)
		{
			foreach (LF_DialogueOption option : start.m_aOptions)
			{
				if (option.m_sNext == "anniversary.letter.secret" && option.m_sCondition == "QuestTurnedIn" && option.m_sParam == QUEST_ID)
					gated = true;
			}
		}

		ctx.True(gated, "the secret sits behind the QuestTurnedIn condition of the letter quest");
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
	}
}
