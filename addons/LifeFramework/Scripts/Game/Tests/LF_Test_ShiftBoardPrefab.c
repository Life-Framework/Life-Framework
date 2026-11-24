// red-proof: batched `tools\cli test --no-build --red LF_Test_ShiftBoardPrefab` skips the spawn via RedProof(); before LF_ShiftBoard.et existed "prefab loads" failed.
//! The work-site shift board as a placeable prop (#4713, chapter #4170). #4712 shipped the board kind
//! and read action; the wall prefab, on the vanilla notice board, carries it, and the posted one (the mine
//! camp, standing on its own posts with the title painted on) does too. Each spawns, carries one read action
//! of the class the kind names, and the kind is registered.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_ShiftBoardPrefab : LF_Test
{
	protected static const ResourceName SHIFT_BOARD = "{A4E81D6C2F9B3057}Prefabs/Props/LF_ShiftBoard.et";
	protected static const ResourceName SHIFT_BOARD_POST = "{B747EFA4D94F3C2F}Prefabs/Props/LF_ShiftBoardPost.et";

	override string GetName()
	{
		return "level/shift-board-prefab";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		CheckBoard(ctx, SHIFT_BOARD, "wall board");
		CheckBoard(ctx, SHIFT_BOARD_POST, "posted board");

		// the posted board's painted title is a sign def with its strings, so the face is never blank
		LF_SignDef def = LF_SignCatalog.Find("shift_board");
		ctx.NotNull(def, "sign def shift_board exists for the posted board's title");
		if (def)
		{
			ctx.True(!def.m_sTitle.IsEmpty(), "shift_board has a title");
			ctx.True(WidgetManager.Translate(def.m_sTitle) != def.m_sTitle, "shift_board title is a localization key that resolves");
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckBoard(LF_TestContext ctx, ResourceName prefab, string what)
	{
		Resource resource = Resource.Load(prefab);
		ctx.True(resource && resource.IsValid(), what + " prefab loads");
		if (!resource || !resource.IsValid())
			return;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.LOCAL;
		IEntity prop;
		if (!RedProof())
			prop = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);

		ctx.NotNull(prop, what + " spawns");
		if (!prop)
			return;

		LF_BoardProvider provider = LF_BoardProviders.Find(LF_ShiftBoard.KIND);
		ctx.NotNull(provider, "kind " + LF_ShiftBoard.KIND + " is registered");
		string actionClass;
		if (provider)
			actionClass = provider.GetReadAction();

		int reads = 0;
		ActionsManagerComponent actions = LF_Component<ActionsManagerComponent>.Find(prop);
		ctx.NotNull(actions, what + " has an actions manager");
		if (actions)
		{
			array<BaseUserAction> actionList = {};
			actions.GetActionsList(actionList);
			string seen;
			foreach (BaseUserAction action : actionList)
			{
				seen = seen + " " + action.ClassName();
				if (action.ClassName() == actionClass)
					reads++;
			}
			ctx.Equal(1, reads, what + " carries one " + actionClass + " (has:" + seen + ")");
			SCR_EntityHelper.DeleteEntityAndChildren(prop);
			return;
		}

		ctx.Equal(1, reads, what + " carries one " + actionClass);
		SCR_EntityHelper.DeleteEntityAndChildren(prop);
	}
}
