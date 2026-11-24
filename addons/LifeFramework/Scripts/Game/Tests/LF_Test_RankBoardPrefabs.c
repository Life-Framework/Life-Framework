// red-proof: 2026-11-24 first run before LF_RankBoard.et and LF_CommendationPlaque.et existed failed "prefab loads" for both; the batched `tools\cli test --no-build --red LF_Test_RankBoardPrefabs` skips the spawn via RedProof().
//! The station's rank board and commendations plaque as placeable wall props (#4698, chapter #4170).
//! #4206 shipped the board kinds and read actions; the two prefabs carry them. Each prefab spawns,
//! carries one read action of the class its board kind names, and the kind is registered.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_RankBoardPrefabs : LF_Test
{
	protected static const ResourceName RANK_BOARD = "{6F2A9C41B7D305E8}Prefabs/Props/Police/LF_RankBoard.et";
	protected static const ResourceName PLAQUE = "{B1E4D7A290C86F35}Prefabs/Props/Police/LF_CommendationPlaque.et";

	override string GetName()
	{
		return "police/rank-board-prefabs";
	}

	//------------------------------------------------------------------------------------------------
	//! Spawn one prop, count its read actions of the kind's action class, and check the kind names it.
	protected void CheckProp(LF_TestContext ctx, ResourceName prefab, string kind, string label)
	{
		Resource resource = Resource.Load(prefab);
		ctx.True(resource && resource.IsValid(), label + " prefab loads");
		if (!resource || !resource.IsValid())
			return;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.LOCAL;
		IEntity prop;
		if (!RedProof())
			prop = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);

		ctx.NotNull(prop, label + " spawns");
		if (!prop)
			return;

		LF_BoardProvider provider = LF_BoardProviders.Find(kind);
		ctx.NotNull(provider, label + " kind " + kind + " is registered");
		string actionClass;
		if (provider)
			actionClass = provider.GetReadAction();

		int reads = 0;
		ActionsManagerComponent actions = LF_Component<ActionsManagerComponent>.Find(prop);
		ctx.NotNull(actions, label + " has an actions manager");
		if (actions)
		{
			array<BaseUserAction> actionList = {};
			actions.GetActionsList(actionList);
			foreach (BaseUserAction action : actionList)
			{
				if (action.ClassName() == actionClass)
					reads++;
			}
		}

		ctx.Equal(1, reads, label + " carries one " + actionClass);
		SCR_EntityHelper.DeleteEntityAndChildren(prop);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		CheckProp(ctx, RANK_BOARD, LF_RankBoard.KIND, "rank board");
		CheckProp(ctx, PLAQUE, LF_CommendationPlaque.KIND, "commendations plaque");
	}
}
