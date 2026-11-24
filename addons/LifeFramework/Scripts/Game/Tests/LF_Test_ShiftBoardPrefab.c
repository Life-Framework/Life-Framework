// red-proof: batched `tools\cli test --no-build --red LF_Test_ShiftBoardPrefab` skips the spawn via RedProof(); before LF_ShiftBoard.et existed "prefab loads" failed.
//! The work-site shift board as a placeable wall prop (#4713, chapter #4170). #4712 shipped the board kind
//! and read action; this prefab, on the vanilla notice board, carries it. It spawns, carries one read action
//! of the class the kind names, and the kind is registered.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_ShiftBoardPrefab : LF_Test
{
	protected static const ResourceName SHIFT_BOARD = "{A4E81D6C2F9B3057}Prefabs/Props/LF_ShiftBoard.et";

	override string GetName()
	{
		return "level/shift-board-prefab";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		Resource resource = Resource.Load(SHIFT_BOARD);
		ctx.True(resource && resource.IsValid(), "shift board prefab loads");
		if (!resource || !resource.IsValid())
			return;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.LOCAL;
		IEntity prop;
		if (!RedProof())
			prop = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);

		ctx.NotNull(prop, "shift board spawns");
		if (!prop)
			return;

		LF_BoardProvider provider = LF_BoardProviders.Find(LF_ShiftBoard.KIND);
		ctx.NotNull(provider, "kind " + LF_ShiftBoard.KIND + " is registered");
		string actionClass;
		if (provider)
			actionClass = provider.GetReadAction();

		int reads = 0;
		ActionsManagerComponent actions = LF_Component<ActionsManagerComponent>.Find(prop);
		ctx.NotNull(actions, "shift board has an actions manager");
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

		ctx.Equal(1, reads, "shift board carries one " + actionClass);
		SCR_EntityHelper.DeleteEntityAndChildren(prop);
	}
}
