// red-proof (#4439): batched run `tools\cli test --no-build --red LF_Test_EveronBarkeeper`: RedProof() skips loading the demand rows and the dialogue, so `has a demand row`, `the dialogue loads` and every check behind them fail (RED-OK everon/barkeeper). Before the NPC prefab and the bars conf exist the same checks fail on `the barkeeper prefab loads` and `has a demand row`.
//! #4439 (crime B5, the barkeeper of 0.3.0; the plan's Josette, a man because every character body is male):
//! under the Everon 1989 header (the pack leg of `tools\cli test`) the barkeeper prefab carries the under-the-counter
//! outlet (`LF_CounterOutletComponent` and its sell action) with a buyer id whose demand rows buy cigarette cartons and
//! vodka bottles at village bar prices, a weekly cap, and the pack's other bar has its own rows. The prices are inside
//! the crime tier 1 band of the plan's economy table: what a full weekly purse of one bar pays is $500 to $650, one
//! hour of the tier 1 crime run. His dialogue loads, is playable and fully voiced, and every node's line id resolves.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronBarkeeper : LF_Test
{
	static const ResourceName DIALOGUE = "{56B1214BB3E57BCF}Configs/Dialogue/LF_Dialogue_Barkeeper.conf";
	static const ResourceName NPC_PREFAB = "{4439B0A5C0000003}Configs/Npcs/LF_Npc_Townsfolk_Barkeeper.et";
	static const ResourceName CARTON = "{37C18698D9B51EAE}Prefabs/Items/Contraband/LF_CigaretteCarton.et";
	static const ResourceName VODKA = "{23AE52677634DDD2}Prefabs/Items/Contraband/LF_VodkaBottle.et";
	static const string BAR_CHOTAIN = "bar_chotain";
	static const string BAR_LEVIE = "bar_levie";
	static const int WEEK_HOURS = 168;
	// Crime tier 1, expected clean per hour (docs/roadmap/next-release-0.3.0.md "The economy table").
	static const int TIER1_LOW = 500;
	static const int TIER1_HIGH = 650;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/barkeeper";
	}

	//------------------------------------------------------------------------------------------------
	protected static bool HasText(string key)
	{
		string text = WidgetManager.Translate(key);
		return !key.IsEmpty() && !text.IsEmpty() && text != key;
	}

	//------------------------------------------------------------------------------------------------
	//! How many actions of `className` the prefab's ActionsManagerComponent adds.
	protected static int CountActions(Resource res, string className)
	{
		IEntitySource source = res.GetResource().ToEntitySource();
		if (!source)
			return 0;

		int count;
		for (int i = 0, n = source.GetComponentCount(); i < n; i++)
		{
			IEntityComponentSource component = source.GetComponent(i);
			if (component.GetClassName() != "ActionsManagerComponent")
				continue;

			BaseContainerList actions = component.GetObjectArray("additionalActions");
			if (!actions)
				continue;

			for (int a = 0; a < actions.Count(); a++)
			{
				if (actions.Get(a).GetClassName() == className)
					count++;
			}
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! The barkeeper prefab: the outlet, its buyer id, the sell action, the talk action, the dialogue id.
	//! \return The buyer id of the outlet ("" when there is none).
	protected string PrefabCheck(LF_TestContext ctx)
	{
		Resource npc = Resource.Load(NPC_PREFAB);
		ctx.True(npc && npc.IsValid(), "the barkeeper prefab loads");
		if (!npc || !npc.IsValid())
			return "";

		string buyer;
		IEntityComponentSource outlet = SCR_BaseContainerTools.FindComponentSource(npc, LF_CounterOutletComponent);
		ctx.True(outlet != null, "the barkeeper prefab carries the under-the-counter outlet");
		if (outlet)
			outlet.Get("m_sBuyerId", buyer);

		ctx.True(!buyer.IsEmpty(), "the outlet names a buyer id");
		ctx.Equal(1, CountActions(npc, "LF_CounterOutletAction"), "the barkeeper carries exactly one sell action");
		ctx.Equal(1, CountActions(npc, "LF_DialogueTalkAction"), "the barkeeper carries exactly one talk action");

		string dialogueId;
		IEntityComponentSource talk = SCR_BaseContainerTools.FindComponentSource(npc, LF_DialogueComponent);
		ctx.True(talk != null, "the barkeeper prefab carries a dialogue");
		if (talk)
			talk.Get("m_sDialogueId", dialogueId);

		ctx.EqualStr("barkeeper", dialogueId, "the barkeeper prefab names his dialogue");
		ctx.EqualStr(DIALOGUE, LF_DialogueRegistry.Resolve("barkeeper"), "the index resolves the barkeeper's dialogue");
		return buyer;
	}

	//------------------------------------------------------------------------------------------------
	//! One bar's rows: both goods, a weekly cap, a price inside the village-bar range, a full purse worth a tier 1 hour.
	protected void RowsCheck(LF_TestContext ctx, LF_BuyerDemandConfig demand, string buyer)
	{
		LF_BuyerDemandRow carton = demand.Find(buyer, CARTON);
		LF_BuyerDemandRow vodka = demand.Find(buyer, VODKA);
		ctx.NotNull(carton, buyer + " has a demand row for the cigarette carton");
		ctx.NotNull(vodka, buyer + " has a demand row for the vodka bottle");
		if (!carton || !vodka)
			return;

		ctx.Equal(WEEK_HOURS, carton.m_iPeriodHours, buyer + " cartons: the cap is weekly");
		ctx.Equal(WEEK_HOURS, vodka.m_iPeriodHours, buyer + " vodka: the cap is weekly");
		ctx.Equal(0, carton.m_iRefillPerHour, buyer + " cartons: a period row has no hourly refill");
		ctx.Equal(0, vodka.m_iRefillPerHour, buyer + " vodka: a period row has no hourly refill");
		ctx.True(carton.m_iCap >= 1 && carton.m_iCap <= 3, buyer + " takes one to three cartons a week");
		ctx.True(vodka.m_iCap >= 3 && vodka.m_iCap <= 8, buyer + " takes three to eight bottles a week");
		ctx.True(carton.m_iPrice >= 100 && carton.m_iPrice <= 200, buyer + " pays $100 to $200 a carton");
		ctx.True(vodka.m_iPrice >= 30 && vodka.m_iPrice <= 80, buyer + " pays $30 to $80 a bottle");

		int week = carton.m_iCap * carton.m_iPrice + vodka.m_iCap * vodka.m_iPrice;
		ctx.True(week >= TIER1_LOW && week <= TIER1_HIGH, string.Format("%1: a full weekly purse pays $%2, inside the tier 1 hour of $%3 to $%4", buyer, week, TIER1_LOW, TIER1_HIGH));
	}

	//------------------------------------------------------------------------------------------------
	//! Every line of the conversation is voiced and captioned, and every node's line id resolves.
	protected void DialogueCheck(LF_TestContext ctx, LF_DialogueConfig config)
	{
		ctx.EqualStr("barkeeper", config.m_sId, "the dialogue is the barkeeper's");
		ctx.EqualStr("Josselin Vidal", WidgetManager.Translate(config.m_sSpeakerKey), "the speaker is Josselin Vidal");
		ctx.True(!config.m_sSoundProject.IsEmpty(), "the dialogue names an audio project");

		array<string> problems = {};
		LF_DialogueFlow.Problems(config, problems);
		ctx.Equal(0, problems.Count(), "the dialogue is playable");

		ctx.True(config.m_aLines && config.m_aLines.Count() >= 5, "the dialogue has its five lines");
		if (config.m_aLines)
		{
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

		if (!config.m_aNodes)
			return;

		foreach (LF_DialogueNode node : config.m_aNodes)
		{
			if (!node)
				continue;

			if (!node.m_sLine.IsEmpty())
				ctx.NotNull(LF_DialogueFlow.FindLine(config, node.m_sLine), "node " + node.m_sId + ": its line id " + node.m_sLine + " resolves");

			if (!node.m_aOptions)
				continue;

			foreach (LF_DialogueOption option : node.m_aOptions)
			{
				if (option)
					ctx.True(HasText(option.m_sTextKey), "node " + node.m_sId + ": a reply has its text");
			}
		}

		LF_DialogueLine buy = LF_DialogueFlow.FindLine(config, "buy");
		ctx.NotNull(buy, "the barkeeper has a line about what he buys");
		if (buy)
		{
			string said = WidgetManager.Translate(buy.m_sTextKey);
			ctx.True(said.Contains("cartons") && said.Contains("vodka"), "the buy line names cartons and vodka");
		}
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// The act: read the pack's demand rows and load the conversation from its conf.
		LF_BuyerDemandConfig demand;
		LF_DialogueConfig config;
		if (!RedProof())
		{
			demand = LF_BuyerDemandConfig.Get();
			config = LF_DialogueConfig.Load(DIALOGUE);
		}

		string buyer = PrefabCheck(ctx);
		ctx.NotNull(demand, "the buyer demand rows load");
		if (demand)
		{
			// The prefab's own buyer id must have rows; the second bar is placed with its own id.
			ctx.EqualStr(BAR_CHOTAIN, buyer, "the prefab's outlet is the Chotain bar's");
			RowsCheck(ctx, demand, BAR_CHOTAIN);
			RowsCheck(ctx, demand, BAR_LEVIE);
		}

		ctx.NotNull(config, "the dialogue loads");
		if (config)
			DialogueCheck(ctx, config);
	}
}
