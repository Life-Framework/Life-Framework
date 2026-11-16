// red-proof: Observed 2026-11-16 with `tools\cli test --no-build --only LF_Test_OnboardingChain` on the unfixed clerk: `[LFTEST] FAIL quests/onboarding-chain`, `- no step rewards money: arrival_done (expected 0, got 250)`. With the cash removed from QuestGiver_Clerk.et and the gift moved to the pack table, green.
// tier: WORLD
//! #1781: the arrival chain ends with a welcome gift, not cash (#795). The clerk's quests link by
//! `m_sRequiredPreviousQuestId` with no gap and no cycle, no step rewards money, and every item a
//! step hands over (its own `m_aRewardItems` plus the pack's welcome-gift rows) resolves to a prefab.
class LF_Test_OnboardingChain : LF_Test
{
	static const ResourceName CLERK_PREFAB = "{DE27E685D6FE93D7}Prefabs/Characters/Quests/QuestGiver_Clerk.et";
	static const string GIVER_ID = "clerk";
	static const int CHAIN_LENGTH = 5;

	override string GetName()
	{
		return "quests/onboarding-chain";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// The clerk is a spawned fixture: its quest definitions register when it initialises.
		IEntity clerk = LF_Utils.SpawnEntityPrefab(CLERK_PREFAB, "60 0 60");
		ctx.True(clerk != null, "the clerk fixture spawns");
		if (!clerk)
			return;

		CheckChain(ctx, LF_QuestManager.GetInstance());
		SCR_EntityHelper.DeleteEntityAndChildren(clerk);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckChain(LF_TestContext ctx, LF_QuestManager manager)
	{
		array<string> chain = {};
		string first;
		foreach (string firstId, LF_QuestDefinition firstDef : manager.GetDefinitions())
		{
			if (manager.GetGiverOfQuest(firstId) == GIVER_ID && firstDef.m_sRequiredPreviousQuestId.IsEmpty())
			{
				ctx.True(first.IsEmpty(), "the clerk's chain has one first step (also " + first + ")");
				first = firstId;
			}
		}

		ctx.False(first.IsEmpty(), "the clerk's chain has a first step");
		if (first.IsEmpty())
			return;

		// Follow the links: each next step names the previous one; a step seen twice is a cycle.
		string current = first;
		while (!current.IsEmpty())
		{
			if (chain.Find(current) != -1)
			{
				ctx.Fail("the chain has a cycle at " + current);
				return;
			}

			chain.Insert(current);
			string next;
			foreach (string nextId, LF_QuestDefinition nextDef : manager.GetDefinitions())
			{
				if (nextDef.m_sRequiredPreviousQuestId == current && manager.GetGiverOfQuest(nextId) == GIVER_ID)
				{
					ctx.True(next.IsEmpty(), "step " + current + " has one next step (also " + next + ")");
					next = nextId;
				}
			}

			current = next;
		}

		ctx.Equal(CHAIN_LENGTH, chain.Count(), "the chain runs from register to done with no gap");

		// Every clerk quest is on the chain: a step off it would be a gap nothing leads to.
		foreach (string onChainId, LF_QuestDefinition onChainDef : manager.GetDefinitions())
		{
			if (manager.GetGiverOfQuest(onChainId) == GIVER_ID)
				ctx.True(chain.Find(onChainId) != -1, "the clerk's quest is on the chain: " + onChainId);
		}

		foreach (string stepId : chain)
		{
			LF_QuestDefinition step = manager.GetDefinition(stepId);
			ctx.Equal(0, step.m_iRewardMoney, "no step rewards money: " + stepId);
			CheckItems(ctx, step);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Every item a step hands over loads: the quest's own and the welcome-gift table's.
	protected void CheckItems(LF_TestContext ctx, LF_QuestDefinition step)
	{
		array<ResourceName> items = {};
		if (step.m_aRewardItems)
			items.Copy(step.m_aRewardItems);
		LF_WelcomeGiftTable.ItemsOf(step.m_sQuestId, items);

		foreach (ResourceName item : items)
		{
			Resource prefab;
			if (!item.IsEmpty())
				prefab = Resource.Load(item);
			ctx.True(prefab != null && prefab.IsValid(), "a reward item resolves: " + step.m_sQuestId + " " + item);
		}
	}
}
