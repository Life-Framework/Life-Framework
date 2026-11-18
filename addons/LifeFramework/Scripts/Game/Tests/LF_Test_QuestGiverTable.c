// red-proof: observed 2026-11-18 `tools\cli test --no-build --red LF_Test_QuestGiverTable` printed `RED PROOF OK`; with the hook skipping the pack source it failed `- the clerk lists its prefab quests plus the pack's added quest` and `- an unknown giver's row never reaches another giver (expected 6, got 5)`. `--only` then passed with the table. `--tier all`: 1026 passed, 0 failed.
//! #3124: a pack gives an existing quest giver more quests by config (LF_QuestGiverTable). The test
//! overlay (Configs/Tests/LF_Test_QuestGiverTableOverlay.conf) is reached through a mission header built
//! in code: the clerk lists its five prefab quests plus one added quest, a row quest whose id the
//! prefab already has is refused (the prefab quest wins), and a row for a giver id nothing registers is
//! counted by the audit and never read.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_QuestGiverTable : LF_Test
{
	static const ResourceName OVERLAY = "{8D14F6A2C95B3E70}Configs/Tests/LF_Test_QuestGiverTableOverlay.conf";
	static const ResourceName CLERK_PREFAB = "{DE27E685D6FE93D7}Prefabs/Characters/Quests/QuestGiver_Clerk.et";
	static const string EXTRA_QUEST = "test_clerk_extra";
	static const string DUPLICATE_QUEST = "arrival_register";
	static const int PREFAB_QUESTS = 5;

	override string GetName()
	{
		return "quests/giver-table";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_ConfigService.Reset();
		LF_QuestGiverTable.ResetStatics();

		LF_MissionHeader header = new LF_MissionHeader();
		header.m_aLF_ConfigOverlays = {};
		header.m_aLF_ConfigOverlays.Insert(OVERLAY);
		LF_MissionConfigPackSource source = new LF_MissionConfigPackSource();
		source.SetHeader(header);
		if (!RedProof())
			LF_ConfigService.SetPackSource(source);

		IEntity clerk = LF_Utils.SpawnEntityPrefab(CLERK_PREFAB, "60 0 60");
		ctx.True(clerk != null, "the clerk fixture spawns");
		if (!clerk)
		{
			Cleanup();
			return;
		}

		LF_QuestGiverComponent giver = LF_QuestGiverComponent.Cast(clerk.FindComponent(LF_QuestGiverComponent));
		ctx.True(giver != null, "the clerk has a quest giver component");
		if (giver)
			Check(ctx, giver);

		SCR_EntityHelper.DeleteEntityAndChildren(clerk);
		Cleanup();
	}

	//------------------------------------------------------------------------------------------------
	protected void Check(LF_TestContext ctx, LF_QuestGiverComponent giver)
	{
		array<ref LF_QuestDefinition> quests = giver.GetQuests();
		ctx.Equal(PREFAB_QUESTS + 1, quests.Count(), "the clerk lists its prefab quests plus the pack's added quest");
		ctx.Equal(PREFAB_QUESTS, giver.m_aQuests.Count(), "the prefab list itself is left as the prefab wrote it");

		int extra;
		int duplicate;
		foreach (LF_QuestDefinition quest : quests)
		{
			if (quest.m_sQuestId == EXTRA_QUEST)
				extra++;
			if (quest.m_sQuestId == DUPLICATE_QUEST)
				duplicate++;
		}

		ctx.Equal(1, extra, "the added quest is offered once");
		ctx.Equal(1, duplicate, "a duplicate quest id is refused: the prefab quest stays the only one");
		ctx.Equal(1, LF_QuestGiverTable.RefusedCount(), "the duplicate is logged and counted once");
		giver.GetQuests();
		ctx.Equal(1, LF_QuestGiverTable.RefusedCount(), "asking again does not log the duplicate again");

		// The prefab's own copy wins: its title key is the prefab's, and the table's copy has no TALK objective.
		LF_QuestDefinition kept;
		foreach (LF_QuestDefinition candidate : quests)
		{
			if (candidate.m_sQuestId == DUPLICATE_QUEST)
				kept = candidate;
		}

		if (kept)
			ctx.True(kept.HasObjectives(), "the prefab's arrival_register (with its objective) wins");

		ctx.Equal(1, LF_QuestGiverTable.Audit(), "the row for a giver nothing registers is counted by the audit");
		ctx.True(LF_QuestManager.GetInstance().GetDefinition(EXTRA_QUEST) != null, "the added quest registers with the manager");
		ctx.Equal(PREFAB_QUESTS + 1, giver.GetQuests().Count(), "an unknown giver's row never reaches another giver");
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup()
	{
		LF_ConfigService.Reset();
		LF_QuestGiverTable.ResetStatics();
	}
}
