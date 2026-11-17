// red-proof: 2026-11-17, on the unfixed code (apple m_iMaxGathers 0) the 11th gather also paid: `--only LF_Test_GatherStockAction` failed "ten gathers pay and the eleventh is refused".
// tier: WORLD
//! gather/stock-action (#2409): the server stops paying when a spot is empty. The spot is the shipped
//! apple tree; ten gathers pay, the eleventh pays nothing.
class LF_Test_GatherStockAction : LF_Test
{
	static const ResourceName TREE = "{1C6F9AD2669C17F9}Prefabs/Vegetation/Tree/t_malus_domestica_2s.et";
	static const ResourceName APPLE = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";

	override string GetName()
	{
		return "gather/stock-action";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity user = LF_TestCharacterFixture.Spawn(ctx);
		if (!user)
			return;

		IEntity tree = LF_Utils.SpawnEntityPrefab(TREE, user.GetOrigin() + "4 0 0");
		ctx.NotNull(tree, "the apple tree spawns");
		LF_GatherAction gather = null;
		if (tree)
			gather = FindGatherAction(tree);
		ctx.NotNull(gather, "the tree carries an LF_GatherAction");

		if (gather)
		{
			int i;
			for (i = 0; i < 10; i++)
				gather.PerformAction(tree, user);

			int tenth = LF_InventoryUtils.GetAmount(user, APPLE);
			gather.PerformAction(tree, user);
			int eleventh = LF_InventoryUtils.GetAmount(user, APPLE);
			ctx.Equal(10, tenth, "ten gathers pay ten apples");
			ctx.Equal(10, eleventh, "ten gathers pay and the eleventh is refused");
		}

		if (tree)
			SCR_EntityHelper.DeleteEntityAndChildren(tree);
		LF_TestCharacterFixture.Delete(user);
	}

	//------------------------------------------------------------------------------------------------
	protected LF_GatherAction FindGatherAction(IEntity tree)
	{
		ActionsManagerComponent actionsManager = LF_Component<ActionsManagerComponent>.Find(tree);
		if (!actionsManager)
			return null;

		array<BaseUserAction> actions = {};
		actionsManager.GetActionsList(actions);
		foreach (BaseUserAction action : actions)
		{
			LF_GatherAction gather = LF_GatherAction.Cast(action);
			if (gather)
				return gather;
		}

		return null;
	}
};
