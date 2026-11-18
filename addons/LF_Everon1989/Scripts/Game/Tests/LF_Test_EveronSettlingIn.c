// red-proof: 2026-11-18, `tools\cli test --no-build --red LF_Test_EveronSettlingIn` (RedProof skips the overlay apply): RED-OK, failed "the pack adds a row for the clerk"; green without the hook (pack leg worlds=LF_Everon1989_Test).
//! #3125 in game: under the Everon 1989 header (the pack leg of `tools\cli test`), the clerk offers a second chain
//! after `arrival_done` that says what hours two to four are for: five parcel runs, a fuel run, a car, then three
//! errands (a firearm permit, a fish sold, a plot sign read), then a foundation laid on a plot (#3195). The neutral core table ships no clerk row; the pack's
//! overlay (LF_QuestGiverTable_Everon1989.conf) adds it. The test reads the rows, not the world: each step needs
//! the one before, every objective key is a signal the game reports, rewards are one-off and small, and every
//! string is a pack key that translates.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronSettlingIn : LF_Test
{
	static const ResourceName OVERLAY = "{FBE2DEDC5C6ACC67}Configs/Quests/LF_QuestGiverTable_Everon1989.conf";
	static const string GIVER = "clerk";
	static const string CHAIN_START = "arrival_done";
	static const int STEPS = 7;
	static const int MAX_STEP_CASH = 100; // lf-fixed: the planner's cap per step (#3125; the foundation step pays 100, #3195)
	static const int MAX_CHAIN_CASH = 250; // lf-fixed: the planner's cap for the chain (#3125, #3195)

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/settling-in";
	}

	//------------------------------------------------------------------------------------------------
	//! The core table as shipped, before any overlay.
	protected LF_QuestGiverConfig LoadCore()
	{
		Resource container = BaseContainerTools.LoadContainer(LF_QuestGiverConfig.CONFIG_PATH);
		if (!container || !container.IsValid() || !container.GetResource())
			return null;

		LF_QuestGiverConfig config = LF_QuestGiverConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (config && !config.m_aGivers)
			config.m_aGivers = new array<ref LF_QuestGiverRow>();

		return config;
	}

	//------------------------------------------------------------------------------------------------
	//! The quest of this id in the row, or null.
	protected LF_QuestDefinition Quest(LF_QuestGiverRow row, string questId)
	{
		foreach (LF_QuestDefinition quest : row.m_aQuests)
		{
			if (quest && quest.m_sQuestId == questId)
				return quest;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// The leg boots under the pack header, and that header lists the overlay.
		LF_MissionHeader header = LF_MissionHeader.GetCurrent();
		ctx.NotNull(header, "the pack leg runs under an LF mission header");
		if (header)
			ctx.True(header.m_aLF_ConfigOverlays && header.m_aLF_ConfigOverlays.Find(OVERLAY) != -1, "the header lists the settling-in overlay");

		LF_QuestGiverConfig table = LoadCore();
		ctx.NotNull(table, "the core quest giver table loads");
		if (!table)
			return;

		ctx.True(table.Find(GIVER) == null, "the core table alone adds nothing to the clerk (the pack decides)");
		// The act: the running header's overlays layered over the core table.
		if (!RedProof())
			table.ApplyLayers();

		LF_QuestGiverRow row = table.Find(GIVER);
		ctx.NotNull(row, "the pack adds a row for the clerk");
		if (!row)
			return;

		ctx.EqualStr(string.Empty, LF_QuestGiverConfig.Problem(row), "the clerk's row is complete");
		ctx.Equal(STEPS, row.m_aQuests.Count(), "the row holds the seven settling-in quests");

		CheckChain(ctx, row);
		CheckObjectives(ctx, row);
		CheckRewards(ctx, row);
		CheckStrings(ctx, row);

		// The live table the clerk's giver reads gives the same quests after its prefab's own.
		LF_QuestGiverTable.ResetStatics();
		array<ref LF_QuestDefinition> prefabOwn = {};
		array<ref LF_QuestDefinition> live = LF_QuestGiverTable.QuestsFor(GIVER, prefabOwn);
		ctx.Equal(STEPS, live.Count(), "LF_QuestGiverTable hands the clerk the seven quests");
		ctx.Equal(0, LF_QuestGiverTable.RefusedCount(), "no settling-in quest id clashes with the clerk's prefab quests");
		LF_QuestGiverTable.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	//! The chain starts after the arrival chain and every step needs one before it: the first one needs
	//! `arrival_done`, the next ones each a quest of this chain, so none stands loose.
	protected void CheckChain(LF_TestContext ctx, LF_QuestGiverRow row)
	{
		array<string> ids = {"settle_parcels", "settle_fuel", "settle_wheels", "settle_permit", "settle_fish", "settle_plot", "settle_foundation"};
		array<string> previous = {CHAIN_START, "settle_parcels", "settle_fuel", "settle_wheels", "settle_wheels", "settle_wheels", "settle_plot"};
		foreach (int i, string id : ids)
		{
			LF_QuestDefinition quest = Quest(row, id);
			ctx.NotNull(quest, "the chain has " + id);
			if (!quest)
				continue;

			ctx.EqualStr(previous[i], quest.m_sRequiredPreviousQuestId, id + " needs " + previous[i]);
			ctx.False(quest.m_bRepeatable, id + " is one-off");

			// Only the first step is the clerk's offer; every later one starts on its own.
			if (i == 0)
				ctx.False(quest.m_bAutoAccept, id + " is offered by the clerk, not started for the player");
			else
				ctx.True(quest.m_bAutoAccept, id + " starts on its own");

			ctx.True(quest.m_bAutoTurnIn, id + " is turned in on its own, with no walk back");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! One signal objective per step, and each key is one the game reports: the job run and sign read keys of
	//! the arrival chain, or the hour-two keys of #3123.
	protected void CheckObjectives(LF_TestContext ctx, LF_QuestGiverRow row)
	{
		map<string, string> keys = new map<string, string>();
		keys.Set("settle_parcels", LF_QuestSignals.JobRunComplete(LF_EJobType.PARCEL));
		keys.Set("settle_fuel", LF_QuestSignals.JobRunComplete(LF_EJobType.FUEL_DELIVERY));
		keys.Set("settle_wheels", LF_QuestProgressSignals.VEHICLE_BOUGHT);
		keys.Set("settle_permit", LF_QuestProgressSignals.PERMIT_COLLECTED);
		keys.Set("settle_fish", LF_QuestProgressSignals.FISH_SOLD);
		keys.Set("settle_plot", LF_QuestSignals.SignRead("plot_for_sale"));
		keys.Set("settle_foundation", LF_QuestProgressSignals.StageDone("foundation"));

		foreach (string id, string key : keys)
		{
			LF_QuestDefinition quest = Quest(row, id);
			if (!quest)
				continue;

			ctx.Equal(1, quest.GetObjectiveCount(), id + " has one objective");
			LF_QuestObjective objective = quest.GetObjective(0);
			if (!objective)
				continue;

			ctx.True(objective.m_eType == LF_EQuestEventType.SIGNAL, id + " counts a signal");
			ctx.EqualStr(key, objective.m_sFilter, id + " counts " + key);
		}

		LF_QuestDefinition parcels = Quest(row, "settle_parcels");
		if (parcels && parcels.GetObjective(0))
			ctx.Equal(5, parcels.GetObjective(0).m_iTarget, "the parcel step is five runs");
	}

	//------------------------------------------------------------------------------------------------
	//! Rewards are small: at most 100 cash a step and 250 for the chain, plus job XP; no items, no ID card.
	protected void CheckRewards(LF_TestContext ctx, LF_QuestGiverRow row)
	{
		int total;
		foreach (LF_QuestDefinition quest : row.m_aQuests)
		{
			ctx.True(quest.m_iRewardMoney <= MAX_STEP_CASH, quest.m_sQuestId + " pays at most 100 cash");
			ctx.True(quest.m_iRewardMoney >= 0, quest.m_sQuestId + " pays no negative cash");
			ctx.False(quest.m_bRewardIdCard, quest.m_sQuestId + " hands no ID card");
			ctx.True(!quest.m_aRewardItems || quest.m_aRewardItems.IsEmpty(), quest.m_sQuestId + " hands no items");
			total += quest.m_iRewardMoney;
		}

		ctx.True(total <= MAX_CHAIN_CASH, string.Format("the whole chain pays at most 250 cash (paid %1)", total));
	}

	//------------------------------------------------------------------------------------------------
	//! Every title, description and turn-in line is a pack key that translates to text.
	protected void CheckStrings(LF_TestContext ctx, LF_QuestGiverRow row)
	{
		foreach (LF_QuestDefinition quest : row.m_aQuests)
		{
			array<string> keys = {quest.m_sTitleKey, quest.m_sDescriptionKey};
			if (!quest.m_sTurnInKey.IsEmpty())
				keys.Insert(quest.m_sTurnInKey);

			foreach (string key : keys)
			{
				ctx.True(key.IndexOf("LF-Quest_Settle") == 1, quest.m_sQuestId + " uses a settling-in string key: " + key);
				string text = WidgetManager.Translate(key);
				ctx.True(!text.IsEmpty() && text != key, quest.m_sQuestId + " key translates: " + key);
			}
		}
	}
}
