// red-proof: batched `tools\cli test --no-build --red LF_Test_EveronShipmentRumours`: RedProof() loads the café owner's plain core dialogue (no pack patch) and an unknown quest param, so no rumour rows resolve and the gate checks fail.
//! #3441 (M7 arc 3, beat 3 of #1036): "Rumours in the pub". Once the crate is opened the café talks. Under the Everon 1989
//! header (the pack leg of `tools\cli test`): the café owner Odilon Marchand (resident `cafe_owner`, the barman's role,
//! #3438) gains a "Any talk in the café lately?" reply on every greeting behind the QuestTurnedInRecent condition, and
//! it leads to one line for a player who reported (`shipment_report`) and another for one who sold (`shipment_sold`),
//! each offered for 3 game days. Her old replies stay. The resident exists and the quest ids are the real ones.
//! `old_man` and `night_shift` are not patched: their greeting nodes already show four replies (the screen's limit).
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronShipmentRumours : LF_Test
{
	static const ResourceName CAFE_OWNER = "{8A8B0025C1E0037F}Configs/Dialogue/LF_Dialogue_CafeOwner.conf";
	static const string RESIDENT_ID = "cafe_owner";
	static const string CONDITION = "QuestTurnedInRecent";
	static const string MENU = "rumour.shipment";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/shipment-rumours";
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_DialogueOption FindReply(LF_DialogueNode node, string next)
	{
		if (!node || !node.m_aOptions)
			return null;

		foreach (LF_DialogueOption option : node.m_aOptions)
		{
			if (option.m_sNext == next)
				return option;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool HasText(string key)
	{
		string text = WidgetManager.Translate(key);
		return !key.IsEmpty() && !text.IsEmpty() && text != key;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MissionHeader header = LF_MissionHeader.GetCurrent();
		ctx.NotNull(header, "the pack leg runs under an LF mission header");

		ResidentCheck(ctx);
		RowCheck(ctx);
		FadeCheck(ctx);
	}

	//------------------------------------------------------------------------------------------------
	//! The speaker is a real resident, and the quest ids the rows gate on are the shipment's real ones.
	protected void ResidentCheck(LF_TestContext ctx)
	{
		LF_ResidentsConfig residents = LF_ResidentsConfig.Get();
		ctx.NotNull(residents, "the resident table loads under the pack header");
		if (residents && residents.m_aResidents)
		{
			bool found;
			foreach (LF_ResidentDef resident : residents.m_aResidents)
			{
				if (resident && resident.m_sId == RESIDENT_ID)
					found = true;
			}

			ctx.True(found, "the rumours are told by the resident " + RESIDENT_ID);
		}

		LF_QuestGiverConfig table = LF_QuestGiverTable.Get();
		ctx.NotNull(table, "the quest giver table loads under the pack header");
		if (!table)
			return;

		LF_QuestGiverRow harbour = table.Find("harbour_master");
		LF_QuestGiverRow desk = table.Find("police_desk");
		ctx.True(HasQuest(harbour, "shipment_sold"), "shipment_sold is a real quest");
		ctx.True(HasQuest(desk, "shipment_report"), "shipment_report is a real quest");
	}

	//------------------------------------------------------------------------------------------------
	protected static bool HasQuest(LF_QuestGiverRow row, string questId)
	{
		if (!row || !row.m_aQuests)
			return false;

		foreach (LF_QuestDefinition quest : row.m_aQuests)
		{
			if (quest && quest.m_sQuestId == questId)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The patched café owner: the rumour reply on each greeting, the menu, both rumour lines, and nothing lost.
	protected void RowCheck(LF_TestContext ctx)
	{
		// The act: the dialogue the café owner plays, the core file with every pack patch applied.
		// Red proof: the plain core file, which has no rumour rows.
		LF_DialogueConfig config;
		if (RedProof())
			config = LF_DialogueConfig.Load(CAFE_OWNER);
		else
			config = LF_DialogueConfig.Layered(CAFE_OWNER);

		ctx.NotNull(config, "the café owner's dialogue loads");
		if (!config)
			return;

		ctx.EqualStr(RESIDENT_ID, config.m_sId, "the dialogue is the café owner's");

		array<string> problems = {};
		LF_DialogueFlow.Problems(config, problems);
		LF_DialogueFlow.HookProblems(config, problems);
		ctx.Equal(0, problems.Count(), "the café owner's dialogue with the rumours is playable");

		// Every greeting offers the rumour reply, behind the fade gate, and keeps what it had.
		array<string> greetings = {"greet", "greet_friendly", "greet_regular"};
		foreach (string greeting : greetings)
		{
			LF_DialogueNode node = LF_DialogueFlow.FindNode(config, greeting);
			ctx.NotNull(node, "the café owner has the greeting " + greeting);
			if (!node)
				continue;

			LF_DialogueOption ask = FindReply(node, MENU);
			ctx.NotNull(ask, greeting + " offers the rumour reply");
			if (ask)
			{
				ctx.EqualStr(CONDITION, ask.m_sCondition, greeting + " gates the rumour reply on a recent turn-in");
				array<string> ids = {};
				float days;
				ctx.True(LF_DialogueHook_QuestTurnedInRecent.ParseParam(ask.m_sParam, ids, days), greeting + " has a readable fade param");
				ctx.True(ids.Contains("shipment_report") && ids.Contains("shipment_sold"), greeting + " gates on both shipment endings");
				ctx.True(days == 3, greeting + " fades after 3 game days");
				ctx.True(HasText(ask.m_sTextKey), greeting + " rumour reply has its text");
			}

			ctx.NotNull(FindReply(node, "zone"), greeting + " keeps asking about the zone");
			ctx.NotNull(FindReply(node, "help"), greeting + " keeps asking for help");
			ctx.NotNull(FindReply(node, "bye"), greeting + " keeps the goodbye");
		}

		// The menu splits on the ending: a reporter and a seller hear different talk, each gated on its own quest.
		LF_DialogueNode menu = LF_DialogueFlow.FindNode(config, MENU);
		ctx.NotNull(menu, "the rumour menu exists");
		RumourCheck(ctx, config, menu, "rumour.shipment.reported", "shipment_report");
		RumourCheck(ctx, config, menu, "rumour.shipment.sold", "shipment_sold");

		LF_DialogueLine reported = LF_DialogueFlow.FindLine(config, "rumour.shipment.reported");
		LF_DialogueLine sold = LF_DialogueFlow.FindLine(config, "rumour.shipment.sold");
		if (reported && sold)
			ctx.True(WidgetManager.Translate(reported.m_sTextKey) != WidgetManager.Translate(sold.m_sTextKey), "a reporter and a seller hear different rumours");
	}

	//------------------------------------------------------------------------------------------------
	protected void RumourCheck(LF_TestContext ctx, LF_DialogueConfig config, LF_DialogueNode menu, string next, string questId)
	{
		LF_DialogueOption reply = FindReply(menu, next);
		ctx.NotNull(reply, "the menu leads to " + next);
		if (reply)
		{
			ctx.EqualStr(CONDITION, reply.m_sCondition, next + " is gated on a recent turn-in");
			array<string> ids = {};
			float days;
			ctx.True(LF_DialogueHook_QuestTurnedInRecent.ParseParam(reply.m_sParam, ids, days), next + " has a readable fade param");
			ctx.Equal(1, ids.Count(), next + " names exactly one quest");
			ctx.True(ids.Contains(questId), next + " is told only to a player who did " + questId);
			ctx.True(days == 3, next + " fades after 3 game days");
		}

		ctx.NotNull(LF_DialogueFlow.FindNode(config, next), "the dialogue has the node " + next);
		LF_DialogueLine line = LF_DialogueFlow.FindLine(config, next);
		ctx.NotNull(line, "the dialogue has the line " + next);
		if (line)
			ctx.True(HasText(line.m_sTextKey), next + " has its caption in the string table");
	}

	//------------------------------------------------------------------------------------------------
	//! The fade: the condition reads `id[,id]:days` and a turn-in stops counting after that many game days.
	protected void FadeCheck(LF_TestContext ctx)
	{
		string param = "shipment_report,shipment_sold:3";
		if (RedProof())
			param = "shipment_report";

		array<string> ids = {};
		float days;
		ctx.True(LF_DialogueHook_QuestTurnedInRecent.ParseParam(param, ids, days), "the fade param reads");
		ctx.Equal(2, ids.Count(), "the fade param names two quests");
		ctx.True(days == 3, "the fade param is 3 game days");

		float day = LF_DialogueHook_QuestTurnedInRecent.GAME_DAY_SECONDS;
		ctx.True(LF_DialogueHook_QuestTurnedInRecent.IsRecent(100, 100 + day * 2.9, 3), "talk lasts through the third game day");
		ctx.False(LF_DialogueHook_QuestTurnedInRecent.IsRecent(100, 100 + day * 3.1, 3), "talk has faded after the third game day");
		ctx.False(LF_DialogueHook_QuestTurnedInRecent.IsRecent(-1, 100, 3), "no turn-in means no talk");
		ctx.False(LF_DialogueHook_QuestTurnedInRecent.ParseParam("shipment_report", ids, days), "a param with no days is refused");
		ctx.False(LF_DialogueHook_QuestTurnedInRecent.ParseParam("shipment_report:0", ids, days), "zero days is refused");
	}
}
