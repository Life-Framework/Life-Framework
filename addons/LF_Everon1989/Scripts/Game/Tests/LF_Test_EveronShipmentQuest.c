// red-proof: observed 2026-11-19 (#3440) before the data existed: `tools\cli test --no-build --only LF_Test_EveronShipmentQuest` failed every assertion (no quest rows, no dialogue, no award rows); green once the pack files and the header lines landed. The act (reading the three tables) is skipped under RedProof().
//! #3440 (M7 arc 3, beat 2 of #1036): "A crate is opened". The harbour master asks a character the town trusts to count
//! the red crate; opening it finds the sample under the tinned fish. The player reports it at the police desk or sells it.
//! Under the Everon 1989 header (the pack leg of `tools\cli test`): the quest rows load through the giver-table overlay,
//! the dialogue resolves through the pack's index and is playable, the ask reply sits behind the harbour crates contract's
//! standing gate (the TOWN gate, the harbour master's faction), and both endings name a real item and move standing.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronShipmentQuest : LF_Test
{
	static const ResourceName SAMPLE = "{A3BFEFD984031D81}Prefabs/Items/Contraband/LF_ShipmentSample.et";
	static const string GATE = "contract:harbour_crates";
	static const string DIALOGUE_ID = "harbour_master";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/shipment-quest";
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
	protected static LF_ReputationAwardDef FindAward(LF_ReputationConfig config, string action, string faction)
	{
		if (!config || !config.m_aAwards)
			return null;

		foreach (LF_ReputationAwardDef award : config.m_aAwards)
		{
			if (award && award.m_sAction == action && award.m_sFaction == faction)
				return award;
		}

		return null;
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
	//! True when the quest pays exactly the standing actions of `list` (";" between ids), each with a row in the tables.
	protected static bool HasAwards(LF_QuestDefinition quest, string list)
	{
		array<string> wanted = {};
		list.Split(";", wanted, false);
		if (!quest.m_aTurnInAwards || quest.m_aTurnInAwards.Count() != wanted.Count())
			return false;

		LF_ReputationConfig config = LF_ReputationConfig.Get();
		foreach (string action : wanted)
		{
			if (quest.m_aTurnInAwards.Find(action) == -1 || !config || !config.FindAward(action))
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool HasText(string key)
	{
		string text = WidgetManager.Translate(key);
		return !key.IsEmpty() && !text.IsEmpty() && text != key;
	}

	//------------------------------------------------------------------------------------------------
	protected void QuestCheck(LF_TestContext ctx)
	{
		LF_QuestGiverConfig table;
		if (!RedProof())
			table = LF_QuestGiverTable.Get();

		ctx.NotNull(table, "the quest giver table loads under the pack header");
		if (!table)
			return;

		LF_QuestGiverRow harbour = table.Find("harbour_master");
		LF_QuestGiverRow desk = table.Find("police_desk");
		ctx.NotNull(harbour, "the harbour master has a quest row");
		ctx.NotNull(desk, "the police desk has a quest row");

		LF_QuestDefinition count = FindQuest(harbour, "shipment_count");
		LF_QuestDefinition sold = FindQuest(harbour, "shipment_sold");
		LF_QuestDefinition report = FindQuest(desk, "shipment_report");
		ctx.NotNull(count, "the harbour master offers shipment_count");
		ctx.NotNull(sold, "the sell ending shipment_sold exists");
		ctx.NotNull(report, "the police desk takes shipment_report");

		ctx.True(Resource.Load(SAMPLE).IsValid(), "the sample item is a real prefab");
		array<LF_QuestDefinition> quests = {count, sold, report};
		foreach (LF_QuestDefinition quest : quests)
		{
			if (!quest)
				continue;

			ctx.True(HasText(quest.m_sTitleKey), quest.m_sQuestId + " has its title in the string table");
			ctx.True(HasText(quest.m_sDescriptionKey), quest.m_sQuestId + " has its description in the string table");
			ctx.True(quest.HasObjectives(), quest.m_sQuestId + " has an objective");
		}

		// Beat 2's opening: counting the crate is holding the sample.
		if (count)
		{
			LF_QuestObjective open = count.GetObjective(0);
			ctx.True(open && open.m_eType == LF_EQuestEventType.COLLECT_REPORT, "counting the crate is a collect objective");
			if (open)
			{
				ctx.EqualStr(SAMPLE, open.m_sFilter, "the count objective names the sample item");
				ctx.True(open.m_bKeepOnTurnIn, "counting leaves the sample in the bag");
			}
		}

		// Report ending: the police desk takes the sample in hand (a collect objective is removed from the bag on turn-in).
		if (report)
		{
			LF_QuestObjective hand = report.GetObjective(0);
			ctx.True(hand && hand.m_eType == LF_EQuestEventType.COLLECT_REPORT, "reporting hands the sample in at the desk");
			if (hand)
				ctx.EqualStr(SAMPLE, hand.m_sFilter, "the report objective names the sample item");

			ctx.EqualStr("shipment_count", report.m_sRequiredPreviousQuestId, "the report ending follows the count");
			ctx.False(report.m_bAutoTurnIn, "the report is turned in at the desk, not on its own");
			ctx.True(report.m_aExclusiveQuestIds && report.m_aExclusiveQuestIds.Find("shipment_sold") != -1, "reporting is a choice against selling");
			ctx.True(HasAwards(report, "shipment_report_town;shipment_report_police;shipment_report_underworld"), "reporting pays the town and the police and costs the underworld");
		}

		// Sell ending: the buyer paying closes it.
		if (sold)
		{
			LF_QuestObjective pay = sold.GetObjective(0);
			ctx.True(pay && pay.m_eType == LF_EQuestEventType.SIGNAL, "selling is closed by a signal");
			if (pay)
				ctx.EqualStr(LF_QuestSignals.BuyerSold(SAMPLE), pay.m_sFilter, "the sell objective listens for the buyer paying for the sample");

			ctx.EqualStr("shipment_count", sold.m_sRequiredPreviousQuestId, "the sell ending follows the count");
			ctx.True(sold.m_bAutoAccept && sold.m_bAutoTurnIn, "the sell ending starts and closes on its own");
			ctx.True(sold.m_aExclusiveQuestIds && sold.m_aExclusiveQuestIds.Find("shipment_report") != -1, "selling is a choice against reporting");
			ctx.True(HasAwards(sold, "shipment_sold_underworld;shipment_sold_town"), "selling pays the underworld and costs the town");
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void GateCheck(LF_TestContext ctx, LF_DialogueConfig config)
	{
		LF_ReputationConfig.Reload();
		LF_ReputationContractsConfig.Reload();
		LF_ReputationGateDef gate = LF_Reputation.FindGate(GATE);
		ctx.NotNull(gate, "the harbour crates gate has a row");
		if (gate)
			ctx.EqualStr("TOWN", gate.m_sFaction, "the standing gate names the harbour master's faction, the town");

		LF_ReputationContractDef def = LF_ReputationContracts.FindDef("harbour_crates");
		ctx.NotNull(def, "the harbour crates contract exists");
		if (def && gate)
			ctx.EqualStr(gate.m_sFaction, def.m_sFaction, "the contract and its gate name the same faction");

		// The question sits on the harbour master's work answer (the greeting has no room: four replies), behind the Contract condition.
		LF_DialogueNode job = LF_DialogueFlow.FindNode(config, "first_job");
		ctx.NotNull(job, "the harbour master still has his work answer");
		LF_DialogueOption ask = FindReply(job, "shipment.ask", "Contract");
		ctx.NotNull(ask, "the question about the crate sits behind the Contract condition");
		if (ask)
		{
			ctx.EqualStr("harbour_crates", ask.m_sParam, "the question reads the harbour crates gate");
			ctx.True(LF_DialogueHook.Find(ask.m_sCondition) != null, "the Contract condition has a hook class");
		}

		// The harbour master's own talk is kept: taking the job, the crates contract and the goodbyes.
		ctx.NotNull(FindReply(job, "go", ""), "the patch keeps taking the job");
		ctx.NotNull(FindReply(job, "bye", ""), "the patch keeps the not-now reply");
		LF_DialogueNode start = LF_DialogueFlow.FindNode(config, config.m_sStart);
		ctx.NotNull(FindReply(start, "first_job", ""), "the greeting still asks about work");
		ctx.NotNull(FindReply(start, "crates_offer", "Contract"), "the greeting still offers the crates");
		ctx.NotNull(FindReply(start, "crates_early", "ContractLocked"), "the greeting still holds the early-crates reply");

		// Taking the count runs the quest talk with the harbour master's giver.
		LF_DialogueNode asked = LF_DialogueFlow.FindNode(config, "shipment.ask");
		LF_DialogueOption take = FindReply(asked, "shipment.count", "QuestNotTurnedIn");
		ctx.NotNull(take, "the count is offered until it is turned in");
		if (take)
		{
			ctx.EqualStr("shipment_count", take.m_sParam, "the count reply reads shipment_count");
			ctx.EqualStr("QuestConverse", take.m_sAction, "the count reply takes the count from the harbour master's giver");
			ctx.True(LF_DialogueHook.Find(take.m_sAction) != null, "the QuestConverse action has a hook class");
		}
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

		LF_DialogueConfig config = LF_DialogueConfig.Layered(path);
		ctx.NotNull(config, "the harbour master dialogue loads with the shipment patch");
		if (!config)
			return;

		ctx.EqualStr(DIALOGUE_ID, config.m_sId, "the shipment talk is part of the harbour master dialogue");
		ctx.EqualStr("Marcel Roux", WidgetManager.Translate(config.m_sSpeakerKey), "the speaker is Marcel Roux");

		array<string> problems = {};
		LF_DialogueFlow.Problems(config, problems);
		LF_DialogueFlow.HookProblems(config, problems);
		ctx.Equal(0, problems.Count(), "the shipment dialogue is playable");

		array<string> ids = {"shipment.ask", "shipment.count", "shipment.thanks_report", "shipment.thanks_sell"};
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

		// Both endings thank the player only once the matching quest is turned in.
		LF_DialogueNode asked = LF_DialogueFlow.FindNode(config, "shipment.ask");
		LF_DialogueOption reported = FindReply(asked, "shipment.thanks_report", "QuestTurnedIn");
		LF_DialogueOption sold = FindReply(asked, "shipment.thanks_sell", "QuestTurnedIn");
		ctx.NotNull(reported, "Marcel thanks a player who reported it");
		ctx.NotNull(sold, "Marcel thanks a player who sold it");
		if (reported)
			ctx.EqualStr("shipment_report", reported.m_sParam, "the report thanks reads shipment_report");

		if (sold)
			ctx.EqualStr("shipment_sold", sold.m_sParam, "the sell thanks reads shipment_sold");

		GateCheck(ctx, config);
	}

	//------------------------------------------------------------------------------------------------
	protected void AwardCheck(LF_TestContext ctx)
	{
		LF_ReputationConfig config;
		if (!RedProof())
			config = LF_ReputationConfig.Get();

		ctx.NotNull(config, "the standing tables load");
		if (!config)
			return;

		LF_ReputationAwardDef reportTown = FindAward(config, "shipment_report_town", "TOWN");
		LF_ReputationAwardDef reportPolice = FindAward(config, "shipment_report_police", "POLICE");
		LF_ReputationAwardDef reportUnder = FindAward(config, "shipment_report_underworld", "UNDERWORLD");
		ctx.True(reportTown && reportTown.m_iPoints > 0, "reporting raises standing with the town");
		ctx.True(reportPolice && reportPolice.m_iPoints > 0, "reporting raises standing with the police");
		ctx.True(reportUnder && reportUnder.m_iPoints < 0, "reporting lowers standing with the underworld");

		LF_ReputationAwardDef soldUnder = FindAward(config, "shipment_sold_underworld", "UNDERWORLD");
		LF_ReputationAwardDef soldTown = FindAward(config, "shipment_sold_town", "TOWN");
		ctx.True(soldUnder && soldUnder.m_iPoints > 0, "selling raises standing with the underworld");
		ctx.True(soldTown && soldTown.m_iPoints < 0, "selling lowers standing with the town a little");
		if (soldTown && reportTown)
			ctx.True(-soldTown.m_iPoints < reportTown.m_iPoints, "the town loses less for a sale than it gains for a report");
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MissionHeader header = LF_MissionHeader.GetCurrent();
		ctx.NotNull(header, "the pack leg runs under an LF mission header");

		QuestCheck(ctx);
		DialogueCheck(ctx);
		AwardCheck(ctx);
		LF_ReputationConfig.Reload();
		LF_ReputationContractsConfig.Reload();
	}
}
